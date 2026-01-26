#!/usr/bin/env python3

import build_mpp
import build_rga
import build_ffmpeg
import build_spdlog

def main():
    build_mpp.main()
    build_rga.main()
    build_ffmpeg.main()
    build_spdlog.main()

if __name__ == "__main__":
    main()
