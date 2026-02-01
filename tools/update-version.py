#!/usr/bin/env python3
# Copyright (c) 2026 Alina Kravetska
# SPDX-License-Identifier: BSD-3-Clause

"""
Update version numbers in all source files and LICENSE.
Reads the latest version from versions.txt (first line, without the colon).
"""

import re
import sys
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

def find_version_pattern(content):
    """Find all version patterns in content (e.g., 0.0.7, 0.0.7b)"""
    # match version patterns like X.X.X or X.X.Xb, X.X.Xc, etc.
    pattern = r'\d+\.\d+\.\d+[a-z]?'
    return re.findall(pattern, content)

def update_file(filepath, old_version, new_version):
    """Update version in a single file"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # replace all occurrences of the old version with the new version
        updated_content = content.replace(old_version, new_version)

        if content != updated_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(updated_content)
            return True
        return False
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return False

def update_version():
    # get the script directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    versions_file = project_root / "versionst.txt"

    # get the latest version
    latest_version = get_latest_version(versions_file)
    print(f"Latest version from versionst.txt: {latest_version}")

    # find all C/H source files
    src_dir = project_root / "src"
    files_to_update = list(src_dir.glob("*.c")) + list(src_dir.glob("*.h"))

    # add LICENSE file
    license_file = project_root / "LICENSE"
    if license_file.exists():
        files_to_update.append(license_file)

    # add README.md
    readme_file = project_root / "README.md"
    if readme_file.exists():
        files_to_update.append(readme_file)

    if not files_to_update:
        print("No source files found to update")
        sys.exit(1)

    print(f"\nFiles to scan for version updates:")
    for f in files_to_update:
        print(f"  - {f.relative_to(project_root)}")

    # find current version(s) in files
    current_versions = set()
    for filepath in files_to_update:
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                versions = find_version_pattern(content)
                current_versions.update(versions)
        except Exception:
            pass

    if not current_versions:
        print("\nNo version patterns found in source files")
        sys.exit(0)

    current_versions = sorted(current_versions, reverse=True)
    print(f"\nCurrent version(s) found: {', '.join(current_versions)}")

    if len(current_versions) == 1 and latest_version in current_versions:
        print(f"\n✓ Files already use version {latest_version}")
        sys.exit(0)

    # update files - replace all found versions with the latest one
    versions_to_update = [v for v in current_versions if v != latest_version]

    updated_count = 0
    for old_version in versions_to_update:
        print(f"\nUpdating {old_version} → {latest_version}...")
        for filepath in files_to_update:
            if update_file(filepath, old_version, latest_version):
                print(f"  ✓ {filepath.relative_to(project_root)}")
                updated_count += 1

    print(f"\n✓ Updated {updated_count} file(s)")

if __name__ == "__main__":
    update_version()
