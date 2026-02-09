# B3DV - Basic 3D Visualizer

Copyright (c) 2026 Jimena Neumann,
SPDX-License-Identifier: BSD-3-Clause

simple 3D visualizer of a ~~small slab made of cubes~~ basic world featuring:
simple terrain, multiple blocks, a worlds (with chunks) system, optimized rendering.
heavily inspired in early iterations of Minecraft by Markus Persson.
uses Raylib

## directory structure

```txt
./external/         - raylib (submodule)
./Screenshots/      - Screenshots
./src/              - source code
./tools             - convenient tools
./Makefile          - build configuration
./build.sh          - Unix-like build script
./build-windows.bat - Windows batch build script
./b3dv              - executable (linux)
./b3dv.exe          - Windows executable
./LICENSE           - license text
```

## Building and Running

### Linux, macOS, FreeBSD, and other Unix-like systems

```bash
$ make
$ ./b3dv             # Run existing executable
```

### Windows

```bash
$ make windows
```

## Dependencies

### Linux/macOS/BSD

- GCC or Clang compiler
- GNU Make
- raylib development libraries

### Windows

- MinGW-w64 (or MSVC)
- GNU Make
- raylib libraries compiled for Windows

### Install Dependencies

**Arch Linux:**

```bash
pacman -S base-devel raylib
```

**Ubuntu/Debian:**

```bash
sudo apt-get update
sudo apt-get install build-essential libraylib-dev
```

**Fedora/RHEL:**

```bash
sudo dnf install gcc make raylib-devel
```

**macOS:**

```bash
brew install raylib
```

**FreeBSD:**

```bash
sudo pkg install raylib
```

**Windows (with MinGW-w64):**

Download from https://www.mingw-w64.org/ or use MSYS2/Cygwin package managers.

# NOTES

As of right now, it has been only tested in linux (raylib 5.5)
If you use other system, kindly test it and tell me if it works - I have tried my best at compatibilitymaxxing, but I don't promise anything.

