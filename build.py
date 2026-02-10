#!/usr/bin/env python3
"""
Camera Player Build Script
Cross-compiles Qt Widgets application to aarch64-linux-gnu using Meson + Zig CC.

Usage:
    python3 build.py              # Default: configure + build
    python3 build.py configure    # Only configure
    python3 build.py build        # Only build
    python3 build.py rebuild      # Clean and rebuild
    python3 build.py clean        # Clean build directory
    python3 build.py translate    # Compile translation files (.ts -> .qm)
"""

import os
import sys
import subprocess
import shutil
import argparse
from pathlib import Path

SRC_DIR = "camera_player"
CROSS_DIR = "cross_toolchains"
CROSS_TARGET = os.environ["CROSS_TARGET"]
THIRD_PARTY_DIR = "third_party"
if os.name == "nt":
    BUILD_DIR = "builddir.win"
    MESON_CROSS_FILE = f"cross-file.win.{CROSS_TARGET}.meson"
else:
    BUILD_DIR = "builddir.nix"
    MESON_CROSS_FILE = f"cross-file.nix.{CROSS_TARGET}.meson"

def setup_environment():
    """Sets up cross-compilation environment variables."""
    script_dir = Path(__file__).parent.resolve()
    cross_dir = script_dir / CROSS_DIR
    third_party_dir = script_dir / THIRD_PARTY_DIR
    
    # Add cross-compilation toolchain to PATH
    path_additions = [
        str(cross_dir / CROSS_TARGET),
        str(third_party_dir / "tools" / "qt5" / "bin")
    ]
    os.environ["PATH"] = os.pathsep.join(path_additions) + os.pathsep + os.environ.get("PATH", "")
    print("PATH:", os.environ["PATH"])
    
    # Add *.pc paths
    pkg_config_path = third_party_dir / "lib" / CROSS_TARGET / "pkgconfig"
    os.environ["PKG_CONFIG_PATH"] = str(pkg_config_path)
    os.environ["PKG_CONFIG_SYSROOT_DIR"] = ""
    print("PKG_CONFIG_PATH:", os.environ["PKG_CONFIG_PATH"])
    
    # Set CMAKE_PREFIX_PATH so CMake can find third-party libraries (e.g., rapidyaml)
    cmake_config_dir = third_party_dir / "lib" / CROSS_TARGET / "cmake"
    os.environ["CMAKE_PREFIX_PATH"] = str(cmake_config_dir)
    print("CMAKE_PREFIX_PATH:", os.environ["CMAKE_PREFIX_PATH"])
    
    return script_dir, cross_dir

def clean(script_dir):
    """Cleans the build directory."""
    build_dir = script_dir / BUILD_DIR
    if build_dir.exists():
        print("Cleaning build directory...")
        shutil.rmtree(build_dir)
        print("Clean complete.")
        return True
    else:
        print("Build directory does not exist, no clean needed.")
        return True

def configure(script_dir, cross_dir, force=False, release=True):
    """Configures Meson build."""
    build_dir = script_dir / BUILD_DIR
    cross_file = cross_dir / MESON_CROSS_FILE
    
    if build_dir.exists():
        if force:
            print("Cleaning old build directory...")
            shutil.rmtree(build_dir)
        else:
            print("Build directory already exists, skipping configuration (use rebuild to force).")
            return True
    
    buildtype = "release" if release else "debug"
    print(f"Configuring Meson ({buildtype})...")
    cmd = [
        "meson", "setup",
        str(build_dir),
        str(script_dir / SRC_DIR),
        "--cross-file", str(cross_file),
        "--buildtype", buildtype,
    ]
    
    result = subprocess.run(cmd)
    return result.returncode == 0

def build(script_dir, release=False):
    """Compiles the project."""
    build_dir = script_dir / BUILD_DIR
    
    if not build_dir.exists():
        print("Error: Build directory does not exist, please run configure first.")
        return False
    
    print("Building...")
    cmd = ["meson", "compile", "-C", str(build_dir)]
    
    result = subprocess.run(cmd)
    if result.returncode != 0:
        return False
        
    if release:
        print("Release build: performing strip...")
        binary_path = build_dir / "camera_player"
        if binary_path.exists():
            # zig-strip requires outputting to a new file, cannot strip in-place
            # Strategy: rename -> strip -> delete renamed
            unstripped_path = binary_path.with_suffix(".unstripped")
            
            try:
                # 1. Rename original
                shutil.move(binary_path, unstripped_path)
                
                # 2. Strip to original name
                strip_cmd = ["zig-strip", "--strip-all", str(unstripped_path), str(binary_path)]
                strip_result = subprocess.run(strip_cmd, shell=True)
                
                if strip_result.returncode == 0:
                    print(f"Strip complete: {binary_path}")
                    # 3. Delete unstripped
                    unstripped_path.unlink()
                else:
                    print("Strip failed, restoring file...")
                    shutil.move(unstripped_path, binary_path)
            except Exception as e:
                print(f"Error during strip process: {e}, unstripped_path: {unstripped_path}, binary_path: {binary_path}")
                if unstripped_path.exists() and not binary_path.exists():
                    shutil.move(unstripped_path, binary_path)
        else:
            print(f"Warning: Could not find binary {binary_path}, skipping strip")
            
    return True

def translate(script_dir):
    """Compiles translation files (.ts -> .qm)."""
    translations_dir = script_dir / SRC_DIR / "translations"
    
    if not translations_dir.exists():
        print("Error: translations directory does not exist.")
        return False
    
    ts_files = list(translations_dir.glob("*.ts"))
    if not ts_files:
        print("No .ts files found.")
        return True
    
    print(f"Compiling {len(ts_files)} translation files...")
    success = True
    
    for ts_file in ts_files:
        qm_file = ts_file.with_suffix(".qm")
        print(f"  {ts_file.name} -> {qm_file.name}")
        
        cmd = ["lrelease", str(ts_file), "-qm", str(qm_file)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"    Error: {result.stderr}")
            success = False
        else:
            print(f"    Done")
    
    return success

def main():
    parser = argparse.ArgumentParser(
        description="Camera Player Cross-compilation Script",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 build.py              # Default: configure + build
  python3 build.py configure    # Only configure
  python3 build.py build        # Only build
  python3 build.py rebuild      # Clean and rebuild
  python3 build.py clean        # Clean build directory
  python3 build.py translate    # Compile translation files
"""
    )
    parser.add_argument(
        "action",
        nargs="?",
        default="all",
        choices=["configure", "build", "rebuild", "clean", "translate", "all"],
        help="Build action (default: all = configure + build)"
    )
    parser.add_argument(
        "--release", "-r",
        action="store_true",
        help="Release build"
    )
    
    args = parser.parse_args()
    
    # default to debug, unless release is specified
    release_build = args.release
    
    print("=" * 50)
    print("Camera Player Cross-compilation")
    print("=" * 50)
    
    # Set environment variables
    script_dir, cross_dir = setup_environment()
    
    print(f"Project directory: {script_dir}")
    print(f"Action: {args.action}")
    print()
    
    # Execute corresponding action
    if args.action == "clean":
        if not clean(script_dir):
            return 1
    elif args.action == "configure":
        if not configure(script_dir, cross_dir, force=True, release=release_build):
            print("Configuration failed!")
            return 1
    elif args.action == "build":
        if not build(script_dir, release=release_build):
            print("Compilation failed!")
            return 1
    elif args.action == "rebuild":
        if not clean(script_dir):
            return 1
        if not configure(script_dir, cross_dir, force=True, release=release_build):
            print("Configuration failed!")
            return 1
        if not build(script_dir, release=release_build):
            print("Compilation failed!")
            return 1
    elif args.action == "translate":
        if not translate(script_dir):
            print("Translation compilation failed!")
            return 1
    else:  # all (default)
        if not configure(script_dir, cross_dir, force=False, release=release_build):
            print("Configuration failed!")
            return 1
        if not build(script_dir, release=release_build):
            print("Compilation failed!")
            return 1
    
    print()
    print("=" * 50)
    print("Done!")
    if args.action in ["all", "build", "rebuild"]:
        print(f"Output: {script_dir / BUILD_DIR / 'camera_player'}")
    print("=" * 50)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
