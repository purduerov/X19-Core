# X19-Core: Purdue ROV ZeroMQ & Protobuf System

This repository hosts the core communication, sensing, and control architecture for the Purdue ROV **X-19** underwater vehicle. The system uses **ZeroMQ (ZMQ)** for low-latency inter-process communication and **Protocol Buffers (Protobuf)** for type-safe, cross-language serialization (C++ and Python).

---

## 📂 Codebase Directory Layout

```
X19-Core/
├── config/                  # Configuration files (ports, address bindings)
├── src/                    # Communication wrappers & middleware
│   ├── cpp/                 # C++ wrapper library
│   └── python/              # Python wrapper library (python.messaging package)
│       └── messaging/
│           ├── publisher.py
│           └── subscriber.py
├── nodes/                   # Independent executable nodes
│   ├── cpp/                 # C++ nodes (thrusters, pid controls, etc.)
│   └── python/              # Python nodes (video, pilot control, etc.)
├── proto/                   # Protobuf message schemas
├── scripts/                 # Utility scripts (setup, compilation)
├── testing/                 # Validation and testing scripts
├── run.sh                   # Main runner shortcut script
└── MVPs.md                  # Development checklist & milestones
```

---

## ⚙️ Setup & Installation

### 1. System & Python Dependencies
Prerequisites include the Protobuf Compiler (`protoc`), ZeroMQ development headers, and Python 3. 

Run the automated setup script to provision your system:
```bash
./scripts/setup.sh
```
*(This installs `protobuf-compiler` and `libzmq3-dev` via apt-get, and builds the Python dependencies defined in `requirements.txt` into your virtual environment).*

### 2. Compile Protobuf Schemas
Compile your `.proto` files in the `proto/` directory to generate C++ and Python bindings:
```bash
./scripts/compile_protos.sh
```
This outputs compiled bindings directly to `src/protocols/cpp` and `src/protocols/python`.

---

## 🚀 Running the Code

Use the root [run.sh](file:///home/aditya/purdue/ROV/X-19/X19-Core/run.sh) script to execute your code with the correct python paths set:

```bash
# Run the hello_pub test node
./run.sh

# Recompile protobuf schemas AND run the hello_pub test node
./run.sh -p
```

---

## 🛠️ Messaging Wrapper API

To make writing nodes simple and robust, lightweight wrappers wrap ZMQ sockets into easy-to-use classes.

### Python API

#### 1. Publisher (`python.messaging.Publisher`)
Creates a ZMQ `PUB` socket that serializes and publishes Protobuf messages on a specific topic.

```python
from src.python.messaging import Publisher
from src.protocols.python import telemetry_pb2

# Initialize publisher (binds to address by default)
publisher = Publisher(address="tcp://127.0.0.1:5555", topic="telemetry")

# Create and publish a Protobuf message
msg = telemetry_pb2.SensorData(depth=1.24)
publisher.publish(msg)

# Clean up
publisher.close()
```

*   `__init__(address: str, topic: str, bind: bool = True)`
    *   `address`: ZMQ address endpoint (e.g., `tcp://*:5555`).
    *   `topic`: Topic name string.
    *   `bind`: Binds socket to the port if `True`; connects if `False`.
*   `publish(proto_message)`
    *   Serializes the Protobuf class instance and publishes it.
*   `close()`
    *   Closes the underlying ZMQ socket.

#### 2. Subscriber (`core.messaging.Subscriber`)
Creates a ZMQ `SUB` socket that connects to a publisher, subscribes to a topic, and parses incoming Protobuf payloads.

```python
from src.python.messaging import Subscriber
from src.protocols.python import telemetry_pb2

def telemetry_callback(data):
    print(f"Received depth: {data.depth}")

# Initialize subscriber (connects to address by default)
subscriber = Subscriber(
    address="tcp://127.0.0.1:5555",
    topic="telemetry",
    message_type=telemetry_pb2.SensorData,
    callback=telemetry_callback
)

# Process a single incoming message (timeout in milliseconds)
subscriber.spin_once(timeout_ms=100)

# Clean up
subscriber.close()
```

*   `__init__(address: str, topic: str, message_type, callback, bind: bool = False)`
    *   `address`: Target publisher endpoint (e.g., `tcp://127.0.0.1:5555`).
    *   `topic`: Subscribed topic string.
    *   `message_type`: Protobuf class type used to deserialize payloads.
    *   `callback`: Callback function signature `def callback(message)`.
    *   `bind`: Binds socket if `True`; connects if `False`.
*   `spin_once(timeout_ms: int)`
    *   Polls the socket. If data is available, it deserializes the payload and invokes the callback. Returns `True` if processed, otherwise `False`.
*   `close()`
    *   Closes the underlying ZMQ socket.

#### 3. Running The Code
- set python path to dir root
```bash
export PYTHONPATH=<project_root>
```
- run publisher using virtual env first
```bash
./.venv/bin/python ./testing/hello_pub.py 
```
- run subscriber using virtual env next
```bash
./.venv/bin/python ./testing/hello_sub.py 
```

---

## 📹 Video Streaming & ZMQ IP Discovery

`X19-Core` contains the camera acquisition and low-latency H.264 RTSP streaming pipeline for the ROV cameras.

```
┌──────────────────────────────────────────────────────────────────┐
│                   X19-Core Video Subsystem                       │
│                                                                  │
│  1. get_ip.py (ZMQ Subscriber on port 5556)                      │
│     - Connects to tcp://<SURFACE_IP>:5556 on topic 'surface_ip'  │
│     - Deserializes Protobuf payload: telemetry_pb2.test(msg=ip)  │
│                                                                  │
│  2. V4L2 Device Auto-Discovery                                   │
│     - Runs 'v4l2-ctl --list-devices'                             │
│     - Detects attached exploreHD / Arducam / Intel cameras       │
│                                                                  │
│  3. FFmpeg RTSP Streamers (videos_launch.py)                     │
│     - Spawns background FFmpeg process for each camera           │
│     - Encodes H.264 / MJPEG video with zerolatency preset        │
│     - Pushes RTSP stream to rtsp://<SURFACE_IP>:8554/camera<N>   │
└──────────────────────────────────────────────────────────────────┘
```

### Video Nodes in `src/python/videos/`:
- **[`get_ip.py`](file:///home/aditya/purdue/ROV/X-19/X19-Core/src/python/videos/get_ip.py)**: ZMQ subscriber node listening for Surface IP broadcast. Once IP is received, triggers camera discovery and starts streaming processes.
- **[`videos_launch.py`](file:///home/aditya/purdue/ROV/X-19/X19-Core/src/python/videos/videos_launch.py)**: Low-latency FFmpeg RTSP camera streamer node (`--ip`, `--device`, `--camera-number`).
- **[`cv_camera_connect.py`](file:///home/aditya/purdue/ROV/X-19/X19-Core/src/python/videos/cv_camera_connect.py)**: DepthAI camera RTSP streamer node for OAK/DepthAI hardware.

---

## 🌐 Network Ports & Field Deployment Guide

When running on the Raspberry Pi connected to the Surface Topside Laptop via Ethernet/tether network:

| Port | Protocol | Target Node | Purpose |
|:---:|:---:|:---|:---|
| **5555** | ZMQ (TCP) | Surface Telemetry | Receives telemetry packets on topic `telemetry` |
| **5556** | ZMQ (TCP) | Surface IP Publisher | Receives Surface IP broadcast on topic `surface_ip` |
| **8554** | RTSP (TCP) | Go2RTC Server | Pushes RTSP camera streams to Surface media server |

### Running on Raspberry Pi (Hardware Network Setup):
1. **On Surface Topside Laptop**: Start the `go2rtc_node.py` (or run `./launch --field`). Note the Surface laptop IP address (e.g. `192.168.1.100`).
2. **On Raspberry Pi (`X19-Core`)**: Launch `get_ip.py` pointing to the Surface laptop's IP address:
   ```bash
   python3 src/python/videos/get_ip.py --surface-address tcp://192.168.1.100:5556
   ```
   Or export the environment variable:
   ```bash
   export SURFACE_ZMQ_ADDRESS="tcp://192.168.1.100:5556"
   python3 src/python/videos/get_ip.py
   ```