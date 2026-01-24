import os
import subprocess

if os.name == "nt":
    ENV_DIR_NAME = "env.win"
    PATH_PYTHON = ENV_DIR_NAME + "/Scripts/python.exe"
else:
    ENV_DIR_NAME = "env.nix"
    PATH_PYTHON = ENV_DIR_NAME + "/bin/python"

def create_env():
    print("Creating environment...")
    if os.name == "nt":
        if os.path.exists(ENV_DIR_NAME):
            print("Windows environment already exists.")
        else:
            print("Creating Windows environment...")
            subprocess.run(["python", "-m", "venv", ENV_DIR_NAME])
    else:
        if os.path.exists(ENV_DIR_NAME):
            print("Nix environment already exists.")
        else:
            print("Creating Nix environment...")
            subprocess.run(["python3", "-m", "venv", ENV_DIR_NAME])


def install_toolchain():
    print("Installing toolchain...")
    subprocess.run([PATH_PYTHON, "-m", "pip", "install", "-r", "requirements.txt"])

def create_zig_wrapper():
    print("Creating zig wrapper...")
    subprocess.run([PATH_PYTHON, "zig_wrapper.py", "gen"])

if __name__ == "__main__":
    create_env()
    install_toolchain()
    create_zig_wrapper()
