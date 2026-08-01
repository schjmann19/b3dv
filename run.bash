#!/usr/bin/env bash
set -e

if [ "$1" = "server" ]; then
    shift
    WORLD_NAME="$1"
    PORT="${2:-42069}"
    if [ -z "$WORLD_NAME" ]; then
        echo "Usage: $0 server <world_name> [port]"
        exit 1
    fi
    if [ ! -x ./b3dv-server ]; then
        echo "Building server..."
        ./build.sh server
    fi
    echo "Running dedicated server for world '$WORLD_NAME' on port $PORT..."
    ./b3dv-server "$WORLD_NAME" "$PORT"
    exit 0
fi


echo "Building client..."
./build.sh client


if [ "$1" = "mangohud" ]; then
    mangohud ./b3dv run
else
    ./b3dv run
fi