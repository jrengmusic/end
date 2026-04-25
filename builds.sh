#!/usr/bin/env bash
# builds.sh - Bootstrap build script for END (cross-platform)
# Replaces both install.sh and build.bat.
# Sets up MSVC environment on Windows (MSYS2), then runs CMake + Ninja.
# Produces PDB debug symbols on Windows for whatdbg (dbgeng.dll-based DAP adapter).
#
# Usage:
#   builds.sh                  - configure + build (Release)
#   builds.sh debug            - configure + build (Debug)
#   builds.sh clean            - delete Builds/Ninja, reconfigure (Release)
#   builds.sh clean debug      - delete Builds/Ninja, reconfigure (Debug)
#   builds.sh install          - clean build Release + install to system location

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---------------------------------------------------------------------------
# OS detection
# ---------------------------------------------------------------------------
detect_os() {
    case "$(uname -s)" in
        Darwin)             echo "macos"   ;;
        Linux)              echo "linux"   ;;
        MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
        *)                  echo "unknown" ;;
    esac
}

OS="$(detect_os)"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
CLEAN=0
CONFIG=""
DO_INSTALL=0

for arg in "$@"; do
    case "$arg" in
        clean)   CLEAN=1 ;;
        install) DO_INSTALL=1 ;;
        debug)   CONFIG="Debug" ;;
    esac
done

if [[ "$DO_INSTALL" -eq 1 ]]; then
    CLEAN=1
    CONFIG="Release"
fi

[[ -z "$CONFIG" ]] && CONFIG="Release"

# ---------------------------------------------------------------------------
# Windows (MSYS2): locate and source MSVC environment
# ---------------------------------------------------------------------------
setup_msvc() {
    # Resolve vswhere.exe — prefer env var, fall back to known path
    local vswhere_win
    local programfiles_x86="${PROGRAMFILES(x86):-}"
    if [[ -n "$programfiles_x86" ]]; then
        vswhere_win="$programfiles_x86\\Microsoft Visual Studio\\Installer\\vswhere.exe"
    else
        vswhere_win="C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe"
    fi

    local vswhere_unix
    vswhere_unix="$(cygpath -u "$vswhere_win" 2>/dev/null || echo "")"

    if [[ ! -f "$vswhere_unix" ]]; then
        echo "ERROR: vswhere.exe not found. Is Visual Studio installed?"
        exit 1
    fi

    # Get VS install path (Windows-style)
    local vs_path_win
    vs_path_win="$("$vswhere_unix" -latest -property installationPath 2>/dev/null | tr -d '\r')"

    if [[ -z "$vs_path_win" ]]; then
        echo "ERROR: vswhere.exe returned no installation path."
        exit 1
    fi

    # vcvarsall.bat location (Windows-style path for cmd.exe)
    local vcvarsall_win="${vs_path_win}\\VC\\Auxiliary\\Build\\vcvarsall.bat"
    local vcvarsall_unix
    vcvarsall_unix="$(cygpath -u "$vcvarsall_win" 2>/dev/null || echo "")"

    if [[ ! -f "$vcvarsall_unix" ]]; then
        echo "ERROR: vcvarsall.bat not found at $vcvarsall_win"
        exit 1
    fi

    # Detect architecture
    local arch="x64"
    if [[ "${PROCESSOR_ARCHITECTURE:-}" == "ARM64" ]]; then
        arch="arm64"
    fi

    echo "Setting up MSVC $arch environment..."

    # Source vcvarsall.bat environment into this bash session
    while IFS='=' read -r key value; do
        key="${key//$'\r'/}"
        value="${value//$'\r'/}"
        [[ -n "$key" ]] && export "$key=$value"
    done < <(cmd.exe //c "call \"$vcvarsall_win\" $arch >nul 2>&1 && set" 2>/dev/null)

    export CC=cl
    export CXX=cl
    echo "Using cl.exe with PDB debug symbols"

    # Prepend VS-bundled cmake and ninja to PATH
    # (MSYS2 cmake 4.x breaks RC compilation with MSVC)
    local vs_path_unix
    vs_path_unix="$(cygpath -u "$vs_path_win")"
    local vs_cmake_bin="$vs_path_unix/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
    local vs_ninja_bin="$vs_path_unix/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    export PATH="$vs_cmake_bin:$vs_ninja_bin:$PATH"

    echo
    echo "CMake:" && cmake --version
    echo
    echo "Ninja:" && ninja --version
    echo
}

# ---------------------------------------------------------------------------
# Parallel job count
# ---------------------------------------------------------------------------
job_count() {
    case "$OS" in
        macos)   sysctl -n hw.logicalcpu ;;
        linux)   nproc ;;
        windows) echo "${NUMBER_OF_PROCESSORS:-4}" ;;
    esac
}

# ---------------------------------------------------------------------------
# Configure
# ---------------------------------------------------------------------------
configure() {
    local config="$1"
    local marker="Builds/Ninja/.build_config"
    local needs_configure=0

    if [[ ! -d "Builds/Ninja" ]]; then
        needs_configure=1
    else
        local existing_config=""
        [[ -f "$marker" ]] && existing_config="$(cat "$marker" | tr -d '\r\n')"
        if [[ "$existing_config" != "$config" ]]; then
            echo "Config changed [$existing_config] -> [$config], reconfiguring..."
            rm -rf "Builds/Ninja"
            needs_configure=1
        fi
    fi

    if [[ "$needs_configure" -eq 1 ]]; then
        echo "Configuring [$config]..."
        if [[ "$OS" == "windows" ]]; then
            cmake -S . -B Builds/Ninja -G Ninja \
                -DCMAKE_BUILD_TYPE="$config" \
                -DCMAKE_C_COMPILER="$CC" \
                -DCMAKE_CXX_COMPILER="$CXX"
        else
            cmake -S . -B Builds/Ninja -G Ninja \
                -DCMAKE_BUILD_TYPE="$config"
        fi
        echo "$config" > "$marker"
    fi
}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
build() {
    local config="$1"
    local jobs
    jobs="$(job_count)"
    echo "Building [$config] with $jobs jobs..."
    cmake --build Builds/Ninja -- -j"$jobs"
}

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install_artifact() {
    echo "Installing..."
    case "$OS" in
        windows)
            local artifact="Builds/Ninja/END_App_artefacts/Release/END.exe"
            local install_dir="$HOME/.local/bin"
            mkdir -p "$install_dir"
            cp "$artifact" "$install_dir/end.exe"
            ;;
        macos)
            local artifact="Builds/Ninja/END_App_artefacts/Release/END.app"
            local install_dir="$HOME/Applications"
            mkdir -p "$install_dir"
            rm -rf "$install_dir/END.app"
            cp -R "$artifact" "$install_dir/END.app"
            ;;
        linux)
            local artifact="Builds/Ninja/END_App_artefacts/Release/END"
            local install_dir="$HOME/.local/bin"
            mkdir -p "$install_dir"
            cp "$artifact" "$install_dir/end"
            ;;
        *)
            echo "Unsupported OS: $(uname -s)"
            exit 1
            ;;
    esac
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if [[ "$OS" == "unknown" ]]; then
    echo "Unsupported OS: $(uname -s)"
    exit 1
fi

if [[ "$OS" == "windows" ]]; then
    setup_msvc
fi

if [[ "$CLEAN" -eq 1 ]]; then
    echo "Cleaning Builds/Ninja..."
    rm -rf "Builds/Ninja"
fi

configure "$CONFIG"
build "$CONFIG"

if [[ "$DO_INSTALL" -eq 1 ]]; then
    install_artifact
fi

echo
echo "Done."
