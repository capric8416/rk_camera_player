import os
import subprocess

def create_test_files():
    # 确保在当前脚本目录运行
    base_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(base_dir)

    print("Generating test.jpg ...")
    # 生成一张简单的 JPEG 图片，1280x720 橙色背景
    subprocess.run([
        "ffmpeg", "-y", "-f", "lavfi", "-i", "color=c=orange:s=1280x720", 
        "-frames:v", "1", "test.jpg"
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    print("Generating test.h264 ...")
    # 生成仅包含 1 帧的 H.264 原始流（Annex B），1280x720 蓝色背景
    subprocess.run([
        "ffmpeg", "-y", "-f", "lavfi", "-i", "color=c=blue:s=1280x720",
        "-frames:v", "1", 
        "-c:v", "libx264", "-profile:v", "baseline",
        # 强制每帧产生 SPS/PPS，并输出裸流 Annex B
        "-x264opts", "sps-id=1:aud=1:keyint=1:min-keyint=1:intra-refresh=1",
        "-f", "h264", "test.h264"
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    print("Generating test.h265 ...")
    # 生成仅包含 1 帧的 H.265/HEVC 原始流（Annex B），1280x720 绿色背景
    subprocess.run([
        "ffmpeg", "-y", "-f", "lavfi", "-i", "color=c=green:s=1280x720",
        "-frames:v", "1",
        "-c:v", "libx265",
        "-x265-params", "keyint=1:min-keyint=1:info=1:aud=1",
        "-f", "hevc", "test.h265"
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    print("All test files generated successfully in", base_dir)

if __name__ == "__main__":
    try:
        create_test_files()
    except subprocess.CalledProcessError as e:
        print(f"Failed to generate test files using ffmpeg. Ensure ffmpeg is installed and in PATH. Error: {e}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
