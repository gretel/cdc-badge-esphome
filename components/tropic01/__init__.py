import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from pathlib import Path

# This component manages SPI directly (shared bus with e-paper display).
# Formal SPI dependency declared so ESPHome validates the bus is configured.
DEPENDENCIES = ["spi"]
CODEOWNERS = ["@riotindustries"]

tropic01_ns = cg.esphome_ns.namespace("tropic01")
Tropic01Component = tropic01_ns.class_(
    "Tropic01Component", cg.PollingComponent
)

# Reference key for sub-platforms (text_sensor, binary_sensor)
CONF_TROPIC01_ID = "tropic01_id"
CONF_CS_PIN = "cs_pin"
CONF_DATA_RATE = "data_rate"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Tropic01Component),
            cv.Required(CONF_CS_PIN): cv.int_range(min=0, max=48),
            cv.Optional(CONF_DATA_RATE, default=10000000): cv.int_range(
                min=1000000, max=40000000
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

    # libtropic include paths (relative to this __init__.py)
    component_dir = Path(__file__).parent
    # src/ for ESP-IDF SPI port (libtropic_port_esp32.cpp, included via unity build)
    cg.add_build_flag(
        f"-I{component_dir / 'src'}"
    )
    # libtropic public headers
    cg.add_build_flag(
        f"-I{component_dir / 'src' / 'libtropic' / 'include'}"
    )
    # libtropic internal headers (needed by .c sources during unity build)
    cg.add_build_flag(
        f"-I{component_dir / 'src' / 'libtropic' / 'src'}"
    )
    # mbedTLS v4 CAL headers
    cg.add_build_flag(
        f"-I{component_dir / 'src' / 'libtropic' / 'cal' / 'mbedtls_v4'}"
    )

    # libtropic configuration (use build flags, not add_define, so C files see them)
    cg.add_build_flag("-DACAB")
    cg.add_build_flag("-DLT_HELPERS")
    cg.add_build_flag("-DLT_LOG_ENABLE_ERROR=1")
    cg.add_build_flag("-DLT_LOG_ENABLE_WARN=0")
    cg.add_build_flag("-DLT_LOG_ENABLE_INFO=1")
    cg.add_build_flag("-DLT_LOG_ENABLE_DEBUG=0")

    # Suppress libtropic's intentional #warning about volatile-secure-memzero
    cg.add_build_flag("-Wno-cpp")
