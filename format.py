#!/usr/bin/env python3
import os
import sys
import subprocess
from pathlib import Path

# Configuration
SRC_DIR = Path("camera_player/src")
EXTENSIONS = {".cpp", ".hpp", ".c", ".h"}

CROSS_DIR = "cross-aarch64-linux-gnu.2.17"
if os.name == "nt":
    PYTHON_ENV_BIN_DIR = "env.win/Scripts"
else:
    PYTHON_ENV_BIN_DIR = "env.nix/bin"

def setup_environment():
    """Adds the python environment bin directory to PATH."""
    script_dir = Path(__file__).parent.resolve()
    env_bin = script_dir / PYTHON_ENV_BIN_DIR
    
    if env_bin.exists():
        os.environ["PATH"] = str(env_bin) + os.pathsep + os.environ["PATH"]

def get_source_files(src_dir):
    """Recursively finds all source files with matching extensions."""
    source_files = []
    if not src_dir.exists():
        print(f"Error: Directory {src_dir} does not exist.")
        return []

    for path in src_dir.rglob("*"):
        if path.is_file() and path.suffix in EXTENSIONS:
            source_files.append(path)
    return source_files

def format_files(files):
    """Runs clang-format on the list of files."""
    if not files:
        print("No source files found to format.")
        return

    print(f"Formatting {len(files)} files...")
    
    # Check if clang-format is available
    try:
        subprocess.run(["clang-format", "--version"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Error: clang-format not found. Please run bootstrap.py first.")
        return

    success_count = 0
    error_count = 0


    for (i, file_path) in enumerate(files):
        try:
            print(f"[{i + 1} / {len(files)}] Formatting: {file_path}")
            subprocess.run(["clang-format", "-i", "--style=file", str(file_path)], check=True)
            success_count += 1
        except subprocess.CalledProcessError as e:
            print(f"Error formatting {file_path}: {e}")
            error_count += 1

    print("-" * 30)
    print(f"Formatting complete.")
    print(f"Success: {success_count}")
    print(f"Errors:  {error_count}")

def main(path: str):
    setup_environment()
    
    if path and os.path.isfile(path):
        format_files([Path(path)])
    else:
        print("Searching for source files...")
        files = get_source_files(path or SRC_DIR)
        
        format_files(files)

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else None)