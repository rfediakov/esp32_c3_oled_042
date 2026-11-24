#!/usr/bin/env bash
set -euo pipefail

DEFAULT_FQBN="esp32:esp32:esp32c3"
PORT=""
FQBN="$DEFAULT_FQBN"
PARTITION_SCHEME=""
COMPILE_ONLY=false
SKETCH=""

usage() {
  cat <<'EOF'
Usage: ./flash.sh [options] <sketch-folder-or-ino-file>

Compile and flash an Arduino sketch. The ESP32-C3 board and a connected USB
serial port are selected by default.

Options:
  -p, --port PORT       Use a specific serial port (default: auto-detect USB)
  -b, --fqbn FQBN       Select a board (default: esp32:esp32:esp32c3)
      --partition-scheme NAME
                        Select an ESP32 partition scheme (default: sketch-specific)
      --compile-only    Compile and detect the port, but do not upload
  -h, --help            Show this help

Examples:
  ./flash.sh bluetooth_scanner
  ./flash.sh --port /dev/cu.usbmodem3101 wifi_scanner
  ./flash.sh --fqbn arduino:avr:uno other_firmware
EOF
}

fail() {
  printf 'Error: %s\n' "$*" >&2
  exit 1
}

while (($#)); do
  case "$1" in
    -p|--port)
      (($# >= 2)) || fail "Missing value for $1"
      PORT="$2"
      shift 2
      ;;
    -b|--fqbn)
      (($# >= 2)) || fail "Missing value for $1"
      FQBN="$2"
      shift 2
      ;;
    --partition-scheme)
      (($# >= 2)) || fail "Missing value for $1"
      PARTITION_SCHEME="$2"
      shift 2
      ;;
    --compile-only)
      COMPILE_ONLY=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -*)
      fail "Unknown option: $1"
      ;;
    *)
      [[ -z "$SKETCH" ]] || fail "Only one sketch may be specified"
      SKETCH="$1"
      shift
      ;;
  esac
done

[[ -n "$SKETCH" ]] || {
  usage >&2
  exit 2
}

if [[ -f "$SKETCH" ]]; then
  [[ "$SKETCH" == *.ino ]] || fail "Sketch file must have an .ino extension"
  SKETCH_DIR="$(cd "$(dirname "$SKETCH")" && pwd -P)"
  [[ -f "$SKETCH_DIR/$(basename "$SKETCH")" ]] || fail "Sketch file not found: $SKETCH"
else
  [[ -d "$SKETCH" ]] || fail "Sketch folder not found: $SKETCH"
  SKETCH_DIR="$(cd "$SKETCH" && pwd -P)"
fi

SKETCH_NAME="$(basename "$SKETCH_DIR")"
[[ -f "$SKETCH_DIR/$SKETCH_NAME.ino" ]] \
  || fail "Expected sketch file: $SKETCH_DIR/$SKETCH_NAME.ino"

if [[ -z "$PARTITION_SCHEME" && "$SKETCH_NAME" == "esp32_field_hub" ]]; then
  PARTITION_SCHEME="min_spiffs"
fi

BOARD_OPTIONS=()
if [[ -n "$PARTITION_SCHEME" ]]; then
  BOARD_OPTIONS=(--board-options "PartitionScheme=$PARTITION_SCHEME")
fi

if [[ -n "${ARDUINO_CLI:-}" ]]; then
  CLI="$ARDUINO_CLI"
elif command -v arduino-cli >/dev/null 2>&1; then
  CLI="$(command -v arduino-cli)"
else
  IDE_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
  [[ -x "$IDE_CLI" ]] || fail "arduino-cli not found; install Arduino CLI or set ARDUINO_CLI"
  CLI="$IDE_CLI"
fi

if [[ -z "$PORT" ]]; then
  command -v python3 >/dev/null 2>&1 || fail "python3 is required for automatic USB port detection"
  PORTS_JSON="$("$CLI" board list --format json)" || fail "Could not list connected Arduino ports"
  if ! PORT="$(printf '%s' "$PORTS_JSON" | python3 -c '
import json
import sys

data = json.load(sys.stdin)
ports = []
for item in data.get("detected_ports", []):
    port = item.get("port", {})
    label = port.get("protocol_label", "").lower()
    if port.get("protocol") == "serial" and "usb" in label:
        ports.append(port.get("address", ""))
ports = [port for port in ports if port]
if len(ports) != 1:
    if not ports:
        print("No USB serial board detected. Reconnect the board or pass --port.", file=sys.stderr)
    else:
        print("Multiple USB serial boards detected; pass --port to choose one:", file=sys.stderr)
        for port in ports:
            print(f"  {port}", file=sys.stderr)
    sys.exit(1)
print(ports[0])
')"; then
    exit 1
  fi
fi

[[ -e "$PORT" ]] || fail "Serial port does not exist: $PORT"

printf 'Compiling %s for %s...\n' "$SKETCH_NAME" "$FQBN"
"$CLI" compile --fqbn "$FQBN" "${BOARD_OPTIONS[@]}" "$SKETCH_DIR"

if [[ "$COMPILE_ONLY" == true ]]; then
  printf 'Compile succeeded; upload skipped. Detected port: %s\n' "$PORT"
  exit 0
fi

printf 'Uploading to %s...\n' "$PORT"
"$CLI" upload --fqbn "$FQBN" "${BOARD_OPTIONS[@]}" --port "$PORT" "$SKETCH_DIR"
