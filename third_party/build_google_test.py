#!/usr/bin/env python3
"""
Build Google Test (gtest and gmock) for aarch64-linux-gnu

https://github.com/google/googletest
v1.17.0
"""

import argparse
import shutil
import sys

import config

# Paths relative to this script
GOOGLE_TEST_DIR = config.TEMP_DIR / "google_test"
SRC_DIR = GOOGLE_TEST_DIR / "src"
BUILD_DIR = GOOGLE_TEST_DIR / "build"
INSTALL_DIR = GOOGLE_TEST_DIR / "install"

def clone():
    """Clone Google Test repository"""
    if SRC_DIR.exists():
        print("Google Test source already exists, skipping clone.")
        return

    print("Cloning Google Test...")
    GOOGLE_TEST_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [
        "git", "clone", 
        "-b", "v1.17.0", 
        "--depth", "1", 
        "https://github.com/google/googletest.git", 
        str(SRC_DIR)
    ]
    config.run_cmd(cmd)

def configure():
    """Configure CMake"""
    print("Configuring Google Test...")
    
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
        "-DBUILD_SHARED_LIBS=OFF",
        "-DINSTALL_GTEST=ON",
        "-DBUILD_GMOCK=ON",
        "-DCMAKE_C_LINKER_DEPFILE_SUPPORTED=OFF",
        "-DCMAKE_CXX_LINKER_DEPFILE_SUPPORTED=OFF"
    ]
    # On Windows, some CMake packages might need extra help finding threads if not in the toolchain
    # But usually Google Test is fine.
    
    config.run_cmd(cmd)

def build():
    """Build Google Test"""
    print("Building Google Test...")
    cmd = ["cmake", "--build", str(BUILD_DIR), "-j"]
    config.run_cmd(cmd)

def install():
    """Install Google Test"""
    print("Installing Google Test...")
    cmd = ["cmake", "--install", str(BUILD_DIR)]
    config.run_cmd(cmd)

def main():
    parser = argparse.ArgumentParser(description="Build Google Test for aarch64")
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
