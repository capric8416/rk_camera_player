#!/usr/bin/env python3
import os
import sys
import subprocess
from pathlib import Path

# Configuration
SRC_DIR = Path("camera_player/src")
EXTENSIONS = {".cpp", ".c"}

CROSS_DIR = "cross-aarch64-linux-gnu.2.17"
if os.name == "nt":
    PYTHON_ENV_BIN_DIR = "env.win/Scripts"
    BUILD_DIR = "builddir.win"
else:
    PYTHON_ENV_BIN_DIR = "env.nix/bin"
    BUILD_DIR = "builddir.nix"

def setup_environment():
    """Adds the python environment bin directory to PATH."""
    script_dir = Path(__file__).parent.resolve()
    env_bin = script_dir / PYTHON_ENV_BIN_DIR
    
    if env_bin.exists():
        os.environ["PATH"] = str(env_bin) + os.pathsep + os.environ["PATH"]
    return script_dir

def get_source_files(src_dir):
    """Recursively finds all source files with matching extensions."""
    source_files = []
    if not src_dir.exists():
        print(f"Error: Directory {src_dir} does not exist.")
        return []

    for path in src_dir.rglob("*"):
        if "third_party" in path.parts:
            continue
        if path.is_file() and path.suffix in EXTENSIONS:
            source_files.append(path)
    return source_files

def check_compile_commands(script_dir):
    """Checks if compile_commands.json exists in the build directory."""
    build_path = script_dir / BUILD_DIR
    compile_commands = build_path / "compile_commands.json"
    
    if not compile_commands.exists():
        print(f"Error: {compile_commands} not found.")
        print(f"Please run 'python build.py configure' first to generate it.")
        return None
    return build_path

def run_tidy(files, build_dir):
    """Runs clang-tidy on the list of files."""
    if not files:
        print("No source files found to check.")
        return

    print(f"Running clang-tidy on {len(files)} files...")
    
    # Check if clang-tidy is available
    try:
        subprocess.run(["clang-tidy", "--version"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Error: clang-tidy not found. Please run bootstrap.py first.")
        return

    success_count = 0
    error_count = 0

    for (i, file_path) in enumerate(files):
        try:
            print(f"[{i + 1} / {len(files)}] Checking: {file_path}")
            # -p specifies the build path where compile_commands.json is located
            subprocess.run(["clang-tidy", "-p", str(build_dir), str(file_path)], check=True)
            success_count += 1
        except subprocess.CalledProcessError as e:
            print(f"Error checking {file_path}: {e}")
            error_count += 1

    print("-" * 30)
    print(f"Analysis complete.")
    print(f"Files processed: {len(files)}")
    print(f"Success: {success_count}")
    print(f"Errors:  {error_count}")

def main(path: str = None):
    script_dir = setup_environment()
    
    build_dir = check_compile_commands(script_dir)
    if not build_dir:
        return

    if path and os.path.isfile(path):
        run_tidy([Path(path)], build_dir)
    else:
        print("Searching for source files...")
        files = get_source_files(Path(path) if path else (script_dir / SRC_DIR))
        files.sort()
        
        run_tidy(files, build_dir)

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Run clang-tidy on project files.")
    parser.add_argument("path", nargs="?", help="Specific file or directory to check (optional)")
    args = parser.parse_args()
    
    main(args.path)
