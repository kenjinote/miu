param(
    [Parameter(Mandatory=$true)][string]$KeyFile,
    [Parameter(Mandatory=$true)][string]$IssuerId,
    [string]$Version = '1.0.44',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build_screenshots_v2'),
    [switch]$Resume
)
$ErrorActionPreference = 'Stop'
$pipelineScript = Join-Path $PSScriptRoot 'store_screenshots.py'
if (-not $Resume) {
    & python $pipelineScript build --all-locales --out-dir $OutputDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Screenshot build failed. No uploads attempted.' }
}
& python $pipelineScript sync --all-locales --out-dir $OutputDirectory --version $Version --platform MAC_OS --key-file $KeyFile --issuer-id $IssuerId
if ($LASTEXITCODE -ne 0) { throw 'Synchronization did not finish. Inspect sync_report.json and rerun with -Resume.' }
