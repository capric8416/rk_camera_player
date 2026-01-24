#!/usr/bin/env python3
"""
重建 .so 软链接
将相同大小的 .so 文件还原为软链结构：
最长版本号作为实际文件，短版本依次软链到长版本
"""

import os
import re
from pathlib import Path
from collections import defaultdict

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
        
        # 按版本号长度排序 (长的在前 = 实际文件)
        sorted_files = sorted(files, key=lambda x: len(get_version_parts(x)), reverse=True)
        
        print(f"\n=== {base_name} ===")
        print(f"实际文件: {sorted_files[0]}")
        
        # 创建软链链条
        for i in range(1, len(sorted_files)):
            target = sorted_files[i - 1]  # 链接目标 (更长版本)
            link = sorted_files[i]        # 软链名称 (更短版本)
            
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
        # 删除原文件
        if link_path.exists() and not link_path.is_symlink():
            link_path.unlink()
            print(f"删除: {link_path.name}")
        
        # 创建软链
        if not link_path.exists():
            link_path.symlink_to(target_name)
            print(f"创建软链: {link_path.name} -> {target_name}")

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("用法: python rebuild_so_symlinks.py <目录> [--execute]")
        print("示例: python rebuild_so_symlinks.py ./lib --execute")
        sys.exit(1)
    
    target_dir = sys.argv[1]
    dry_run = "--execute" not in sys.argv
    
    rebuild_symlinks(target_dir, dry_run)