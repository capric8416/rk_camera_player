#!/usr/bin/env python3

import argparse
import os
import sys
from pathlib import Path

import build_mpp
import build_rga
import build_ffmpeg
import build_spdlog

def main():
    parser = argparse.ArgumentParser(description="构建第三方依赖库")
    parser.add_argument(
        "lib",
        nargs="?",
        choices=["mpp", "rga", "ffmpeg", "spdlog", "all"],
        default="all",
        help="指定要编译的库 (默认: all)"
    )
    args = parser.parse_args()

    cwd = os.getcwd()
    lib_dir = Path(__file__).parent / "lib"
    os.chdir(lib_dir)

    # 清除 sys.argv 避免子模块的 argparse 报错
    sys.argv = [sys.argv[0]]

    build_map = {
        "mpp": build_mpp.main,
        "rga": build_rga.main,
        "ffmpeg": build_ffmpeg.main,
        "spdlog": build_spdlog.main,
    }

    if args.lib == "all":
        for build_func in build_map.values():
            build_func()
    else:
        build_func = build_map.get(args.lib)
        if build_func:
            build_func()

    os.chdir(cwd)

if __name__ == "__main__":
    main()
