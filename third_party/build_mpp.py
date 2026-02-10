#!/usr/bin/env python3
"""
Build Rockchip MPP library for aarch64-linux-gnu

https://gitee.com/nyanmisaka/mpp.git
commit a9380ef333102ac318628f83b5f7a460d377749e (origin/jellyfin-mpp)
Author: nyanmisaka <nst799610810@gmail.com>
Date:   Fri Nov 28 19:30:13 2025 +0800

    fix[sys_cfg]: Do not align FBC hor_stride

    Signed-off-by: nyanmisaka <nst799610810@gmail.com>
"""

import argparse
import shutil
import sys

import config

# Paths relative to this script
MPP_DIR = config.TEMP_DIR / "mpp"
SRC_DIR = MPP_DIR / "src"
BUILD_DIR = MPP_DIR / "build"
INSTALL_DIR = MPP_DIR / "install"

def clone():
    """Clone MPP repository"""
    if SRC_DIR.exists():
        print("MPP source already exists, skipping clone.")
        return

    print("Cloning MPP...")
    MPP_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [
        "git", "clone", 
        "-b", "jellyfin-mpp", 
        "--depth", "1", 
        "https://gitee.com/nyanmisaka/mpp.git", 
        str(SRC_DIR)
    ]
    config.run_cmd(cmd)

def configure():
    """Configure CMake"""
    print("Configuring MPP...")
    
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
        "-DBUILD_SHARED_LIBS=ON",
        "-DBUILD_TEST=OFF",
        "-DCMAKE_C_LINKER_DEPFILE_SUPPORTED=OFF",
        "-DCMAKE_CXX_LINKER_DEPFILE_SUPPORTED=OFF"
    ]
    config.run_cmd(cmd)

def build():
    """Build MPP"""
    print("Building MPP...")
    cmd = ["cmake", "--build", str(BUILD_DIR), "-j"]
    config.run_cmd(cmd)

def install():
    """Install MPP"""
    print("Installing MPP...")
    cmd = ["cmake", "--install", str(BUILD_DIR)]
    config.run_cmd(cmd)

def main():
    parser = argparse.ArgumentParser(description="Build MPP for aarch64")
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
