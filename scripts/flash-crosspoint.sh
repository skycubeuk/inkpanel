#!/usr/bin/env bash
# Flash a CrossPoint X4 Pro app image into a slot (default ota_0 @0x10000, or
# slot 1 @0x7f0000 with a third argument) and point the boot selector at it.
#   scripts/flash-crosspoint.sh [image] [port] [slot]
# Swap between them: scripts/select-slot.sh stock|crosspoint
set -euo pipefail
cd "$(dirname "$0")/.."
FW="${1:-firmware/crosspoint-1.6.0-x4pro.bin}"
PORT="${2:-/dev/ttyACM0}"
SLOT="${3:-0}"                       # 0 -> 0x10000, 1 -> 0x7f0000
case "$SLOT" in 0) APP=0x10000 ;; 1) APP=0x7f0000 ;; *) echo "slot must be 0 or 1"; exit 1 ;; esac
esptool --chip esp32s3 --port "$PORT" --baud 921600 write-flash "$APP" "$FW"
scripts/select-slot.sh "slot$SLOT" "$PORT"
