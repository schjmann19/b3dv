# B3DV - Basic 3D Visualizer

Copyright (c) 2026 Jimena Neumann,
SPDX-License-Identifier: BSD-3-Clause

Simple 3D world made of voxels featuring:
simple terrain, multiple blocks, a worlds (with chunks) system, optimized rendering.
Heavily inspired in early iterations of Minecraft by Markus Persson.
Uses Raylib and Zig build system.

## directory structure

```txt
./zig-out/          - zig build output
./versions.txt      - changelog
./tools/            - convenient tools to update the version string and create a release archive
./src/              - source code
./Screenshots/      - Screenshots
./run.bash          - I'm lazy
./options.conf      - game settings
./LICENSE           - license text
./include/          - header files
./external/         - raylib (submodule)
./chathistory       - self explanatory
./build.zig         - build definition
./build.sh          - build script (zig build + copy)
./b3dv              - executable (Linux)
./assets            - textures, localized text, and wallpapers / menu backgrounds
./.old              - stuff that isn't used anymore

```

## Building and Running

### Linux

```bash
./run.bash
```

Or manually:

```bash
zig build -Drelease-safe
cp zig-out/bin/b3dv ./b3dv
./b3dv run
```

## Dependencies

### Linux

- Zig (latest)
- raylib development libraries

### Install Dependencies

**Arch Linux:**

```bash
sudo pacman -S zig raylib
```

**Ubuntu/Debian:**

```bash
sudo apt update
sudo apt install zig libraylib-dev
```

**Fedora/RHEL:**

```bash
sudo dnf install zig raylib-devel
```

**macOS:**

```bash
brew install zig raylib
```

## NOTES

- Windows support has been dropped. This is a Linux household.
- Tested on Linux with raylib 5.5. and 6.0

