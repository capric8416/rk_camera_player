#!/usr/bin/env python3
"""
Build libusb (v1.0.21) for aarch64-linux-gnu

Version: v1.0.21
"""

import argparse
import os
import shutil
import sys

import config

# ================= Configuration Area =================
LIBUSBDIR = config.TEMP_DIR / "libusb_1.0.21"
LIBUSBSRC = LIBUSBDIR / "src"
BUILD_DIR = LIBUSBDIR / "build" 
INSTALL_DIR = LIBUSBDIR / "install"

def clone():
    """Clone libusb repository"""
    if LIBUSBSRC.exists():
        print("libusb source already exists, skipping clone.")
        return

    print("Cloning libusb v1.0.21...")
    cmd = [
        "git", "clone",
        "-b", "v1.0.21",
        "https://github.com/libusb/libusb.git",
        str(LIBUSBSRC)
    ]
    config.run_cmd(cmd)

def configure():
    """Configure libusb"""
    print("Configuring libusb...")

    if not LIBUSBSRC.exists():
        print("Error: Source directory not found. Run clone first.")
        sys.exit(1)

    # Clean and create build directory
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    
    # Generate configure script if it doesn't exist
    configure_script = LIBUSBSRC / "configure"
    if not configure_script.exists():
        print("Running autogen.sh...")
        # autogen.sh requires NOCONFIGURE=1 to just generate scripts without running configure
        env_vars = os.environ.copy()
        env_vars["NOCONFIGURE"] = "1"
        config.run_cmd(["bash", "autogen.sh"], cwd=LIBUSBSRC, env=env_vars)
    
    # Create a local temporary directory to avoid using system TEMP
    LOCAL_TMP_DIR = BUILD_DIR / "tmp"
    LOCAL_TMP_DIR.mkdir(exist_ok=True)

    # [CRITICAL] All paths must be converted to POSIX style (forward slashes)
    conf_path = configure_script.as_posix()
    install_prefix = INSTALL_DIR.as_posix()

    EXT = ".cmd" if os.name == "nt" else ""

    args = [
        "bash",
        conf_path,  # Use posix path
        f"--prefix={install_prefix}",
        "--host=aarch64-linux",
        
        # === User Flags ===
        "--enable-static",
        "--disable-shared",
        "--disable-udev", # Often problematic in cross-compilation unless explicitly needed
    ]

    # Allow CC env to override compiler
    env_vars = os.environ.copy()
    env_vars.update({
        "TMPDIR": LOCAL_TMP_DIR.as_posix(),
        "TEMP": LOCAL_TMP_DIR.as_posix(),
        "TMP": LOCAL_TMP_DIR.as_posix(),
        "CC": f"zig-cc{EXT}",
        "CXX": f"zig-cxx{EXT}",
        "LD": f"zig-cc{EXT}",
        "AR": f"zig-ar{EXT}",
        "RANLIB": f"zig-ranlib{EXT}",
        "STRIP": f"zig-strip{EXT}",
    })

    # Run configure
    config.run_cmd(args, cwd=BUILD_DIR, env=env_vars)

def build():
    """Build libusb"""
    print("Building libusb...")

    jobs = os.cpu_count() or 1
    args = ["make", "-j", str(jobs)]
    config.run_cmd(args, cwd=BUILD_DIR)

def install():
    """Install libusb"""
    print("Installing libusb...")
    args = ["make", "install"]
    config.run_cmd(args, cwd=BUILD_DIR)
    print(f"libusb build complete. Libraries installed to {INSTALL_DIR}")

def main():
    parser = argparse.ArgumentParser(description="Build libusb for aarch64 using Zig")
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
