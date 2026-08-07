#!/usr/bin/env python3

import sys
import time
import argparse
import subprocess


def main():
    parser = argparse.ArgumentParser(description="ROV Camera RTSP Streamer Node")
    parser.add_argument("--ip", required=True, help="Surface Go2RTC IP address")
    parser.add_argument("--device", default="/dev/video0", help="V4L2 Video Device Path")
    parser.add_argument("--camera-number", type=int, default=1, help="Camera Index (1-4)")
    args = parser.parse_args()

    ip_address = args.ip
    dev_name = args.device
    camera_num = args.camera_number

    rtsp_url = f"rtsp://{ip_address}:8554/camera{camera_num}"
    print(f"🎥 [Camera Node] Starting stream for camera {camera_num} on {dev_name} → {rtsp_url}", flush=True)

    while True:
        try:
            command = [
                "ffmpeg",
                "-f", "v4l2",
                "-video_size", "640x480",
                "-input_format", "mjpeg",
                "-i", f"{dev_name}",
                "-c:v", "libx264",
                "-preset", "ultrafast",
                "-tune", "zerolatency",
                "-pix_fmt", "yuv420p",
                "-g", "1",
                "-fflags", "nobuffer",
                "-flags", "low_delay",
                "-probesize", "32",
                "-analyzeduration", "0",
                "-rtsp_transport", "tcp",
                "-f", "rtsp",
                rtsp_url
            ]

            process = subprocess.Popen(command)
            retcode = process.wait()
            print(f"⚠️ [Camera Node] FFmpeg process exited with code {retcode}. Restarting in 3 seconds...", flush=True)
            time.sleep(3)

        except KeyboardInterrupt:
            print("\n🛑 [Camera Node] Stopping camera stream...", flush=True)
            break
        except Exception as e:
            print(f"❌ [Camera Node] Unexpected error: {e}. Restarting in 5 seconds...", flush=True)
            time.sleep(5)


if __name__ == "__main__":
    main()

