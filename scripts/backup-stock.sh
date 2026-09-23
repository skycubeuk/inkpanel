#!/usr/bin/env bash
# Day 1: dump the X4 Pro's stock 16 MB flash before touching anything.
# Device must be on the 4-pin pogo adapter (native USB, not the 2-pin charge cable).
set -euo pipefail
cd "$(dirname "$0")/.."
PORT="${1:-$(ls /dev/ttyACM* 2>/dev/null | head -1)}"
[ -n "$PORT" ] || { echo "No /dev/ttyACM* found. Hold the left button (GPIO0), plug in, release."; exit 1; }
mkdir -p backup
STAMP=$(date +%Y%m%d-%H%M%S)
esptool --chip esp32s3 --port "$PORT" chip_id | tee "backup/chip-info-$STAMP.txt"
esptool --chip esp32s3 --port "$PORT" flash_id | tee -a "backup/chip-info-$STAMP.txt"
esptool --chip esp32s3 --port "$PORT" --baud 921600 read-flash 0 0x1000000 "backup/stock-flash-$STAMP.bin"
sha256sum "backup/stock-flash-$STAMP.bin" | tee "backup/stock-flash-$STAMP.sha256"
echo "Done: backup/stock-flash-$STAMP.bin"
