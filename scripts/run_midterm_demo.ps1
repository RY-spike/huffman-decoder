param([string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$previousLocation = Get-Location
$demoStatus = $null
try {
    Set-Location -LiteralPath $repoRoot
    if (-not $OutputDirectory) {
        $OutputDirectory = 'build/midterm-demo-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
    }
    if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a new output directory; existing runs are preserved.' }
    foreach ($required in @('build/Release/midterm_demo.exe', 'build/Release/benchmark_files.exe',
                            'data/corpus/alice_gutenberg11.txt', 'data/corpus/pride_gutenberg1342.txt')) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Missing $required. Prepare the Release build and corpus before the defense; see docs/midterm/demo-guide.md."
        }
    }
    $ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
    if ($ctestCommand) { $ctestExe = $ctestCommand.Source } else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { throw 'CTest was not found.' }
        $vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $ctestExe = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
        if (-not (Test-Path -LiteralPath $ctestExe)) { throw 'CTest was not found in Visual Studio.' }
    }
    $sourceFiles = @(& git ls-files --cached --others --exclude-standard) |
        Where-Object { $_ -match '\.(cpp|hpp)$' -or $_ -eq 'CMakeLists.txt' } | Sort-Object -Unique
    foreach ($binary in @('build/Release/midterm_demo.exe', 'build/Release/benchmark_files.exe')) {
        $binaryTime = (Get-Item -LiteralPath $binary).LastWriteTimeUtc
        foreach ($source in $sourceFiles) {
            if ((Get-Item -LiteralPath $source).LastWriteTimeUtc -gt $binaryTime) {
                throw "Source is newer than $binary; rebuild Release before the demo."
            }
        }
    }
    New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
    $demoStatus = Join-Path $OutputDirectory 'status.txt'
    'RUNNING' | Set-Content -LiteralPath $demoStatus -Encoding utf8
    $started = [DateTime]::UtcNow.ToString('o')
    Write-Output 'STEP 1/3: ABCA manual example'
    $smallLog = @(& './build/Release/midterm_demo.exe' 2>&1)
    $smallExit = $LASTEXITCODE
    $smallLog | Set-Content -LiteralPath (Join-Path $OutputDirectory 'abca.log') -Encoding utf8
    $smallLog | Write-Output
    if ($smallExit -ne 0) { throw 'ABCA demo failed.' }
    Write-Output 'STEP 2/3: all four CTest suites'
    # --no-tests=error prevents an empty test registration from looking successful.
    $testLog = @(& $ctestExe --test-dir build -C Release --output-on-failure --no-tests=error 2>&1)
    $testExit = $LASTEXITCODE
    $testLog | Set-Content -LiteralPath (Join-Path $OutputDirectory 'ctest.log') -Encoding utf8
    $testLog | Write-Output
    if ($testExit -ne 0) { throw 'CTest failed.' }
    Write-Output 'STEP 3/3: offline full-text round trips (demonstration timings only)'
    $textOutput = Join-Path $OutputDirectory 'text-check'
    & './scripts/run_benchmarks.ps1' -OutputDirectory $textOutput -Warmup 1 -Runs 3 -SkipBuild
    if ((Get-Content -LiteralPath (Join-Path $textOutput 'status.txt') | Select-Object -Last 1) -ne 'PASS') {
        throw 'Full-text verification did not finish.'
    }
    $hashes = @(Import-Csv -LiteralPath (Join-Path $textOutput 'file-checksums.csv'))
    if ($hashes.Count -ne 6 -or @($hashes | Where-Object { $_.matches_input -ne 'True' }).Count -ne 0) {
        throw 'Expected six matching restored file hashes.'
    }
    $receipt = [ordered]@{
        purpose='Midterm demonstration; not a replacement for the 2026-10-23 formal measurements'
        started_utc=$started; finished_utc=[DateTime]::UtcNow.ToString('o')
        git_head=(& git rev-parse HEAD).Trim(); working_tree_state=@(& git status --porcelain)
        abca='PASS'; ctest_exit_code=$testExit; restored_file_hashes_matched=$hashes.Count
        demo_executable_sha256=(Get-FileHash -LiteralPath 'build/Release/midterm_demo.exe').Hash.ToLowerInvariant()
        demo_source_sha256=(Get-FileHash -LiteralPath 'examples/midterm_demo.cpp').Hash.ToLowerInvariant()
        demo_script_sha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash.ToLowerInvariant()
        full_text_metadata='text-check/metadata.json'
    }
    $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'receipt.json') -Encoding utf8
    'PASS' | Add-Content -LiteralPath $demoStatus -Encoding utf8
    Write-Output "MIDTERM DEMO PASS: ABCA, CTest, and 6 restored-file hashes. Records: $OutputDirectory"
    Write-Output 'Use results/2026-10-23-byte-text/summary.csv for the reported performance numbers.'
} catch {
    if ($demoStatus) { "FAILED: $($_.Exception.Message)" | Add-Content -LiteralPath $demoStatus -Encoding utf8 }
    throw
} finally {
    Set-Location -LiteralPath $previousLocation
}
