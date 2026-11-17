param(
    [string]$ArtifactDir,
    [string]$ZipPath,
    [string]$Target = "D:\Apps\Portable\nekobox",
    [string]$BackupRoot = "D:\Build",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Get-FileSha256([string]$Path) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace("-", "")
        } finally {
            $stream.Dispose()
        }
    } finally {
        $sha.Dispose()
    }
}

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
    foreach ($p in (Get-Process -Name nekobox -ErrorAction SilentlyContinue)) {
        try { $null = $p.CloseMainWindow() } catch {}
    }
    for ($i = 0; $i -lt 6; $i++) {
        if (-not (Get-Process -Name nekobox -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    Get-Process -Name nekobox -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

    for ($i = 0; $i -lt 12; $i++) {
        if (-not (Get-Process -Name nekobox_core -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }

    if (Get-Process -Name nekobox_core -ErrorAction SilentlyContinue) {
        $launcherFile = Join-Path $Target "nekobox_core.exe.elevated_launcher"
        if (Test-Path $launcherFile) {
            try {
                $launcher = Get-Content $launcherFile -Raw -Encoding UTF8 | ConvertFrom-Json
                if ($launcher.task) {
                    & schtasks.exe /End /TN $launcher.task 2>$null | Out-Null
                }
            } catch {}
        }
        Get-Process -Name nekobox_core -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2
    }
}

$excludeFiles = @()
$targetCore = Join-Path $Target "nekobox_core.exe"
if ((Get-Process -Name nekobox_core -ErrorAction SilentlyContinue) -and (Test-Path $targetCore)) {
    $srcCoreHash = Get-FileSha256 $core
    $dstCoreHash = Get-FileSha256 $targetCore
    if ($srcCoreHash -eq $dstCoreHash) {
        Write-Host "== nekobox_core.exe is unchanged (SHA-256 match); skipping locked elevated process" -ForegroundColor Yellow
        $excludeFiles += "nekobox_core.exe"
    } else {
        throw "nekobox_core.exe is still running with elevated privileges and differs from the new build. Exit NekoBox from the tray or run PowerShell as Administrator."
    }
}

$backup = Join-Path $BackupRoot ("backup-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
New-Item -ItemType Directory -Force $backup | Out-Null
Write-Host "== backup current files -> $backup" -ForegroundColor Cyan
foreach ($name in @("nekobox.exe", "nekobox.dll", "nekobox_core.exe", "nekobox_core.exe.elevated_launcher", "updater.exe", "xray.exe")) {
    $src = Join-Path $Target $name
    if (Test-Path $src) { Copy-Item $src $backup }
}

$settings = Join-Path $Target "settings"
if (Test-Path $settings) {
    Copy-Item $settings (Join-Path $backup "settings") -Recurse
}

Write-Host "== copying new build (settings untouched)" -ForegroundColor Cyan
$roboArgs = @($newRoot.FullName, $Target, "/E", "/XD", "settings", "/R:3", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS", "/NP")
if ($excludeFiles.Count -gt 0) {
    $roboArgs += "/XF"
    $roboArgs += $excludeFiles
}
& robocopy @roboArgs | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }

Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Done. Backup: $backup" -ForegroundColor Green
Write-Host "Start $Target\nekobox.exe" -ForegroundColor Green
