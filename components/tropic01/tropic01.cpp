#include "tropic01.h"
#include "libtropic.h"
#include "libtropic_common.h"
#include "libtropic_mbedtls_v4.h"
#include "esphome/core/log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include <cstdio>
#include <cstring>

// SPI port device context — used by lt_port_* functions below.
struct lt_dev_esp32_t {
    spi_device_handle_t spi;
    gpio_num_t cs_pin;
};

namespace esphome::tropic01 {

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
  auto *dev = new lt_dev_esp32_t{};
  dev->cs_pin = cs_pin_;
  dev->spi = nullptr;

  // Allocate libtropic handle
  handle_ = new lt_handle_t{};
  std::memset(handle_, 0, sizeof(lt_handle_t));
  handle_->l2.device = dev;

  // Allocate crypto context (mbedTLS v4 CAL) — lt_init needs this
  handle_->l3.crypto_ctx = new lt_ctx_mbedtls_v4_t{};
  std::memset(handle_->l3.crypto_ctx, 0, sizeof(lt_ctx_mbedtls_v4_t));

  lt_ret_t ret = lt_init(handle_);
  if (ret != LT_OK) {
    ESP_LOGE(TAG, "lt_init failed: 0x%X (%s)", (unsigned)ret,
             lt_ret_verbose(ret));
    delete static_cast<lt_ctx_mbedtls_v4_t *>(handle_->l3.crypto_ctx);
    delete dev;
    delete handle_;
    handle_ = nullptr;
    return false;
  }

  initialized_ = true;
  ESP_LOGI(TAG, "TROPIC01 initialized successfully");
  return true;
}

void Tropic01Component::setup() {
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
  ChipMode mode = read_chip_mode();

  if (chip_mode_sensor_) {
    chip_mode_sensor_->publish_state(mode_to_str(mode));
  }

  if (alarm_sensor_) {
    alarm_sensor_->publish_state(mode == ChipMode::ALARM);
  }

  if (fw_riscv_sensor_ || fw_spect_sensor_) {
    read_fw_versions();
  }

  if (chip_serial_sensor_) {
    read_chip_id();
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

bool Tropic01Component::read_fw_versions() {
  uint8_t ver_riscv[4] = {};
  lt_ret_t ret = lt_get_info_riscv_fw_ver(handle_, ver_riscv);
  if (ret == LT_OK && fw_riscv_sensor_) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u", ver_riscv[3], ver_riscv[2],
                  ver_riscv[1]);
    fw_riscv_sensor_->publish_state(buf);
  }

  uint8_t ver_spect[4] = {};
  ret = lt_get_info_spect_fw_ver(handle_, ver_spect);
  if (ret == LT_OK && fw_spect_sensor_) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u", ver_spect[3], ver_spect[2],
                  ver_spect[1]);
    fw_spect_sensor_->publish_state(buf);
  }

  return true;
}

bool Tropic01Component::read_chip_id() {
  struct lt_chip_id_t chip_id;
  lt_ret_t ret = lt_get_info_chip_id(handle_, &chip_id);
  if (ret != LT_OK) {
    ESP_LOGW(TAG, "lt_get_info_chip_id failed: 0x%X", (unsigned)ret);
    return false;
  }

  // Format serial number fields
  char buf[128];
  std::snprintf(buf, sizeof(buf),
                "SN:%u LOT:%02X%02X%02X%02X%02X W:%u XY:%u,%u",
                chip_id.ser_num.sn,
                chip_id.ser_num.lot_id[0], chip_id.ser_num.lot_id[1],
                chip_id.ser_num.lot_id[2], chip_id.ser_num.lot_id[3],
                chip_id.ser_num.lot_id[4],
                chip_id.ser_num.wafer_id,
                chip_id.ser_num.x_coord, chip_id.ser_num.y_coord);
  if (chip_serial_sensor_) {
    chip_serial_sensor_->publish_state(buf);
  }
  ESP_LOGI(TAG, "Chip ID: %s", buf);
  return true;
}

void Tropic01Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TROPIC01:");
  ESP_LOGCONFIG(TAG, "  CS Pin: GPIO%d", cs_pin_);
  ESP_LOGCONFIG(TAG, "  SPI Data Rate: %u Hz", spi_data_rate_);
  ESP_LOGCONFIG(TAG, "  Initialized: %s", YESNO(initialized_));
  ESP_LOGCONFIG(TAG, "  Secure Session: %s", YESNO(session_established_));
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

  ESP_LOGI(TAG, "Establishing L3 secure session (SH0)...");
  lt_ret_t ret = lt_verify_chip_and_start_secure_session(
      handle_, sh0priv_prod0, sh0pub_prod0, TR01_PAIRING_KEY_SLOT_INDEX_0);

  if (ret != LT_OK) {
    ESP_LOGE(TAG, "Secure session failed: 0x%X (%s)",
             (unsigned)ret, lt_ret_verbose(ret));
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
  if (ret != LT_OK) {
    ESP_LOGE(TAG, "R-Memory write slot %u (%u bytes) failed: 0x%X (%s)",
             slot, data_size, (unsigned)ret, lt_ret_verbose(ret));
    this->status_set_warning();
  } else {
    ESP_LOGI(TAG, "R-Memory write slot %u: %u bytes", slot, data_size);
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
  lt_ret_t ret = lt_r_mem_data_read(handle_, slot, r_mem_read_buf_,
                                     sizeof(r_mem_read_buf_), &actual);
  if (ret != LT_OK) {
    ESP_LOGE(TAG, "R-Memory read slot %u failed: 0x%X (%s)",
             slot, (unsigned)ret, lt_ret_verbose(ret));
    this->status_set_warning();
  } else {
    read_len = actual;
    ESP_LOGI(TAG, "R-Memory read slot %u: %u bytes", slot, actual);
    this->status_clear_warning();
  }
}

void Tropic01Component::r_mem_erase(uint16_t slot) {
  if (!ensure_secure_session()) {
    ESP_LOGE(TAG, "r_mem_erase: no secure session");
    return;
  }

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

#include "libtropic_port.h"
#include "driver/spi_master.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// SPI bus: shared with display (SPI2_HOST), manual CS, 10 MHz
#define SPI_BUS_HOST  SPI2_HOST
#define SPI_DMA_CHAN  SPI_DMA_CH_AUTO

// Track bus init so second call is a no-op
static bool s_spi_initialized = false;

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

    if (!s_spi_initialized) {
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
        s_spi_initialized = true;
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
    if (dev->spi) { spi_bus_remove_device(dev->spi); dev->spi = nullptr; }
    gpio_set_level(dev->cs_pin, 1);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_csn_low(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) return LT_PARAM_ERR;
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
    if (!s2 || !s2->device) return LT_PARAM_ERR;
    auto *dev = static_cast<lt_dev_esp32_t *>(s2->device);
    if (!dev->spi) return LT_L1_SPI_ERROR;

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
