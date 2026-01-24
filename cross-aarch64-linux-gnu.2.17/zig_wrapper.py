#!/usr/bin/env python3
import sys
import subprocess
import os
from pathlib import Path

# ================= 配置区域 =================
TARGET_ARCH = "aarch64-linux-gnu.2.17" # 根据需要修改
ZIG_BIN = "zig"

TOOL_MAP = {
    "cc":      ["cc"],
    "cxx":     ["c++"],
    "c++":     ["c++"],
    "cpp":     ["c++", "-E"],
    "ar":      ["ar"],
    "dlltool": ["dlltool"],
    "lib":     ["lib"],
    "ranlib":  ["ranlib"],
    "objcopy": ["objcopy"],
    "rc":      ["rc"],
    "windres": ["rc"],
    "nm":      ["nm"],
    "strip":   ["objcopy", "--strip-all"]
}
# ===========================================

def generate_aliases():
    """生成替身脚本 (兼容 Git Bash 和 Windows CMD)"""
    here = Path(__file__).parent.resolve()
    wrapper_script = here / os.path.basename(__file__)
    
    # 创建 zig_toolchain 目录
    if os.name == "nt":
        bin_dir = here / "env.win" / "Scripts"
    else:
        bin_dir = here / "env.nix" / "bin"
    bin_dir.mkdir(exist_ok=True)
    
    print(f"[+] Generating Universal wrappers in: {bin_dir}")
    print(f"[+] Target Architecture: {TARGET_ARCH}")

    if os.name == "nt":
        for tool in TOOL_MAP.keys():
            # 生成 Windows Batch 脚本 (给 CMD / PowerShell 用)
            # -------------------------------------------------
            cmd_path = bin_dir / (f"zig-{tool}.cmd")
            batch_content = f'@echo off\n\npython "{wrapper_script}" {tool} %*\n'
            
            with open(cmd_path, "w", encoding="utf-8") as f:
                f.write(batch_content)

        print("[√] Done. Generated .cmd files.")
    else:
        for tool in TOOL_MAP.keys():
            # 生成 Unix Shell 脚本 (给 Git Bash / configure 用)
            # -------------------------------------------------
            sh_path = bin_dir / f"zig-{tool}"
            # 使用 "$@" 传递所有参数
            sh_content = f'#!/bin/sh\n\npython3 "{wrapper_script.as_posix()}" {tool} "$@"\n'
        
            with open(sh_path, "w", encoding="utf-8", newline='\n') as f:
                f.write(sh_content)
        
            # 尝试赋予执行权限 (Linux/Mac/GitBash 需要)
            try:
                os.chmod(sh_path, 0o755)
            except:
                pass

        print("[√] Done. Generated shell scripts.")

def run_tool(tool_name, args):
    """运行实际的 Zig 命令 (保持不变)"""
    if tool_name not in TOOL_MAP:
        print(f"Error: Unknown tool '{tool_name}'", file=sys.stderr)
        sys.exit(1)

    zig_subcmd = TOOL_MAP[tool_name]
    cmd = [ZIG_BIN] + zig_subcmd
    
    # 只有编译器相关的命令才注入 -target
    if tool_name in ["cc", "cxx", "c++", "cpp"]:
        cmd.extend(["-target", TARGET_ARCH])
    
    # 过滤掉已弃用的链接器优化标志
    args = [arg for arg in args if arg != "-Wl,-O1"]
    
    cmd.extend(args)

    try:
        # 在 Windows 上如果不设置 shell=True，有时找不到 PATH 里的 zig，
        # 但通常 subprocess.run 默认行为是安全的。如果报错可改为 shell=True。
        result = subprocess.run(cmd)
        sys.exit(result.returncode)
    except FileNotFoundError:
        print("Error: 'zig' command not found in PATH.", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)

def main():
    # 添加环境变量
    if os.name == "nt":
        os.environ["PATH"] = os.path.join(os.path.dirname(__file__), "env.win", "Scripts") + ";" + os.environ["PATH"]
    else:
        os.environ["PATH"] = os.path.join(os.path.dirname(__file__), "env.nix", "bin") + ":" + os.environ["PATH"]
    
    # 如果参数是 gen，或者没有参数且双击运行，则生成工具链
    if len(sys.argv) == 2 and sys.argv[1] == "gen":
        generate_aliases()
        return

    # 逻辑判断：是作为包装器运行，还是被直接调用？
    # 检查 argv[1] 是否是工具名 (例如: python zig_wrapper.py cc -c main.c)
    if len(sys.argv) >= 2 and sys.argv[1] in TOOL_MAP:
        run_tool(sys.argv[1], sys.argv[2:])
    else:
        # 默认行为：生成工具链
        generate_aliases()

if __name__ == "__main__":
    main()