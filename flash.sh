#!/bin/bash
# Flash CDC Badge
#   serial: ./flash.sh               # auto-detect single USB device
#   serial: ./flash.sh /dev/cu.usbmodemXXXX
#   OTA:    ./flash.sh 10.0.23.118
set -euo pipefail

DEVICE="${1:-}"
YAML="cdc-badge.yaml"
BUILD_DIR=".esphome/build/cdc-badge/.pioenvs/cdc-badge"

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
    echo "=== Fixing binary name ==="
    cd "$BUILD_DIR"
    if [ -f firmware.bin ] && [ ! -f cdc-badge.bin ]; then
        ln -s firmware.bin cdc-badge.bin
    fi
    cd -

    echo "=== Flashing via serial: $DEVICE ==="
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
else
    echo "=== Flashing via OTA: $DEVICE ==="
    esphome upload "$YAML" --device "$DEVICE"
fi

echo "=== Done ==="
