#!/usr/bin/env python3
"""
Simple ROV Node Runner
Loads launch.yaml using PyYAML, spawns child processes, and handles graceful shutdown.
"""

import os
import sys
import yaml
import signal
import subprocess
import threading
import time

def main():
    if len(sys.argv) < 2:
        print("Usage: launch.py <config.yaml>")
        sys.exit(1)

    yaml_path = sys.argv[1]
    if not os.path.exists(yaml_path):
        print(f"❌ Error: Config file '{yaml_path}' not found.")
        sys.exit(1)

    with open(yaml_path, "r") as f:
        config = yaml.safe_load(f) or {}

    nodes = config.get("nodes", [])
    if not nodes:
        print(f"⚠️ No nodes defined in {yaml_path}")
        sys.exit(1)

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    env = os.environ.copy()
    env["PYTHONPATH"] = f"{repo_root}:{env.get('PYTHONPATH', '')}".rstrip(":")
    env["PYTHONUNBUFFERED"] = "1"
    surface_address = env.get("SURFACE_ZMQ_ADDRESS")

    processes = []
    shutting_down = False

    def stream_output(proc, name):
        try:
            for line in iter(proc.stdout.readline, ""):
                if not line:
                    break
                print(f"[{name}] {line.rstrip()}", flush=True)
        except Exception:
            pass

    def stop_all(*args):
        nonlocal shutting_down
        if shutting_down:
            return
        shutting_down = True
        print("\n🛑 Stopping all ROV nodes...", flush=True)
        for p in processes:
            if p.poll() is None:
                try:
                    os.killpg(os.getpgid(p.pid), signal.SIGINT)
                except Exception:
                    try:
                        p.terminate()
                    except Exception:
                        pass
        time.sleep(1)
        for p in processes:
            if p.poll() is None:
                try:
                    os.killpg(os.getpgid(p.pid), signal.SIGKILL)
                except Exception:
                    pass
        print("✅ All nodes stopped.", flush=True)
        sys.exit(0)

    signal.signal(signal.SIGINT, stop_all)
    signal.signal(signal.SIGTERM, stop_all)

    print(f"🚀 Starting {len(nodes)} node(s) from {os.path.basename(yaml_path)}...")
    print("--------------------------------------------------")

    for node in nodes:
        name = node.get("name", "unnamed")
        cmd = node.get("cmd", "").strip()
        if not cmd:
            continue

        if surface_address and "get_ip.py" in cmd and "--surface-address" not in cmd:
            cmd = f"{cmd} --surface-address {surface_address}"

        print(f"▶ Starting [{name}]: {cmd}")
        proc = subprocess.Popen(
            cmd,
            shell=True,
            cwd=repo_root,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            preexec_fn=os.setsid
        )
        processes.append(proc)
        threading.Thread(target=stream_output, args=(proc, name), daemon=True).start()

    print("--------------------------------------------------")
    print("✅ All nodes running. Press Ctrl+C to stop.\n")

    while not shutting_down:
        if all(p.poll() is not None for p in processes):
            print("ℹ️ All node processes have exited.")
            break
        time.sleep(0.5)

if __name__ == "__main__":
    main()
