$ErrorActionPreference = "Stop"

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

function Install-ZigIfMissing {
    if (Has-Command "zig") { return }

    if (Has-Command "winget") {
        winget install -e --id zig.zig --accept-package-agreements --accept-source-agreements
        return
    }

    if (Has-Command "choco") {
        choco install zig -y
        return
    }

    throw "[setup] zig not found and no installer was detected"
}

function Ensure-MSYS2 {
    $msysRoot = "C:\msys64"
    if (Test-Path $msysRoot) { return $msysRoot }

    if (Has-Command "winget") {
        winget install -e --id MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements
    } else {
        throw "[setup] MSYS2 is required to install raylib but winget is not available"
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
