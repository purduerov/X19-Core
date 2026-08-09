#!/usr/bin/env python3
"""
Stereo depth pipeline for the OAK-D W Pro on X19-Core.

The OAK-D W Pro computes a stereo depth map ONBOARD (on its Myriad X VPU) from
its two wide-FOV mono cameras (CAM_B = left, CAM_C = right). This node builds
that DepthAI pipeline, pulls the depth frames back to the Pi over XLink,
analyzes a central region of interest (nearest obstacle / median distance),
and publishes only the small summarized result over ZMQ for Surface (Path B).

Nothing here pushes raw video over ZMQ -- only the analyzed summary. The RGB
video feed is handled separately by cv_camera_connect.py (go2rtc / RTSP).
"""

import os
import sys
import time
import signal
import argparse
import threading

# Ensure project root is importable (mirrors get_ip.py)
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "../../..")))

try:
    import depthai as dai
except ImportError:
    dai = None

try:
    import numpy as np
except ImportError:
    np = None

from src.python.messaging import Publisher
from src.protocols.python import telemetry_pb2

quitEvent = threading.Event()
signal.signal(signal.SIGTERM, lambda *_a: quitEvent.set())
signal.signal(signal.SIGINT, lambda *_a: quitEvent.set())


def build_pipeline(fps, resolution):
    """Create the OAK-D stereo depth pipeline (depth is computed on-camera)."""
    pipeline = dai.Pipeline()

    mono_left = pipeline.create(dai.node.MonoCamera)
    mono_right = pipeline.create(dai.node.MonoCamera)
    stereo = pipeline.create(dai.node.StereoDepth)
    xout = pipeline.create(dai.node.XLinkOut)
    xout.setStreamName("depth")

    # OAK-D W Pro: CAM_B = left mono, CAM_C = right mono.
    # setCamera("left"/"right") resolves the socket from the device calibration.
    mono_left.setCamera("left")
    mono_left.setResolution(resolution)
    mono_left.setFps(fps)
    mono_right.setCamera("right")
    mono_right.setResolution(resolution)
    mono_right.setFps(fps)

    # High-density preset. Depth output is uint16 millimeters, aligned to the
    # right mono camera by default.
    stereo.setDefaultProfilePreset(dai.node.StereoDepth.PresetMode.HIGH_DENSITY)
    stereo.initialConfig.setMedianFilter(dai.MedianFilter.KERNEL_7x7)
    stereo.setLeftRightCheck(True)   # rejects occluded/mismatched pixels
    stereo.setSubpixel(True)         # finer depth resolution at range
    stereo.setDepthAlign(dai.CameraBoardSocket.CAM_C)

    mono_left.out.link(stereo.left)
    mono_right.out.link(stereo.right)
    stereo.depth.link(xout.input)

    return pipeline


def analyze_depth(frame, roi_frac):
    """
    Summarize a depth frame (uint16 numpy, millimeters).

    Samples a central ROI so we report the distance to whatever is directly in
    front of the ROV rather than the whole scene. A value of 0 means the pixel
    was invalid / unmatched. Returns (center_m, min_m, max_m, coverage).
    """
    h, w = frame.shape[:2]
    rh, rw = int(h * roi_frac), int(w * roi_frac)
    y0, x0 = (h - rh) // 2, (w - rw) // 2
    roi = frame[y0:y0 + rh, x0:x0 + rw]

    valid = roi[roi > 0]
    if valid.size == 0:
        return 0.0, 0.0, 0.0, 0.0

    center_m = float(np.median(valid)) / 1000.0
    min_m = float(valid.min()) / 1000.0
    max_m = float(valid.max()) / 1000.0
    coverage = float(valid.size) / float(roi.size)
    return center_m, min_m, max_m, coverage


def main():
    parser = argparse.ArgumentParser(
        description="OAK-D W Pro stereo depth analyzer + ZMQ publisher"
    )
    # ZMQ port convention across the ROV nodes:
    #   5555 -> telemetry  (SensorData: depth/temp, hello_pub.py)
    #   5556 -> surface_ip (go2rtc IP handshake, go2rtc_node.py / get_ip.py)
    #   5557 -> stereo     (this node; consumed by Surface stereo_subscriber_node.py)
    # Keep this a distinct port so large-ish stereo results never contend with
    # the control/telemetry sockets.
    parser.add_argument(
        "--publish-address",
        default=os.getenv("STEREO_ZMQ_ADDRESS", "tcp://*:5557"),
        help="ZMQ bind address for stereo results (default: tcp://*:5557)",
    )
    parser.add_argument("--topic", default="stereo", help="ZMQ topic (default: stereo)")
    parser.add_argument("--fps", type=int, default=15, help="Stereo pair FPS (default: 15)")
    parser.add_argument(
        "--roi", type=float, default=0.3,
        help="Central ROI fraction analyzed, 0-1 (default: 0.3)",
    )
    parser.add_argument(
        "--print-only", action="store_true",
        help="Analyze and print without publishing over ZMQ",
    )
    args = parser.parse_args()

    if dai is None:
        print("[Stereo Depth] DepthAI not installed. Run `pip install depthai`.", flush=True)
        sys.exit(1)
    if np is None:
        print(" [Stereo Depth] numpy not installed. Run `pip install numpy`.", flush=True)
        sys.exit(1)

    resolution = dai.MonoCameraProperties.SensorResolution.THE_400_P

    publisher = None
    if not args.print_only:
        print(f"[Stereo Depth] Publishing '{args.topic}' on {args.publish_address}", flush=True)
        publisher = Publisher(address=args.publish_address, topic=args.topic)

    print(" [Stereo Depth] Starting OAK-D pipeline...", flush=True)
    pipeline = build_pipeline(args.fps, resolution)

    with dai.Device(pipeline) as device:
        depth_queue = device.getOutputQueue(name="depth", maxSize=4, blocking=False)
        print(" [Stereo Depth] Streaming depth. Press Ctrl+C to stop.", flush=True)

        while not quitEvent.is_set():
            in_depth = depth_queue.tryGet()
            if in_depth is None:
                time.sleep(0.005)
                continue

            frame = in_depth.getFrame()  # uint16 numpy, millimeters
            h, w = frame.shape[:2]
            center_m, min_m, max_m, coverage = analyze_depth(frame, args.roi)

            print(
                f" [Stereo Depth] center={center_m:.2f}m min={min_m:.2f}m "
                f"max={max_m:.2f}m coverage={coverage * 100:.0f}%",
                flush=True,
            )

            if publisher is not None:
                msg = telemetry_pb2.StereoDepth(
                    timestamp_us=int(time.time() * 1_000_000),
                    width=w,
                    height=h,
                    center_distance_m=center_m,
                    min_distance_m=min_m,
                    max_distance_m=max_m,
                    coverage=coverage,
                )
                publisher.publish(msg)

    if publisher is not None:
        publisher.close()
    print(" [Stereo Depth] Stopped.", flush=True)


if __name__ == "__main__":
    main()
