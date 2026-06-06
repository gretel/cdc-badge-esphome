import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import CONF_TROPIC01_ID, Tropic01Component, tropic01_ns

DEPENDENCIES = ["tropic01"]

CONF_ALARM = "alarm"
CONF_MAINTENANCE = "maintenance"

Tropic01AlarmBinarySensor = tropic01_ns.class_(
    "Tropic01AlarmBinarySensor", binary_sensor.BinarySensor
)
Tropic01MaintenanceBinarySensor = tropic01_ns.class_(
    "Tropic01MaintenanceBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TROPIC01_ID): cv.use_id(Tropic01Component),
        cv.Optional(CONF_ALARM): binary_sensor.binary_sensor_schema(
            Tropic01AlarmBinarySensor,
            icon="mdi:alert",
            device_class="tamper",
        ),
        cv.Optional(CONF_MAINTENANCE): binary_sensor.binary_sensor_schema(
            Tropic01MaintenanceBinarySensor,
            icon="mdi:wrench",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TROPIC01_ID])

    if alarm_config := config.get(CONF_ALARM):
        sens = await binary_sensor.new_binary_sensor(alarm_config)
        cg.add(parent.add_listener(sens))

    if maint_config := config.get(CONF_MAINTENANCE):
        sens = await binary_sensor.new_binary_sensor(maint_config)
        cg.add(parent.add_listener(sens))
