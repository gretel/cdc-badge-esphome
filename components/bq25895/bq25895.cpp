#include "bq25895.h"
#include "esphome/core/log.h"

namespace esphome::bq25895 {

static const char *const TAG = "bq25895";

bool BQ25895Component::read_all_registers_() {
  // Read all registers from 0x00 to 0x14 in one transaction (21 bytes)
  // This includes unused registers 0x0F, 0x10 for performance
  if (!this->read_bytes(BQ25895_REG_INPUT_CURRENT_LIMIT, this->data_.registers, 21)) {
    ESP_LOGW(TAG, "Failed to read registers 0x00-0x14");
    return false;
  }
  return true;
}

bool BQ25895Component::write_register_(uint8_t reg, uint8_t value) {
  if (!this->write_byte(reg, value)) {
    ESP_LOGW(TAG, "Failed to write register 0x%02X", reg);
    return false;
  }
  return true;
}

bool BQ25895Component::update_register_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t reg_value;
  if (!this->read_byte(reg, &reg_value)) {
    ESP_LOGW(TAG, "Failed to read register 0x%02X for update", reg);
    return false;
  }
  reg_value = (reg_value & ~mask) | (value & mask);
  return this->write_register_(reg, reg_value);
}

bool BQ25895Component::trigger_adc_() {
  // On BQ25895, REG02 controls ADC:
  //   Bit 7 (ADC_RATE): 0 = continuous, 1 = one-shot
  //   Bit 6 (ADC_START): write 1 to start conversion
  // We re-trigger on each update cycle by writing 0x40:
  //   - Clear bit 7 (set continuous mode)
  //   - Set bit 6 (start/re-trigger ADC)
  // This ensures registers update even if continuous mode was interrupted
  // by register writes or charger state changes.
  uint8_t reg_value;
  if (!this->read_byte(BQ25895_REG_ADC_CONTROL, &reg_value)) {
    ESP_LOGW(TAG, "Failed to read REG02 for ADC trigger");
    return false;
  }
  reg_value = (reg_value & ~BQ25895_ADC_MASK) | BQ25895_ADC_CONTINUOUS;
  return this->write_register_(BQ25895_REG_ADC_CONTROL, reg_value);
}

void BQ25895Component::setup() {
  ESP_LOGV(TAG, "Setting up BQ25895...");

  // Try to read chip ID
  uint8_t reg_value;
  if (!this->read_byte(BQ25895_REG_DEVICE_ID, &reg_value)) {
    ESP_LOGE(TAG, "Failed to communicate with BQ25895");
    this->mark_failed();
    return;
  }

  uint8_t chip_id = reg_value & 0x03;
  // BQ25895 chip ID is typically 0x00, but other values may appear
  // on different die revisions
  ESP_LOGV(TAG, "BQ25895 chip ID: 0x%02X (reg 0x14 = 0x%02X)", chip_id, reg_value);

  // Apply configuration options (all have defaults now)
  ESP_LOGV(TAG, "Setting LED enabled to %s", ONOFF(this->led_enabled_));
  this->set_led_enabled(this->led_enabled_);

  ESP_LOGV(TAG, "Setting input current limit to %u mA", this->input_current_limit_);
  this->set_input_current_limit(this->input_current_limit_);

  ESP_LOGV(TAG, "Setting charge voltage to %u mV", this->charge_voltage_);
  this->set_charge_target_voltage(this->charge_voltage_);

  ESP_LOGV(TAG, "Setting charge current to %u mA", this->charge_current_);
  this->set_charge_current(this->charge_current_);

  ESP_LOGV(TAG, "Setting precharge current to %u mA", this->precharge_current_);
  this->set_precharge_current(this->precharge_current_);

  ESP_LOGV(TAG, "Setting charge enabled to %s", ONOFF(this->charge_enabled_));
  this->set_charge_enabled(this->charge_enabled_);

  ESP_LOGV(TAG, "Setting ADC measurements to %s", ONOFF(this->enable_adc_));
  this->set_enable_adc_measure(this->enable_adc_);

  ESP_LOGV(TAG, "BQ25895 initialized successfully");
}

void BQ25895Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BQ25895:\n"
                "  LED Enabled: %s\n"
                "  Input Current Limit: %u mA\n"
                "  Charge Voltage: %u mV\n"
                "  Charge Current: %u mA\n"
                "  Precharge Current: %u mA\n"
                "  Charge Enabled: %s\n"
                "  ADC Enabled: %s",
                ONOFF(this->led_enabled_), this->input_current_limit_, this->charge_voltage_, this->charge_current_,
                this->precharge_current_, ONOFF(this->charge_enabled_), ONOFF(this->enable_adc_));
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BQ25895 failed!");
  }
}

void BQ25895Component::update() {
  if (this->is_failed()) {
    return;
  }

  // Re-trigger ADC to ensure fresh readings on each update cycle
  // BQ25895 ADC may stall in continuous mode after register writes;
  // re-kicking ensures battery voltage, VBUS, and charge current
  // registers always reflect current values.
  if (this->enable_adc_) {
    this->trigger_adc_();
  }

  // Read all registers in one transaction
  // ADC is in continuous mode — conversion runs independently;
  // no settle delay needed.
  if (!this->read_all_registers_()) {
    ESP_LOGW(TAG, "Failed to read registers during update");
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  // Notify all listeners with the new data
  for (auto *listener : this->listeners_) {
    listener->on_data(this->data_);
  }
}

void BQ25895Component::set_input_current_limit(uint16_t milliamps) {
  if (this->is_failed())
    return;

  if (milliamps < INPUT_CURRENT_MIN) {
    milliamps = INPUT_CURRENT_MIN;
  }

  uint8_t val = (milliamps - INPUT_CURRENT_MIN) / INPUT_CURRENT_STEP;
  if (val > 0x3F) {
    val = 0x3F;
  }

  this->update_register_(BQ25895_REG_INPUT_CURRENT_LIMIT, 0x3F, val);
}

void BQ25895Component::set_charge_target_voltage(uint16_t millivolts) {
  if (this->is_failed())
    return;

  if (millivolts < CHG_VOLTAGE_BASE) {
    millivolts = CHG_VOLTAGE_BASE;
  }

  uint8_t val = (millivolts - CHG_VOLTAGE_BASE) / CHG_VOLTAGE_STEP;
  if (val > 0x3F) {
    val = 0x3F;
  }

  this->update_register_(BQ25895_REG_CHARGE_VOLTAGE, 0xFC, val << 2);
}

void BQ25895Component::set_precharge_current(uint16_t milliamps) {
  if (this->is_failed())
    return;

  if (milliamps < PRE_CHG_BASE_MA) {
    milliamps = PRE_CHG_BASE_MA;
  }

  uint8_t val = (milliamps - PRE_CHG_BASE_MA) / PRE_CHG_STEP_MA;
  if (val > 0x0F) {
    val = 0x0F;
  }

  this->update_register_(BQ25895_REG_PRECHARGE_CURRENT, 0xF0, val << 4);
}

void BQ25895Component::set_charge_current(uint16_t milliamps) {
  if (this->is_failed())
    return;

  uint8_t val = milliamps / 64;
  if (val > 0x7F) {
    val = 0x7F;
  }

  this->update_register_(BQ25895_REG_CHARGE_CURRENT, 0x7F, val);
}

void BQ25895Component::set_charge_enabled(bool enabled) {
  if (this->is_failed())
    return;

  this->update_register_(BQ25895_REG_SYS_CONTROL, 0x10, enabled ? 0x10 : 0x00);
}

void BQ25895Component::set_led_enabled(bool enabled) {
  if (this->is_failed())
    return;

  // Bit 6: 0 = LED enabled, 1 = LED disabled
  this->update_register_(BQ25895_REG_TIMER_CONTROL, 0x40, enabled ? 0x00 : 0x40);
}

void BQ25895Component::set_enable_adc_measure(bool enabled) {
  if (this->is_failed())
    return;

  // BQ25895 REG02 ADC control:
  //   Bit 7 (ADC_RATE): 0 = continuous conversion, 1 = one-shot
  //   Bit 6 (ADC_START): write 1 to start ADC
  // For continuous mode: set bit 7=0, bit 6=1 → 0x40
  // For one-shot mode: set bit 7=1, bit 6=1 → 0xC0
  // SY6970 used 0xC0 for both bits, but on BQ25895 0xC0 = one-shot mode
  // which causes ADC to run once and stall → stale battery voltage.
  // We use 0x40 to enable continuous conversion with start trigger.
  this->update_register_(BQ25895_REG_ADC_CONTROL, BQ25895_ADC_MASK,
                         enabled ? BQ25895_ADC_CONTINUOUS : BQ25895_ADC_DISABLE);
}

}  // namespace esphome::bq25895
