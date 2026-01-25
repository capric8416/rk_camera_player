#!/usr/bin/env python3

import os
import stat

QT5_TOOLS_BIN_DIR = "tools/qt5/bin"

def add_executable_permission(dir):
    current_dir = os.getcwd()
    os.chdir(dir)

    for file in os.listdir():
        if not file.endswith(".exe") and not file.endswith(".conf"):
            print(file)
            os.chmod(file, os.stat(file).st_mode | stat.S_IEXEC)

    os.chdir(current_dir)

def main():
    if not os.name == "nt":
        add_executable_permission(QT5_TOOLS_BIN_DIR)
    else:
        print("This script is only for Linux/macOS")

if __name__ == "__main__":
    main()

