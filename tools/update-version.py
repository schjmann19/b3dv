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
    pattern = r'\d+\.\d+\.\d+[a-z]?'
    return re.findall(pattern, content)

def update_file(filepath, old_version, new_version):
    """Update version in a single file"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

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
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    versions_file = project_root / "versionst.txt"

    latest_version = get_latest_version(versions_file)
    print(f"Latest version from versionst.txt: {latest_version}")

    files_to_update = []

    # src files
    src_dir = project_root / "src"
    files_to_update += list(src_dir.glob("*.c"))
    files_to_update += list(src_dir.glob("*.h"))

    # LICENSE
    license_file = project_root / "LICENSE"
    if license_file.exists():
        files_to_update.append(license_file)

    # README
    readme_file = project_root / "README.md"
    if readme_file.exists():
        files_to_update.append(readme_file)

    # assets/**/*.txt (recursive)
    assets_dir = project_root / "assets"
    if assets_dir.exists():
        files_to_update += list(assets_dir.glob("**/*.txt"))

    # NEVER modify the source-of-truth file
    files_to_update = [
        f for f in files_to_update
        if f.resolve() != versions_file.resolve()
    ]

    if not files_to_update:
        print("No source files found to update")
        sys.exit(1)

    print(f"\nFiles to scan for version updates:")
    for f in files_to_update:
        print(f"  - {f.relative_to(project_root)}")

    current_versions = set()
    for filepath in files_to_update:
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                current_versions.update(find_version_pattern(content))
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
