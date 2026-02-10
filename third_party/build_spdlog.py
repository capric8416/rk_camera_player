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
import shutil
import sys

import config

# Paths relative to this script
SPDLOG_DIR = config.TEMP_DIR / "spdlog"
SRC_DIR = SPDLOG_DIR / "src"
BUILD_DIR = SPDLOG_DIR / "build"
INSTALL_DIR = SPDLOG_DIR / "install"

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
    config.run_cmd(cmd)

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
        f"-DCMAKE_TOOLCHAIN_FILE={config.TOOLCHAIN_FILE}",
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
    config.run_cmd(cmd)

def build():
    """Build spdlog"""
    print("Building spdlog...")
    cmd = ["cmake", "--build", str(BUILD_DIR), "-j"]
    config.run_cmd(cmd)

def install():
    """Install spdlog"""
    print("Installing spdlog...")
    cmd = ["cmake", "--install", str(BUILD_DIR)]
    config.run_cmd(cmd)

def main():
    parser = argparse.ArgumentParser(description="Build spdlog for aarch64")
    parser.add_argument("action", nargs="?", default="all", 
                       choices=["clone", "configure", "build", "install", "fix_pc_prefix", "all"],
                       help="Action to perform")
    args = parser.parse_args()
    
    config.setup_env()
    
    if args.action == "clone":
        clone()
    elif args.action == "configure":
        configure()
    elif args.action == "build":
        build()
    elif args.action == "install":
        install()
    elif args.action == "fix_pc_prefix":
        config.fix_pc_prefix(INSTALL_DIR)
    elif args.action == "all":
        clone()
        configure()
        build()
        install()
        config.fix_pc_prefix(INSTALL_DIR)

if __name__ == "__main__":
    main()
