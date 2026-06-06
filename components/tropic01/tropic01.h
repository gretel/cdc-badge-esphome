#pragma once

#include "esphome/core/component.h"
#include "driver/gpio.h"
#include "tropic01_base.h"

#include <memory>
#include <vector>

#include "driver/spi_master.h"

// Forward declare libtropic types (defined in libtropic_*.h, included in .cpp)
// Only lt_dev_esp32_t is fully defined here for unique_ptr; the others use
// raw pointers with explicit cleanup in destructor to avoid mbedTLS header chain.
struct lt_handle_t;
struct lt_ctx_mbedtls_v4_t;

// SPI port device context — full definition needed for unique_ptr<lt_dev_esp32_t>
struct lt_dev_esp32_t {
  spi_device_handle_t spi{};
  gpio_num_t cs_pin{GPIO_NUM_10};
};

namespace esphome::tropic01 {

// Operating mode of the TROPIC01 secure element.
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

  // Listener registration — sub-platform sensors call this from to_code
  void add_listener(Tropic01Listener *listener) { listeners_.push_back(listener); }

  // R-Memory storage (requires L3 secure session).
  // r_mem_read returns 0 on success, non-zero error code on failure.
  // Empty/unwritten slots return LT_L3_R_MEM_DATA_READ_SLOT_EMPTY (non-zero).
  void r_mem_write(uint16_t slot, const uint8_t *data, uint16_t data_size);
  int r_mem_read(uint16_t slot, uint16_t &read_len);
  const uint8_t *r_mem_buf() const { return r_mem_read_buf_; }
  void r_mem_erase(uint16_t slot);

  // Component lifecycle
  void setup() override;
  void update() override;
  void dump_config() override;
  void on_safe_shutdown() override;
  void on_shutdown() override;
  bool teardown() override;
  void on_powerdown() override;
  ~Tropic01Component();
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Expose device pointer for SPI port workaround (heap corruption recovery).
  void *get_dev() const { return dev_.get(); }

  // True if the default factory pairing key is no longer valid
  // (user has customized the pairing key).  When true, we refuse
  // to establish secure sessions with the default key to avoid
  // lockout from repeated failed handshakes.
  bool default_key_invalid() const { return default_key_invalid_; }

 protected:
  bool init_tropic();
  void update_sensors();
  ChipMode read_chip_mode();
  bool read_fw_versions(Tropic01Data &data);
  bool read_chip_id(Tropic01Data &data);
  bool ensure_secure_session();

  gpio_num_t cs_pin_{GPIO_NUM_10};
  uint32_t spi_data_rate_{10 * 1000 * 1000};

  std::vector<Tropic01Listener *> listeners_;

  // Resources (destroyed in reverse declaration order)
  lt_handle_t *handle_{nullptr};
  lt_ctx_mbedtls_v4_t *crypto_ctx_{nullptr};
  std::unique_ptr<lt_dev_esp32_t> dev_;
  bool initialized_{false};
  bool session_established_{false};
  bool default_key_invalid_{false};  // Default pairing key no longer matches slot 0
  bool lockout_warned_{false};       // Only log lockout warning once
  uint8_t r_mem_read_buf_[512]{};
};

}  // namespace esphome::tropic01
