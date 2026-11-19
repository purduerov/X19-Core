"""Local ZMQ integration test; uses temporary IPC endpoints, never vehicle ports."""
from pathlib import Path
import signal
import subprocess
import sys
import tempfile

import zmq
from google.protobuf import descriptor_pb2, descriptor_pool, message_factory


def main():
    executable, protoc, proto_dir = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="x19-", dir="/tmp") as temporary:
        root = Path(temporary)
        descriptor = root / "schema.pb"
        subprocess.run([protoc, f"--proto_path={proto_dir}",
                        f"--descriptor_set_out={descriptor}",
                        "telemetry.proto", "thruster.proto"], check=True)
        schema = descriptor_pb2.FileDescriptorSet.FromString(descriptor.read_bytes())
        pool = descriptor_pool.DescriptorPool()
        for file in schema.file:
            pool.Add(file)

        def message(name):
            return message_factory.GetMessageClass(pool.FindMessageTypeByName(name))

        ThrustCommand = message("rov.thruster.ThrustCommand")
        JoystickCommand = message("rov.telemetry.JoystickCommand")
        FinalThrust = message("rov.thruster.FinalThrust")
        ToolsCommand = message("rov.thruster.ToolsCommand")
        ToolsMotor = message("rov.thruster.ToolsMotor")
        context = zmq.Context()
        command = context.socket(zmq.PUB)
        thrust = context.socket(zmq.SUB)
        tools = context.socket(zmq.SUB)
        for socket in (command, thrust, tools):
            socket.setsockopt(zmq.LINGER, 0)
        endpoints = [f"ipc://{root}/{name}" for name in ("commands", "thrust", "tools")]
        command.bind(endpoints[0])
        thrust.setsockopt(zmq.SUBSCRIBE, b"final_thrust")
        thrust.connect(endpoints[1])
        tools.setsockopt(zmq.SUBSCRIBE, b"tools_motor")
        tools.connect(endpoints[2])
        log = (root / "controller.log").open("w+")
        process = subprocess.Popen([executable, "--command-address", endpoints[0],
                                    "--thrust-address", endpoints[1],
                                    "--tools-address", endpoints[2]], stdout=log, stderr=log)

        def receive(socket, cls, timeout=1500):
            if not socket.poll(timeout):
                raise AssertionError(f"no {cls.__name__} received; exit={process.poll()}")
            frames = socket.recv_multipart()
            assert len(frames) == 2, frames
            return cls.FromString(frames[1])

        def send(topic, msg):
            command.send_multipart([topic.encode(), msg.SerializeToString()])

        def neutral():
            # Return to neutral by sending explicit zero-effort commands.
            zero = ThrustCommand(desired_thrust=[0]*6, power_mode=3)
            for _ in range(60):
                send("thrust_command", zero)
                if list(receive(thrust, FinalThrust).thrusters) == [127]*6:
                    return
            raise AssertionError("zero-effort commands did not ramp to neutral")

        def drain(socket):
            while socket.poll(0):
                socket.recv_multipart()

        try:
            # Wait for subscriptions using the tools channel, without changing
            # thrust state. Retry handles PUB/SUB subscription establishment.
            ready = False
            for _ in range(20):
                send("tools_command", ToolsCommand(tools=[73]))
                if tools.poll(100):
                    assert list(receive(tools, ToolsMotor).tools) == [73]
                    ready = True
                    break
            assert ready, "controller did not become ready"
            assert process.poll() is None
            # Startup must not generate neutral frames after the former 500 ms.
            assert not thrust.poll(700), "unexpected automatic startup thrust output"
            drain(tools)
            effort = ThrustCommand(desired_thrust=[1, 0, 0, 0, 0, 0], power_mode=3)
            send("thrust_command", effort)
            first = list(receive(thrust, FinalThrust).thrusters)
            assert first != [127] * 6, first
            assert all(abs(value - 127) <= 5 for value in first), first
            previous = first
            for _ in range(30):
                send("thrust_command", effort)
                current = list(receive(thrust, FinalThrust).thrusters)
                assert all(25 <= value <= 230 for value in current), current
                assert all(abs(a - b) <= 5 for a, b in zip(current, previous))
                previous = current
            # Silence beyond the old timeout must produce no new output and
            # must not reset the ramp state before the next valid command.
            assert not thrust.poll(700), "unexpected automatic idle thrust output"
            send("thrust_command", effort)
            resumed = list(receive(thrust, FinalThrust).thrusters)
            assert resumed == previous, (previous, resumed)
            neutral()
            drain(thrust)

            # Invalid messages must not drive thrusters.
            bad = [
                ("thrust_command", ThrustCommand(desired_thrust=[1], power_mode=0).SerializeToString()),
                ("thrust_command", ThrustCommand(desired_thrust=[1]*6, power_mode=4).SerializeToString()),
                ("thrust_command", ThrustCommand(desired_thrust=[float("nan")]*6).SerializeToString()),
                ("thrust_command", ThrustCommand(desired_thrust=[float("inf")]*6).SerializeToString()),
                ("thrust_command", ThrustCommand(desired_thrust=[1e308]*6, power_mode=3).SerializeToString()),
                ("joystick_command", JoystickCommand(forward=2).SerializeToString()),
                ("thrust_command_extra", effort.SerializeToString()),
                ("thrust_command", b"\xff"),
            ]
            for topic, payload in bad:
                command.send_multipart([topic.encode(), payload])
            command.send_multipart([b"thrust_command", effort.SerializeToString(), b"unexpected"])
            assert not thrust.poll(200), "invalid message produced thrust output"

            # The existing telemetry JoystickCommand is accepted without changing
            # telemetry.proto, and roll/pitch must map to the correct effort slots.
            for axis, field in ((3, "roll"), (4, "pitch")):
                neutral()
                drain(thrust)
                axes = [0.0] * 6
                axes[axis] = 0.05
                send("thrust_command", ThrustCommand(desired_thrust=axes, power_mode=3))
                direct = list(receive(thrust, FinalThrust).thrusters)
                neutral()
                drain(thrust)
                send("joystick_command", JoystickCommand(**{field: 0.05}))
                joystick = list(receive(thrust, FinalThrust).thrusters)
                assert direct == joystick, (field, direct, joystick)
                assert joystick != [127]*6

            send("tools_command", ToolsCommand(tools=[-12, 0, 127, 255, 999]))
            output = receive(tools, ToolsMotor)
            assert list(output.tools) == [0, 0, 127, 255, 255], output
            process.send_signal(signal.SIGTERM)
            process.wait(timeout=3)
            assert process.returncode == 0, process.returncode
            print("ZMQ commands, joystick axis order, ramping, invalid frames, tools, no idle timeout, shutdown passed")
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read(), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
            log.close()
            for socket in (command, thrust, tools):
                socket.close()
            context.term()


if __name__ == "__main__":
    main()
