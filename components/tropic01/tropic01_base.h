#pragma once

#include <cstdint>

namespace esphome::tropic01 {

// Runtime data snapshot delivered to all listeners on each update cycle.
// String fields are pre-formatted — listeners publish directly without parsing.
struct Tropic01Data {
  char chip_mode[16]{};     // "MAINTENANCE", "APPLICATION", "ALARM", "UNKNOWN"
  char fw_riscv[32]{};      // "1.2.3"
  char fw_spect[32]{};      // "1.2.3"
  char chip_serial[128]{};  // "SN:123 LOT:ABCDE W:1 XY:2,3"
  bool alarm{false};         // Chip in ALARM mode (tamper)
  bool maintenance{false};   // Chip in MAINTENANCE mode (no App FW / bootloader)
};

// Listener interface — sub-platform sensors implement this to receive data.
class Tropic01Listener {
 public:
  virtual void on_data(const Tropic01Data &data) = 0;
};

}  // namespace esphome::tropic01
