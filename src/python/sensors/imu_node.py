#!/usr/bin/env python3

import sys
import time
import signal
import argparse
import threading

try:
    import depthai as dai
except ImportError:
    dai = None

from src.python.messaging import Publisher
from src.protocols.python import telemetry_pb2

quitEvent = threading.Event()

signal.signal(signal.SIGTERM, lambda *_args: quitEvent.set())
signal.signal(signal.SIGINT, lambda *_args: quitEvent.set())


def build_pipeline(report_rate_hz: int) -> "dai.Pipeline":
    pipeline = dai.Pipeline()

    imu = pipeline.create(dai.node.IMU)
    imu.enableIMUSensor(dai.IMUSensor.ACCELEROMETER_CALIBRATED, report_rate_hz)
    imu.enableIMUSensor(dai.IMUSensor.GYROSCOPE_CALIBRATED, report_rate_hz)
    imu.enableIMUSensor(dai.IMUSensor.ROTATION_VECTOR, report_rate_hz)
    imu.setBatchReportThreshold(1)
    imu.setMaxBatchReports(10)

    xout = pipeline.create(dai.node.XLinkOut)
    xout.setStreamName("imu")
    imu.out.link(xout.input)

    return pipeline


def packet_to_proto(packet) -> "telemetry_pb2.ImuData":
    msg = telemetry_pb2.ImuData()

    accel = packet.acceleroMeter
    msg.timestamp_us = int(accel.getTimestampDevice().total_seconds() * 1e6)
    msg.acceleration.x = accel.x
    msg.acceleration.y = accel.y
    msg.acceleration.z = accel.z

    gyro = packet.gyroscope
    msg.angular_velocity.x = gyro.x
    msg.angular_velocity.y = gyro.y
    msg.angular_velocity.z = gyro.z

    rv = packet.rotationVector
    msg.orientation.x = rv.i
    msg.orientation.y = rv.j
    msg.orientation.z = rv.k
    msg.orientation.w = rv.real
    msg.orientation.accuracy_rad = rv.rotationVectorAccuracy

    return msg


def main():
    parser = argparse.ArgumentParser(description="OAK-D W Pro IMU Publisher Node")
    parser.add_argument("--address", default="tcp://127.0.0.1:5557",
                         help="ZMQ address to bind the IMU publisher to")
    parser.add_argument("--topic", default="imu", help="ZMQ topic to publish under")
    parser.add_argument("--rate-hz", type=int, default=100,
                         help="Requested IMU report rate in Hz (BNO086 rounds up)")
    args = parser.parse_args()

    if dai is None:
        print("[IMU Node] DepthAI library not installed.", flush=True)
        sys.exit(1)

    pipeline = build_pipeline(args.rate_hz)
    publisher = Publisher(address=args.address, topic=args.topic)

    print(f"[IMU Node] Publishing on {args.address} (topic='{args.topic}'). "
          f"Press Ctrl+C to stop.", flush=True)

    try:
        with dai.Device(pipeline) as device:
            imu_queue = device.getOutputQueue(name="imu", maxSize=50, blocking=False)
            while not quitEvent.is_set():
                imu_data = imu_queue.tryGet()
                if imu_data is None:
                    time.sleep(0.001)
                    continue
                for packet in imu_data.packets:
                    publisher.publish(packet_to_proto(packet))
    finally:
        publisher.close()


if __name__ == "__main__":
    main()
