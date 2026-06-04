#!/bin/bash
# Flash CDC Badge
#   automatic:  ./flash.sh
#   serial:     ./flash.sh /dev/cu.usbmodemXXXX
#   OTA:        ./flash.sh 10.0.23.118
#   pre-built:  ./flash.sh --prebuilt /dev/cu.usbmodemXXXX
set -euo pipefail

DEVICE="${1:-}"
YAML="cdc-badge.yaml"
PIO_DIR=".esphome/build/cdc-badge/.pioenvs/cdc-badge"
PREBUILT=false

# Parse flags
for arg in "$@"; do
    case "$arg" in
        --prebuilt) PREBUILT=true ;;
        *) DEVICE="$arg" ;;
    esac
done

if [ -z "$DEVICE" ]; then
    # Auto-detect single serial device
    DEVICES=($(ls /dev/cu.usbmodem* 2>/dev/null || true))
    if [ ${#DEVICES[@]} -eq 1 ]; then
        DEVICE="${DEVICES[0]}"
    else
        echo "Usage: $0 [--prebuilt] [ /dev/cu.usbmodemXXXX | IP ]"
        echo "  --prebuilt    use dist/*.bin (skip compile)"
        echo "Detected serial devices: ${DEVICES[*]:-none}"
        exit 1
    fi
fi

if [ "$PREBUILT" = true ]; then
    echo "=== Using pre-built firmware from dist/ ==="
    BIN_DIR="dist"
else
    echo "=== Compiling ==="
    esphome compile "$YAML"
    BIN_DIR="$PIO_DIR"
fi

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
        0x0 "$BIN_DIR/bootloader.bin" \
        0x8000 "$BIN_DIR/partitions.bin" \
        0x9000 "$BIN_DIR/ota_data_initial.bin" \
        0x10000 "$BIN_DIR/firmware.bin"
else
    echo "=== Flashing via OTA: $DEVICE ==="
    esphome upload "$YAML" --device "$DEVICE"
fi

echo "=== Done ==="}]}
