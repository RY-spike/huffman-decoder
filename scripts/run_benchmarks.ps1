param(
    [string[]]$InputFiles = @('data/corpus/alice_gutenberg11.txt', 'data/corpus/pride_gutenberg1342.txt'),
    [string]$OutputDirectory = '',
    [ValidateRange(0, 100000)][int]$Warmup = 3,
    [ValidateRange(1, 100000)][int]$Runs = 30,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$previousLocation = Get-Location
try {
    Set-Location -LiteralPath $repoRoot
    if (-not $OutputDirectory) {
        $OutputDirectory = 'results/run-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
    }
    if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a new output directory for each run.' }
    foreach ($inputFile in $InputFiles) {
        if (-not (Test-Path -LiteralPath $inputFile -PathType Leaf)) {
            throw "Missing input: $inputFile. Run scripts/fetch_corpus.ps1 first for the default corpus."
        }
    }
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCommand) {
        $cmakeExe = $cmakeCommand.Source
    } else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { throw 'CMake was not found.' }
        $vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $cmakeExe = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    }
    if (-not $SkipBuild) {
        & $cmakeExe -S . -B build -G 'Visual Studio 17 2022' -A x64
        if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
        & $cmakeExe --build build --config Release
        if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
    }
    $executable = Join-Path $repoRoot 'build/Release/benchmark_files.exe'
    if (-not (Test-Path -LiteralPath $executable)) { throw 'Benchmark executable is missing.' }
    $sourceFiles = @(& git ls-files --cached --others --exclude-standard) |
        Where-Object { $_ -match '\.(cpp|hpp)$' -or $_ -eq 'CMakeLists.txt' } |
        Sort-Object -Unique
    if ($SkipBuild) {
        $binaryTime = (Get-Item -LiteralPath $executable).LastWriteTimeUtc
        foreach ($file in $sourceFiles) {
            if ((Get-Item -LiteralPath $file).LastWriteTimeUtc -gt $binaryTime) {
                throw "Source is newer than the executable: $file. Run without -SkipBuild."
            }
        }
    }
    $sources = Get-Content -LiteralPath 'data/sources.json' -Raw | ConvertFrom-Json
    $inputRecords = @()
    for ($i = 0; $i -lt $InputFiles.Count; ++$i) {
        $file = $InputFiles[$i]
        $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        $source = $sources | Where-Object { $_.filename -eq [IO.Path]::GetFileName($file) } | Select-Object -First 1
        if ($source -and $source.sha256 -and $source.sha256 -ne $hash) {
            throw "Corpus hash does not match data/sources.json: $file"
        }
        $inputRecords += [pscustomobject]@{
            dataset_id=$i+1; path=$file; bytes=(Get-Item -LiteralPath $file).Length
            sha256=$hash; source_url=if ($source) {$source.url} else {$null}
        }
    }
    $sourceHashes = @($sourceFiles | ForEach-Object {
        [pscustomobject]@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
    $arguments = @('--output-dir', $OutputDirectory, '--warmup', "$Warmup", '--runs', "$Runs")
    foreach ($file in $InputFiles) { $arguments += @('--input', $file) }
    $started = [DateTime]::UtcNow.ToString('o')
    $cpu = (Get-ItemProperty -LiteralPath 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0' -Name ProcessorNameString).ProcessorNameString.Trim()
    $powerScheme = (& powercfg /getactivescheme | Out-String).Trim()
    $baseCommit = (& git rev-parse HEAD).Trim()
    $gitState = @(& git status --porcelain)
    $console = @(& $executable @arguments 2>&1)
    $benchmarkExit = $LASTEXITCODE
    if (Test-Path -LiteralPath $OutputDirectory) {
        $console | Set-Content -LiteralPath (Join-Path $OutputDirectory 'console.log') -Encoding utf8
    }
    if ($benchmarkExit -ne 0) { throw "Benchmark failed: $($console -join ' ')" }
    $fileChecks = @()
    $verification = Import-Csv -LiteralPath (Join-Path $OutputDirectory 'verification.csv')
    foreach ($entry in $verification) {
        if ($entry.status -ne 'PASS') { throw 'A verification row did not pass.' }
        if ($entry.restored_file) {
            $hash = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory $entry.restored_file) -Algorithm SHA256).Hash.ToLowerInvariant()
            $source = $inputRecords[[int]$entry.dataset_id - 1]
            if ($hash -ne $source.sha256) { throw "Restored file differs: $($entry.restored_file)" }
            $fileChecks += [pscustomobject]@{
                dataset_id=$entry.dataset_id; variant=$entry.variant
                restored_file=$entry.restored_file; sha256=$hash; matches_input=$true
            }
        }
    }
    $fileChecks | Export-Csv -LiteralPath (Join-Path $OutputDirectory 'file-checksums.csv') -NoTypeInformation -Encoding utf8
    $metadata = [ordered]@{
        started_utc=$started; finished_utc=[DateTime]::UtcNow.ToString('o')
        cpu=$cpu; logical_processors=[Environment]::ProcessorCount
        os=[Environment]::OSVersion.VersionString; process_bits=[IntPtr]::Size*8
        power_scheme=$powerScheme
        affinity='OS default; core type and background load not controlled'
        build_configuration='Release'; executable='build/Release/benchmark_files.exe'
        executable_sha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
        source_base_commit=$baseCommit; working_tree_state=$gitState; source_sha256=$sourceHashes
        arguments=$arguments; inputs=$inputRecords; restored_files=$fileChecks
    }
    $metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'metadata.json') -Encoding utf8
    $console | Write-Output
    Write-Output "Results saved in $OutputDirectory"
} finally {
    Set-Location -LiteralPath $previousLocation
}
