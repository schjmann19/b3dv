#!/usr/bin/env python3
# Copyright (c) 2026 Jimena Neumann
# SPDX-License-Identifier: BSD-3-Clause

"""
Create a release archive (.tar.gz) containing:
- Executable (b3dv)
- assets folder
- LICENSE file

Release file is named b3dv-{version}.tar.gz and placed in ./releases/
Version is read from ./versions.txt (first line, semantic versioning format)
"""

import sys
import tarfile
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
    """create release archive with executable, assets, and LICENSE"""
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

    # create releases directory if it doesn't exist
    releases_dir.mkdir(exist_ok=True)

    # define output tar file
    tar_filename = f"b3dv-{version}.tar.gz"
    tar_path = releases_dir / tar_filename

    print(f"Creating archive: {tar_path}")

    # create the tar.gz file
    with tarfile.open(tar_path, 'w:gz') as tf:
        # add linux executable
        if exe_linux.exists():
            tf.add(exe_linux, arcname="b3dv")
            print(f"  Added: b3dv")
        else:
            print(f"  Warning: {exe_linux} not found")

        # add LICENSE
        if license_file.exists():
            tf.add(license_file, arcname="LICENSE")
            print(f"  Added: LICENSE")
        else:
            print(f"  Warning: {license_file} not found")

        # add assets folder recursively
        if assets_dir.exists():
            for file_path in assets_dir.rglob('*'):
                if file_path.is_file():
                    arcname = file_path.relative_to(project_root)
                    tf.add(file_path, arcname=arcname)
            print(f"  Added: assets/ (recursively)")
        else:
            print(f"  Warning: {assets_dir} not found")

    # verify file was created
    if tar_path.exists():
        size_mb = tar_path.stat().st_size / (1024 * 1024)
        print(f"\nRelease created successfully!")
        print(f"  File: {tar_filename}")
        print(f"  Size: {size_mb:.2f} MB")
        print(f"  Location: {tar_path}")
    else:
        print(f"\nError: Failed to create release file")
        sys.exit(1)


if __name__ == "__main__":
    create_release()
