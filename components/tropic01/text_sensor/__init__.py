import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import CONF_TROPIC01_ID, Tropic01Component, tropic01_ns

DEPENDENCIES = ["tropic01"]

CONF_CHIP_MODE = "chip_mode"
CONF_FW_RISCV = "fw_version_riscv"
CONF_FW_SPECT = "fw_version_spect"
CONF_CHIP_SERIAL = "chip_serial"

Tropic01ChipModeTextSensor = tropic01_ns.class_(
    "Tropic01ChipModeTextSensor", text_sensor.TextSensor
)
Tropic01FwRiscvTextSensor = tropic01_ns.class_(
    "Tropic01FwRiscvTextSensor", text_sensor.TextSensor
)
Tropic01FwSpectTextSensor = tropic01_ns.class_(
    "Tropic01FwSpectTextSensor", text_sensor.TextSensor
)
Tropic01ChipSerialTextSensor = tropic01_ns.class_(
    "Tropic01ChipSerialTextSensor", text_sensor.TextSensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TROPIC01_ID): cv.use_id(Tropic01Component),
        cv.Optional(CONF_CHIP_MODE): text_sensor.text_sensor_schema(
            Tropic01ChipModeTextSensor,
            icon="mdi:chip",
        ),
        cv.Optional(CONF_FW_RISCV): text_sensor.text_sensor_schema(
            Tropic01FwRiscvTextSensor,
            icon="mdi:counter",
        ),
        cv.Optional(CONF_FW_SPECT): text_sensor.text_sensor_schema(
            Tropic01FwSpectTextSensor,
            icon="mdi:counter",
        ),
        cv.Optional(CONF_CHIP_SERIAL): text_sensor.text_sensor_schema(
            Tropic01ChipSerialTextSensor,
            icon="mdi:serial-port",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TROPIC01_ID])

    if chip_mode_config := config.get(CONF_CHIP_MODE):
        sens = await text_sensor.new_text_sensor(chip_mode_config)
        cg.add(parent.add_listener(sens))

    if fw_riscv_config := config.get(CONF_FW_RISCV):
        sens = await text_sensor.new_text_sensor(fw_riscv_config)
        cg.add(parent.add_listener(sens))

    if fw_spect_config := config.get(CONF_FW_SPECT):
        sens = await text_sensor.new_text_sensor(fw_spect_config)
        cg.add(parent.add_listener(sens))

    if chip_serial_config := config.get(CONF_CHIP_SERIAL):
        sens = await text_sensor.new_text_sensor(chip_serial_config)
        cg.add(parent.add_listener(sens))
