#!/usr/bin/env python3
import sys
import subprocess
import os
from pathlib import Path

# ================= Configuration Area =================
TARGET_ARCH = os.environ["CROSS_TARGET"]

ZIG_BIN = ["zig"]
PYTHON_BIN = "python"

HERE = Path(__file__).parent.resolve()
WRAPPER_SCRIPT = HERE / os.path.basename(__file__)
BIN_DIR = HERE / TARGET_ARCH

TOOL_MAP = {
    "cc":      ["cc"],
    "cxx":     ["c++"],
    "c++":     ["c++"],
    "cpp":     ["c++", "-E"],
    "ld":      ["cc"],
    "ar":      ["ar"],
    "dlltool": ["dlltool"],
    "lib":     ["lib"],
    "ranlib":  ["ranlib"],
    "objcopy": ["objcopy"],
    "rc":      ["rc"],
    "windres": ["rc"],
    "strip":   ["objcopy", "--strip-all"]
}
# ===========================================

def generate_aliases():
    """Generates alias scripts (compatible with Git Bash and Windows CMD)."""
    
    # Create toolchain directory
    BIN_DIR.mkdir(exist_ok=True)
    
    print(f"[+] Generating Universal wrappers in: {BIN_DIR}")
    print(f"[+] Target Architecture: {TARGET_ARCH}")

    if os.name == "nt":
        for tool in TOOL_MAP.keys():
            # Generate Windows Batch scripts (for CMD / PowerShell)
            # -------------------------------------------------
            cmd_path = BIN_DIR / (f"zig-{tool}.cmd")
            # Use sys.executable to bypass mise shims and significantly improve startup performance on Windows
            batch_content = f'@echo off\n\n"{sys.executable}" "{WRAPPER_SCRIPT}" {tool} %*\n'
            
            with open(cmd_path, "w", encoding="utf-8") as f:
                f.write(batch_content)

        print("[v] Done. Generated .cmd files.")
    else:
        for tool in TOOL_MAP.keys():
            # Generate Unix Shell scripts (for Git Bash / configure)
            # -------------------------------------------------
            sh_path = BIN_DIR / f"zig-{tool}"
            # Use "$@" to pass all arguments
            sh_content = f'#!/bin/sh\n\n"{sys.executable}" "{WRAPPER_SCRIPT.as_posix()}" {tool} "$@"\n'
        
            with open(sh_path, "w", encoding="utf-8", newline='\n') as f:
                f.write(sh_content)
        
            # Try to grant execution permissions (needed for Linux/Mac/GitBash)
            try:
                os.chmod(sh_path, 0o755)
            except:
                pass

        print("[v] Done. Generated shell scripts.")

def run_tool(tool_name, args):
    """Run the actual Zig command (keeps original logic)."""
    if tool_name not in TOOL_MAP:
        print(f"Error: Unknown tool '{tool_name}'", file=sys.stderr)
        sys.exit(1)

    zig_subcmd = TOOL_MAP[tool_name]
    cmd = ZIG_BIN + zig_subcmd
    
    # Only inject -target for compiler-related commands
    if tool_name in ["cc", "cxx", "c++", "cpp", "ld"]:
        cmd.extend(["-target", TARGET_ARCH])
    
    # Filter out deprecated linker optimization flags
    args = [arg for arg in args if arg != "-Wl,-O1"]
    
    cmd.extend(args)

    try:
        # On Windows, if shell=True is not set, zig in PATH might sometimes be missed,
        # but subprocess.run default behavior is usually safe. If it errors, change to shell=True.
        result = subprocess.run(cmd)
        sys.exit(result.returncode)
    except FileNotFoundError:
        print("Error: 'zig' command not found in PATH.", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)

def main():
    # Add environment variables
    bin_dir_str = str(BIN_DIR)
    current_path = os.environ.get("PATH", "")
    if bin_dir_str not in current_path:
        os.environ["PATH"] = bin_dir_str + os.pathsep + current_path
    
    # If parameters is 'gen', or no parameters and run by double-clicking, generate toolchain
    if len(sys.argv) == 2 and sys.argv[1] == "gen":
        generate_aliases()
        return

    # Logical judgment: run as wrapper, or called directly?
    # Check if argv[1] is a tool name (e.g., python zig_wrapper.py cc -c main.c)
    if len(sys.argv) >= 2 and sys.argv[1] in TOOL_MAP:
        run_tool(sys.argv[1], sys.argv[2:])
    else:
        # Default behavior: generate toolchain
        generate_aliases()

if __name__ == "__main__":
    main()
