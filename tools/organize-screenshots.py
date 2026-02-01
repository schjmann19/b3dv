#!/usr/bin/env python3
"""
Move all files with 'screenshot' in the name to ./Screenshots/
If a file would overwrite an existing one, add a timestamp to the name instead.
"""

import os
import shutil
from pathlib import Path
from datetime import datetime

def organize_screenshots():
    # Get the project root (parent of tools directory)
    tools_dir = Path(__file__).parent; project_root = tools_dir.parent
    screenshots_dir = project_root / "Screenshots"

    # Ensure Screenshots directory exists
    screenshots_dir.mkdir(exist_ok=True)

    moved_count = 0

    # search for all files with 'screenshot' in the name
    for file_path in project_root.rglob("*"):
        # only process files, not directories
        if not file_path.is_file():
            continue

        # 'screenshot' is in the filename (case-insensitive)
        if "screenshot" not in file_path.name.lower():
            continue

        # skip if it's already in the Screenshots directory
        if file_path.parent == screenshots_dir:
            continue

        # skip if it's this script itself
        if file_path.resolve() == Path(__file__).resolve():
            continue

        target_path = screenshots_dir / file_path.name

        # Check if target already exists
        if target_path.exists():
            # Add timestamp to filename
            stem = target_path.stem
            suffix = target_path.suffix
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            target_path = screenshots_dir / f"{stem}_{timestamp}{suffix}"

        print(f"Moving: {file_path} -> {target_path}")
        shutil.move(str(file_path), str(target_path))
        moved_count += 1

    print(f"\n{moved_count} files moved")

if __name__ == "__main__":
    organize_screenshots()
