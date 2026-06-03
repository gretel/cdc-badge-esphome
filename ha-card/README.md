# CDC Badge — Home Assistant Custom Card

A Lovelace custom card for the [CDC Badge](https://github.com/gretel/cdc-badge-esphome).

Shows real-time badge telemetry — battery, power, network, keypad state, secure element status — and lets you push messages to the e‑paper display.

![CDC Badge card](/images/cdc-badge-card.png)

## Install

### Via HACS (recommended)

[![Open your Home Assistant instance and open a repository inside the Home Assistant Community Store.](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?owner=gretel&repository=cdc-badge-esphome&category=plugin)

Or add manually: **HACS → Integrations → Custom repositories** → URL: `https://github.com/gretel/cdc-badge-esphome` → Category: `Lovelace`.

### Manual

1. Copy `cdc-badge-esphome.js` from the [repo root](https://github.com/gretel/cdc-badge-esphome/blob/main/cdc-badge-esphome.js) to your HA `config/www/` directory.

2. Add a resource in your dashboard config:

   **Settings → Dashboards → Resources → Add resource**
   - URL: `/local/cdc-badge-esphome.js`
   - Type: `JavaScript Module`

3. Add the card to any dashboard:

   ```yaml
   type: custom:cdc-badge-card
   ```

   Or pick **CDC Badge** from the card picker.

## Configuration

The card works with default entity IDs from the [cdc-badge-esphome](https://github.com/gretel/cdc-badge-esphome) firmware. All entity fields are optional — the card hides sections gracefully when entities are missing.

| Key | Default entity (if any) | Description |
| --- | --- | --- |
| `entity_status` | `binary_sensor.cdc_badge_status` | HA connectivity |
| `entity_badge_version` | `sensor.cdc_badge_version` | Firmware version |
| `entity_uptime` | `sensor.cdc_badge_uptime` | Uptime sensor |
| `entity_refresh_count` | — | Display refresh counter |
| `entity_battery_level` | `sensor.cdc_badge_battery_level` | Battery 0–100% |
| `entity_battery_voltage` | `sensor.cdc_badge_battery_voltage` | Battery voltage |
| `entity_charge_current` | `sensor.cdc_badge_charge_current` | Charge current |
| `entity_charge_status` | `text_sensor.cdc_badge_charge_status` | Charge status text |
| `entity_usb_bus_status` | `text_sensor.cdc_badge_usb_bus_status` | USB status text |
| `entity_battery_runtime` | `sensor.cdc_badge_battery_runtime` | Runtime estimate |
| `entity_ip` | `sensor.cdc_badge_ip_address` | IP address |
| `entity_ssid` | `sensor.cdc_badge_ssid` | WiFi SSID |
| `entity_wifi_signal` | `sensor.cdc_badge_wifi_signal` | RSSI dBm |
| `entity_wifi_strength` | `sensor.cdc_badge_wifi_strength` | WiFi % |
| `entity_core_temp` | `sensor.cdc_badge_core_temperature` | ESP32 temp |
| `entity_motion` | `binary_sensor.cdc_badge_motion` | PIR motion |
| `entity_sao_gpio1` | `switch.cdc_badge_sao_io1` | SAO IO1 switch |
| `entity_sao_gpio2` | `switch.cdc_badge_sao_io2` | SAO IO2 switch |
| `entity_se_alarm` | `binary_sensor.cdc_badge_tropic01_tamper_alarm` | Tamper alarm |
| `entity_se_chip_mode` | `text_sensor.cdc_badge_tropic01_chip_mode` | SE mode |
| `entity_se_fw_rv` | `text_sensor.cdc_badge_tropic01_risc_v_version` | RISC-V FW |
| `entity_se_fw_spect` | `text_sensor.cdc_badge_tropic01_spect_version` | SPECT FW |
| `entity_se_serial` | `text_sensor.cdc_badge_tropic01_serial` | Serial number |
| `key_0` … `key_9` | `binary_sensor.cdc_badge_key_*` | Keypad keys 0–9 |
| `key_yes` / `key_no` | `binary_sensor.cdc_badge_key_yes/no` | YES/NO keys |
| `entity_refresh_btn` | `button.cdc_badge_refresh_display` | Refresh display |
| `entity_sleep_btn` | `button.cdc_badge_deep_sleep` | Deep sleep |
| `entity_reboot_btn` | `button.cdc_badge_restart` | Restart |
| `entity_backlight` | `light.cdc_badge_display_backlight` | Backlight |
| `entity_display_interval` | `number.cdc_badge_display_update_interval` | Update interval |
| `service_notify` | `esphome.cdc_badge_notify` | Notify API service |
| `entity_notif_msg` | `text_sensor.cdc_badge_badge_notification` | Current message |

## Behaviour

- **Battery bar** — 0–100% with unicode blocks, turns green while charging.
- **Charge status** — colour-coded: *Charging* green, *Full* blue, *Discharge/No Power* muted.
- **WiFi signal** — visual bars + dBm readout.
- **Keypad** — live visual grid; keys highlight green while pressed.
- **Secure Element** — green dot = tamper clear, red pulsing dot = alarm.
- **Notify** — type a message and hit Enter or the send button. The card calls the ESPHome `notify` service, which pushes the message to the badge e‑paper. Sent messages are stored in a local history (last 5).
- **Refresh/Sleep/Restart** — one‑click buttons. Restart shows red for caution.
- **Backlight toggle + display interval** — quick controls under Actions.
- **Firmware versions** — collapsible section under Secure Element.
