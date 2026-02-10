#!/usr/bin/env python3
"""
Build Rockchip RGA library for aarch64-linux-gnu
# Cross-compilation using Meson + Zig CC

https://gitee.com/nyanmisaka/rga.git
commit 1d330cc28551943bed3380261a5a9c6fbd58ff53 (origin/jellyfin-rga)
Author: nyanmisaka <nst799610810@gmail.com>
Date:   Sun Oct 19 16:11:31 2025 +0800

    normal: fix inverted RGB/BGR order in FBCE of RGA3

    Signed-off-by: nyanmisaka <nst799610810@gmail.com>
"""

import argparse
import os
import shutil
import sys

import config

# RGA directory structure
RGA_DIR = config.TEMP_DIR / "rga"
SRC_DIR = RGA_DIR / "src"
BUILD_DIR = RGA_DIR / "build"
INSTALL_DIR = RGA_DIR / "install"

def clone():
    """Clone RGA repository"""
    if SRC_DIR.exists():
        print("RGA source already exists, skipping clone.")
        return

    print("Cloning RGA...")
    RGA_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [
        "git", "clone", 
        "-b", "jellyfin-rga", 
        "--depth", "1", 
        "https://gitee.com/nyanmisaka/rga.git", 
        str(SRC_DIR)
    ]
    config.run_cmd(cmd)

def configure():
    """Configure Meson build"""
    print("Configuring RGA...")
    
    if not SRC_DIR.exists():
        print("Error: Source directory not found. Run clone first.")
        sys.exit(1)
        
    # Clean old build directory to ensure fresh configuration
    if BUILD_DIR.exists():
        print("Cleaning build directory...")
        shutil.rmtree(BUILD_DIR)
    
    cmd = [
        "meson", "setup",
        str(BUILD_DIR),
        str(SRC_DIR),
        "--cross-file", str(config.MESON_CROSS_FILE),
        f"--prefix={INSTALL_DIR.as_posix()}", # Use Posix path to avoid Windows backslash issues
        "--libdir=lib",
        "--buildtype=release",
        "--default-library=shared",
        "-Dcpp_args=-fpermissive", # Critical: allow old-style C++ syntax
        "-Dlibdrm=false",          # Disable libdrm dependency
        "-Dlibrga_demo=false"      # Do not compile demo
    ]
    
    config.run_cmd(cmd)

def build():
    """Compile RGA"""
    print("Building RGA...")
    # -C specifies the build directory
    cmd = ["meson", "compile", "-C", str(BUILD_DIR)]
    config.run_cmd(cmd)

def install():
    """Install RGA to local directory"""
    print("Installing RGA...")
    cmd = ["meson", "install", "-C", str(BUILD_DIR)]
    config.run_cmd(cmd)

def main():
    parser = argparse.ArgumentParser(description="Build RGA for aarch64 using Meson")
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
