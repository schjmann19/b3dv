#!/usr/bin/env bash

# Copyright (c) 2026 Jimena Neumann
# SPDX-License-Identifier: BSD-3-Clause

# B3DV build script
# Usage: ./build.sh [--run]

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

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --run) RUN_AFTER_BUILD=true ;;
    esac
done

LOCAL_RAYLIB_DIR="./external/raylib"

echo -e "${YELLOW}=== B3DV Build System ===${NC}"
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

# Build raylib
if [[ ! -f "$LOCAL_RAYLIB_DIR/src/libraylib.a" ]]; then
    echo -e "${YELLOW}Building raylib...${NC}"
    pushd "$LOCAL_RAYLIB_DIR/src" > /dev/null

    if [[ $(uname -s) == "Darwin" ]]; then
        make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    else
        make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC -j$(nproc 2>/dev/null || echo 4)
    fi

    popd > /dev/null
    echo -e "${GREEN}✓ Raylib built${NC}"
    echo ""
else
    echo -e "${GREEN}✓ Raylib already built${NC}"
    echo ""
fi

# Clean
echo -e "${YELLOW}Cleaning...${NC}"
rm -f b3dv *.o
echo -e "${GREEN}✓ Cleaned${NC}"
echo ""

# Build B3DV
echo -e "${YELLOW}Building B3DV...${NC}"
gcc src/main.c src/world_generation.c src/vec_math.c src/rendering.c src/utils.c \
    -o b3dv -Wall -Wextra -O2 \
    -I${LOCAL_RAYLIB_DIR}/src \
    -L${LOCAL_RAYLIB_DIR}/src \
    -l:libraylib.a \
    -lm -lGL -lpthread -ldl -lrt -lX11

echo -e "${GREEN}✓ Build successful!${NC}"
echo ""

# Run if requested
if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo -e "${YELLOW}Running B3DV...${NC}"
    ./b3dv
else
    echo -e "${GREEN}✓ Binary ready: ./b3dv${NC}"
    echo ""
    echo -e "${YELLOW}Usage:${NC}"
    echo "  ./b3dv        # Run the application"
    echo "  ./build.sh --run  # Build and run"
fi