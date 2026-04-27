#!/bin/bash
# Build the project with optimization and copy the binary to the current directory

set -e

echo "Building b3dv with optimization..."
zig build -Doptimize=ReleaseFast

echo "Copying binary to ./b3dv..."
cp zig-out/bin/b3dv ./b3dv

echo "Done! Binary available at ./b3dv"