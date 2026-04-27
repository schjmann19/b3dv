# B3DV - Basic 3D Visualizer

Copyright (c) 2026 Jimena Neumann,
SPDX-License-Identifier: BSD-3-Clause

Simple 3D visualizer of a basic world featuring:
simple terrain, multiple blocks, a worlds (with chunks) system, optimized rendering.
Heavily inspired in early iterations of Minecraft by Markus Persson.
Uses Raylib and Zig build system.

## directory structure

```txt
./external/         - raylib (submodule)
./Screenshots/      - Screenshots
./src/              - source code
./tools/            - convenient tools
./run.sh            - build script (zig build + copy)
./b3dv              - executable (Linux)
./LICENSE           - license text
./zig-out/          - zig build output
```

## Building and Running

### Linux

```bash
./run.sh
./b3dv
```

Or manually:

```bash
zig build -Drelease-safe
cp zig-out/bin/b3dv ./b3dv
./b3dv
```

## Dependencies

### Linux

- Zig (latest)
- raylib development libraries

### Install Dependencies

**Arch Linux:**

```bash
pacman -S zig raylib
```

**Ubuntu/Debian:**

```bash
sudo apt-get update
sudo apt-get install raylib-dev
```

**Fedora/RHEL:**

```bash
sudo dnf install raylib-devel
```

**macOS:**

```bash
brew install raylib
```

# NOTES

- Windows support has been dropped. This is a Linux household.
- Tested on Linux with raylib 5.5. and 6.0

