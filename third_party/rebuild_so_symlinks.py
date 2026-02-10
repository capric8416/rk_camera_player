#!/usr/bin/env python3
"""
重建 .so 软链接
将相同大小的 .so 文件还原为软链结构：
最长版本号作为实际文件，短版本依次软链到长版本
"""

import os
import re
import sys
from collections import defaultdict
from pathlib import Path

import config

CROSS_TARGET = config.CROSS_TARGET

def get_lib_base_name(filename):
    """提取库基础名称 (不含版本号)"""
    # libQt5Core.so.5.15.18 -> libQt5Core
    match = re.match(r'^(lib[^.]+)\.so', filename)
    return match.group(1) if match else None

def get_version_parts(filename):
    """提取版本号部分用于排序"""
    # libQt5Core.so.5.15.18 -> [5, 15, 11]
    # libQt5Core.so.5.15 -> [5, 15]
    # libQt5Core.so.5 -> [5]
    # libQt5Core.so -> []
    match = re.match(r'^lib[^.]+\.so(?:\.(.+))?$', filename)
    if match and match.group(1):
        parts = match.group(1).split('.')
        return [int(p) if p.isdigit() else p for p in parts]
    return []

def rebuild_symlinks(directory, dry_run=True):
    """重建目录中的 .so 软链接"""
    directory = Path(directory)
    
    # 按库名分组
    lib_groups = defaultdict(list)
    
    for f in directory.iterdir():
        if f.is_file() and '.so' in f.name:
            base_name = get_lib_base_name(f.name)
            if base_name:
                lib_groups[base_name].append(f.name)
    
    actions = []
    
    for base_name, files in lib_groups.items():
        if len(files) <= 1:
            continue
        
        # 筛选出非软链的文件作为候选 "实际文件"
        real_candidates = [f for f in files if not (directory / f).is_symlink()]
        
        if not real_candidates:
            print(f"!!! 严重错误: {base_name} 组中全都是软链! 原始文件已丢失。")
            print(f"    请从备份或 SDK 中恢复实际的 .so 文件 (通常是 .so.0 或 .so.1)")
            print(f"    跳过处理该组。\n")
            continue
        
        candidates = real_candidates

        # 在候选者中，优先选文件最大的 (认为是真实文件)
        # 其次按版本号长度 and 数值排序 (长的/大的在前)
        sorted_files = sorted(candidates, key=lambda x: (
            (directory / x).stat().st_size,
            len(get_version_parts(x)),
            get_version_parts(x)
        ), reverse=True)
        
        master_file = sorted_files[0]
        
        print(f"\n=== {base_name} ===")
        print(f"实际文件: {master_file}")
        
        # 其他所有文件 (包括不在 candidates 里的软链文件) 都应该指向 master_file
        # 但我们需要建立链式结构吗？
        # 之前的逻辑是: master <- link1 <- link2
        # lib.so.1.2 (master)
        # lib.so.1 -> lib.so.1.2
        # lib.so -> lib.so.1
        
        # 重新对 *所有* 文件进行排序，以便建立链条
        # 这里的排序依然需要遵循 "长的在前"，以便 A->B, B->C
        
        # 但必须把 master_file 放在第一位
        others = [f for f in files if f != master_file]
        others_sorted = sorted(others, key=lambda x: (len(get_version_parts(x)), get_version_parts(x)), reverse=True)
        
        # 完整的链条列表: [master, link1, link2, ...]
        chain = [master_file] + others_sorted
        
        # 创建软链链条
        for i in range(1, len(chain)):
            target = chain[i - 1]  # 链接目标 (更长/更新版本)
            link = chain[i]        # 软链名称 (更短/更旧版本)
            
            link_path = directory / link
            target_name = target  # 相对路径
            
            actions.append((link_path, target_name))
            print(f"软链: {link} -> {target}")
    
    if dry_run:
        print("\n" + "=" * 50)
        print("[DRY RUN] 以上为预览，添加 --execute 执行")
        return
    
    # 执行删除和创建软链
    print("\n" + "=" * 50)
    print("执行软链重建...")
    
    for link_path, target_name in actions:
        # 删除原文件或旧链接 (包括坏链)
        try:
            if link_path.is_symlink() or link_path.exists():
                link_path.unlink()
                print(f"删除: {link_path.name}")
        except Exception as e:
            print(f"删除失败 {link_path.name}: {e}")
            continue
        
        # 创建软链
        try:
            link_path.symlink_to(target_name)
            print(f"创建软链: {link_path.name} -> {target_name}")
        except Exception as e:
            print(f"创建软链失败 {link_path.name}: {e}")

def main():
    cwd = os.getcwd()
    lib_dir = config.LIB_DIR
    os.chdir(lib_dir)
    rebuild_symlinks(lib_dir, dry_run=False)
    os.chdir(cwd)


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        main()
    else:
        target_dir = sys.argv[1]
        dry_run = "--execute" not in sys.argv
        rebuild_symlinks(target_dir, dry_run)
