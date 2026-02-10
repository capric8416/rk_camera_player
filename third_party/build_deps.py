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

def remove_readonly(func, path, _):
    """Clear the read-only bit and retry deletion (for Windows .git folders)."""
    os.chmod(path, stat.S_IWRITE)
    func(path)

def main():
    parser = argparse.ArgumentParser(description="构建第三方依赖库")
    parser.add_argument(
        "lib",
        nargs="?",
        choices=["mpp", "rga", "ffmpeg", "spdlog", "all"],
        default="all",
        help="指定要编译的库 (默认: all)"
    )
    parser.add_argument(
        "-R", "--rebuild",
        action="store_true",
        help="强制重新构建 (删除 temp 目录)"
    )
    args = parser.parse_args()

    cwd = os.getcwd()
    lib_dir = config.SCRIPT_DIR / "lib"
    os.chdir(lib_dir)

    # 清除 sys.argv 避免子模块的 argparse 报错
    sys.argv = [sys.argv[0]]

    build_map = {
        "mpp": build_mpp.main,
        "rga": build_rga.main,
        "ffmpeg": build_ffmpeg.main,
        "spdlog": build_spdlog.main,
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

    if args.lib == "all":
        for name in build_map.keys():
            do_build(name)
    else:
        do_build(args.lib)

    os.chdir(cwd)

if __name__ == "__main__":
    main()
