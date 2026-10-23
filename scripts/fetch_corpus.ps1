param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$corpusDir = Join-Path $repoRoot 'data/corpus'
New-Item -ItemType Directory -Path $corpusDir -Force | Out-Null
$sources = Get-Content -LiteralPath (Join-Path $repoRoot 'data/sources.json') -Raw | ConvertFrom-Json
foreach ($source in $sources) {
    $target = Join-Path $corpusDir $source.filename
    if (-not (Test-Path -LiteralPath $target)) {
        $temporary = $target + '.download'
        Invoke-WebRequest -Uri $source.url -OutFile $temporary -TimeoutSec 45
        $firstLine = Get-Content -LiteralPath $temporary -TotalCount 1
        if ($firstLine -notmatch 'Project Gutenberg' -or (Get-Item -LiteralPath $temporary).Length -lt 10000) {
            throw "Unexpected download content for $($source.filename)"
        }
        if ($source.sha256 -and (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $source.sha256) {
            throw "Source has changed: $($source.filename). Review its version before recording a new experiment."
        }
        Move-Item -LiteralPath $temporary -Destination $target
    }
    $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($source.sha256 -and $hash -ne $source.sha256) {
        throw "Existing corpus hash differs: $($source.filename)"
    }
    [pscustomobject]@{File=$source.filename; Bytes=(Get-Item -LiteralPath $target).Length; SHA256=$hash}
}
