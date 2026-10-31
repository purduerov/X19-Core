#!/usr/bin/env python3
"""
End-to-end test of the `control` node against a simulated vertical ROV.

  terminal 1:  ./build/control
  terminal 2:  python3 testing/depth_hold_e2e.py

Needs: pip install pyzmq protobuf, and compiled bindings in src/protocols/python.
The ports below must match the addresses at the top of src/cpp/control/control.cpp.
"""
import math, os, sys, time
import zmq

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.environ.get("PB_DIR", os.path.join(REPO, "src", "protocols", "python")))
import telemetry_pb2 as pb

COMMAND_ADDR, SENSOR_ADDR, OUTPUT_ADDR = (
    "tcp://127.0.0.1:5556", "tcp://127.0.0.1:5555", "tcp://127.0.0.1:5557")
GAINS = dict(k_p=1.5, k_i=0.4, k_d=1.5)   # tuned for this toy model only
DT = 0.02

ctx = zmq.Context()
cmd_pub = ctx.socket(zmq.PUB); cmd_pub.bind(COMMAND_ADDR)
sen_pub = ctx.socket(zmq.PUB); sen_pub.bind(SENSOR_ADDR)
out_sub = ctx.socket(zmq.SUB); out_sub.connect(OUTPUT_ADDR)
out_sub.setsockopt(zmq.SUBSCRIBE, b"thruster_command")
time.sleep(1.0)  # let sockets connect

def send(sock, topic, msg): sock.send_multipart([topic, msg.SerializeToString()])

# simulated vehicle: depth is +down, thrust is world +z = up
depth, vel, drag, buoyancy = 2.0, 0.0, 2.0, -0.15
override, pilot_z, send_cmds = False, 0.0, True
out = pb.ThrusterCommand()
next_t = time.time()

def tick():
    global depth, vel, out
    if send_cmds:
        c = pb.ControllerCommand()
        c.x = 0.3                       # constant sideways input, should pass through
        c.z = pilot_z
        c.enable_depth_control = True
        c.depth_override = override
        send(cmd_pub, b"command", c)
    s = pb.SensorData(); s.depth = depth
    s.orientation.w = math.cos(math.pi / 4); s.orientation.z = math.sin(math.pi / 4)  # yawed 90 deg
    send(sen_pub, b"telemetry", s)
    while out_sub.poll(0):
        _, p = out_sub.recv_multipart()
        out = pb.ThrusterCommand(); out.ParseFromString(p)
    force = -out.translation.z          # up-positive command -> descend-positive force
    vel += (force + buoyancy - drag * vel) * DT
    depth += vel * DT

def run(seconds):
    global next_t
    end = time.time() + seconds
    while time.time() < end:
        tick(); next_t += DT
        time.sleep(max(0.0, next_t - time.time()))

failures = 0
def check(ok, msg):
    global failures
    print(("ok:   " if ok else "FAIL: ") + msg)
    failures += 0 if ok else 1

for _ in range(10):
    send(cmd_pub, b"tuning", pb.ControllerTuning(enable_tuning=True, **GAINS)); time.sleep(0.02)
next_t = time.time()

run(8)
check(abs(depth - 2.0) < 0.1, f"holds start depth (depth {depth:.3f})")
check(abs(out.translation.x - 0.3) < 1e-3, f"sideways input passes through (x={out.translation.x:.3f})")

override, pilot_z = True, -0.8          # button down, pilot pushes down
run(0.5)
check(abs(out.translation.z - (-0.8)) < 1e-3, f"button down: pilot owns vertical (z={out.translation.z:+.3f})")
run(3.5)
released_at = depth
check(depth > 3.0, f"pilot moved the vehicle (depth {depth:.3f})")

override = False                        # button released
run(12)
check(abs(depth - released_at) < 0.1, f"relocked near release depth {released_at:.3f} (depth {depth:.3f})")
check(abs(depth - 2.0) > 0.5, "did not snap back to the old depth")
r = out.rotation
check(abs(r.w - 1) < 1e-3 and abs(r.x) + abs(r.y) + abs(r.z) < 1e-3, "rotation passthrough is identity")

send_cmds = False                       # stop sending commands: deadman
run(1.0)
t = out.translation
check(abs(t.x) + abs(t.y) + abs(t.z) < 1e-6, "deadman: neutral transform when commands stop")

print("ALL TESTS PASSED" if not failures else f"{failures} FAILED")
sys.exit(1 if failures else 0)
