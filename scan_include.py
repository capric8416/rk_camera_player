#!/usr/bin/env python3
import os
import sys
import argparse
import re
from pathlib import Path

def get_source_files(path):
    """Recursively finds source files or returns the file if path is a file."""
    path = Path(path)
    if not path.exists():
        print(f"Error: Path {path} does not exist.")
        return []
    
    if path.is_file():
        return [path]
    
    extensions = {".cpp", ".hpp", ".c", ".h", ".cc", ".hh"}
    files = []
    for p in path.rglob("*"):
        if p.is_file() and p.suffix in extensions:
            files.append(p)
    return files

def extract_includes(files):
    """Extracts #include lines from the given files."""
    includes = set()
    include_pattern = re.compile(r'^\s*#include\s+["<].*[">]')
    
    print(f"Scanning {len(files)} files...")
    
    for file_path in files:
        try:
            with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    line = line.strip()
                    if include_pattern.match(line):
                        includes.add(line)
        except Exception as e:
            print(f"Warning: Failed to read {file_path}: {e}")
            
    return sorted(list(includes))

def main():
    parser = argparse.ArgumentParser(description="Scan source files for #include directives and output to a .cpp file.")
    parser.add_argument("input_path", help="Input directory or file to scan")
    parser.add_argument("-o", "--output", help="Output .cpp file path", default="includes_dump.cpp")
    
    args = parser.parse_args()
    
    files = get_source_files(args.input_path)
    if not files:
        print("No source files found.")
        return
        
    includes = extract_includes(files)
    
    if not includes:
        print("No #include directives found.")
        return
        
    output_path = Path(args.output)
    try:
        with open(output_path, "w", encoding="utf-8") as f:
            f.write("// Auto-generated include dump\n")
            for inc in includes:
                f.write(inc + "\n")
        print(f"Successfully wrote {len(includes)} unique includes to {output_path}")
    except Exception as e:
        print(f"Error writing output file: {e}")

if __name__ == "__main__":
    main()
