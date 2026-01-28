#!/usr/bin/env bash

# Copyright (c) 2026 Jimena Neumann
# SPDX-License-Identifier: BSD-3-Clause

# B3DV build script with automatic raylib installation
# Usage: ./build.sh [--run] [--local-raylib]

set -e

# Colors for output
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    NC=''
fi

RUN_AFTER_BUILD=false
FORCE_LOCAL_RAYLIB=false
LOCAL_RAYLIB_DIR="./external/raylib"

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --run) RUN_AFTER_BUILD=true ;;
        --local-raylib) FORCE_LOCAL_RAYLIB=true ;;
    esac
done

detect_os() {
    local os_type
    os_type=$(uname -s)
    case "$os_type" in
        Linux*)     echo "linux" ;;
        Darwin*)    echo "macos" ;;
        FreeBSD*)   echo "freebsd" ;;
        OpenBSD*)   echo "openbsd" ;;
        NetBSD*)    echo "netbsd" ;;
        DragonFly*) echo "dragonfly" ;;
        *)          echo "unknown" ;;
    esac
}

detect_package_manager() {
    if command -v pacman &> /dev/null; then
        echo "pacman"
    elif command -v apt-get &> /dev/null; then
        echo "apt"
    elif command -v dnf &> /dev/null; then
        echo "dnf"
    elif command -v yum &> /dev/null; then
        echo "yum"
    elif command -v zypper &> /dev/null; then
        echo "zypper"
    elif command -v apk &> /dev/null; then
        echo "apk"
    elif command -v xbps-install &> /dev/null; then
        echo "xbps"
    elif command -v brew &> /dev/null; then
        echo "brew"
    elif command -v pkg &> /dev/null; then
        echo "pkg"
    elif command -v pkgin &> /dev/null; then
        echo "pkgin"
    else
        echo "unknown"
    fi
}

# Check if raylib is available system-wide
check_system_raylib() {
    # Method 1: pkg-config
    if pkg-config --exists raylib 2>/dev/null; then
        return 0
    fi

    # Method 2: Check common header locations
    for header_path in "/usr/include" "/usr/local/include" "/opt/local/include" "/usr/pkg/include" "/opt/homebrew/include"; do
        if [[ -f "$header_path/raylib.h" ]]; then
            return 0
        fi
    done

    # Method 3: macOS Homebrew
    if [[ "$OS" == "macos" ]] && command -v brew &> /dev/null; then
        if [[ -d "$(brew --prefix raylib 2>/dev/null)/include" ]]; then
            return 0
        fi
    fi

    return 1
}

# Build raylib locally
build_local_raylib() {
    echo -e "${YELLOW}Building raylib locally...${NC}"

    mkdir -p external

    if [[ ! -d "$LOCAL_RAYLIB_DIR" ]]; then
        echo -e "${BLUE}Downloading raylib...${NC}"

        # Try git clone first
        if command -v git &> /dev/null; then
            git clone --depth 1 --branch 5.0 https://github.com/raysan5/raylib.git "$LOCAL_RAYLIB_DIR"
        # Fallback to wget/curl
        elif command -v wget &> /dev/null; then
            wget -O /tmp/raylib.tar.gz https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
            tar -xzf /tmp/raylib.tar.gz -C external/
            mv external/raylib-5.0 "$LOCAL_RAYLIB_DIR"
            rm /tmp/raylib.tar.gz
        elif command -v curl &> /dev/null; then
            curl -L -o /tmp/raylib.tar.gz https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
            tar -xzf /tmp/raylib.tar.gz -C external/
            mv external/raylib-5.0 "$LOCAL_RAYLIB_DIR"
            rm /tmp/raylib.tar.gz
        else
            echo -e "${RED}Error: git, wget, or curl required to download raylib${NC}"
            return 1
        fi
    fi

    # Build raylib
    pushd "$LOCAL_RAYLIB_DIR/src" > /dev/null

    echo -e "${BLUE}Compiling raylib...${NC}"
    if [[ "$OS" == "macos" ]]; then
        make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    else
        make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC -j$(nproc 2>/dev/null || echo 4)
    fi

    popd > /dev/null

    echo -e "${GREEN}✓ Local raylib built successfully${NC}"
    return 0
}

# Get compiler flags for local raylib
get_local_raylib_flags() {
    echo "-I${LOCAL_RAYLIB_DIR}/src -L${LOCAL_RAYLIB_DIR}/src -l:libraylib.a"
}

OS=$(detect_os)
PKG_MANAGER=$(detect_package_manager)

echo -e "${YELLOW}=== B3DV Build System ($(uname -s)) ===${NC}"
echo -e "${BLUE}OS: $OS | Package Manager: $PKG_MANAGER${NC}"
echo ""

# Check for C compiler
if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
    echo -e "${RED}✗ No C compiler found (gcc/clang required)${NC}"
    exit 1
fi

# Check for make
if ! command -v make &> /dev/null; then
    echo -e "${RED}✗ make not found${NC}"
    exit 1
fi

# Raylib strategy
USE_LOCAL_RAYLIB=false

if [[ "$FORCE_LOCAL_RAYLIB" == true ]]; then
    echo -e "${YELLOW}Using local raylib (forced)${NC}"
    USE_LOCAL_RAYLIB=true
elif check_system_raylib; then
    echo -e "${GREEN}✓ System raylib found${NC}"
    USE_LOCAL_RAYLIB=false
else
    echo -e "${YELLOW}⚠ System raylib not found${NC}"
    echo -e "${BLUE}→ Will build raylib locally${NC}"
    USE_LOCAL_RAYLIB=true
fi

echo ""

# Build local raylib if needed
if [[ "$USE_LOCAL_RAYLIB" == true ]]; then
    if [[ ! -f "$LOCAL_RAYLIB_DIR/src/libraylib.a" ]]; then
        if ! build_local_raylib; then
            echo -e "${RED}✗ Failed to build local raylib${NC}"
            echo -e "${YELLOW}Please install raylib system-wide:${NC}"
            case "$PKG_MANAGER" in
                pacman) echo "  sudo pacman -S raylib" ;;
                apt) echo "  sudo apt-get install libraylib-dev" ;;
                dnf) echo "  sudo dnf install raylib-devel" ;;
                brew) echo "  brew install raylib" ;;
                *) echo "  See: https://github.com/raysan5/raylib" ;;
            esac
            exit 1
        fi
    else
        echo -e "${GREEN}✓ Local raylib already built${NC}"
    fi

    EXTRA_CFLAGS=$(get_local_raylib_flags)
    export RAYLIB_CFLAGS="$EXTRA_CFLAGS"
fi

# Clean
echo -e "${YELLOW}Cleaning...${NC}"
make clean 2>/dev/null || true
echo -e "${GREEN}✓ Cleaned${NC}"
echo ""

# Build
echo -e "${YELLOW}Building B3DV...${NC}"

if [[ "$USE_LOCAL_RAYLIB" == true ]]; then
    # Build with local raylib flags
    gcc src/main.c src/world_generation.c src/vec_math.c src/rendering.c src/utils.c \
        -o world -Wall -Wextra -O2 $EXTRA_CFLAGS -lm $(pkg-config --libs --cflags glfw3 2>/dev/null || echo "-lGL -lpthread -ldl -lrt -lX11" )
else
    # Build with system raylib
    make "$OS" 2>/dev/null || make all 2>/dev/null || make
fi

if [[ $? -eq 0 ]]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
    echo ""
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

# Run if requested
if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo -e "${YELLOW}Running B3DV...${NC}"
    ./world
else
    echo -e "${GREEN}✓ Binary ready: ./world${NC}"
    echo ""
    echo -e "${YELLOW}Usage:${NC}"
    echo "  ./world                    # Run the application"
    echo "  ./build.sh --run           # Build and run"
    echo "  ./build.sh --local-raylib  # Force local raylib build"
fi