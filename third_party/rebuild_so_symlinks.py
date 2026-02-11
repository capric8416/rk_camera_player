#!/usr/bin/env python3
"""
Rebuild .so symlinks
Restore .so files of the same size into a symlink structure:
The file with the longest version number is treated as the actual file, and shorter versions link to it.
"""

import os
import re
import sys
from collections import defaultdict
from pathlib import Path

import config

CROSS_TARGET = config.CROSS_TARGET

def get_lib_base_name(filename):
    """Extract library base name (without version)"""
    # libQt5Core.so.5.15.18 -> libQt5Core
    match = re.match(r'^(lib[^.]+)\.so', filename)
    return match.group(1) if match else None

def get_version_parts(filename):
    """Extract version parts for sorting"""
    # libQt5Core.so.5.15.18 -> [5, 15, 11]
    # libQt5Core.so.5.15 -> [5, 15]
    # libQt5Core.so.5 -> [5]
    # libQt5Core.so -> []
    match = re.match(r'^lib[^.]+\.so(?:\.(.+))?$', filename)
    if match and match.group(1):
        parts = match.group(1).split('.')
        return [int(p) if p.isdigit() else p for p in parts]
    return []

def rebuild_symlinks(directory, dry_run=True):
    """Rebuild .so symlinks in the directory"""
    directory = Path(directory)
    
    # Group by library name
    lib_groups = defaultdict(list)
    
    for f in directory.iterdir():
        if f.is_file() and '.so' in f.name:
            base_name = get_lib_base_name(f.name)
            if base_name:
                lib_groups[base_name].append(f.name)
    
    actions = []
    
    for base_name, files in lib_groups.items():
        if len(files) <= 1:
            continue
        
        # Filter out non-symlink files as candidates for "actual file"
        real_candidates = [f for f in files if not (directory / f).is_symlink()]
        
        if not real_candidates:
            print(f"!!! CRITICAL ERROR: {base_name} group contains only symlinks! Original file is lost.")
            print(f"    Please restore actual .so files from backup or SDK (usually .so.0 or .so.1)")
            print(f"    Skipping this group.\n")
            continue
        
        candidates = real_candidates

        # Among candidates, prioritize the largest file (assumed to be the actual file)
        # Secondly, sort by version number length and numeric value (longest/largest first)
        sorted_files = sorted(candidates, key=lambda x: (
            (directory / x).stat().st_size,
            len(get_version_parts(x)),
            get_version_parts(x)
        ), reverse=True)
        
        master_file = sorted_files[0]
        
        print(f"\n=== {base_name} ===")
        print(f"Actual file: {master_file}")
        
        # All other files (including symlinks not in candidates) should point to master_file
        # But do we need to establish a chain structure?
        # Previous logic was: master <- link1 <- link2
        # lib.so.1.2 (master)
        # lib.so.1 -> lib.so.1.2
        # lib.so -> lib.so.1
        
        # Re-sort *all* files to establish the chain
        # Sorting still needs to follow "longest first" for A->B, B->C logic
        
        # Must put master_file first
        others = [f for f in files if f != master_file]
        others_sorted = sorted(others, key=lambda x: (len(get_version_parts(x)), get_version_parts(x)), reverse=True)
        
        # Complete chain list: [master, link1, link2, ...]
        chain = [master_file] + others_sorted
        
        # Create symlink chain
        for i in range(1, len(chain)):
            target = chain[i - 1]  # Link target (longer/newer version)
            link = chain[i]        # Symlink name (shorter/older version)
            
            link_path = directory / link
            target_name = target  # Relative path
            
            actions.append((link_path, target_name))
            print(f"Symlink: {link} -> {target}")
    
    if dry_run:
        print("\n" + "=" * 50)
        print("[DRY RUN] This is a preview, add --execute to run")
        return
    
    # Execute deletion and symlink creation
    print("\n" + "=" * 50)
    print("Executing symlink rebuilding...")
    
    for link_path, target_name in actions:
        # Delete original file or old link (including broken ones)
        try:
            if link_path.is_symlink() or link_path.exists():
                link_path.unlink()
                print(f"Deleting: {link_path.name}")
        except Exception as e:
            print(f"Delete failed {link_path.name}: {e}")
            continue
        
        # Create symlink
        try:
            link_path.symlink_to(target_name)
            print(f"Creating symlink: {link_path.name} -> {target_name}")
        except Exception as e:
            print(f"Create symlink failed {link_path.name}: {e}")

def main():
    cwd = os.getcwd()
    lib_dir = config.LIB_DIR
    if not lib_dir.exists():
        print(f"Library directory {lib_dir} does not exist")
        return
    os.chdir(lib_dir)
    rebuild_symlinks(lib_dir, dry_run=False)
    os.chdir(cwd)


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        main()
    else:
        target_dir = sys.argv[1]
        dry_run = "--execute" not in sys.argv
        rebuild_symlinks(target_dir, dry_run)
