#!/usr/bin/env python3
# Copyright (c) 2026 Jimena Neumann
# SPDX-License-Identifier: BSD-3-Clause

"""
Create a release archive (.zip) containing:
- Executables (b3dv and b3dv.exe)
- assets folder
- LICENSE file

Release file is named b3dv-{version}.zip and placed in ./releases/
Version is read from ./versions.txt (first line)
"""

import sys
import zipfile
from pathlib import Path


def get_latest_version(versions_file):
    """Extract the latest version from versions.txt"""
    try:
        with open(versions_file, 'r') as f:
            first_line = f.readline().strip()
            # remove trailing colon if present
            version = first_line.rstrip(':')
            return version
    except FileNotFoundError:
        print(f"Error: {versions_file} not found")
        sys.exit(1)


def create_release():
    """create release archive with executables, assets, and LICENSE"""
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    # get version
    versions_file = project_root / "versions.txt"
    version = get_latest_version(versions_file)
    print(f"Creating release for version {version}...")

    # define paths
    releases_dir = project_root / "releases"
    assets_dir = project_root / "assets"
    license_file = project_root / "LICENSE"
    exe_linux = project_root / "b3dv"
    exe_windows = project_root / "b3dv.exe"

    # create releases directory if it doesn't exist
    releases_dir.mkdir(exist_ok=True)

    # define output zip file
    zip_filename = f"b3dv-{version}.zip"
    zip_path = releases_dir / zip_filename

    print(f"Creating archive: {zip_path}")

    # create the zip file
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zf:
        # add linux executable
        if exe_linux.exists():
            zf.write(exe_linux, arcname="b3dv")
            print(f"  Added: b3dv")
        else:
            print(f"  Warning: {exe_linux} not found")

        # add windows executable
        if exe_windows.exists():
            zf.write(exe_windows, arcname="b3dv.exe")
            print(f"  Added: b3dv.exe")
        else:
            print(f"  Warning: {exe_windows} not found")

        # add LICENSE
        if license_file.exists():
            zf.write(license_file, arcname="LICENSE")
            print(f"  Added: LICENSE")
        else:
            print(f"  Warning: {license_file} not found")

        # add assets folder recursively
        if assets_dir.exists():
            for file_path in assets_dir.rglob('*'):
                if file_path.is_file():
                    arcname = file_path.relative_to(project_root)
                    zf.write(file_path, arcname=arcname)
            print(f"  Added: assets/ (recursively)")
        else:
            print(f"  Warning: {assets_dir} not found")

    # verify file was created
    if zip_path.exists():
        size_mb = zip_path.stat().st_size / (1024 * 1024)
        print(f"\nRelease created successfully!")
        print(f"  File: {zip_filename}")
        print(f"  Size: {size_mb:.2f} MB")
        print(f"  Location: {zip_path}")
    else:
        print(f"\nError: Failed to create release file")
        sys.exit(1)


if __name__ == "__main__":
    create_release()
