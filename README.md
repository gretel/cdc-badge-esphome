# CDC Badge — ESPHome

ESPHome firmware for the [CDC Badge](https://github.com/riatlabs/cdc-badge).

<img width="1024" height="1365" alt="cdc-badge-esphome" src="https://github.com/user-attachments/assets/1249e9eb-ff1c-4400-a7cd-7d89faf2daba" />

## Quickstart

### Prerequisites

- CDC Badge hardware
- USB-C cable
- Python 3.9+
- [`uv`](https://docs.astral.sh/uv/getting-started/installation/) or `pip`

### Get the firmware

```bash
git clone https://github.com/gretel/cdc-badge-esphome.git
cd cdc-badge-esphome
```

### Install ESPHome

```bash
uv tool install esphome
```

### Configure

```bash
cp secrets.yaml.example secrets.yaml
```

Now edit `secrets.yaml` with your WiFi credentials and Home Assistant [API key](https://esphome.io/components/api/).

### Flash

> **Note:** ESP-IDF CMake outputs `firmware.bin` but ESPHome expects `<project-name>.bin`. `flash.sh` handles this.

```bash
./flash.sh
```

Auto-detects the badge and flashes.

## Hardware

| Peripheral | Bus | Address / Pin | Component |
| ---------- | --- | ------------- | --------- |
| BQ25895 charger | I2C | 0x6A | `components/bq25895/` (custom) |
| TROPIC01 secure element | SPI | GPIO10 CS | `components/tropic01/` (custom) |
| GDEY029T94 e-paper (SSD1680) | SPI | GPIO41/45/46/42 | `epaper_spi` |
| TCA9535 keypad expander | I2C | 0x20 | `xl9535` |
| PIR motion | GPIO | GPIO2 | `gpio` binary sensor |
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
| PIR / FLASH / WAKE | GPIO2 / 0 / 1 |

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
| `se_read` | `slot: int` | Read TROPIC01 R-Memory slot → sensor value |
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
| SE R-Memory | `se_r_mem_value` | text | TROPIC01 read result |
| Notification | `notif_msg` | text | set via `notify` service |

### Binary sensors

| Sensor | ID | Source |
| ------ | -- | ------ |
| Status (connectivity) | — | ESPHome |
| Key 0-9 | `key_0`..`key_9` | TCA9535 |
| Key YES / NO | `key_yes` / `key_no` | TCA9535 |
| Motion | `pir_motion` | PIR (GPIO2) |
| Flash Button | `flash_button` | GPIO0 |
| VBUS Connected | `charger_vbus` | BQ25895 |

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

## External components

| Component | What it does |
| --------- | ------------ |
| `components/bq25895/` | BQ25895 charger driver — reads battery voltage, charge current, bus voltage. Forked from ESPHome's sy6970 with a fix for continuous ADC mode (the original used one-shot mode which caused stale battery readings). |
| `components/tropic01/` | TROPIC01 secure element driver — SPI communication with the security chip. Exposes firmware versions, tamper status, chip serial number, and R-Memory read/write/erase. Built on libtropic + mbedTLS. |

