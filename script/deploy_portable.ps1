param(
    [string]$ArtifactDir,
    [string]$ZipPath,
    [string]$Target = "D:\Apps\Portable\nekobox",
    [string]$BackupRoot = "D:\Build",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

if (-not $ArtifactDir -and -not $ZipPath) {
    throw "Specify either -ArtifactDir (directory with artifacts.tgz) or -ZipPath (direct *windows64*.zip)."
}

$work = Join-Path $BackupRoot "deploy"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory -Force $work | Out-Null

if ($ZipPath) {
    if (-not (Test-Path $ZipPath)) { throw "ZipPath not found: $ZipPath" }
    $zip = Get-Item $ZipPath
} else {
    $tgz = Get-ChildItem $ArtifactDir -Recurse -Filter "artifacts.tgz" | Select-Object -First 1
    if (-not $tgz) { throw "artifacts.tgz not found in $ArtifactDir" }

    Write-Host "== extracting artifacts.tgz" -ForegroundColor Cyan
    & tar.exe -xzf $tgz.FullName -C $work
    if ($LASTEXITCODE -ne 0) { throw "tar failed with exit code $LASTEXITCODE" }

    $zip = Get-ChildItem $work -Recurse -Filter "*windows64*.zip" | Select-Object -First 1
    if (-not $zip) { throw "windows64 zip not found inside artifacts.tgz" }
}

Write-Host ("== build: " + $zip.Name + " (" + [math]::Round($zip.Length / 1MB, 1) + " MB)") -ForegroundColor Cyan

$unpacked = Join-Path $work "unpacked"
Expand-Archive $zip.FullName -DestinationPath $unpacked -Force
$newRoot = Get-ChildItem $unpacked -Directory | Select-Object -First 1
if (-not $newRoot) { $newRoot = Get-Item $unpacked }

$exe = Join-Path $newRoot.FullName "nekobox.exe"
$core = Join-Path $newRoot.FullName "nekobox_core.exe"
if (-not (Test-Path $exe)) { throw "nekobox.exe is missing in archive" }
if (-not (Test-Path $core)) { throw "nekobox_core.exe is missing in archive" }
Write-Host ("== files in build: " + (Get-ChildItem $newRoot.FullName | Measure-Object).Count)
Write-Host ("== nekobox.exe: " + [math]::Round((Get-Item $exe).Length / 1MB, 1) + " MB")

$running = Get-Process -Name nekobox, nekobox_core -ErrorAction SilentlyContinue
if ($running) {
    if (-not $Force) {
        throw "NekoBox is running (PID: $($running.Id -join ', ')). Close it first or pass -Force."
    }
    Write-Host "== stopping NekoBox" -ForegroundColor Yellow
    $running | Stop-Process -Force
    Start-Sleep -Seconds 3
}

$backup = Join-Path $BackupRoot ("backup-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
New-Item -ItemType Directory -Force $backup | Out-Null
Write-Host "== backup current files -> $backup" -ForegroundColor Cyan
foreach ($name in @("nekobox.exe", "nekobox.dll", "nekobox_core.exe", "updater.exe", "xray.exe")) {
    $src = Join-Path $Target $name
    if (Test-Path $src) { Copy-Item $src $backup }
}

$settings = Join-Path $Target "settings"
if (Test-Path $settings) {
    Copy-Item $settings (Join-Path $backup "settings") -Recurse
}

Write-Host "== copying new build (settings untouched)" -ForegroundColor Cyan
robocopy $newRoot.FullName $Target /E /XD settings /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }

Write-Host ""
Write-Host "Done. Backup: $backup" -ForegroundColor Green
Write-Host "Start $Target\nekobox.exe" -ForegroundColor Green
