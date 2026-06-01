#pragma once

#include "../bq25895.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::bq25895 {

template<uint8_t REG, uint8_t SHIFT, uint8_t MASK, uint8_t TRUE_VALUE>
class StatusBinarySensor : public BQ25895Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25895Data &data) override {
    uint8_t value = (data.registers[REG] >> SHIFT) & MASK;
    this->publish_state(value == TRUE_VALUE);
  }
};

template<uint8_t REG, uint8_t SHIFT, uint8_t MASK, uint8_t FALSE_VALUE>
class InverseStatusBinarySensor : public BQ25895Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25895Data &data) override {
    uint8_t value = (data.registers[REG] >> SHIFT) & MASK;
    this->publish_state(value != FALSE_VALUE);
  }
};

// Custom binary sensor for charging (true when pre-charge or fast charge)
class BQ25895ChargingBinarySensor : public BQ25895Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25895Data &data) override {
    uint8_t chrg_stat = (data.registers[BQ25895_REG_STATUS] >> 3) & 0x03;
    bool charging = chrg_stat != CHARGE_STATUS_NOT_CHARGING && chrg_stat != CHARGE_STATUS_CHARGE_DONE;
    this->publish_state(charging);
  }
};

// Specialized sensor types using templates
// VBUS connected: BUS_STATUS != NO_INPUT
using BQ25895VbusConnectedBinarySensor = InverseStatusBinarySensor<BQ25895_REG_STATUS, 5, 0x07, BUS_STATUS_NO_INPUT>;

// Charge done: CHARGE_STATUS == CHARGE_DONE
using BQ25895ChargeDoneBinarySensor = StatusBinarySensor<BQ25895_REG_STATUS, 3, 0x03, CHARGE_STATUS_CHARGE_DONE>;

}  // namespace esphome::bq25895
