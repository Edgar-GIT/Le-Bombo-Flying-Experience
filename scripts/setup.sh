#!/usr/bin/env sh

set -eu

# This script auto installs zig and raylib when package managers are available.

is_macos=0
is_linux=0
case "$(uname -s)" in
    Darwin) is_macos=1 ;;
    Linux) is_linux=1 ;;
esac

need_sudo() {
    if [ "$(id -u)" -ne 0 ]; then
        echo "sudo"
    else
        echo ""
    fi
}

install_zig_linux() {
    if command -v apt-get >/dev/null 2>&1; then
        SUDO="$(need_sudo)"
        ${SUDO} apt-get update
        ${SUDO} apt-get install -y zig
        return
    fi
    if command -v dnf >/dev/null 2>&1; then
        SUDO="$(need_sudo)"
        ${SUDO} dnf install -y zig
        return
    fi
    if command -v pacman >/dev/null 2>&1; then
        SUDO="$(need_sudo)"
        ${SUDO} pacman -S --needed --noconfirm zig
        return
    fi
    echo "[setup] zig not installed and no supported package manager found"
    exit 1
}

install_raylib_linux() {
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists raylib; then
        return
    fi
    if command -v apt-get >/dev/null 2>&1; then
        SUDO="$(need_sudo)"
        ${SUDO} apt-get update
        ${SUDO} apt-get install -y libraylib-dev
        return
    fi
    if command -v dnf >/dev/null 2>&1; then
        SUDO="$(need_sudo)"
        ${SUDO} dnf install -y raylib-devel
        return
    fi
    if command -v pacman >/dev/null 2>&1; then
        SUDO="$(need_sudo)"
        ${SUDO} pacman -S --needed --noconfirm raylib
        return
    fi
    echo "[setup] raylib not installed and no supported package manager found"
    exit 1
}

install_zig_macos() {
    if ! command -v brew >/dev/null 2>&1; then
        echo "[setup] Homebrew is required on macOS to install dependencies"
        exit 1
    fi
    brew list zig >/dev/null 2>&1 || brew install zig
}

install_raylib_macos() {
    if ! command -v brew >/dev/null 2>&1; then
        echo "[setup] Homebrew is required on macOS to install dependencies"
        exit 1
    fi
    brew list raylib >/dev/null 2>&1 || brew install raylib
}

if [ "$is_linux" -eq 1 ]; then
    if ! command -v zig >/dev/null 2>&1; then
        echo "[setup] Installing zig"
        install_zig_linux
    fi
    echo "[setup] Ensuring raylib is installed"
    install_raylib_linux
    echo "[setup] Linux dependencies ready"
    exit 0
fi

if [ "$is_macos" -eq 1 ]; then
    if ! command -v zig >/dev/null 2>&1; then
        echo "[setup] Installing zig with Homebrew"
        install_zig_macos
    fi
    echo "[setup] Ensuring raylib is installed with Homebrew"
    install_raylib_macos
    echo "[setup] macOS dependencies ready"
    exit 0
fi

echo "[setup] Unsupported OS for setup.sh"
exit 1
