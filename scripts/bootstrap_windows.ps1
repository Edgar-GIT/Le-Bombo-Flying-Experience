$ErrorActionPreference = "Stop"

# This bootstrap installs required tools and builds/runs the game on Windows.

function Has-Command {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
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

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $scriptDir "setup_windows.ps1")

$zigExe = Find-ZigExe
if (-not $zigExe) {
    throw "[bootstrap] zig was not found after installation"
}

Write-Host "[bootstrap] using zig: $zigExe"
& $zigExe build setup
& $zigExe build run
