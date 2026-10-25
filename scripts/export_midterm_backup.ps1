param(
    [string]$DemoDirectory = 'results/2026-10-25-midterm-demo',
    [string]$OutputFile = 'docs/midterm/demo-backup.html'
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$previousLocation = Get-Location
function Escape-Html([object]$Value) { [System.Net.WebUtility]::HtmlEncode([string]$Value) }
try {
    Set-Location -LiteralPath $repoRoot
    if ((Get-Content -LiteralPath (Join-Path $DemoDirectory 'status.txt') | Select-Object -Last 1) -ne 'PASS') {
        throw 'Only a completed, successful demo can be exported.'
    }
    $receipt = Get-Content -LiteralPath (Join-Path $DemoDirectory 'receipt.json') -Raw | ConvertFrom-Json
    $abca = Escape-Html (Get-Content -LiteralPath (Join-Path $DemoDirectory 'abca.log') -Raw)
    $ctest = Escape-Html (Get-Content -LiteralPath (Join-Path $DemoDirectory 'ctest.log') -Raw)
    $checks = @(Import-Csv -LiteralPath (Join-Path $DemoDirectory 'text-check/file-checksums.csv'))
    if ($checks.Count -ne 6 -or @($checks | Where-Object { $_.matches_input -ne 'True' }).Count -ne 0) {
        throw 'Six matching restored-file hashes are required.'
    }
    $formal = 'results/2026-10-23-byte-text'
    $summary = Import-Csv -LiteralPath (Join-Path $formal 'summary.csv')
    $environment = Get-Content -LiteralPath (Join-Path $formal 'metadata.json') -Raw | ConvertFrom-Json
    $rows = @($summary | Where-Object { $_.scope -eq 'decode_symbols' } | ForEach-Object {
        $name = if ($_.dataset_id -eq '1') { 'Alice' } else { 'Pride' }
        '<tr><td>' + $name + '</td><td>' + (Escape-Html $_.variant) + '</td><td>' +
        (Escape-Html $_.median_ms) + '</td><td>' + (Escape-Html $_.speedup) + '</td></tr>'
    }) -join "`n"
    $hashRows = @($checks | ForEach-Object {
        '<tr><td>' + (Escape-Html $_.dataset_id) + '</td><td>' + (Escape-Html $_.variant) +
        '</td><td class="hash">' + (Escape-Html $_.sha256) + '</td><td>PASS</td></tr>'
    }) -join "`n"
    $demoDate = Escape-Html $receipt.finished_utc
    $commit = Escape-Html $receipt.git_head
    $cpu = Escape-Html $environment.cpu
    $source = Escape-Html $DemoDirectory
    $html = @"
<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>霍夫曼解码 中期演示记录</title>
<style>
body{margin:0;background:#f2f4f7;color:#19212d;font-family:'Microsoft YaHei',sans-serif;line-height:1.6}
main{max-width:1050px;margin:32px auto;padding:28px 36px;background:white;border-radius:12px}
h1{font-size:28px;margin:0 0 12px}h2{font-size:21px;margin:28px 0 12px}p{margin:10px 0}
.note{padding:14px 18px;background:#eaf1fa;border-left:4px solid #37669b}.pass{color:#17623a;font-weight:bold}
pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#17212f;color:#e9f0f7;padding:18px;border-radius:6px;font:14px/1.7 Consolas,monospace}
table{border-collapse:collapse;width:100%;font-size:14px}th,td{border:1px solid #d8dfe7;padding:10px;text-align:left}th{background:#edf1f5}
.hash{font:12px/1.6 Consolas,monospace;overflow-wrap:anywhere}.meta{font-size:13px;color:#4b596b;overflow-wrap:anywhere}
@media(max-width:700px){main{margin:0;padding:20px}table{font-size:12px}th,td{padding:6px}}
@media print{body{background:white}main{margin:0;max-width:none;padding:0}pre{color:black;background:#f4f4f4}h2{break-after:avoid}tr{break-inside:avoid}}
</style></head><body><main>
<h1>霍夫曼解码 中期演示记录</h1>
<p class="note">这是已完成实验的静态备用记录，不是实时运行界面。学校要求5分钟PPT自述，程序展示用于备用或追问。</p>
<p class="pass">演示检查通过：ABCA还原、4项CTest、6份真实文本还原文件校验。</p>
<p class="meta">演示完成时间 UTC：$demoDate<br>运行时基准提交：$commit（含当时未提交改动，详见receipt.json）。<br>记录目录：$source</p>
<h2>手算样例与三个解码版本</h2><pre>$abca</pre>
<h2>自动化检查的实际输出</h2><pre>$ctest</pre>
<h2>原文与还原文件校验</h2>
<p>数据1为Alice，数据2为Pride。下表来自独立落盘哈希检查，6份输出均与对应输入一致。</p>
<table><thead><tr><th>数据</th><th>版本</th><th>SHA-256</th><th>结果</th></tr></thead><tbody>$hashRows</tbody></table>
<h2>正式性能结果 2026年10月23日</h2>
<p>纯解码scope；$cpu；x64 Release；每组预热3次、测量30次。此处引用正式CSV，不使用本次3次演示计时。</p>
<table><thead><tr><th>文本</th><th>版本</th><th>中位耗时 ms</th><th>相对基线加速比</th></tr></thead><tbody>$rows</tbody></table>
<p>buffer在本轮两份文本上约为基线速度的1.5倍。计时排除文件读写、编码、建表、预分配和结果比较。Base独立收益尚不能认定稳定；未固定核心与频率，结果不代表所有数据。</p>
<p class="meta">正式来源：results/2026-10-23-byte-text/summary.csv。演示来源：$source。词级、混合表和AVX2不属于本轮已验证成果。</p>
</main></body></html>
"@
    $parent = Split-Path -Parent $OutputFile
    if ($parent -and -not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent | Out-Null }
    $html | Set-Content -LiteralPath $OutputFile -Encoding utf8
    Write-Output "Saved offline backup: $OutputFile"
} finally { Set-Location -LiteralPath $previousLocation }
