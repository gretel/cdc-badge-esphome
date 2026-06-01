#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "driver/gpio.h"

// Forward declare libtropic handle (defined in libtropic_common.h)
struct lt_handle_t;

namespace esphome::tropic01 {

enum class ChipMode : uint8_t {
  UNKNOWN = 0,
  MAINTENANCE,
  APPLICATION,
  ALARM,
};

class Tropic01Component : public PollingComponent {
 public:
  void set_cs_pin(int pin) { cs_pin_ = static_cast<gpio_num_t>(pin); }
  void set_spi_data_rate(uint32_t hz) { spi_data_rate_ = hz; }

  // Entity setters (called by codegen)
  void set_chip_mode_sensor(text_sensor::TextSensor *s) { chip_mode_sensor_ = s; }
  void set_fw_riscv_sensor(text_sensor::TextSensor *s) { fw_riscv_sensor_ = s; }
  void set_fw_spect_sensor(text_sensor::TextSensor *s) { fw_spect_sensor_ = s; }
  void set_chip_serial_sensor(text_sensor::TextSensor *s) { chip_serial_sensor_ = s; }
  void set_alarm_sensor(binary_sensor::BinarySensor *s) { alarm_sensor_ = s; }

  // R-Memory storage (requires L3 secure session)
  void r_mem_write(uint16_t slot, const uint8_t *data, uint16_t data_size);
  void r_mem_read(uint16_t slot, uint16_t &read_len);
  const uint8_t *r_mem_buf() const { return r_mem_read_buf_; }
  void r_mem_erase(uint16_t slot);

  // Component lifecycle
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  bool init_tropic();
  void update_sensors();
  ChipMode read_chip_mode();
  bool read_fw_versions();
  bool read_chip_id();

  gpio_num_t cs_pin_{GPIO_NUM_10};
  uint32_t spi_data_rate_{10 * 1000 * 1000};

  text_sensor::TextSensor *chip_mode_sensor_{nullptr};
  text_sensor::TextSensor *fw_riscv_sensor_{nullptr};
  text_sensor::TextSensor *fw_spect_sensor_{nullptr};
  text_sensor::TextSensor *chip_serial_sensor_{nullptr};
  binary_sensor::BinarySensor *alarm_sensor_{nullptr};

  bool ensure_secure_session();

  lt_handle_t *handle_{nullptr};
  bool initialized_{false};
  bool session_established_{false};
  uint8_t r_mem_read_buf_[512]{};
};

}  // namespace esphome::tropic01
