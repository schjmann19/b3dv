#!/usr/bin/env bash

# Copyright (c) 2026 Jimena Neumann
# SPDX-License-Identifier: BSD-3-Clause

# B3DV build script
# Usage: ./build.sh [--run]

set -e

# Colors for output
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    NC='\033[0m'
else
    RED='' GREEN='' YELLOW='' BLUE='' NC=''
fi

RUN_AFTER_BUILD=false

for arg in "$@"; do
    case "$arg" in
        --run) RUN_AFTER_BUILD=true ;;
    esac
done

LOCAL_RAYLIB_DIR="./external/raylib"

echo "${YELLOW}=== B3DV Build System ===${NC}"
echo

# --- Toolchain detection ----------------------------------------------------

# Respect CC, fall back sanely
if [ -z "$CC" ]; then
    if command -v cc >/dev/null 2>&1; then
        CC=cc
    elif command -v clang >/dev/null 2>&1; then
        CC=clang
    elif command -v gcc >/dev/null 2>&1; then
        CC=gcc
    else
        echo "${RED}✗ No C compiler found (cc/clang/gcc)${NC}"
        exit 1
    fi
fi

export CC

if ! command -v make >/dev/null 2>&1; then
    echo "${RED}✗ make not found${NC}"
    exit 1
fi

echo "${GREEN}✓ Using CC=${CC}${NC}"
echo

# --- Build raylib ------------------------------------------------------------

if [ ! -f "$LOCAL_RAYLIB_DIR/src/libraylib.a" ]; then
    echo "${YELLOW}Building raylib...${NC}"
    (
        cd "$LOCAL_RAYLIB_DIR/src"

        if [ "$(uname -s)" = "Darwin" ]; then
            JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
        else
            JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
        fi

        make \
            PLATFORM=PLATFORM_DESKTOP \
            RAYLIB_LIBTYPE=STATIC \
            CC="$CC" \
            -j"$JOBS"
    )
    echo "${GREEN}✓ Raylib built${NC}"
    echo
else
    echo "${GREEN}✓ Raylib already built${NC}"
    echo
fi

# --- Build B3DV via Makefile -------------------------------------------------

echo "${YELLOW}Building B3DV...${NC}"
make clean
make
echo "${GREEN}✓ Build successful!${NC}"
echo

# --- Run ---------------------------------------------------------------------

if [ "$RUN_AFTER_BUILD" = true ]; then
    echo "${YELLOW}Running B3DV...${NC}"
    ./b3dv
else
    echo "${GREEN}✓ Binary ready: ./b3dv${NC}"
    echo
    echo "${YELLOW}Usage:${NC}"
    echo "  ./b3dv"
    echo "  ./build.sh --run"
fi
