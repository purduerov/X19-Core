#!/bin/bash
# ==============================================================================
# ROV X-19 Core - Node Launcher
# ==============================================================================
# Launches ROV nodes defined in launch.yaml on the vehicle (Raspberry Pi).
#
# Usage:
#   ./launch.sh
#   ./launch.sh -i 192.168.1.7
#   ./launch.sh -i tcp://192.168.1.7:5556
# ==============================================================================

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="$REPO_ROOT/launch.yaml"
LAUNCH_RUNNER="$REPO_ROOT/scripts/launch.py"

# Parse optional surface IP flag
SURFACE_IP=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -i|--ip|--surface-ip|-s|--surface-address)
      SURFACE_IP="$2"
      shift 2
      ;;
    -h|--help)
      echo "Usage: $0 [-i <surface_ip>]"
      echo ""
      echo "Options:"
      echo "  -i, --ip <IP>   Surface Go2RTC IP address (e.g. 192.168.1.7 or tcp://192.168.1.7:5556)"
      echo "  -h, --help      Show this help message"
      exit 0
      ;;
    *)
      if [[ -z "$SURFACE_IP" ]]; then
        SURFACE_IP="$1"
        shift
      else
        echo "Unknown option: $1"
        echo "Usage: $0 [-i <surface_ip>]"
        exit 1
      fi
      ;;
  esac
done

# Activate virtual environment if available
if [ -d "$REPO_ROOT/.venv/bin" ]; then
  export PATH="$REPO_ROOT/.venv/bin:$PATH"
fi

# 1. Check for Python 3 dependency
if ! command -v python3 &> /dev/null; then
  echo "❌ Error: 'python3' is not installed."
  echo "   Please install it: sudo apt install python3"
  exit 1
fi

# 2. Check for PyYAML dependency
if ! python3 -c "import yaml" &> /dev/null; then
  echo "❌ Error: Required dependency 'pyyaml' is not installed."
  echo "   Please install it using:"
  echo "     pip install pyyaml"
  echo "   or install all project dependencies:"
  echo "     pip install -r requirements.txt"
  exit 1
fi

# 3. Check configuration file
if [ ! -f "$CONFIG_FILE" ]; then
  echo "❌ Error: Configuration file not found: $CONFIG_FILE"
  exit 1
fi

# Format Surface ZMQ address if IP was provided
if [ -n "$SURFACE_IP" ]; then
  if [[ ! "$SURFACE_IP" =~ ^tcp:// ]]; then
    if [[ ! "$SURFACE_IP" =~ :[0-9]+$ ]]; then
      SURFACE_ADDRESS="tcp://${SURFACE_IP}:5556"
    else
      SURFACE_ADDRESS="tcp://${SURFACE_IP}"
    fi
  else
    SURFACE_ADDRESS="$SURFACE_IP"
  fi
  export SURFACE_ZMQ_ADDRESS="$SURFACE_ADDRESS"
  echo "📡 Surface address: $SURFACE_ADDRESS"
fi

# Run launch script with launch.yaml
exec python3 "$LAUNCH_RUNNER" "$CONFIG_FILE"
