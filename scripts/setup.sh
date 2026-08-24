#!/usr/bin/env bash
# Setup script for Simplish — installs build and code-quality dependencies.
# Supports macOS (Homebrew) and Linux (apt/dnf). For Windows, use Visual Studio
# Installer and install CMake and Ninja; see docs/engine/REQUIREMENTS.md.
#
# Dependencies fetched at build time (FetchContent) do not need to be installed
# here: GLM, Jolt, ozz-animation, ENet, RVO2, spdlog, Catch2, FreeType, HarfBuzz,
# FSR2, SDL3, OpenAL Soft, etc. This script installs only the toolchain and
# system packages required to run CMake and the compiler.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Minimum versions (used for checks only; package managers install latest)
CMAKE_MIN_VERSION="3.25"

usage() {
    echo "Usage: $0 [--skip-optional]"
    echo "  --skip-optional  Do not install optional tools (clang-format, clang-tidy)."
    exit 0
}

SKIP_OPTIONAL=false
for arg in "$@"; do
    case "$arg" in
        --skip-optional) SKIP_OPTIONAL=true ;;
        -h|--help) usage ;;
        *) echo "Unknown argument: $arg"; usage ;;
    esac
done

echo "Simplish setup — installing dependencies for $(uname -s)"
echo "Project directory: ${PROJECT_DIR}"
echo ""

# ---------------------------------------------------------------------------
# macOS (Homebrew)
# ---------------------------------------------------------------------------
setup_macos() {
    if ! command -v brew &>/dev/null; then
        echo "Homebrew is not installed. Install it from https://brew.sh"
        echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi

    echo "Installing build tools (cmake, ninja, git)..."
    brew install cmake ninja git

    # Ensure Clang is available (Xcode Command Line Tools or Xcode)
    if ! command -v clang++ &>/dev/null; then
        echo "Clang not found. Install Xcode Command Line Tools:"
        echo "  xcode-select --install"
        exit 1
    fi

    if [ "${SKIP_OPTIONAL}" = false ]; then
        echo "Installing optional code-quality tools (clang-format, clang-tidy)..."
        brew install clang-format llvm  # llvm provides clang-tidy
        echo "  Add LLVM to PATH if needed: export PATH=\"\$(brew --prefix llvm)/bin:\$PATH\""
    fi

    echo "macOS setup complete."
}

# ---------------------------------------------------------------------------
# Linux — detect package manager and install
# ---------------------------------------------------------------------------
setup_linux() {
    if command -v apt-get &>/dev/null; then
        setup_linux_apt
    elif command -v dnf &>/dev/null; then
        setup_linux_dnf
    else
        echo "Unsupported Linux package manager. Install manually:"
        echo "  - CMake >= ${CMAKE_MIN_VERSION}, Ninja, Git, Clang, build-essential"
        echo "  - Optional: clang-format, clang-tidy"
        echo "  - Vulkan (optional for rendering): libvulkan-dev"
        exit 1
    fi
}

setup_linux_apt() {
    echo "Using apt (Debian/Ubuntu). Updating package list..."
    sudo apt-get update

    echo "Installing build tools..."
    sudo apt-get install -y \
        cmake \
        ninja-build \
        git \
        build-essential \
        clang

    if [ "${SKIP_OPTIONAL}" = false ]; then
        echo "Installing optional code-quality tools..."
        sudo apt-get install -y clang-format clang-tidy
    fi

    # Vulkan (optional; needed for Vulkan backend on desktop)
    echo "Installing Vulkan development headers (optional for rendering)..."
    sudo apt-get install -y libvulkan-dev || true

    echo "Linux (apt) setup complete."
}

setup_linux_dnf() {
    echo "Using dnf (Fedora/RHEL). Installing build tools..."
    sudo dnf install -y \
        cmake \
        ninja-build \
        git \
        gcc-c++ \
        clang

    if [ "${SKIP_OPTIONAL}" = false ]; then
        echo "Installing optional code-quality tools..."
        sudo dnf install -y clang-tools-extra || true
    fi

    echo "Installing Vulkan development headers (optional for rendering)..."
    sudo dnf install -y vulkan-headers || true

    echo "Linux (dnf) setup complete."
}

# ---------------------------------------------------------------------------
# Version check — CMake must be >= 3.25
# ---------------------------------------------------------------------------
check_cmake_version() {
    if ! command -v cmake &>/dev/null; then
        echo "CMake was not installed or is not on PATH."
        exit 1
    fi

    local version
    version="$(cmake --version | head -n1 | sed -n 's/.*version \([0-9.]*\).*/\1/p')"
    local major minor
    major="${version%%.*}"
    minor="${version#*.}"
    minor="${minor%%.*}"

    if [ -z "$major" ] || [ -z "$minor" ]; then
        echo "Could not parse CMake version: ${version}"
        return
    fi

    if [ "$major" -lt 3 ] || { [ "$major" -eq 3 ] && [ "$minor" -lt 25 ]; }; then
        echo "CMake ${version} is too old. Simplish requires CMake >= ${CMAKE_MIN_VERSION}."
        echo "  - macOS: brew upgrade cmake"
        echo "  - Ubuntu/Debian: consider https://apt.kitware.com/ or snap install cmake --classic"
        echo "  - Fedora: dnf install cmake (or upgrade)"
        exit 1
    fi

    echo "CMake version ${version} OK"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
case "$(uname -s)" in
    Darwin)
        setup_macos
        ;;
    Linux)
        setup_linux
        ;;
    *)
        echo "Unsupported OS: $(uname -s)"
        echo "This script supports macOS and Linux. On Windows, install:"
        echo "  - Visual Studio 2022 (or Build Tools) with C++ workload"
        echo "  - CMake >= ${CMAKE_MIN_VERSION} from https://cmake.org/download/"
        echo "  - Ninja (optional; CMake can use VS generator)"
        echo "  - Git for Windows"
        exit 1
        ;;
esac

check_cmake_version

echo ""
echo "Next steps:"
echo "  1. cd ${PROJECT_DIR}"
echo "  2. ./scripts/build.sh --debug    # configure and build"
echo "  3. ./scripts/test.sh            # run tests"
echo ""
