#!/usr/bin/env python3

import os
import sys
from pathlib import Path
import importlib.util

# Paths
SCRIPT_DIR = Path(__file__).parent.resolve()
CROSS_DIR = SCRIPT_DIR / "cross-aarch64-linux-gnu.2.17"
THIRD_PARTY_DIR = SCRIPT_DIR / "third_party"

def import_module_from_path(module_name, file_path):
    """Import a module from a specific file path"""
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if not spec or not spec.loader:
        raise ImportError(f"Could not import {module_name} from {file_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module

def main():
    print(f"Bootstrapping compilation environment in {SCRIPT_DIR}...\n")
    
    # Save original CWD and path to restore if needed
    original_cwd = os.getcwd()

    try:
        # 1. Setup toolchains (cross-aarch64-linux-gnu.2.17)
        print("=== Setting up toolchains ===")
        os.chdir(CROSS_DIR)
        # Use importlib to avoid path conflicts and ensure correct loading
        setup_module = import_module_from_path("cross_setup", CROSS_DIR / "setup.py")
        setup_module.main()
        print("OK\n")

        # 2. Third party scripts
        print("=== Configuring third_party libraries ===")
        os.chdir(THIRD_PARTY_DIR)
        
        # Add executable permissions
        print("--- Adding permissions ---")
        perms_module = import_module_from_path("add_executable_permission", THIRD_PARTY_DIR / "add_executable_permission.py")
        perms_module.main()
        
        # Rebuild symlinks
        print("--- Rebuilding symlinks ---")
        symlinks_module = import_module_from_path("rebuild_so_symlinks", THIRD_PARTY_DIR / "rebuild_so_symlinks.py")
        symlinks_module.main()
        print("OK\n")

        # 3. Unarchive 7z files
        print("=== Unarchiving 7z files ===")
        os.chdir(CROSS_DIR)
        unarchive_module = import_module_from_path("unarchive", CROSS_DIR / "unarchive.py")
        unarchive_module.unarchive()
        print("OK\n")

        print("Bootstrap completed successfully!")
        print("You can now run: python build.py rebuild")
        return 0

    except Exception as e:
        print(f"\nERROR: Bootstrap failed: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        os.chdir(original_cwd)

if __name__ == "__main__":
    if not CROSS_DIR.exists():
        print(f"Error: Directory not found: {CROSS_DIR}")
        sys.exit(1)
    if not THIRD_PARTY_DIR.exists():
        print(f"Error: Directory not found: {THIRD_PARTY_DIR}")
        sys.exit(1)
        
    sys.exit(main())