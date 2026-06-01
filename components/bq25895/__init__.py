import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@gretel"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_BQ25895_ID = "bq25895_id"
CONF_ENABLE_STATUS_LED = "enable_status_led"
CONF_INPUT_CURRENT_LIMIT = "input_current_limit"
CONF_CHARGE_VOLTAGE = "charge_voltage"
CONF_CHARGE_CURRENT = "charge_current"
CONF_PRECHARGE_CURRENT = "precharge_current"
CONF_CHARGE_ENABLED = "charge_enabled"
CONF_ENABLE_ADC = "enable_adc"

bq25895_ns = cg.esphome_ns.namespace("bq25895")
BQ25895Component = bq25895_ns.class_(
    "BQ25895Component", cg.PollingComponent, i2c.I2CDevice
)
BQ25895Listener = bq25895_ns.class_("BQ25895Listener")

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ25895Component),
            cv.Optional(CONF_ENABLE_STATUS_LED, default=True): cv.boolean,
            cv.Optional(CONF_INPUT_CURRENT_LIMIT, default=500): cv.int_range(
                min=100, max=3200
            ),
            cv.Optional(CONF_CHARGE_VOLTAGE, default=4208): cv.int_range(
                min=3840, max=4608
            ),
            cv.Optional(CONF_CHARGE_CURRENT, default=2048): cv.int_range(
                min=0, max=5056
            ),
            cv.Optional(CONF_PRECHARGE_CURRENT, default=128): cv.int_range(
                min=64, max=1024
            ),
            cv.Optional(CONF_CHARGE_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_ADC, default=True): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x6A))
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_ENABLE_STATUS_LED],
        config[CONF_INPUT_CURRENT_LIMIT],
        config[CONF_CHARGE_VOLTAGE],
        config[CONF_CHARGE_CURRENT],
        config[CONF_PRECHARGE_CURRENT],
        config[CONF_CHARGE_ENABLED],
        config[CONF_ENABLE_ADC],
    )
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
