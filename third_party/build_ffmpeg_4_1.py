#!/usr/bin/env python3
"""
Build FFmpeg (Release 4.1) for aarch64-linux-gnu

commit 7ef6f317f8f90eb34d97e016af02905898827dc4 (release/4.1)
Author: Rémi Denis-Courmont <remi@remlab.net>
Date:   Sun Jul 16 18:18:02 2023 +0300

    avcodec/x86/mathops: clip constants used with shift instructions within inline assembly

    Fixes assembling with binutil as >= 2.41

    Signed-off-by: James Almer <jamrial@gmail.com>
    (cherry picked from commit effadce6c756247ea8bae32dc13bb3e6f464f0eb)
"""

import argparse
import os
import shutil
import subprocess
import sys

import config

# ================= Configuration Area =================
# FFmpeg directory structure
FFMPEG_DIR = config.TEMP_DIR / "ffmpeg_4.1"
FFMPEG_SRC = FFMPEG_DIR / "src"
BUILD_DIR = FFMPEG_DIR / "build" 
INSTALL_DIR = FFMPEG_DIR / "install"

def clone():
    """Clone FFmpeg repository"""
    if FFMPEG_SRC.exists():
        print("FFmpeg source already exists, skipping clone.")
        return

    print("Cloning FFmpeg 4.1...")
    cmd = [
        "git", "clone",
        "--depth", "1",
        "-b", "release/4.1",
        "https://github.com/FFmpeg/FFmpeg.git",
        str(FFMPEG_SRC)
    ]
    config.run_cmd(cmd)
    
    apply_patch()

def apply_patch():
    """Apply necessary patches to FFmpeg 4.1 source code"""
    print("Applying patches...")
    tty_c_path = FFMPEG_SRC / "libavformat" / "tty.c"
    
    if tty_c_path.exists():
        with open(tty_c_path, "r", encoding="utf-8") as f:
            content = f.read()
            
        # Fix incompatible function pointer types initializing 'int (*)(AVProbeData *)' 
        # with an expression of type 'int (const AVProbeData *)'
        old_str = "static int read_probe(const AVProbeData *p)"
        new_str = "static int read_probe(AVProbeData *p)"
        
        if old_str in content:
            content = content.replace(old_str, new_str, count=1)
            with open(tty_c_path, "w", encoding="utf-8") as f:
                f.write(content)
            print("  - Patched libavformat/tty.c (read_probe signature)")
        else:
            print("  - libavformat/tty.c already patched or signature not found")

def configure():
    """Configure FFmpeg"""
    print("Configuring FFmpeg...")

    if not FFMPEG_SRC.exists():
        print("Error: Source directory not found. Run clone first.")
        sys.exit(1)

    # Clean and create build directory
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    
    # Create a local temporary directory to avoid using system TEMP (prevents path issues with spaces/backslashes)
    LOCAL_TMP_DIR = BUILD_DIR / "tmp"
    LOCAL_TMP_DIR.mkdir(exist_ok=True)

    configure_script = FFMPEG_SRC / "configure"
    
    # [CRITICAL] All paths must be converted to POSIX style (forward slashes)
    conf_path = configure_script.as_posix()
    install_prefix = INSTALL_DIR.as_posix()

    EXT = ".cmd" if os.name == "nt" else ""
    NM = "llvm-nm" if os.name == "nt" else "nm"
    x264_install = config.TEMP_DIR / "x264_0.164.3095" / "install"

    args = [
        "bash",
        conf_path,  # Use posix path
        f"--prefix={install_prefix}",
        "--arch=aarch64",
        "--target-os=linux",
        "--cross-prefix=",
        "--enable-cross-compile",
        f"--cc=zig-cc{EXT}",
        f"--cxx=zig-cxx{EXT}",
        f"--ar=zig-ar{EXT}",
        f"--host-cc=zig-cc{EXT}", 
        f"--ranlib=zig-ranlib{EXT}",
        f"--nm={NM}",
        
        # User Configuration
        "--enable-gpl",
        "--enable-libx264",
        "--enable-static",
        "--disable-shared",
        "--disable-stripping",
        "--disable-doc",
        "--disable-programs",
        "--build-suffix=-4.1",
        f"--incdir={install_prefix}/include/ffmpeg_4.1",
        f"--extra-cflags=-fPIC -I{x264_install.as_posix()}/include",
        f"--extra-ldflags=-Wl,-Bsymbolic -L{x264_install.as_posix()}/lib",
        "--extra-libs=-ldl -lpthread -lm",
    ]

    # [CRITICAL] Force set TMPDIR to the posix path we created
    env_vars = {
        "TMPDIR": LOCAL_TMP_DIR.as_posix(),
        "TEMP": LOCAL_TMP_DIR.as_posix(),
        "TMP": LOCAL_TMP_DIR.as_posix()
    }

    # Run configure
    config.run_cmd(args, cwd=BUILD_DIR, env=env_vars)

def build():
    """Build FFmpeg"""
    print("Building FFmpeg...")

    # Fix config.h
    config_h_path = BUILD_DIR / "config.h"
    with open(config_h_path, "r", encoding="utf-8") as f:
        content = f.read()
    if "#define HAVE_SYSCTL 1" in content:
        print("PATCH: Replacing HAVE_SYSCTL with 0")
        content = content.replace("#define HAVE_SYSCTL 1", "#define HAVE_SYSCTL 0")
        with open(config_h_path, "w", encoding="utf-8") as f:
            f.write(content)

    # Workaround: Pre-create output directories
    print("Pre-creating output directories...")
    mkdirs_mak = BUILD_DIR / "mkdirs.mak"
    with open(mkdirs_mak, "w", encoding="utf-8") as f:
        f.write("include Makefile\nprint_dirs:\n\t@echo $(OUTDIRS)\n")
    
    try:
        # Need to capture output
        env = os.environ.copy()
        # Ensure path is set up (re-calling config setup_env logic within subprocess)
        # But we already did it in main()
        output = subprocess.check_output(
            ["make", "-f", str(mkdirs_mak), "print_dirs"],
            cwd=BUILD_DIR,
            env=env,
            encoding="utf-8"
        )
        
        dirs = output.strip().split()
        for d in dirs:
            dir_path = BUILD_DIR / d
            if not dir_path.exists():
                dir_path.mkdir(parents=True, exist_ok=True)
        print(f"Created {len(dirs)} output directories.")
        
    except Exception as e:
        print(f"Warning: Failed to pre-create directories: {e}")
    finally:
        if mkdirs_mak.exists():
            mkdirs_mak.unlink()

    jobs = os.cpu_count() or 1
    args = ["make", "-j", str(jobs)]
    config.run_cmd(args, cwd=BUILD_DIR)

def install():
    """Install FFmpeg"""
    print("Installing FFmpeg...")
    args = ["make", "install"]
    config.run_cmd(args, cwd=BUILD_DIR)
    print(f"FFmpeg build complete. Libraries installed to {INSTALL_DIR}")

def main():
    parser = argparse.ArgumentParser(description="Build FFmpeg for aarch64 using Zig")
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
        config.fix_pc_prefix(INSTALL_DIR, sub_include_dir="ffmpeg_4.1")
    elif args.action == "all":
        clone()
        configure()
        build()
        install()
        config.fix_pc_prefix(INSTALL_DIR, sub_include_dir="ffmpeg_4.1")

if __name__ == "__main__":
    main()