#!/usr/bin/env python3

import os
import sys
import time
import signal
import ipaddress
import subprocess
import threading
import argparse

# Ensure project root is in sys.path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "../../..")))


from src.python.messaging import Subscriber
from src.protocols.python import telemetry_pb2


class IpSubscriberNode:
    """
    ZeroMQ IP Subscriber Node for X19-Core.
    Subscribes to the surface_ip topic published by Surface go2rtc_node,
    extracts the surface IP address, and launches RTSP camera streaming subprocesses.
    """
    def __init__(self, surface_address: str = "tcp://192.168.1.7", topic: str = "surface_ip"):
        self.surface_address = surface_address
        self.topic = topic
        self.cameras_launched = False
        self.shutting_down = False

        print(f"📡 [ZMQ IP Sub Node] Connecting to ZMQ publisher at '{self.surface_address}' on topic '{self.topic}'...", flush=True)
        self.subscriber = Subscriber(
            address=self.surface_address,
            topic=self.topic,
            message_type=telemetry_pb2.test,
            callback=self.on_ip_received,
            bind=False
        )

    def on_ip_received(self, msg):
        received_ip = getattr(msg, "msg", "").strip()
        if not received_ip:
            return

        if self.cameras_launched:
            return

        try:
            ipaddress.ip_address(received_ip)
            print(f"✅ [ZMQ IP Sub Node] Received Surface IP: '{received_ip}'", flush=True)
            self.cameras_launched = True
            self.launch_cameras(received_ip)
        except ValueError:
            print(f"⚠️ [ZMQ IP Sub Node] Invalid IP received: '{received_ip}'", flush=True)

    def launch_cameras(self, ip: str):
        print(f"🚀 [ZMQ IP Sub Node] Discovering video devices for RTSP streaming to {ip}:8554...", flush=True)
        try:
            output = subprocess.run(["v4l2-ctl", "--list-devices"], capture_output=True, text=True).stdout
        except FileNotFoundError:
            print("❌ [ZMQ IP Sub Node] 'v4l2-ctl' not found. Please install v4l-utils.", flush=True)
            return

        lines = output.splitlines()
        explorehd_devices = []

        i = 0
        while i < len(lines):
            if "exploreHD" in lines[i] or "Arducam" in lines[i] or "Intel" in lines[i]:
                devices = []
                i += 1
                while i < len(lines) and lines[i].startswith("\t"):
                    devices.append(lines[i].strip())
                    i += 1
                if len(devices) >= 1:
                    explorehd_devices.append(devices[0])
            else:
                i += 1

        print(f"📹 [ZMQ IP Sub Node] Discovered devices: {explorehd_devices}", flush=True)

        if not explorehd_devices:
            print("⚠️ [ZMQ IP Sub Node] No exploreHD/Arducam/Intel cameras found via v4l2-ctl.", flush=True)
            return

        cam_idx = 1
        script_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "videos_launch.py")

        for device in explorehd_devices:
            if cam_idx > 4:
                print("ℹ️ [ZMQ IP Sub Node] Device limit (4) reached.", flush=True)
                break

            print(f"🎥 [ZMQ IP Sub Node] Launching camera streamer #{cam_idx} for device {device} -> rtsp://{ip}:8554/camera{cam_idx}", flush=True)
            cmd = [
                sys.executable,
                script_path,
                "--ip", ip,
                "--device", device,
                "--camera-number", str(cam_idx)
            ]
            thread = threading.Thread(target=subprocess.run, args=(cmd,), kwargs={"check": False}, daemon=True)
            thread.start()
            cam_idx += 1

    def close(self):
        if not self.shutting_down:
            self.shutting_down = True
            print("🛑 [ZMQ IP Sub Node] Shutting down...", flush=True)
            if hasattr(self, "subscriber") and self.subscriber:
                self.subscriber.close()


def main():
    parser = argparse.ArgumentParser(description="ZMQ Surface IP Subscriber & Camera Launcher")
    parser.add_argument(
        "--surface-address",
        default=os.getenv("SURFACE_ZMQ_ADDRESS", "tcp://192.168.1.7:5556"),
        help="ZMQ Publisher address of Surface Go2RTC node (default: tcp://192.168.1.7:5556)"
    )
    args = parser.parse_args()

    node = IpSubscriberNode(surface_address=args.surface_address)

    def signal_handler(sig, frame):
        node.close()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    print("🚀 [ZMQ IP Sub Node] Waiting for Surface IP message...", flush=True)
    try:
        while not node.shutting_down:
            node.subscriber.spin_once(timeout_ms=100)
            time.sleep(0.01)
    except KeyboardInterrupt:
        pass
    finally:
        node.close()


if __name__ == "__main__":
    main()

