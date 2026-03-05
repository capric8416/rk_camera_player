#!/usr/bin/env python3

import argparse
import os
import shutil
import stat
import sys
from pathlib import Path

import config
import build_mpp
import build_rga
import build_ffmpeg
import build_spdlog
import build_ffmpeg_4_1
import build_x264_0_164_3095
import build_libusb
import build_google_test

def remove_readonly(func, path, _):
    """Clear the read-only bit and retry deletion (for Windows .git folders)."""
    os.chmod(path, stat.S_IWRITE)
    func(path)

def main():
    parser = argparse.ArgumentParser(description="Build third-party dependencies")
    parser.add_argument(
        "lib",
        nargs="?",
        choices=["mpp", "rga", "ffmpeg", "spdlog", "ffmpeg_4_1", "google_test", "libusb", "x264_0_164_3095", "all"],
        default="all",
        help="Library to build (default: all)"
    )
    parser.add_argument(
        "-R", "--rebuild",
        action="store_true",
        help="Force rebuild (delete temp directory)"
    )
    parser.add_argument(
        "-D", "--deploy",
        action="store_true",
        help="Deploy build results to unified directory (temp/install)"
    )
    args = parser.parse_args()

    cwd = os.getcwd()
    lib_dir = config.SCRIPT_DIR / "lib"
    os.chdir(lib_dir)

    # Clear sys.argv to avoid argparse errors in submodules
    sys.argv = [sys.argv[0]]

    build_map = {
        "mpp": build_mpp.main,
        "rga": build_rga.main,
        "libusb": build_libusb.main,
        "ffmpeg": build_ffmpeg.main,
        "spdlog": build_spdlog.main,
        "ffmpeg_4_1": build_ffmpeg_4_1.main,
        "google_test": build_google_test.main,
        "x264_0_164_3095": build_x264_0_164_3095.main,
    }

    def do_build(name):
        if args.rebuild:
            temp_path = config.TEMP_DIR / name
            if temp_path.exists():
                print(f"Cleaning {name} build directory: {temp_path}")
                shutil.rmtree(temp_path, onexc=remove_readonly)
        
        build_func = build_map.get(name)
        if build_func:
            build_func()
            
        if args.deploy:
            src_install = config.TEMP_DIR / name / "install"
            dst_install = config.TEMP_DIR / "install"
            if src_install.exists():
                print(f"Deploying {name} to {dst_install}...")
                shutil.copytree(src_install, dst_install, dirs_exist_ok=True)

    if args.lib == "all":
        for name in build_map.keys():
            do_build(name)
    else:
        do_build(args.lib)

    os.chdir(cwd)

if __name__ == "__main__":
    main()
