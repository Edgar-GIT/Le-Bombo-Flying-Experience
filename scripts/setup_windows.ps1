$ErrorActionPreference = "Stop"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptRoot

# This script auto installs zig and raylib on Windows when tools are available.

function Has-Command {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Add-PathPersistent {
    param([string]$Entry)
    if (-not (Test-Path $Entry)) { return }

    $currentUserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ([string]::IsNullOrWhiteSpace($currentUserPath)) {
        [Environment]::SetEnvironmentVariable("Path", $Entry, "User")
    } elseif (-not ($currentUserPath.Split(";") -contains $Entry)) {
        [Environment]::SetEnvironmentVariable("Path", "$Entry;$currentUserPath", "User")
    }

    if (-not ($env:Path.Split(";") -contains $Entry)) {
        $env:Path = "$Entry;$env:Path"
    }
}

function Find-ZigExe {
    $cmd = Get-Command zig -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $wingetRoot = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path $wingetRoot) {
        $zig = Get-ChildItem -Path $wingetRoot -Recurse -Filter zig.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($zig) { return $zig.FullName }
    }

    return $null
}

function Install-ZigDirect {
    param([string]$InstallRoot)

    New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
    $indexUrl = "https://ziglang.org/download/index.json"
    $index = Invoke-RestMethod -Uri $indexUrl -UseBasicParsing

    $stable = $index.PSObject.Properties |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
        Sort-Object { [Version]$_.Name } -Descending |
        Select-Object -First 1
    if (-not $stable) {
        throw "[setup] failed to resolve zig stable version from index.json"
    }

    $pkg = $stable.Value."x86_64-windows"
    if (-not $pkg -or -not $pkg.tarball) {
        throw "[setup] failed to resolve zig x86_64-windows package"
    }

    $zipPath = Join-Path $env:TEMP "lbfe_zig_windows.zip"
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    Invoke-WebRequest -Uri $pkg.tarball -OutFile $zipPath -UseBasicParsing

    $extractRoot = Join-Path $InstallRoot "zig-portable"
    if (Test-Path $extractRoot) { Remove-Item $extractRoot -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
    Expand-Archive -Path $zipPath -DestinationPath $extractRoot -Force

    $zigExe = Get-ChildItem -Path $extractRoot -Recurse -Filter zig.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $zigExe) {
        throw "[setup] zig.exe not found after extracting portable archive"
    }

    Add-PathPersistent -Entry (Split-Path -Parent $zigExe.FullName)
}

function Install-ZigIfMissing {
    if (Find-ZigExe) { return }

    if (Has-Command "winget") {
        winget install -e --id zig.zig --accept-package-agreements --accept-source-agreements
        return
    }

    if (Has-Command "choco") {
        choco install zig -y
        return
    }

    $portableRoot = Join-Path $ProjectRoot ".tools"
    Install-ZigDirect -InstallRoot $portableRoot
}

function Install-MSYS2Direct {
    param([string]$MSYSRoot)

    $installer = Join-Path $env:TEMP "msys2-installer.exe"
    $url = "https://github.com/msys2/msys2-installer/releases/latest/download/msys2-x86_64-latest.exe"
    if (Test-Path $installer) { Remove-Item $installer -Force }
    Invoke-WebRequest -Uri $url -OutFile $installer -UseBasicParsing

    Start-Process -FilePath $installer -ArgumentList @("in", "--confirm-command", "--accept-messages", "--root", $MSYSRoot) -Wait -NoNewWindow
}

function Ensure-MSYS2 {
    $msysRoot = "C:\msys64"
    if (Test-Path $msysRoot) { return $msysRoot }

    if (Has-Command "winget") {
        winget install -e --id MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements
    } else {
        Install-MSYS2Direct -MSYSRoot $msysRoot
    }

    if (-not (Test-Path $msysRoot)) {
        throw "[setup] MSYS2 install did not create C:\msys64"
    }
    return $msysRoot
}

function Install-RaylibMSYS2 {
    param([string]$MSYSRoot)

    $bash = Join-Path $MSYSRoot "usr\bin\bash.exe"
    if (-not (Test-Path $bash)) {
        throw "[setup] MSYS2 bash was not found"
    }

    & $bash -lc "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-raylib"
    if ($LASTEXITCODE -ne 0) {
        throw "[setup] pacman failed installing raylib"
    }
}

Install-ZigIfMissing
$msysRoot = Ensure-MSYS2
Install-RaylibMSYS2 -MSYSRoot $msysRoot

Add-PathPersistent -Entry "$msysRoot\ucrt64\bin"
Add-PathPersistent -Entry "$msysRoot\usr\bin"

[Environment]::SetEnvironmentVariable("RAYLIB_ROOT", "C:/msys64/ucrt64", "User")
[Environment]::SetEnvironmentVariable("RAYLIB_LIB_DIR", "C:/msys64/ucrt64/lib", "User")
[Environment]::SetEnvironmentVariable("RAYLIB_INCLUDE_DIR", "C:/msys64/ucrt64/include", "User")

$env:RAYLIB_ROOT = "C:/msys64/ucrt64"
$env:RAYLIB_LIB_DIR = "C:/msys64/ucrt64/lib"
$env:RAYLIB_INCLUDE_DIR = "C:/msys64/ucrt64/include"

Write-Host "[setup] Windows dependencies ready"
