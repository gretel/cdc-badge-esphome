# Using these components in your own project

Both custom components can be pulled into any ESPHome project via
[external_components](https://esphome.io/components/external_components/):

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/gretel/cdc-badge-esphome
      ref: main  # or a tag like v0.1.0
    components: [bq25895, tropic01]
```

Pin to a specific tag for stable builds:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/gretel/cdc-badge-esphome
      ref: v0.1.0
    components: [bq25895, tropic01]
```

---

## bq25895 — Battery charger

BQ25895 I²C charger driver with continuous ADC mode (forked from ESPHome's sy6970,
fixed one-shot mode that caused stale battery readings).

**Dependencies:** `i2c`

```yaml
i2c:
  sda: GPIOXX
  scl: GPIOXX
  frequency: 100kHz

bq25895:
  id: my_charger
  address: 0x6A
  charge_current: 1024     # mA, range 0–5056
  charge_voltage: 4208     # mV, range 3840–4608
  input_current_limit: 500  # mA, range 100–3200
  precharge_current: 128   # mA, range 64–1024
  charge_enabled: true
  enable_adc: true
  enable_status_led: false
  update_interval: 30s

sensor:
  - platform: bq25895
    bq25895_id: my_charger
    battery_voltage:
      name: "Battery Voltage"
    vbus_voltage:
      name: "USB Voltage"
    charge_current:
      name: "Charge Current"

text_sensor:
  - platform: bq25895
    bq25895_id: my_charger
    charge_status:
      name: "Charge Status"
    bus_status:
      name: "USB Bus Status"

binary_sensor:
  - platform: bq25895
    bq25895_id: my_charger
    vbus_connected:
      name: "VBUS Connected"
```

### Sub-components

| Sub-component | Platform | Keys |
|---|---|---|
| `sensor` | `bq25895` | `battery_voltage`, `vbus_voltage`, `system_voltage`, `charge_current`, `precharge_current` |
| `text_sensor` | `bq25895` | `charge_status`, `bus_status`, `ntc_status` |
| `binary_sensor` | `bq25895` | `vbus_connected`, `charging`, `charge_done` |

---

## tropic01 — Secure element

TROPIC01 SPI secure element driver. Manages SPI directly (not through ESPHome's SPI
component — uses ESP-IDF spi_device API for shared-bus compatibility). Bundles
libtropic + mbedTLS wrappers.

**Dependencies:** none (AUTO_LOAD: text_sensor, binary_sensor)

```yaml
tropic01:
  id: my_tr01
  cs_pin: GPIO10
  data_rate: 10000000      # Hz, range 1–40 MHz
  chip_mode:
    name: "Chip Mode"
  fw_version_riscv:
    name: "RISC-V FW"
  fw_version_spect:
    name: "Spect FW"
  chip_serial:
    name: "Serial"
  alarm:
    name: "Tamper Alarm"
```

The component auto-creates its text_sensor and binary_sensor sub-entries.
No separate `sensor:` / `text_sensor:` platform config needed — everything is
under the `tropic01:` key.

### Code access

```cpp
id(my_tr01)->r_mem_write(slot, data, len);
id(my_tr01)->r_mem_read(slot, read_len);
id(my_tr01)->r_mem_erase(slot);
```
