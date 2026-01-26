#!/usr/bin/env python3

import os
import subprocess

def unarchive():
    if os.name != 'nt':
        return
    for root, dirs, files in os.walk('.'):
        for file in files:
            if file.endswith('.7z'):
                subprocess.run(['7zr', 'x', file], cwd=root)



if __name__ == '__main__':
    unarchive()