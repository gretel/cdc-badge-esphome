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

// Global pointer for SPI port workaround — libtropic v3 may null l2.device.
// Set in setup(), used by lt_port_spi_* functions before setup() completes
// the component may not exist yet, but we only need the workaround after
// a secure session has been established (which is after setup()).
Tropic01Component *g_tr01_component = nullptr;

Tropic01Component::~Tropic01Component() {
  if (g_tr01_component == this) {
    g_tr01_component = nullptr;
  }
  delete handle_;
  delete crypto_ctx_;
  // dev_ destroyed automatically by unique_ptr
}

static const char *const TAG = "tropic01";

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
    // unique_ptrs auto-clean on scope exit
    return false;
  }

  lt_ret_t ret = lt_init(handle_);
  if (ret != LT_OK) {
    ESP_LOGE(TAG, "lt_init failed: 0x%X (%s)", (unsigned)ret,
             lt_ret_verbose(ret));
    // unique_ptrs auto-clean on scope exit
    return false;
  }

  initialized_ = true;
  ESP_LOGI(TAG, "TROPIC01 initialized successfully");
  return true;
}

void Tropic01Component::setup() {
  // Register this instance for SPI port workaround
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

  read_fw_versions(data);
  read_chip_id(data);

  // Notify all registered sub-platform sensors
  for (auto *listener : listeners_) {
    listener->on_data(data);
  }

  if (mode == ChipMode::ALARM) {
    ESP_LOGW(TAG, "TROPIC01 in ALARM mode — tamper detected!");
    this->status_set_warning();
  } else {
    this->status_clear_warning();
  }
}

ChipMode Tropic01Component::read_chip_mode() {
  lt_tr01_mode_t mode;
  ESP_LOGI(TAG, "read_chip_mode h=%p mode=%p", (void*)handle_, (void*)&mode);
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
  // Release libtropic resources
  delete handle_;
  handle_ = nullptr;
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
  if (session_established_)
    return true;

  // WORKAROUND: libtropic v3.3.0 may corrupt `l2.device` during secure session
  // establishment (missing RSP_LEN check in lt_l2_frame_check → buffer overflow).
  // Save the device pointer before the call and restore it after.
  // Fixed in libtropic v4.0.0.
  void *const saved_device = handle_ ? handle_->l2.device : nullptr;
  lt_ret_t ret = lt_verify_chip_and_start_secure_session(
      handle_, sh0priv_prod0, sh0pub_prod0, TR01_PAIRING_KEY_SLOT_INDEX_0);
  if (handle_ && saved_device) {
    handle_->l2.device = saved_device;
  }

  if (ret != LT_OK) {
    ESP_LOGE(TAG, "Secure session failed: 0x%X (%s)",
             (unsigned)ret, lt_ret_verbose(ret));
    ESP_LOGE(TAG, "  initialized=%d session_established=%d",
             initialized_, session_established_);
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

  ESP_LOGI(TAG, "r_mem_write slot=%u size=%u h=%p data=%p",
           slot, data_size, (void*)handle_, (void*)data);
  lt_ret_t ret = lt_r_mem_data_write(handle_, slot, data, data_size);
  if (ret != LT_OK) {
    ESP_LOGE(TAG, "R-Memory write slot %u (%u bytes) failed: 0x%X (%s)",
             slot, data_size, (unsigned)ret, lt_ret_verbose(ret));
    this->status_set_warning();
  } else {
    ESP_LOGI(TAG, "R-Memory write slot %u: %u bytes OK", slot, data_size);
    this->status_clear_warning();
  }
}

void Tropic01Component::r_mem_read(uint16_t slot, uint16_t &read_len) {
  read_len = 0;
  if (!ensure_secure_session()) {
    ESP_LOGE(TAG, "r_mem_read: no secure session");
    return;
  }

  uint16_t actual = 0;
  ESP_LOGI(TAG, "r_mem_read slot=%u start h=%p data=%p dsz=%u act=%p",
           slot, (void*)handle_, (void*)r_mem_read_buf_,
           (unsigned)sizeof(r_mem_read_buf_), (void*)&actual);
  lt_ret_t ret = lt_r_mem_data_read(handle_, slot, r_mem_read_buf_,
                                     sizeof(r_mem_read_buf_), &actual);
  if (ret != LT_OK) {
    ESP_LOGE(TAG, "R-Memory read slot %u failed: 0x%X (%s)",
             slot, (unsigned)ret, lt_ret_verbose(ret));
    this->status_set_warning();
  } else {
    read_len = actual;
    ESP_LOGI(TAG, "R-Memory read slot %u: %u bytes data=[%.*s]",
             slot, actual, actual, r_mem_read_buf_);
    this->status_clear_warning();
  }
}

void Tropic01Component::r_mem_erase(uint16_t slot) {
  if (!ensure_secure_session()) {
    ESP_LOGE(TAG, "r_mem_erase: no secure session");
    return;
  }

  ESP_LOGI(TAG, "r_mem_erase slot=%u start h=%p",
           slot, (void*)handle_);
  lt_ret_t ret = lt_r_mem_data_erase(handle_, slot);
  if (ret != LT_OK) {
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

// SPI bus: shared with display (SPI2_HOST), manual CS, 10 MHz
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
        return LT_L1_SPI_ERROR;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 10 * 1000 * 1000;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1;
    devcfg.queue_size = 1;

    if (spi_bus_add_device(SPI_BUS_HOST, &devcfg, &dev->spi) != ESP_OK) {
        ESP_LOGE("TR01-SPI", "Failed to add TROPIC01 SPI device");
        return LT_L1_SPI_ERROR;
    }
    ESP_LOGI("TR01-SPI", "TROPIC01 SPI ready (CS=GPIO%d, 10MHz)", dev->cs_pin);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_deinit(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) return LT_PARAM_ERR;
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);
    ESP_LOGI("TR01-DEINIT", "lt_port_deinit called, spi=%p", (void*)dev->spi);
    if (dev->spi) { spi_bus_remove_device(dev->spi); dev->spi = nullptr; }
    gpio_set_level(dev->cs_pin, 1);
    return LT_OK;
}

// Forward declaration for workaround — declared at namespace scope above.
// Workaround: libtropic v3.3.0 may null `l2.device` via buffer overflow during
// secure session (missing RSP_LEN check). We re-wrap from global dev_ as fallback.
// Fixed in libtropic v4.0.0.
namespace esphome::tropic01 {
extern Tropic01Component *g_tr01_component;
}

extern "C" lt_ret_t lt_port_spi_csn_low(lt_l2_state_t *s2) {
    if (!s2) { ESP_LOGW("TR01-CSN", "csn_low: s2 null"); return LT_PARAM_ERR; }
    // Workaround: restore device pointer if libtropic corrupted it
    if (!s2->device) {
        s2->device = esphome::tropic01::g_tr01_component
                         ? esphome::tropic01::g_tr01_component->get_dev()
                         : nullptr;
        if (!s2->device) {
            ESP_LOGW("TR01-CSN", "csn_low: device still null after workaround");
            return LT_PARAM_ERR;
        }
        ESP_LOGI("TR01-CSN", "csn_low: restored device=%p", s2->device);
    }
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);
    gpio_set_level(dev->cs_pin, 0);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_csn_high(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) return LT_PARAM_ERR;
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);
    gpio_set_level(dev->cs_pin, 1);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_transfer(lt_l2_state_t *s2, uint8_t offset,
                                          uint16_t tx_len, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!s2) { ESP_LOGW("TR01-SPI", "xfer: s2 null"); return LT_PARAM_ERR; }
    // Workaround: restore device pointer if libtropic corrupted it
    if (!s2->device) {
        s2->device = esphome::tropic01::g_tr01_component
                         ? esphome::tropic01::g_tr01_component->get_dev()
                         : nullptr;
        if (!s2->device) {
            ESP_LOGW("TR01-SPI", "xfer: device still null after workaround");
            return LT_PARAM_ERR;
        }
        ESP_LOGI("TR01-SPI", "xfer: restored device=%p", s2->device);
    }
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);
    if (!dev->spi) { ESP_LOGW("TR01-SPI", "xfer: dev->spi null"); return LT_L1_SPI_ERROR; }

    spi_transaction_t t = {};
    t.length = tx_len * 8;
    t.rxlength = tx_len * 8;
    t.tx_buffer = s2->buff + offset;
    t.rx_buffer = s2->buff + offset;

    esp_err_t ret = spi_device_polling_transmit(dev->spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE("TR01-SPI", "SPI xfer failed: %d", ret);
        return LT_L1_SPI_ERROR;
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
