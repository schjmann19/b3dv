#!/usr/bin/env sh
# Build the project with optimization and copy the built binaries to the current directory

set -e

build_client() {
    echo "Building b3dv client with optimization..."
    zig build -Doptimize=ReleaseFast --prefix zig-out install
    echo "Copying binary to ./b3dv..."
    cp zig-out/bin/b3dv-client ./b3dv
}

build_server() {
    echo "Building b3dv-server with optimization..."
    zig build -Doptimize=ReleaseFast --prefix zig-out install
    echo "Copying binary to ./b3dv-server..."
    cp zig-out/bin/b3dv-server ./b3dv-server
}

if [ "$#" -eq 0 ]; then
    build_client
    build_server
    echo "Done! Binaries available at ./b3dv and ./b3dv-server"
    exit 0
fi

case "$1" in
    client)
        build_client
        echo "Done! Binary available at ./b3dv"
        ;;
    server)
        build_server
        echo "Done! Binary available at ./b3dv-server"
        ;;
    *)
        echo "Usage: $0 [client|server]"
        echo "  $0          Build both client and server"
        echo "  $0 client   Build only the client as ./b3dv"
        echo "  $0 server   Build only the server as ./b3dv-server"
        exit 1
        ;;
esac
