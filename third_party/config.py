#!/usr/bin/env python3
"""
Common configuration and utilities for build scripts.
"""
import os
import subprocess
import sys
from pathlib import Path

# Environment Variables
CROSS_TARGET = os.environ["CROSS_TARGET"]

# Common Paths
SCRIPT_DIR = Path(__file__).parent.resolve()
TEMP_DIR = SCRIPT_DIR / "temp"
PROJECT_ROOT = SCRIPT_DIR.parent
CROSS_DIR = PROJECT_ROOT / "cross_toolchains"
LIB_DIR = SCRIPT_DIR / "lib" / CROSS_TARGET
TOOLCHAIN_FILE = CROSS_DIR / f"toolchain.{CROSS_TARGET}.cmake"
if os.name == "nt":
    MESON_CROSS_FILE = CROSS_DIR / f"cross-file.win.{CROSS_TARGET}.meson"
else:
    MESON_CROSS_FILE = CROSS_DIR / f"cross-file.nix.{CROSS_TARGET}.meson"

def setup_env():
    """Set up cross-compilation environment variables."""
    paths = [
        str(CROSS_DIR),
        str(CROSS_DIR / CROSS_TARGET),
    ]
    
    # MSYS2 for FFmpeg build on Windows
    if os.name == "nt":
        msys2_bin = CROSS_DIR / "msys2" / "usr" / "bin"
        if msys2_bin.exists():
            paths.append(str(msys2_bin))

    os.environ["PATH"] = os.pathsep.join(paths + [os.environ.get("PATH", "")])
    print(f"Environment set up. Toolchain: {CROSS_TARGET}")

def run_cmd(cmd, cwd=None, env=None):
    """Run a command with optional environment and standard error handling."""
    cmd_list = cmd if isinstance(cmd, list) else cmd.split()
    print(f"Running: {' '.join(cmd_list)}")
    
    run_env = os.environ.copy()
    if env:
        run_env.update(env)

    # Use shell on Windows for FFmpeg-style commands if needed, 
    # but generally subprocess.run is fine without shell=True for simple lists.
    # FFmpeg script uses shell=True on Windows.
    use_shell = (os.name == "nt")
    result = subprocess.run(cmd, cwd=cwd, env=run_env, shell=use_shell)
    
    if result.returncode != 0:
        print(f"Error: Command failed with code {result.returncode}")
        sys.exit(result.returncode)
    return result

def fix_pc_prefix(install_dir):
    """Fix pkg-config files to use relative paths for portability."""
    pc_dir = install_dir / "lib" / "pkgconfig"
    if not pc_dir.exists():
        print(f"Warning: pkgconfig directory not found at {pc_dir}")
        return

    for pc_file in pc_dir.glob("*.pc"):
        try:
            with open(pc_file, "r", encoding="utf-8") as f:
                lines = f.readlines()
            
            new_lines = []
            for line in lines:
                if line.startswith("prefix="):
                    new_lines.append("prefix=${pcfiledir}/../../..\n")
                elif line.startswith("exec_prefix="):
                    new_lines.append("exec_prefix=${prefix}\n")
                elif line.startswith("libdir="):
                    new_lines.append("libdir=${pcfiledir}/..\n")
                elif line.startswith("includedir="):
                    new_lines.append("includedir=${prefix}/include\n")
                else:
                    new_lines.append(line)
            
            with open(pc_file, "w", encoding="utf-8") as f:
                f.writelines(new_lines)
            print(f"Fixed: {pc_file.name}")
        except Exception as e:
            print(f"Error fixing {pc_file.name}: {e}")
