import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, binary_sensor
from esphome.const import (
    CONF_ID,
)
from pathlib import Path
from esphome.core import CORE

# SPI not declared as formal dependency — this component manages SPI via
# ESP-IDF directly (shared bus with display, manual CS).
DEPENDENCIES = []
AUTO_LOAD = ["text_sensor", "binary_sensor"]
CODEOWNERS = ["@riotindustries"]

tropic01_ns = cg.esphome_ns.namespace("tropic01")
Tropic01Component = tropic01_ns.class_(
    "Tropic01Component", cg.PollingComponent
)

CONF_CS_PIN = "cs_pin"
CONF_DATA_RATE = "data_rate"
CONF_CHIP_MODE = "chip_mode"
CONF_FW_RISCV = "fw_version_riscv"
CONF_FW_SPECT = "fw_version_spect"
CONF_CHIP_SERIAL = "chip_serial"
CONF_ALARM = "alarm"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Tropic01Component),
            cv.Required(CONF_CS_PIN): cv.int_range(min=0, max=48),
            cv.Optional(CONF_DATA_RATE, default=10000000): cv.int_range(
                min=1000000, max=40000000
            ),
            cv.Optional(CONF_CHIP_MODE): text_sensor.text_sensor_schema(
                icon="mdi:chip"
            ),
            cv.Optional(CONF_FW_RISCV): text_sensor.text_sensor_schema(
                icon="mdi:counter"
            ),
            cv.Optional(CONF_FW_SPECT): text_sensor.text_sensor_schema(
                icon="mdi:counter"
            ),
            cv.Optional(CONF_CHIP_SERIAL): text_sensor.text_sensor_schema(
                icon="mdi:serial-port"
            ),
            cv.Optional(CONF_ALARM): binary_sensor.binary_sensor_schema(
                icon="mdi:alert",
                device_class="tamper",
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_cs_pin(config[CONF_CS_PIN]))
    cg.add(var.set_spi_data_rate(config[CONF_DATA_RATE]))

    # Register sensors
    if chip_mode_config := config.get(CONF_CHIP_MODE):
        sens = await text_sensor.new_text_sensor(chip_mode_config)
        cg.add(var.set_chip_mode_sensor(sens))

    if fw_riscv_config := config.get(CONF_FW_RISCV):
        sens = await text_sensor.new_text_sensor(fw_riscv_config)
        cg.add(var.set_fw_riscv_sensor(sens))

    if fw_spect_config := config.get(CONF_FW_SPECT):
        sens = await text_sensor.new_text_sensor(fw_spect_config)
        cg.add(var.set_fw_spect_sensor(sens))

    if chip_serial_config := config.get(CONF_CHIP_SERIAL):
        sens = await text_sensor.new_text_sensor(chip_serial_config)
        cg.add(var.set_chip_serial_sensor(sens))

    if alarm_config := config.get(CONF_ALARM):
        sens = await binary_sensor.new_binary_sensor(alarm_config)
        cg.add(var.set_alarm_sensor(sens))

    # libtropic include paths, resolved from the ESPHome config directory
    component_dir = (Path(CORE.config_path).parent / "components" / "tropic01").resolve()

    # src/ for ESP-IDF SPI port (libtropic_port_esp32.cpp, included via unity build)
    cg.add_build_flag(f"-I{(component_dir / 'src').as_posix()}")
    # libtropic public headers
    cg.add_build_flag(f"-I{(component_dir / 'src' / 'libtropic' / 'include').as_posix()}")
    # libtropic internal headers (needed by .c sources during unity build)
    cg.add_build_flag(f"-I{(component_dir / 'src' / 'libtropic' / 'src').as_posix()}")
    # mbedTLS v4 CAL headers
    cg.add_build_flag(f"-I{(component_dir / 'src' / 'libtropic' / 'cal' / 'mbedtls_v4').as_posix()}")

    # libtropic configuration (use build flags, not add_define, so C files see them)
    cg.add_build_flag("-DACAB")
    cg.add_build_flag("-DLT_HELPERS")
    cg.add_build_flag("-DLT_LOG_ENABLE_ERROR=1")
    cg.add_build_flag("-DLT_LOG_ENABLE_WARN=0")
    cg.add_build_flag("-DLT_LOG_ENABLE_INFO=1")
    cg.add_build_flag("-DLT_LOG_ENABLE_DEBUG=0")

    # Suppress libtropic's intentional #warning about volatile-secure-memzero
    cg.add_build_flag("-Wno-cpp")
