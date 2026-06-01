#include "../bq25895.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::bq25895 {

// Template for voltage sensors (converts mV to V)
template<uint8_t REG, uint8_t MASK, uint16_t BASE, uint16_t STEP>
class VoltageSensor : public BQ25895Listener, public sensor::Sensor {
 public:
  void on_data(const BQ25895Data &data) override {
    uint8_t val = data.registers[REG] & MASK;
    uint16_t voltage_mv = BASE + (val * STEP);
    this->publish_state(voltage_mv * 0.001f);  // Convert mV to V
  }
};

// Template for current sensors (returns mA)
template<uint8_t REG, uint8_t MASK, uint16_t BASE, uint16_t STEP>
class CurrentSensor : public BQ25895Listener, public sensor::Sensor {
 public:
  void on_data(const BQ25895Data &data) override {
    uint8_t val = data.registers[REG] & MASK;
    uint16_t current_ma = BASE + (val * STEP);
    this->publish_state(current_ma);
  }
};

// Specialized sensor types using templates
using BQ25895VbusVoltageSensor = VoltageSensor<BQ25895_REG_VBUS_VOLTAGE, 0x7F, VBUS_BASE_MV, VBUS_STEP_MV>;
using BQ25895BatteryVoltageSensor = VoltageSensor<BQ25895_REG_BATV, 0x7F, VBAT_BASE_MV, VBAT_STEP_MV>;
using BQ25895SystemVoltageSensor = VoltageSensor<BQ25895_REG_VINDPM_STATUS, 0x7F, VSYS_BASE_MV, VSYS_STEP_MV>;
using BQ25895ChargeCurrentSensor = CurrentSensor<BQ25895_REG_CHARGE_CURRENT_MONITOR, 0x7F, 0, CHG_CURRENT_STEP_MA>;

// Precharge current sensor needs special handling (bit shift)
class BQ25895PrechargeCurrentSensor : public BQ25895Listener, public sensor::Sensor {
 public:
  void on_data(const BQ25895Data &data) override {
    uint8_t iprechg = (data.registers[BQ25895_REG_PRECHARGE_CURRENT] >> 4) & 0x0F;
    uint16_t iprechg_ma = PRE_CHG_BASE_MA + (iprechg * PRE_CHG_STEP_MA);
    this->publish_state(iprechg_ma);
  }
};

}  // namespace esphome::bq25895
