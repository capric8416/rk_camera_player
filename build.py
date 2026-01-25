#!/usr/bin/env python3
"""
Camera Player 构建脚本
使用 Meson + Zig CC 交叉编译 Qt Widgets 应用到 aarch64-linux-gnu

用法:
    python3 build.py              # 默认: configure + build
    python3 build.py configure    # 仅配置
    python3 build.py build        # 仅编译
    python3 build.py rebuild      # 清理并重新构建
    python3 build.py clean        # 清理构建目录
    python3 build.py translate    # 编译翻译文件 (.ts -> .qm)
"""

import os
import sys
import subprocess
import shutil
import argparse
from pathlib import Path

SRC_DIR = "camera_player"
CROSS_DIR = "cross-aarch64-linux-gnu.2.17"
THIRD_PARTY_DIR = "third_party"
if os.name == "nt":
    BUILD_DIR = "builddir.win"
    PYTHON_ENV_BIN_DIR = "env.win/Scripts"
    MESON_CROSS_FILE = "cross-file.win.meson"
else:
    BUILD_DIR = "builddir.nix"
    PYTHON_ENV_BIN_DIR = "env.nix/bin"
    MESON_CROSS_FILE = "cross-file.nix.meson"

def setup_environment():
    """设置环境变量"""
    script_dir = Path(__file__).parent.resolve()
    cross_dir = script_dir / CROSS_DIR
    third_party_dir = script_dir / THIRD_PARTY_DIR
    
    # 添加交叉编译工具链到 PATH
    path_additions = [
        str(cross_dir / PYTHON_ENV_BIN_DIR),
        str(third_party_dir / "tools" / "qt5" / "bin")
    ]
    os.environ["PATH"] = os.pathsep.join(path_additions) + os.pathsep + os.environ.get("PATH", "")
    
    # 添加 *.pc 路径
    pkg_config_path = third_party_dir / "lib" / "pkgconfig"
    os.environ["PKG_CONFIG_PATH"] = str(pkg_config_path)
    os.environ["PKG_CONFIG_SYSROOT_DIR"] = ""
    
    return script_dir, cross_dir

def clean(script_dir):
    """清理构建目录"""
    build_dir = script_dir / BUILD_DIR
    if build_dir.exists():
        print("清理构建目录...")
        shutil.rmtree(build_dir)
        print("清理完成")
        return True
    else:
        print("构建目录不存在，无需清理")
        return True

def configure(script_dir, cross_dir, force=False, release=True):
    """配置 Meson 构建"""
    build_dir = script_dir / BUILD_DIR
    cross_file = cross_dir / MESON_CROSS_FILE
    
    if build_dir.exists():
        if force:
            print("清理旧构建目录...")
            shutil.rmtree(build_dir)
        else:
            print("构建目录已存在，跳过配置 (使用 rebuild 强制重新配置)")
            return True
    
    buildtype = "release" if release else "debug"
    print(f"配置 Meson ({buildtype})...")
    cmd = [
        "meson", "setup",
        str(build_dir),
        str(script_dir / SRC_DIR),
        "--cross-file", str(cross_file),
        "--buildtype", buildtype,
    ]
    
    result = subprocess.run(cmd)
    return result.returncode == 0

def build(script_dir, release=False):
    """编译项目"""
    build_dir = script_dir / BUILD_DIR
    
    if not build_dir.exists():
        print("错误: 构建目录不存在，请先运行 configure")
        return False
    
    print("编译中...")
    cmd = ["meson", "compile", "-C", str(build_dir)]
    
    result = subprocess.run(cmd)
    if result.returncode != 0:
        return False
        
    if release:
        print("Release 构建: 执行 strip...")
        binary_path = build_dir / "camera_player"
        if binary_path.exists():
            # zig-strip 需要输出到新文件，不能原地 strip
            # 策略: rename -> strip -> delete renamed
            unstripped_path = binary_path.with_suffix(".unstripped")
            
            try:
                # 1. Rename original
                shutil.move(binary_path, unstripped_path)
                
                # 2. Strip to original name
                strip_cmd = ["zig-strip", "--strip-all", str(unstripped_path), str(binary_path)]
                strip_result = subprocess.run(strip_cmd, shell=True)
                
                if strip_result.returncode == 0:
                    print(f"Strip 完成: {binary_path}")
                    # 3. Delete unstripped
                    unstripped_path.unlink()
                else:
                    print("Strip 失败, 还原文件...")
                    shutil.move(unstripped_path, binary_path)
            except Exception as e:
                print(f"Strip 过程出错: {e}, unstripped_path: {unstripped_path}, binary_path: {binary_path}")
                if unstripped_path.exists() and not binary_path.exists():
                    shutil.move(unstripped_path, binary_path)
        else:
            print(f"警告: 找不到二进制文件 {binary_path}, 跳过 strip")
            
    return True

def translate(script_dir):
    """编译翻译文件 (.ts -> .qm)"""
    translations_dir = script_dir / SRC_DIR / "translations"
    
    if not translations_dir.exists():
        print("错误: translations 目录不存在")
        return False
    
    ts_files = list(translations_dir.glob("*.ts"))
    if not ts_files:
        print("没有找到 .ts 文件")
        return True
    
    print(f"编译 {len(ts_files)} 个翻译文件...")
    success = True
    
    for ts_file in ts_files:
        qm_file = ts_file.with_suffix(".qm")
        print(f"  {ts_file.name} -> {qm_file.name}")
        
        cmd = ["lrelease", str(ts_file), "-qm", str(qm_file)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"    错误: {result.stderr}")
            success = False
        else:
            print(f"    完成")
    
    return success

def main():
    parser = argparse.ArgumentParser(
        description="Camera Player 交叉编译脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python3 build.py              # 默认: configure + build
  python3 build.py configure    # 仅配置
  python3 build.py build        # 仅编译
  python3 build.py rebuild      # 清理并重新构建
  python3 build.py clean        # 清理构建目录
  python3 build.py translate    # 编译翻译文件
"""
    )
    parser.add_argument(
        "action",
        nargs="?",
        default="all",
        choices=["configure", "build", "rebuild", "clean", "translate", "all"],
        help="构建动作 (默认: all = configure + build)"
    )
    parser.add_argument(
        "--release", "-r",
        action="store_true",
        help="Release 构建"
    )
    
    args = parser.parse_args()
    
    # default to debug, unless release is specified
    release_build = args.release
    
    print("=" * 50)
    print("Camera Player 交叉编译")
    print("=" * 50)
    
    # 设置环境变量
    script_dir, cross_dir = setup_environment()
    
    print(f"项目目录: {script_dir}")
    print(f"动作: {args.action}")
    print()
    
    # 执行对应动作
    if args.action == "clean":
        if not clean(script_dir):
            return 1
    elif args.action == "configure":
        if not configure(script_dir, cross_dir, force=True, release=release_build):
            print("配置失败!")
            return 1
    elif args.action == "build":
        if not build(script_dir, release=release_build):
            print("编译失败!")
            return 1
    elif args.action == "rebuild":
        if not clean(script_dir):
            return 1
        if not configure(script_dir, cross_dir, force=True, release=release_build):
            print("配置失败!")
            return 1
        if not build(script_dir, release=release_build):
            print("编译失败!")
            return 1
    elif args.action == "translate":
        if not translate(script_dir):
            print("翻译编译失败!")
            return 1
    else:  # all (默认)
        if not configure(script_dir, cross_dir, force=False, release=release_build):
            print("配置失败!")
            return 1
        if not build(script_dir, release=release_build):
            print("编译失败!")
            return 1
    
    print()
    print("=" * 50)
    print("完成!")
    if args.action in ["all", "build", "rebuild"]:
        print(f"输出: {script_dir / BUILD_DIR / 'camera_player'}")
    print("=" * 50)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
