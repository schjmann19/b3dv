#!/usr/bin/env bash
./build.sh || exit 1

if [[ $1 == "mangohud" ]]; then
    mangohud ./b3dv run
else
    ./b3dv run
fi