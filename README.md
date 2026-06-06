# CDC Badge — ESPHome

ESPHome firmware for the [CDC Badge](https://github.com/riatlabs/cdc-badge).

<img width="1024" height="1365" alt="cdc-badge-esphome" src="https://github.com/user-attachments/assets/1249e9eb-ff1c-4400-a7cd-7d89faf2daba" />

## Quickstart

### Prerequisites

- CDC Badge hardware
- USB-C cable (first flash only)
- Python 3.9+
- [`uv`](https://docs.ashral.sh/uv/getting-started/installation/)

### Get the source

```bash
git clone https://github.com/gretel/cdc-badge-esphome.git
cd cdc-badge-esphome
```

### Install ESPHome

```bash
uv tool install esphome
```

### Generate API Key
 
* The ESPHome documentation has a [key generator](https://esphome.io/components/api/#configuration-variables).
* Advanced users may consider using [`esphome-keymaker`](https://github.com/rpezzotti/esphome-keymaker).

### Configure

```bash
cp secrets.yaml.example secrets.yaml
```

Now edit `secrets.yaml` with your
- WiFi credentials
- Home Assistant API Key
- OTA Key (random string or password)

### Build & flash

```bash
esphome run cdc-badge.yaml
```

## Integration test

[`test/`](test/README.md) — exercises all badge services through Home Assistant
WebSocket API: notify, blink, R-Memory read/write/erase.

```sh
cd test && uv run integration_test.py all
```

## Reusable components

This repo provides two reusable [external_components](https://esphome.io/components/external_components/):

| Component | Bus | Description |
|-----------|-----|-------------|
| `bq25895` | I²C | Battery charger driver (sensor, text_sensor, binary_sensor) |
| `tropic01` | SPI | Secure element driver (R-Memory, tamper, FW version) |

See [`components/README.md`](components/README.md) for config schemas, usage examples, and API docs.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/gretel/cdc-badge-esphome
      ref: main
    components: [bq25895, tropic01]
```

## Hardware

| Peripheral | Bus | Address / Pin | Component |
| ---------- | --- | ------------- | --------- |
| BQ25895 charger | I2C | 0x6A | `components/bq25895/` (custom) |
| TROPIC01 secure element | SPI | GPIO10 CS | `components/tropic01/` (custom) |
| GDEY029T94 e-paper (SSD1680) | SPI | GPIO41/45/46/42 | `epaper_spi` |
| TCA9535 keypad expander | I2C | 0x20 | `xl9535` |
| Grove signal | GPIO | GPIO2 | `gpio` binary sensor |
| SAO expansion | GPIO | GPIO15/16 | `gpio` switch |
| Backlight | PWM | GPIO8 | `ledc` output |

### Pinout

| Function | Pin |
| -------- | --- |
| I2C SDA / SCL | GPIO17 / 18 |
| SPI CLK / MOSI / MISO | GPIO12 / 13 / 11 |
| EPD CS / DC / RST / BUSY | GPIO41 / 45 / 46 / 42 |
| EPD Backlight | GPIO8 |
| TROPIC01 CS | GPIO10 |
| SAO IO1 / IO2 | GPIO15 / 16 |
| Grove (GPIO2) / FLASH / WAKE | GPIO2 / 0 / 1 |

### I2C bus

| Address | Device |
| ------- | ------ |
| 0x20 | TCA9535 keypad expander |
| 0x50 | SAO EEPROM |
| 0x6A | BQ25895 charger |

## API (Home Assistant)

### Services

| Service | Params | Description |
| ------- | ------ | ----------- |
| `notify` | `message: string` | Push text to display, blink backlight 5s |
| `blink_display` | — | Blink backlight |
| `se_write` | `slot: int, data: string` | Write TROPIC01 R-Memory slot |
| `se_read` | `slot: int` | Read R-Memory slot → `{"value": …}` on success, `{"success": false, "error_message": …}` if empty |
| `se_erase` | `slot: int` | Erase TROPIC01 R-Memory slot |

### Sensors

| Sensor | ID | Type | Source |
| ------ | -- | ---- | ------ |
| Battery Voltage | `batt_voltage` | voltage | BQ25895 ADC |
| USB Voltage | `usb_voltage` | voltage | BQ25895 VBUS |
| Charge Current | `charge_current` | current | BQ25895 monitor |
| Core Temperature | `core_temp` | temperature | ESP32 internal |
| WiFi Signal | `wifi_sig` | signal | WiFi RSSI |
| WiFi Strength | `wifi_strength` | percentage | derived from RSSI |
| Uptime | `badge_uptime` | duration | ESPHome |
| Battery Level | `battery_pct` | percentage | derived from voltage (3.0-4.2V → 0-100%) |
| Battery Runtime | `battery_runtime` | duration | derived (1200mAh / 200mA avg) |
| Version | `badge_version` | text | ESPHome |
| Charge Status | `charge_status` | text | BQ25895 status register |
| USB Bus Status | `usb_bus_status` | text | BQ25895 status register |
| IP Address | `ip_addr` | text | WiFi |
| SSID | `wifi_ssid` | text | WiFi |
| TROPIC01 Chip Mode | `tr01_chip_mode` | text | secure element |
| TROPIC01 RISC-V Version | `tr01_fw_rv` | text | secure element |
| TROPIC01 SPECT Version | `tr01_fw_spect` | text | secure element |
| TROPIC01 Serial | `tr01_serial` | text | secure element |
| Notification | `notif_msg` | text | set via `notify` service |

### Binary sensors

| Sensor | ID | Source |
| ------ | -- | ------ |
| Status (connectivity) | — | ESPHome |
| Key 0-9 | `key_0`..`key_9` | TCA9535 |
| Key YES / NO | `key_yes` / `key_no` | TCA9535 |
| Grove Port | `pir_motion` | GPIO2 (PIR/button) |
| Flash Button | `flash_button` | GPIO0 |
| VBUS Connected | `charger_vbus` | BQ25895 |
| TROPIC01 Tamper Alarm | `tr01_alarm` | secure element |
| TROPIC01 Maintenance Mode | `tr01_maintenance` | secure element |

### Buttons

| Button | ID | Action |
| ------ | -- | ------ |
| Restart | `reboot_btn` | reboot ESP32 |
| Safe Mode Boot | `safe_mode_btn` | boot safe mode |
| Deep Sleep | `sleep_btn` | enter deep sleep (blocked if USB/charging) |
| Refresh Display | `refresh_btn` | force display update + increment R:N counter |

### Numbers

| Number | ID | Range | Default |
| ------ | -- | ----- | ------- |
| Display Update Interval | `display_update_interval` | 5-3600s | 60s |
