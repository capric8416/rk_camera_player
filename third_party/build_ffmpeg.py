#!/usr/bin/env python3
"""
Build FFmpeg (Release 7.1) for aarch64-linux-gnu

commit 419cdf9dccaa726402e3e8d8a28a6b5df177aa12 (origin/release/7.1)
Author: James Almer <jamrial@gmail.com>
Date:   Fri Dec 20 12:57:48 2024 -0300

    avformat/mov: add an offset to IAMF streams

    Using audio_substream_id for AVStream ids is not ideal give that in containers
    like mp4, the IAMF structure is opaque to the outside and other streams may
    share such id values.

    Signed-off-by: James Almer <jamrial@gmail.com>
    (cherry picked from commit a8f23745074acd8a1934e3653caebd5a0ba05c6f)
"""

import argparse
import os
import shutil
import subprocess
import sys

import config

# ================= Configuration Area =================
# FFmpeg directory structure
FFMPEG_DIR = config.TEMP_DIR / "ffmpeg"
FFMPEG_SRC = FFMPEG_DIR / "src"
BUILD_DIR = FFMPEG_DIR / "build" 
INSTALL_DIR = FFMPEG_DIR / "install"

# Compatibility Shim
COMPAT_SRC = config.SCRIPT_DIR / "compat_random.c"
COMPAT_OBJ = config.SCRIPT_DIR / "compat_random.o"
COMPAT_LIB = config.SCRIPT_DIR / "libcompat_random.a"

def clone():
    """Clone FFmpeg repository"""
    if FFMPEG_SRC.exists():
        print("FFmpeg source already exists, skipping clone.")
        return

    print("Cloning FFmpeg 7.1...")
    cmd = [
        "git", "clone",
        "--depth", "1",
        "-b", "release/7.1",
        "https://github.com/FFmpeg/FFmpeg.git",
        str(FFMPEG_SRC)
    ]
    config.run_cmd(cmd)

def build_compat_shim():
    """Build compat_random.c"""
    if COMPAT_SRC.exists():
        print("Compiling compat_random.c...")
        cc_cmd = ["zig-cc", "-c", str(COMPAT_SRC), "-o", str(COMPAT_OBJ)]
        ar_cmd = ["zig-ar", "rcs", str(COMPAT_LIB), str(COMPAT_OBJ)]
        config.run_cmd(cc_cmd)
        config.run_cmd(ar_cmd)
        return True
    return False

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

    # Try to build compat shim
    has_compat = build_compat_shim()

    extra_ldflags = ""
    # Note: paths must be converted to POSIX format, otherwise LDFLAGS will have issues
    if has_compat and COMPAT_LIB.exists():
        extra_ldflags = COMPAT_LIB.as_posix()
    elif "COMPAT_LIB" in os.environ:
         extra_ldflags = os.environ["COMPAT_LIB"]

    configure_script = FFMPEG_SRC / "configure"
    
    # [CRITICAL] All paths must be converted to POSIX style (forward slashes)
    conf_path = configure_script.as_posix()
    install_prefix = INSTALL_DIR.as_posix()

    EXT = ".cmd" if os.name == "nt" else ""
    NM = "llvm-nm" if os.name == "nt" else "nm"

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
        
        # === Feature Pruning ===
        "--disable-everything",
        "--enable-muxer=mov,mp4",
        "--enable-demuxer=mov,mp4",
        "--enable-protocol=file",
        "--enable-static",
        "--disable-shared",
        "--disable-doc",
        "--disable-ffplay",
        "--disable-ffprobe",
        "--disable-ffmpeg",
        "--disable-programs",
        "--disable-avdevice",
        "--disable-avfilter",
        "--disable-swscale",
        "--disable-postproc",
        "--disable-swresample",
        "--disable-hwaccels",
        "--disable-parsers",
        "--disable-bsfs",
        "--enable-bsf=hevc_mp4toannexb,h264_mp4toannexb",
        "--disable-filters",
        "--disable-indevs",
        "--disable-outdevs",
        "--disable-encoders",
        "--disable-decoders",
        "--disable-network",
        "--disable-pthreads",
        "--disable-optimizations",
        "--disable-asm",
        
        # === Extra Flags ===
        f"--extra-ldflags={extra_ldflags}"
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
        config.fix_pc_prefix(INSTALL_DIR)
    elif args.action == "all":
        clone()
        configure()
        build()
        install()
        config.fix_pc_prefix(INSTALL_DIR)

if __name__ == "__main__":
    main()