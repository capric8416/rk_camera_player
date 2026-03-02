#!/usr/bin/env python3
"""
Build x264 (Release 0.164.3095) for aarch64-linux-gnu

commit baee400fa9ced6f5481a728138fed6e867b0ff7f (master)
Author: Anton Mitrofanov <BugMaster@narod.ru>
Date:   Thu Jun 2 01:31:50 2022 +0300

    CI: Fix vlc-contrib processing on macos

    Use perl for in-place editing because sed doesn't work with symlinks.
"""

import argparse
import os
import shutil
import subprocess
import sys

import config

# ================= Configuration Area =================
# FFmpeg directory structure
X264DIR = config.TEMP_DIR / "x264_0.164.3095"
X264SRC = X264DIR / "src"
BUILD_DIR = X264DIR / "build" 
INSTALL_DIR = X264DIR / "install"

def clone():
    """Clone x264 repository"""
    if X264SRC.exists():
        print("x264 source already exists, skipping clone.")
        return

    print("Cloning x264 0.164.3095...")
    cmd = [
        "git", "clone",
        "https://code.videolan.org/videolan/x264.git",
        str(X264SRC)
    ]
    config.run_cmd(cmd)
    
    # Checkout specific commit
    cmd = [
        "git", "-C", str(X264SRC), "checkout", "baee400fa9ced6f5481a728138fed6e867b0ff7f"
    ]
    config.run_cmd(cmd)

def configure():
    """Configure x264"""
    print("Configuring x264...")

    if not X264SRC.exists():
        print("Error: Source directory not found. Run clone first.")
        sys.exit(1)

    # Clean and create build directory
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    
    # Create a local temporary directory to avoid using system TEMP (prevents path issues with spaces/backslashes)
    LOCAL_TMP_DIR = BUILD_DIR / "tmp"
    LOCAL_TMP_DIR.mkdir(exist_ok=True)

    configure_script = X264SRC / "configure"
    
    # [CRITICAL] All paths must be converted to POSIX style (forward slashes)
    conf_path = configure_script.as_posix()
    install_prefix = INSTALL_DIR.as_posix()

    EXT = ".cmd" if os.name == "nt" else ""
    NM = "llvm-nm" if os.name == "nt" else "nm"

    args = [
        "bash",
        conf_path,  # Use posix path
        f"--prefix={install_prefix}",
        "--host=aarch64-linux",
        "--cross-prefix=",
        
        # === User Flags ===
        "--enable-static",
        "--enable-pic",
        "--disable-cli",
        "--disable-asm",  # x264 ARM assembly often has issues with modern toolchains
        "--extra-cflags=-fPIC",
        "--extra-ldflags=-Wl,-Bsymbolic -lm -lpthread -ldl",
    ]

    # [CRITICAL] Force set TMPDIR to the posix path we created
    # Allow CC env to override compiler since zig-cc doesn't match standard prefix logic
    env_vars = os.environ.copy()
    env_vars.update({
        "TMPDIR": LOCAL_TMP_DIR.as_posix(),
        "TEMP": LOCAL_TMP_DIR.as_posix(),
        "TMP": LOCAL_TMP_DIR.as_posix(),
        "CC": f"zig-cc{EXT}",
        "LD": f"zig-cc{EXT}",
        "AS": f"zig-cc{EXT}",
        "AR": f"zig-ar{EXT}",
        "RANLIB": f"zig-ranlib{EXT}",
        "STRIP": f"zig-strip{EXT}",
        "STRINGS": "llvm-strings" if os.name == "nt" else "strings",
        "NM": NM,
    })

    # Run configure
    config.run_cmd(args, cwd=BUILD_DIR, env=env_vars)

def build():
    """Build x264"""
    print("Building x264...")

    jobs = os.cpu_count() or 1
    args = ["make", "-j", str(jobs)]
    config.run_cmd(args, cwd=BUILD_DIR)

def install():
    """Install x264"""
    print("Installing x264...")
    args = ["make", "install"]
    config.run_cmd(args, cwd=BUILD_DIR)
    print(f"x264 build complete. Libraries installed to {INSTALL_DIR}")

def main():
    parser = argparse.ArgumentParser(description="Build x264 for aarch64 using Zig")
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