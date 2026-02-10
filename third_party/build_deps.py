#!/usr/bin/env python3

import os
from pathlib import Path

import build_mpp
import build_rga
import build_ffmpeg
import build_spdlog

def main():
    cwd = os.getcwd()
    lib_dir = f"{Path(__file__).parent}/lib"
    os.chdir(lib_dir)

    build_mpp.main()
    build_rga.main()
    build_ffmpeg.main()
    build_spdlog.main()

    os.chdir(cwd)

if __name__ == "__main__":
    main()
