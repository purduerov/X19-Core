#!/usr/bin/env python3

import sys
import time
import signal
import argparse
import threading
import subprocess

try:
    import depthai as dai
except ImportError:
    dai = None

quitEvent = threading.Event()

signal.signal(signal.SIGTERM, lambda *_args: quitEvent.set())
signal.signal(signal.SIGINT, lambda *_args: quitEvent.set())


def main():
    parser = argparse.ArgumentParser(description="DepthAI Camera RTSP Streamer")
    parser.add_argument("--ip", default="127.0.0.1", help="Surface Go2RTC IP address")
    args = parser.parse_args()

    if dai is None:
        print("❌ [CV Camera Connect] DepthAI library not installed.", flush=True)
        sys.exit(1)

    ip = args.ip
    rtsp_url = f"rtsp://{ip}:8554/cv_camera"

    # Create DepthAI pipeline
    pipeline = dai.Pipeline()

    camRgb = pipeline.create(dai.node.ColorCamera)
    camRgb.setBoardSocket(dai.CameraBoardSocket.CAM_A)
    camRgb.setResolution(dai.ColorCameraProperties.SensorResolution.THE_720_P)
    camRgb.setFps(30)

    video = pipeline.create(dai.node.VideoEncoder)
    video.setDefaultProfilePreset(30, dai.VideoEncoderProperties.Profile.H264_MAIN)

    camRgb.video.link(video.input)

    xout = pipeline.create(dai.node.XLinkOut)
    xout.setStreamName("h264")
    video.bitstream.link(xout.input)

    device = dai.Device(pipeline)

    encoded_xout = device.getOutputQueue(name="h264", maxSize=30, blocking=True)

    # Start FFmpeg process
    ffmpeg_cmd = [
        "ffmpeg",
        "-y",
        "-f", "h264",
        "-i", "pipe:0",
        "-codec:v", "copy",
        "-g", "10",
        "-f", "rtsp",
        rtsp_url
    ]
    ffmpeg = subprocess.Popen(ffmpeg_cmd, stdin=subprocess.PIPE)

    print(f"🎥 [CV Camera Connect] Streaming to {rtsp_url}. Press Ctrl+C to stop.", flush=True)
    try:
        while not quitEvent.is_set():
            packet = encoded_xout.get()
            if packet is not None:
                ffmpeg.stdin.write(packet.getData())
    finally:
        ffmpeg.stdin.close()
        ffmpeg.wait()
        device.close()


if __name__ == "__main__":
    main()

