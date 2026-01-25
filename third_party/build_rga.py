#!/usr/bin/env python3
"""
Build Rockchip RGA library for aarch64-linux-gnu
使用 Meson + Zig CC 交叉编译

https://gitee.com/nyanmisaka/rga.git
commit 1d330cc28551943bed3380261a5a9c6fbd58ff53 (origin/jellyfin-rga)
Author: nyanmisaka <nst799610810@gmail.com>
Date:   Sun Oct 19 16:11:31 2025 +0800

    normal: fix inverted RGB/BGR order in FBCE of RGA3

    Signed-off-by: nyanmisaka <nst799610810@gmail.com>
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# Paths relative to this script
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent
CROSS_DIR_NAME = "cross-aarch64-linux-gnu.2.17"
CROSS_DIR = PROJECT_ROOT / CROSS_DIR_NAME

# RGA 目录结构
RGA_DIR = SCRIPT_DIR / "rga"
SRC_DIR = RGA_DIR / "src"
BUILD_DIR = RGA_DIR / "build"
INSTALL_DIR = RGA_DIR / "install"

# 确定平台相关的 Meson Cross File 和 Python Env
if os.name == "nt":
    MESON_CROSS_FILE = CROSS_DIR / "cross-file.win.meson"
    PYTHON_ENV_BIN_DIR = "env.win/Scripts"
else:
    MESON_CROSS_FILE = CROSS_DIR / "cross-file.nix.meson"
    PYTHON_ENV_BIN_DIR = "env.nix/bin"

def setup_environment():
    """设置交叉编译环境变量"""
    # 添加交叉编译工具链到 PATH
    cross_bin = CROSS_DIR / PYTHON_ENV_BIN_DIR
    os.environ["PATH"] = str(cross_bin) + os.pathsep + os.environ.get("PATH", "")
    
    print(f"Toolchain path added: {cross_bin}")
    print(f"Using Cross file: {MESON_CROSS_FILE}")

def run_cmd(cmd, cwd=None):
    """运行命令并检查返回码"""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print(f"Error: Command failed with code {result.returncode}")
        sys.exit(result.returncode)

def clone():
    """克隆 RGA 仓库"""
    if SRC_DIR.exists():
        print("RGA source already exists, skipping clone.")
        return

    print("Cloning RGA...")
    RGA_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [
        "git", "clone", 
        "-b", "jellyfin-rga", 
        "--depth", "1", 
        "https://gitee.com/nyanmisaka/rga.git", 
        str(SRC_DIR)
    ]
    run_cmd(cmd)

def configure():
    """配置 Meson 构建"""
    print("Configuring RGA...")
    
    if not SRC_DIR.exists():
        print("Error: Source directory not found. Run clone first.")
        sys.exit(1)
        
    # 清理旧的构建目录以确保配置纯净
    if BUILD_DIR.exists():
        print("Cleaning build directory...")
        shutil.rmtree(BUILD_DIR)
    
    # 参考 Bash 脚本:
    # meson setup rkrga rkrga_build \
    #     --prefix=/usr \              -> 改为本地 INSTALL_DIR
    #     --libdir=lib \
    #     --buildtype=release \
    #     --default-library=shared \
    #     -Dcpp_args=-fpermissive \
    #     -Dlibdrm=false \
    #     -Dlibrga_demo=false
    
    cmd = [
        "meson", "setup",
        str(BUILD_DIR),
        str(SRC_DIR),
        "--cross-file", str(MESON_CROSS_FILE),
        f"--prefix={INSTALL_DIR.as_posix()}", # 使用 Posix 路径防止 Windows 反斜杠问题
        "--libdir=lib",
        "--buildtype=release",
        "--default-library=shared",
        "-Dcpp_args=-fpermissive", # 关键: 允许旧式 C++ 语法
        "-Dlibdrm=false",          # 关闭 libdrm 依赖
        "-Dlibrga_demo=false"      # 不编译 demo
    ]
    
    run_cmd(cmd)

def build():
    """编译 RGA"""
    print("Building RGA...")
    # -C 指定构建目录
    cmd = ["meson", "compile", "-C", str(BUILD_DIR)]
    run_cmd(cmd)

def install():
    """安装 RGA 到本地目录"""
    print("Installing RGA...")
    cmd = ["meson", "install", "-C", str(BUILD_DIR)]
    run_cmd(cmd)

def fix_pc_prefix():
    """修复 pkgconfig 文件中的 prefix 路径，使其可重定位"""
    print("Fixing pkgconfig files...")
    pc_dir = INSTALL_DIR / "lib" / "pkgconfig"
    
    if not pc_dir.exists():
        print("Warning: pkgconfig directory not found, skipping fix.")
        return

    for pc_file in pc_dir.glob("*.pc"):
        try:
            with open(pc_file, "r", encoding="utf-8") as f:
                content = f.read()
            
            # 将绝对路径替换为相对路径变量
            # prefix=/absolute/path/to/install -> prefix=${pcfiledir}/../..
            content = content.replace(
                f"prefix={INSTALL_DIR.as_posix()}", 
                "prefix=${pcfiledir}/../.."
            )
            
            with open(pc_file, "w", encoding="utf-8") as f:
                f.write(content)
            print(f"Fixed: {pc_file.name}")
        except Exception as e:
            print(f"Error fixing {pc_file.name}: {e}")

def main():
    parser = argparse.ArgumentParser(description="Build RGA for aarch64 using Meson")
    parser.add_argument("action", nargs="?", default="all", 
                       choices=["clone", "configure", "build", "install", "fix_pc_prefix", "all"],
                       help="Action to perform")
    args = parser.parse_args()
    
    setup_environment()
    
    if args.action == "clone":
        clone()
    elif args.action == "configure":
        configure()
    elif args.action == "build":
        build()
    elif args.action == "install":
        install()
    elif args.action == "fix_pc_prefix":
        fix_pc_prefix()
    elif args.action == "all":
        clone()
        configure()
        build()
        install()
        fix_pc_prefix()

if __name__ == "__main__":
    main()
