#!/usr/bin/env sh
# Build the project with optimization and copy the binary to the current directory

set -e

echo "Building b3dv with optimization..."
zig build -Doptimize=ReleaseFast

echo "Copying binary to ./b3dv..."
cp zig-out/bin/b3dv ./b3dv

echo "Done! Binary available at ./b3dv"

# If first argument is 'run', execute order 66
if [ "$1" = "run" ]; then
	echo "Running ./b3dv..."
	exec ./b3dv
fi