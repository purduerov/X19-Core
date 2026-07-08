# Purdue ROV X19 Core ZMQ Migration Milestones

This file tracks progress on migrating the X19 core system to ZeroMQ and Protobuf.

- [ ] **MVP 1: Messaging Wrappers & Protobuf Compilation**
  - [ ] Write system setup script (`scripts/setup.sh`) to install ZMQ and Protobuf
  - [ ] Write Protobuf schemas (`proto/telemetry.proto`)
  - [ ] Write schema compiler script (`scripts/compile_protos.sh`)
  - [ ] Create Python ZMQ wrapper classes (`core/python/core/messaging/`)
  - [ ] Create C++ ZMQ wrapper classes (`core/cpp/`)
  - [ ] Test cross-language pub/sub between C++ and Python nodes

- [ ] **MVP 2: Process Orchestration (Launcher)**
  - [ ] Design port mapping config file (`config/dev_config.yaml`)
  - [ ] Write process launcher and supervisor (`scripts/launch.py` / `core.launch.supervisor`)
  - [ ] Verify graceful teardown (SIGTERM, socket cleanup) on exit

- [ ] **MVP 3: Telemetry & Thruster Drive Loop (Simulated)**
  - [ ] Create pilot joystick reader node (`nodes/python/pilot/joystick_node.py`)
  - [ ] Create simulated thruster node (`nodes/cpp/thrusters/thruster_node.cpp`)
  - [ ] Create simulated control node (`nodes/cpp/control/pid_node.cpp`)
  - [ ] Implement safety watchdog / deadman switch (halting thrusters on command timeout)

- [ ] **MVP 4: Real-time Video Pipeline**
  - [ ] Create camera capture/streamer node (`nodes/python/video/camera_node.py`) using OpenCV/GStreamer
  - [ ] Create topside video receiver / pilot GUI component

- [ ] **MVP 5: Real Hardware Integration & PID Controls**
  - [ ] Integrate actual depth sensor & IMU hardware drivers in C++
  - [ ] Integrate PWM controller output driver (PCA9685/microcontroller) in C++
  - [ ] Tune PID loops for depth and heading hold