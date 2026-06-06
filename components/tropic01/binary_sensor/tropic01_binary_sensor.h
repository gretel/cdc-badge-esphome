#pragma once

#include "../tropic01_base.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::tropic01 {

// Tamper alarm — true when chip is in ALARM mode.
class Tropic01AlarmBinarySensor : public Tropic01Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const Tropic01Data &data) override { this->publish_state(data.alarm); }
};

}  // namespace esphome::tropic01
