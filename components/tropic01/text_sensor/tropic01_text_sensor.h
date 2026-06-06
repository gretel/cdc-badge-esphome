#pragma once

#include "../tropic01_base.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::tropic01 {

// Generic text sensor that publishes a string field from Tropic01Data.
template<auto Member>
class Tropic01DataTextSensor : public Tropic01Listener, public text_sensor::TextSensor {
 public:
  void on_data(const Tropic01Data &data) override { this->publish_state(data.*Member); }
};

// Specialised types — each reads a different Tropic01Data field.
using Tropic01ChipModeTextSensor = Tropic01DataTextSensor<&Tropic01Data::chip_mode>;
using Tropic01FwRiscvTextSensor = Tropic01DataTextSensor<&Tropic01Data::fw_riscv>;
using Tropic01FwSpectTextSensor = Tropic01DataTextSensor<&Tropic01Data::fw_spect>;
using Tropic01ChipSerialTextSensor = Tropic01DataTextSensor<&Tropic01Data::chip_serial>;

}  // namespace esphome::tropic01
