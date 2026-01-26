#!/usr/bin/env python3
"""
Build spdlog library for aarch64-linux-gnu

https://github.com/gabime/spdlog.git
commit 79524ddd08a4ec981b7fea76afd08ee05f83755d (tag: v1.17.0)
Author: gabime <gmelman1@gmail.com>
Date:   Sun Jan 4 18:06:01 2026 +0200

    spdlog version 1.17.0
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# Paths relative to this script
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent
CROSS_DIR_NAME = "cross-aarch64-linux-gnu.2.17"
CROSS_DIR = PROJECT_ROOT / CROSS_DIR_NAME
TOOLCHAIN_FILE = CROSS_DIR / "toolchain.cmake"

SPDLOG_DIR = SCRIPT_DIR / "spdlog"
SRC_DIR = SPDLOG_DIR / "src"
BUILD_DIR = SPDLOG_DIR / "build"
INSTALL_DIR = SPDLOG_DIR / "install"

def setup_environment():
    """Setup environment variables for cross-compilation"""
    if os.name == "nt":
        python_env_bin_dir = "env.win/Scripts"
    else:
        python_env_bin_dir = "env.nix/bin"
    
    # Add cross toolchain to PATH
    cross_bin = CROSS_DIR / python_env_bin_dir
    os.environ["PATH"] = str(cross_bin) + os.pathsep + os.environ.get("PATH", "")
    
    print(f"Toolchain path added: {cross_bin}")

def run_cmd(cmd, cwd=None):
    """Run a command and check return code"""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print(f"Error: Command failed with code {result.returncode}")
        sys.exit(result.returncode)

def clone():
    """Clone spdlog repository"""
    if SRC_DIR.exists():
        print("spdlog source already exists, skipping clone.")
        return

    print("Cloning spdlog...")
    SPDLOG_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [
        "git", "clone", 
        "-b", "v1.17.0", 
        "--depth", "1", 
        "https://github.com/gabime/spdlog.git", 
        str(SRC_DIR)
    ]
    run_cmd(cmd)

def configure():
    """Configure CMake"""
    print("Configuring spdlog...")
    
    # Ensure source exists
    if not SRC_DIR.exists():
        print("Error: Source directory not found. Run clone first.")
        sys.exit(1)
        
    # Clean build directory to ensure fresh configuration
    if BUILD_DIR.exists():
        print("Cleaning build directory...")
        shutil.rmtree(BUILD_DIR)
        
    cmd = [
        "cmake",
        "-G", "Ninja",
        "-S", str(SRC_DIR),
        "-B", str(BUILD_DIR),
        f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN_FILE}",
        f"-DCMAKE_INSTALL_PREFIX={INSTALL_DIR}",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DSPDLOG_BUILD_SHARED=OFF",
        "-DSPDLOG_BUILD_EXAMPLE=OFF",
        "-DSPDLOG_BUILD_TESTS=OFF",
        "-DSPDLOG_INSTALL=ON",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DCMAKE_C_LINKER_DEPFILE_SUPPORTED=OFF",
        "-DCMAKE_CXX_LINKER_DEPFILE_SUPPORTED=OFF"
    ]
    run_cmd(cmd)

def build():
    """Build spdlog"""
    print("Building spdlog...")
    cmd = ["cmake", "--build", str(BUILD_DIR), "-j"]
    run_cmd(cmd)

def install():
    """Install spdlog"""
    print("Installing spdlog...")
    cmd = ["cmake", "--install", str(BUILD_DIR)]
    run_cmd(cmd)

def fix_pc_prefix():
    """Fix pkgconfig files"""
    print("Fixing pkgconfig files...")
    pc_dir = INSTALL_DIR / "lib" / "pkgconfig"
    if not pc_dir.exists():
        return

    for pc_file in pc_dir.glob("*.pc"):
        with open(pc_file, "r") as f:
            content = f.read()
        content = content.replace(
            f"prefix={str(INSTALL_DIR.as_posix())}", 
            "prefix=${pcfiledir}/../.."
        )
        with open(pc_file, "w") as f:
            f.write(content)

def main():
    parser = argparse.ArgumentParser(description="Build spdlog for aarch64")
    parser.add_argument("action", nargs="?", default="all", 
                       choices=["clone", "configure", "build", "install", "fix_pc_prefix", "all"],
                       help="Action to perform")
    args = parser.parse_args()
    
    setup_environment()
    
    if args.action == "clone":
        clone()
    elif args.action == "configure":
        configure()
    elif args.action == "build":
        build()
    elif args.action == "install":
        install()
    elif args.action == "fix_pc_prefix":
        fix_pc_prefix()
    elif args.action == "all":
        clone()
        configure()
        build()
        install()
        fix_pc_prefix()

if __name__ == "__main__":
    main()
