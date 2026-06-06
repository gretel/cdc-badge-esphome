#pragma once

#include "../tropic01_base.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::tropic01 {

// Tamper alarm — true when chip is in ALARM mode.
class Tropic01AlarmBinarySensor : public Tropic01Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const Tropic01Data &data) override { this->publish_state(data.alarm); }
};

// Maintenance mode — true when chip is in MAINTENANCE (bootloader, no App FW).
// When true, L3 commands (R-Memory, ECC) are unavailable.
// If the pairing key was customized, attempting to exit maintenance mode
// with the default key risks lockout — sensor alerts the user instead.
class Tropic01MaintenanceBinarySensor : public Tropic01Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const Tropic01Data &data) override { this->publish_state(data.maintenance); }
};

}  // namespace esphome::tropic01
