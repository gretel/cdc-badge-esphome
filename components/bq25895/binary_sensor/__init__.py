import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, DEVICE_CLASS_POWER

from .. import CONF_BQ25895_ID, BQ25895Component, bq25895_ns

DEPENDENCIES = ["bq25895"]

CONF_VBUS_CONNECTED = "vbus_connected"
CONF_CHARGING = "charging"
CONF_CHARGE_DONE = "charge_done"

BQ25895VbusConnectedBinarySensor = bq25895_ns.class_(
    "BQ25895VbusConnectedBinarySensor", binary_sensor.BinarySensor
)
BQ25895ChargingBinarySensor = bq25895_ns.class_(
    "BQ25895ChargingBinarySensor", binary_sensor.BinarySensor
)
BQ25895ChargeDoneBinarySensor = bq25895_ns.class_(
    "BQ25895ChargeDoneBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25895_ID): cv.use_id(BQ25895Component),
        cv.Optional(CONF_VBUS_CONNECTED): binary_sensor.binary_sensor_schema(
            BQ25895VbusConnectedBinarySensor,
            device_class=DEVICE_CLASS_CONNECTIVITY,
        ),
        cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(
            BQ25895ChargingBinarySensor,
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_CHARGE_DONE): binary_sensor.binary_sensor_schema(
            BQ25895ChargeDoneBinarySensor,
            device_class=DEVICE_CLASS_POWER,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25895_ID])

    if vbus_connected_config := config.get(CONF_VBUS_CONNECTED):
        sens = await binary_sensor.new_binary_sensor(vbus_connected_config)
        cg.add(parent.add_listener(sens))

    if charging_config := config.get(CONF_CHARGING):
        sens = await binary_sensor.new_binary_sensor(charging_config)
        cg.add(parent.add_listener(sens))

    if charge_done_config := config.get(CONF_CHARGE_DONE):
        sens = await binary_sensor.new_binary_sensor(charge_done_config)
        cg.add(parent.add_listener(sens))
