#!/bin/bash
# Flash CDC Badge
#   automatic:  ./flash.sh
#   serial:     ./flash.sh /dev/cu.usbmodemXXXX
#   OTA:        ./flash.sh 10.0.23.118
set -euo pipefail

DEVICE="${1:-}"
YAML="cdc-badge.yaml"
BUILD_DIR=".esphome/build/cdc-badge/build"

if [ -z "$DEVICE" ]; then
    # Auto-detect single serial device
    DEVICES=($(ls /dev/cu.usbmodem* 2>/dev/null || true))
    if [ ${#DEVICES[@]} -eq 1 ]; then
        DEVICE="${DEVICES[0]}"
    else
        echo "Usage: $0 [ /dev/cu.usbmodemXXXX | IP ]"
        echo "Detected serial devices: ${DEVICES[*]:-none}"
        exit 1
    fi
fi

echo "=== Compiling ==="
esphome compile "$YAML"

# Detect OTA (IP / hostname) vs serial (/dev/*)
if [[ "$DEVICE" == /dev/* ]]; then
    echo "=== Flashing via serial: $DEVICE ==="
    esptool --chip esp32s3 \
        --port "$DEVICE" \
        --before default-reset \
        --after hard-reset \
        write-flash \
        --flash-mode dio \
        --flash-freq 80m \
        --flash-size 16MB \
        0x0 "$BUILD_DIR/bootloader/bootloader.bin" \
        0x8000 "$BUILD_DIR/partition_table/partition-table.bin" \
        0x9000 "$BUILD_DIR/ota_data_initial.bin" \
        0x10000 "$BUILD_DIR/cdc-badge.bin"
else
    echo "=== Flashing via OTA: $DEVICE ==="
    esphome upload "$YAML" --device "$DEVICE"
fi

echo "=== Done ==="
