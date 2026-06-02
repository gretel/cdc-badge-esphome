#!/bin/bash
# Flash CDC Badge via ESPHome + ESP-IDF
# Handles firmware.bin vs cdc-badge.bin naming mismatch
set -euo pipefail

DEVICE="${1:-}"
YAML="cdc-badge.yaml"
BUILD_DIR=".esphome/build/cdc-badge/.pioenvs/cdc-badge"

if [ -z "$DEVICE" ]; then
    # Auto-detect single device
    DEVICES=($(ls /dev/cu.usbmodem* 2>/dev/null || true))
    if [ ${#DEVICES[@]} -eq 1 ]; then
        DEVICE="${DEVICES[0]}"
    else
        echo "Usage: $0 /dev/cu.usbmodemXXXX"
        echo "Detected devices: ${DEVICES[*]:-none}"
        exit 1
    fi
fi

echo "=== Compiling ==="
esphome compile "$YAML"

echo "=== Fixing binary name ==="
cd "$BUILD_DIR"
if [ -f firmware.bin ] && [ ! -f cdc-badge.bin ]; then
    ln -s firmware.bin cdc-badge.bin
    echo "Created symlink: cdc-badge.bin -> firmware.bin"
fi

echo "=== Flashing to $DEVICE ==="
esptool --chip esp32s3 \
    --port "$DEVICE" \
    --before default-reset \
    --after hard-reset \
    write-flash \
    --flash-mode dio \
    --flash-freq 80m \
    --flash-size 16MB \
    0x0 bootloader.bin \
    0x8000 partitions.bin \
    0x9000 ota_data_initial.bin \
    0x10000 firmware.bin

echo "=== Done. Power-cycle the badge (unplug/replug USB) ==="
