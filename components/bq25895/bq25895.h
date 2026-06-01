#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include <vector>

namespace esphome::bq25895 {

// BQ25895 Register addresses with descriptive names
static const uint8_t BQ25895_REG_INPUT_CURRENT_LIMIT = 0x00;  // Input current limit control
static const uint8_t BQ25895_REG_VINDPM = 0x01;               // Input voltage limit
static const uint8_t BQ25895_REG_ADC_CONTROL = 0x02;          // ADC control and function disable
static const uint8_t BQ25895_REG_SYS_CONTROL = 0x03;          // Charge enable and system config
static const uint8_t BQ25895_REG_CHARGE_CURRENT = 0x04;       // Fast charge current limit
static const uint8_t BQ25895_REG_PRECHARGE_CURRENT = 0x05;    // Pre-charge/termination current
static const uint8_t BQ25895_REG_CHARGE_VOLTAGE = 0x06;       // Charge voltage limit
static const uint8_t BQ25895_REG_TIMER_CONTROL = 0x07;        // Charge timer and status LED control
static const uint8_t BQ25895_REG_IR_COMP = 0x08;              // IR compensation
static const uint8_t BQ25895_REG_FORCE_DPDM = 0x09;           // Force DPDM detection
static const uint8_t BQ25895_REG_BOOST_CONTROL = 0x0A;        // Boost mode voltage/current
static const uint8_t BQ25895_REG_STATUS = 0x0B;               // System status (bus, charge status)
static const uint8_t BQ25895_REG_FAULT = 0x0C;                // Fault status (NTC)
static const uint8_t BQ25895_REG_VINDPM_STATUS = 0x0D;        // Input voltage limit status (also sys voltage)
static const uint8_t BQ25895_REG_BATV = 0x0E;                 // Battery voltage ADC
static const uint8_t BQ25895_REG_VBUS_VOLTAGE = 0x11;         // VBUS voltage
static const uint8_t BQ25895_REG_CHARGE_CURRENT_MONITOR = 0x12; // Charge current
static const uint8_t BQ25895_REG_INPUT_VOLTAGE_LIMIT = 0x13;  // Input voltage limit
static const uint8_t BQ25895_REG_DEVICE_ID = 0x14;            // Part information

// Constants for voltage and current calculations
static const uint16_t VBUS_BASE_MV = 2600;        // mV
static const uint16_t VBUS_STEP_MV = 100;          // mV
static const uint16_t VBAT_BASE_MV = 2304;         // mV
static const uint16_t VBAT_STEP_MV = 20;           // mV
static const uint16_t VSYS_BASE_MV = 2304;         // mV
static const uint16_t VSYS_STEP_MV = 20;           // mV
static const uint16_t CHG_CURRENT_STEP_MA = 50;    // mA
static const uint16_t PRE_CHG_BASE_MA = 64;        // mA
static const uint16_t PRE_CHG_STEP_MA = 64;        // mA
static const uint16_t CHG_VOLTAGE_BASE = 3840;     // mV
static const uint16_t CHG_VOLTAGE_STEP = 16;       // mV
static const uint16_t INPUT_CURRENT_MIN = 100;     // mA
static const uint16_t INPUT_CURRENT_STEP = 50;     // mA

// ADC control register (REG02) bit definitions for BQ25895
// Bit 7: ADC_RATE — 0 = continuous conversion, 1 = one-shot
// Bit 6: ADC_START — write 1 to start ADC (self-clearing in one-shot mode)
static const uint8_t BQ25895_ADC_MASK = 0xC0;
static const uint8_t BQ25895_ADC_CONTINUOUS = 0x40;   // continuous + start
static const uint8_t BQ25895_ADC_ONE_SHOT = 0xC0;     // one-shot + start
static const uint8_t BQ25895_ADC_DISABLE = 0x00;

// Bus Status values (REG_0B[7:5])
enum BusStatus {
  BUS_STATUS_NO_INPUT = 0,
  BUS_STATUS_USB_SDP = 1,
  BUS_STATUS_USB_CDP = 2,
  BUS_STATUS_USB_DCP = 3,
  BUS_STATUS_HVDCP = 4,
  BUS_STATUS_ADAPTER = 5,
  BUS_STATUS_NO_STD_ADAPTER = 6,
  BUS_STATUS_OTG = 7,
};

// Charge Status values (REG_0B[4:3])
enum ChargeStatus {
  CHARGE_STATUS_NOT_CHARGING = 0,
  CHARGE_STATUS_PRE_CHARGE = 1,
  CHARGE_STATUS_FAST_CHARGE = 2,
  CHARGE_STATUS_CHARGE_DONE = 3,
};

// Structure to hold all register data read in one transaction
struct BQ25895Data {
  uint8_t registers[21];  // Registers 0x00-0x14 (includes unused 0x0F, 0x10)
};

// Listener interface for components that want to receive BQ25895 data updates
class BQ25895Listener {
 public:
  virtual void on_data(const BQ25895Data &data) = 0;
};

class BQ25895Component : public PollingComponent, public i2c::I2CDevice {
 public:
  BQ25895Component(bool led_enabled, uint16_t input_current_limit, uint16_t charge_voltage, uint16_t charge_current,
                   uint16_t precharge_current, bool charge_enabled, bool enable_adc)
      : led_enabled_(led_enabled),
        input_current_limit_(input_current_limit),
        charge_voltage_(charge_voltage),
        charge_current_(charge_current),
        precharge_current_(precharge_current),
        charge_enabled_(charge_enabled),
        enable_adc_(enable_adc) {}
  void setup() override;
  void dump_config() override;
  void update() override;

  // Listener registration
  void add_listener(BQ25895Listener *listener) { this->listeners_.push_back(listener); }

  // Configuration methods to be called from lambdas
  void set_input_current_limit(uint16_t milliamps);
  void set_charge_target_voltage(uint16_t millivolts);
  void set_precharge_current(uint16_t milliamps);
  void set_charge_current(uint16_t milliamps);
  void set_charge_enabled(bool enabled);
  void set_led_enabled(bool enabled);
  void set_enable_adc_measure(bool enabled = true);

 protected:
  bool read_all_registers_();
  bool write_register_(uint8_t reg, uint8_t value);
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);
  bool trigger_adc_();

  BQ25895Data data_{};
  std::vector<BQ25895Listener *> listeners_;

  // Configuration values to set during setup()
  bool led_enabled_;
  uint16_t input_current_limit_;
  uint16_t charge_voltage_;
  uint16_t charge_current_;
  uint16_t precharge_current_;
  bool charge_enabled_;
  bool enable_adc_;
};

}  // namespace esphome::bq25895
