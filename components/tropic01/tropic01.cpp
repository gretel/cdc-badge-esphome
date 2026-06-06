#include "tropic01.h"
#include "libtropic.h"
#include "libtropic_common.h"
#include "libtropic_mbedtls_v4.h"
#include "libtropic_port.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#include "psa/crypto.h"
#pragma GCC diagnostic pop
#include "esphome/core/log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include <cstdio>
#include <cstring>

namespace esphome::tropic01 {

// Global instance pointer for SPI port workaround.
// lt_port_spi_* functions may find l2.device nullified by heap corruption;
// they restore it from this global.
Tropic01Component *g_tr01_component = nullptr;

static const char *const TAG = "tropic01";

Tropic01Component::~Tropic01Component() {
  if (g_tr01_component == this) {
    g_tr01_component = nullptr;
  }
  delete handle_;
  delete crypto_ctx_;
  // dev_ destroyed automatically by unique_ptr
}

static const char *mode_to_str(ChipMode m) {
  switch (m) {
    case ChipMode::MAINTENANCE:
      return "MAINTENANCE";
    case ChipMode::APPLICATION:
      return "APPLICATION";
    case ChipMode::ALARM:
      return "ALARM";
    default:
      return "UNKNOWN";
  }
}

static ChipMode tr01_mode_to_chip(lt_tr01_mode_t m) {
  switch (m) {
    case LT_TR01_MAINTENANCE:
      return ChipMode::MAINTENANCE;
    case LT_TR01_APPLICATION:
      return ChipMode::APPLICATION;
    case LT_TR01_ALARM:
      return ChipMode::ALARM;
    default:
      return ChipMode::UNKNOWN;
  }
}

bool Tropic01Component::init_tropic() {
  if (initialized_)
    return true;

  // Allocate device context for the SPI port layer
  dev_ = std::make_unique<lt_dev_esp32_t>();
  dev_->cs_pin = cs_pin_;
  dev_->spi = nullptr;

  // Allocate crypto context (mbedTLS v4 CAL)
  crypto_ctx_ = new lt_ctx_mbedtls_v4_t{};
  std::memset(crypto_ctx_, 0, sizeof(lt_ctx_mbedtls_v4_t));

  // Allocate libtropic handle and wire up dependencies
  handle_ = new lt_handle_t{};
  std::memset(handle_, 0, sizeof(lt_handle_t));
  handle_->l2.device = dev_.get();
  handle_->l3.crypto_ctx = crypto_ctx_;

  // Initialise mbedTLS PSA Crypto API — required before any hash/AES operations
  psa_status_t psa_ret = psa_crypto_init();
  if (psa_ret != PSA_SUCCESS) {
    ESP_LOGE(TAG, "psa_crypto_init failed: %d", (int)psa_ret);
    return false;
  }

  lt_ret_t ret = lt_init(handle_);
  if (ret != LT_OK) {
    ESP_LOGE(TAG, "lt_init failed: 0x%X (%s)", (unsigned)ret,
             lt_ret_verbose(ret));
    return false;
  }

  // v4.0.0 lt_init() auto-reboots from MAINTENANCE to APPLICATION. If that
  // reboot was unsuccessful (e.g. App FW missing or invalid), the chip
  // remains in MAINTENANCE and lt_init() still returns LT_OK. On the first
  // boot of ESPHome we make one explicit recovery attempt to move the chip
  // out of MAINTENANCE; this is a no-op if the chip is already in APPLICATION.
  lt_tr01_mode_t mode = LT_TR01_APPLICATION;
  ret = lt_get_tr01_mode(handle_, &mode);
  if (ret == LT_OK && mode != LT_TR01_APPLICATION) {
    ESP_LOGW(TAG, "TROPIC01 in %s mode after lt_init auto-reboot — attempting explicit recovery",
             mode_to_str(tr01_mode_to_chip(mode)));
    lt_ret_t reboot_ret = lt_reboot(handle_, TR01_REBOOT);
    if (reboot_ret == LT_OK) {
      // Chip reset. Re-read mode to confirm; session state (if any) is gone
      // chip-side, but lt_reboot does not touch handle_->l3.session_status.
      // We never started a session in this init path, so nothing to clear.
      ret = lt_get_tr01_mode(handle_, &mode);
      ESP_LOGI(TAG, "TROPIC01 explicit recovery OK — now in %s mode",
               ret == LT_OK ? mode_to_str(tr01_mode_to_chip(mode)) : "UNKNOWN");
    } else if (reboot_ret == LT_REBOOT_UNSUCCESSFUL) {
      ESP_LOGW(TAG, "TROPIC01 explicit recovery failed (LT_REBOOT_UNSUCCESSFUL)");
      ESP_LOGW(TAG, "  App FW may be missing or invalid — secure features (R-Memory, ECC) unavailable");
      if (default_key_invalid_) {
        ESP_LOGW(TAG, "  Default pairing key invalid — do NOT attempt secure session, lockout risk");
      }
    } else {
      ESP_LOGE(TAG, "TROPIC01 explicit recovery failed: 0x%X (%s)",
               (unsigned)reboot_ret, lt_ret_verbose(reboot_ret));
    }
  }

  initialized_ = true;
  ESP_LOGI(TAG, "TROPIC01 initialized successfully");
  return true;
}

void Tropic01Component::setup() {
  g_tr01_component = this;

  ESP_LOGCONFIG(TAG, "Setting up TROPIC01...");
  if (!init_tropic()) {
    ESP_LOGE(TAG, "TROPIC01 init failed, will retry in update loop");
    this->status_set_error();
  }
}

void Tropic01Component::update() {
  if (!initialized_) {
    ESP_LOGD(TAG, "SE not initialized, retrying init...");
    if (!init_tropic())
      return;
  }

  update_sensors();
}

void Tropic01Component::update_sensors() {
  Tropic01Data data{};

  // Populate all fields
  ChipMode mode = read_chip_mode();
  std::strncpy(data.chip_mode, mode_to_str(mode), sizeof(data.chip_mode) - 1);
  data.alarm = (mode == ChipMode::ALARM);
  data.maintenance = (mode == ChipMode::MAINTENANCE);

  read_fw_versions(data);
  read_chip_id(data);

  // Notify all registered sub-platform sensors
  for (auto *listener : listeners_) {
    listener->on_data(data);
  }

  if (mode == ChipMode::ALARM) {
    ESP_LOGW(TAG, "TROPIC01 in ALARM mode — tamper detected!");
    this->status_set_warning();
  } else if (mode == ChipMode::MAINTENANCE) {
    // MAINTENANCE mode is reported via the maintenance sensor and logged, but
    // does NOT mark the component as warning — the badge is still usable for
    // display/keypad/WiFi/BLE. Secure ops (R-Memory, ECC) are unavailable
    // until valid App FW is present, but the chip's state is not a fault.
    if (default_key_invalid_ && !lockout_warned_) {
      lockout_warned_ = true;
      ESP_LOGE(TAG, "TROPIC01 in MAINTENANCE mode with custom pairing key — lockout risk!");
      ESP_LOGE(TAG, "  Default key no longer matches slot 0. Secure operations unavailable.");
      ESP_LOGE(TAG, "  To recover, provide the correct pairing key or flash known-good FW.");
    } else if (!default_key_invalid_) {
      ESP_LOGW(TAG, "TROPIC01 in MAINTENANCE mode — App FW not loaded (R-Memory/ECC unavailable)");
    }
  } else {
    this->status_clear_warning();
  }
}

ChipMode Tropic01Component::read_chip_mode() {
  lt_tr01_mode_t mode;
  lt_ret_t ret = lt_get_tr01_mode(handle_, &mode);
  if (ret != LT_OK) {
    ESP_LOGW(TAG, "lt_get_tr01_mode failed: 0x%X", (unsigned)ret);
    if (ret == LT_L1_CHIP_ALARM_MODE) {
      return ChipMode::ALARM;
    }
    return ChipMode::UNKNOWN;
  }
  return tr01_mode_to_chip(mode);
}

bool Tropic01Component::read_fw_versions(Tropic01Data &data) {
  uint8_t ver_riscv[4] = {};
  lt_ret_t ret = lt_get_info_riscv_fw_ver(handle_, ver_riscv);
  if (ret == LT_OK) {
    std::snprintf(data.fw_riscv, sizeof(data.fw_riscv), "%u.%u.%u",
                  ver_riscv[3], ver_riscv[2], ver_riscv[1]);
  }

  uint8_t ver_spect[4] = {};
  ret = lt_get_info_spect_fw_ver(handle_, ver_spect);
  if (ret == LT_OK) {
    std::snprintf(data.fw_spect, sizeof(data.fw_spect), "%u.%u.%u",
                  ver_spect[3], ver_spect[2], ver_spect[1]);
  }

  return true;
}

bool Tropic01Component::read_chip_id(Tropic01Data &data) {
  struct lt_chip_id_t chip_id;
  lt_ret_t ret = lt_get_info_chip_id(handle_, &chip_id);
  if (ret != LT_OK) {
    ESP_LOGW(TAG, "lt_get_info_chip_id failed: 0x%X", (unsigned)ret);
    return false;
  }

  std::snprintf(data.chip_serial, sizeof(data.chip_serial),
                "SN:%u LOT:%02X%02X%02X%02X%02X W:%u XY:%u,%u",
                chip_id.ser_num.sn,
                chip_id.ser_num.lot_id[0], chip_id.ser_num.lot_id[1],
                chip_id.ser_num.lot_id[2], chip_id.ser_num.lot_id[3],
                chip_id.ser_num.lot_id[4],
                chip_id.ser_num.wafer_id,
                chip_id.ser_num.x_coord, chip_id.ser_num.y_coord);
  ESP_LOGI(TAG, "Chip ID: %s", data.chip_serial);
  return true;
}

void Tropic01Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TROPIC01:");
  ESP_LOGCONFIG(TAG, "  CS Pin: GPIO%d", cs_pin_);
  ESP_LOGCONFIG(TAG, "  SPI Data Rate: %u Hz", spi_data_rate_);
  ESP_LOGCONFIG(TAG, "  Initialized: %s", YESNO(initialized_));
  ESP_LOGCONFIG(TAG, "  Secure Session: %s", YESNO(session_established_));
}

void Tropic01Component::on_safe_shutdown() {
  // No critical pre-shutdown work needed for this component
}

void Tropic01Component::on_shutdown() {
  // Mark session as closed — any in-flight R-Memory op will fail fast
  session_established_ = false;
}

bool Tropic01Component::teardown() {
  // Remove SPI device from shared bus (leaves bus init to ESPHome SPI component)
  if (handle_ && handle_->l2.device) {
    lt_port_deinit(&handle_->l2);
  }
  return true;
}

void Tropic01Component::on_powerdown() {
  // Secure teardown: lt_deinit() wipes crypto keys and session data
  if (handle_) {
    lt_deinit(handle_);
    delete handle_;
    handle_ = nullptr;
  }
  delete crypto_ctx_;
  crypto_ctx_ = nullptr;
  dev_.reset();
  initialized_ = false;
}

// =========================================================================
// R-Memory (User Data) — requires L3 Secure Session
// =========================================================================

bool Tropic01Component::ensure_secure_session() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot start session: TROPIC01 not initialized");
    return false;
  }

  // If the default pairing key was already proven invalid, refuse to retry
  // — repeated failed handshakes with the wrong key could trigger ALARM mode.
  if (default_key_invalid_) {
    ESP_LOGE(TAG, "Cannot establish session: default key no longer matches slot 0");
    return false;
  }

  // libtropic may invalidate the session internally on error (e.g. write to
  // non-erased slot).  Detect this by checking the handle's actual session
  // status rather than trusting our cached flag alone.
  if (session_established_) {
    if (handle_ && handle_->l3.session_status == LT_SECURE_SESSION_ON) {
      return true;
    }
    ESP_LOGI(TAG, "Session was invalidated by previous error, re-establishing...");
    session_established_ = false;
  }

  lt_ret_t ret = lt_verify_chip_and_start_secure_session(
      handle_, lt_sh0priv_prod0, lt_sh0pub_prod0, TR01_PAIRING_KEY_SLOT_INDEX_0);

  if (ret != LT_OK) {
    // First failure with default key — mark it invalid to prevent
    // lockout from repeated failed handshakes.
    default_key_invalid_ = true;
    ESP_LOGE(TAG, "Secure session failed: 0x%X (%s)",
             (unsigned)ret, lt_ret_verbose(ret));
    ESP_LOGE(TAG, "  Default pairing key rejected by chip — pairing key was customized!");
    ESP_LOGE(TAG, "  Subsequent session attempts blocked to prevent lockout.");
    return false;
  }

  session_established_ = true;
  ESP_LOGI(TAG, "Secure session established");
  return true;
}

void Tropic01Component::r_mem_write(uint16_t slot, const uint8_t *data,
                                     uint16_t data_size) {
  if (!ensure_secure_session()) {
    ESP_LOGE(TAG, "r_mem_write: no secure session");
    return;
  }

  lt_ret_t ret = lt_r_mem_data_write(handle_, slot, data, data_size);
  if (ret == LT_L3_R_MEM_DATA_WRITE_FAILED) {
    ESP_LOGE(TAG, "R-Memory write slot %u failed: chip rejected (slot needs erase first)", slot);
    ESP_LOGE(TAG, "  Call se_erase(slot=%u) before writing to this slot again", slot);
    this->status_set_warning();
  } else if (ret != LT_OK) {
    ESP_LOGE(TAG, "R-Memory write slot %u (%u bytes) failed: 0x%X (%s)",
             slot, data_size, (unsigned)ret, lt_ret_verbose(ret));
    this->status_set_warning();
  } else {
    ESP_LOGI(TAG, "R-Memory write slot %u: %u bytes OK", slot, data_size);
    this->status_clear_warning();
  }
}

int Tropic01Component::r_mem_read(uint16_t slot, uint16_t &read_len) {
  read_len = 0;
  if (!ensure_secure_session()) {
    ESP_LOGE(TAG, "r_mem_read: no secure session");
    return -1;
  }

  uint16_t actual = 0;
  lt_ret_t ret = lt_r_mem_data_read(handle_, slot, r_mem_read_buf_,
                                     sizeof(r_mem_read_buf_), &actual);
  if (ret != LT_OK) {
    // Empty slot is an expected condition, not a component error.
    // Don't set status_warning — the service caller decides how to handle it.
    ESP_LOGI(TAG, "R-Memory read slot %u: %s (0x%X)",
             slot, lt_ret_verbose(ret), (unsigned)ret);
    return (int)ret;
  }

  read_len = actual;
  ESP_LOGI(TAG, "R-Memory read slot %u: %u bytes", slot, actual);
  this->status_clear_warning();
  return 0;
}

void Tropic01Component::r_mem_erase(uint16_t slot) {
  if (!ensure_secure_session()) {
    ESP_LOGE(TAG, "r_mem_erase: no secure session");
    return;
  }

  lt_ret_t ret = lt_r_mem_data_erase(handle_, slot);
  if (ret == LT_L3_R_MEM_DATA_ERASE_FAILED) {
    ESP_LOGE(TAG, "R-Memory erase slot %u failed: chip rejected the operation", slot);
    this->status_set_warning();
  } else if (ret != LT_OK) {
    ESP_LOGE(TAG, "R-Memory erase slot %u failed: 0x%X (%s)",
             slot, (unsigned)ret, lt_ret_verbose(ret));
    this->status_set_warning();
  } else {
    ESP_LOGI(TAG, "R-Memory erase slot %u", slot);
    this->status_clear_warning();
  }
}

}  // namespace esphome::tropic01

// =========================================================================
// ESP-IDF SPI port layer for libtropic (TROPIC01)
// Implements the port functions declared in libtropic_port.h.
// These must be extern "C" with C linkage because libtropic expects them.
// =========================================================================

#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdarg.h>

// SPI bus: shared with display (SPI2_HOST), manual CS
// We init the bus ourselves (handles ESP_ERR_INVALID_STATE if ESPHome's
// spi component already initialized SPI2_HOST).  DEPENDENCIES = ["spi"]
// ensures the SPI framework is loaded before our setup().
#define SPI_BUS_HOST  SPI2_HOST
#define SPI_DMA_CHAN  SPI_DMA_CH_AUTO

extern "C" lt_ret_t lt_port_init(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) return LT_PARAM_ERR;
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << dev->cs_pin;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(dev->cs_pin, 1);

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = GPIO_NUM_13;
    buscfg.miso_io_num = GPIO_NUM_11;
    buscfg.sclk_io_num = GPIO_NUM_12;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;
    buscfg.flags = SPICOMMON_BUSFLAG_MASTER;
    esp_err_t err = spi_bus_initialize(SPI_BUS_HOST, &buscfg, SPI_DMA_CHAN);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE("TR01-SPI", "SPI bus init failed: %d", err);
        return LT_HAL_ERROR;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 10 * 1000 * 1000;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1;
    devcfg.queue_size = 1;

    if (spi_bus_add_device(SPI_BUS_HOST, &devcfg, &dev->spi) != ESP_OK) {
        ESP_LOGE("TR01-SPI", "Failed to add TROPIC01 SPI device");
        return LT_HAL_ERROR;
    }
    ESP_LOGI("TR01-SPI", "TROPIC01 SPI ready (CS=GPIO%d, 10MHz)", dev->cs_pin);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_deinit(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) return LT_PARAM_ERR;
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);
    if (dev->spi) { spi_bus_remove_device(dev->spi); dev->spi = nullptr; }
    gpio_set_level(dev->cs_pin, 1);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_csn_low(lt_l2_state_t *s2) {
    if (!s2) return LT_PARAM_ERR;
    if (!s2->device) {
        // Workaround: heap corruption during OTA powerdown may null l2.device.
        // Restore from the global component instance.
        namespace tr01 = esphome::tropic01;
        if (tr01::g_tr01_component) {
            s2->device = tr01::g_tr01_component->get_dev();
        }
        if (!s2->device) return LT_PARAM_ERR;
    }
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);
    gpio_set_level(dev->cs_pin, 0);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_csn_high(lt_l2_state_t *s2) {
    if (!s2) return LT_PARAM_ERR;
    if (!s2->device) {
        namespace tr01 = esphome::tropic01;
        if (tr01::g_tr01_component) {
            s2->device = tr01::g_tr01_component->get_dev();
        }
        if (!s2->device) return LT_PARAM_ERR;
    }
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);
    gpio_set_level(dev->cs_pin, 1);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_transfer(lt_l2_state_t *s2, uint8_t offset,
                                          uint16_t tx_len, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!s2) return LT_PARAM_ERR;
    if (!s2->device) {
        namespace tr01 = esphome::tropic01;
        if (tr01::g_tr01_component) {
            s2->device = tr01::g_tr01_component->get_dev();
        }
        if (!s2->device) return LT_PARAM_ERR;
    }
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);
    if (!dev->spi) { return LT_HAL_ERROR; }

    spi_transaction_t t = {};
    t.length = tx_len * 8;
    t.rxlength = tx_len * 8;
    t.tx_buffer = s2->buff + offset;
    t.rx_buffer = s2->buff + offset;

    esp_err_t ret = spi_device_polling_transmit(dev->spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE("TR01-SPI", "SPI xfer failed: %d", ret);
        return LT_HAL_ERROR;
    }
    return LT_OK;
}

extern "C" lt_ret_t lt_port_delay(lt_l2_state_t *s2, uint32_t ms) {
    (void)s2;
    vTaskDelay(pdMS_TO_TICKS(ms));
    return LT_OK;
}

extern "C" lt_ret_t lt_port_random_bytes(lt_l2_state_t *s2, void *buff, size_t count) {
    (void)s2;
    esp_fill_random(buff, count);
    return LT_OK;
}

extern "C" int lt_port_log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}
