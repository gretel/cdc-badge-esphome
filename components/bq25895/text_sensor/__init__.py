import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import CONF_BQ25895_ID, BQ25895Component, bq25895_ns

DEPENDENCIES = ["bq25895"]

CONF_BUS_STATUS = "bus_status"
CONF_CHARGE_STATUS = "charge_status"
CONF_NTC_STATUS = "ntc_status"

BQ25895BusStatusTextSensor = bq25895_ns.class_(
    "BQ25895BusStatusTextSensor", text_sensor.TextSensor
)
BQ25895ChargeStatusTextSensor = bq25895_ns.class_(
    "BQ25895ChargeStatusTextSensor", text_sensor.TextSensor
)
BQ25895NtcStatusTextSensor = bq25895_ns.class_(
    "BQ25895NtcStatusTextSensor", text_sensor.TextSensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25895_ID): cv.use_id(BQ25895Component),
        cv.Optional(CONF_BUS_STATUS): text_sensor.text_sensor_schema(
            BQ25895BusStatusTextSensor
        ),
        cv.Optional(CONF_CHARGE_STATUS): text_sensor.text_sensor_schema(
            BQ25895ChargeStatusTextSensor
        ),
        cv.Optional(CONF_NTC_STATUS): text_sensor.text_sensor_schema(
            BQ25895NtcStatusTextSensor
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25895_ID])

    if bus_status_config := config.get(CONF_BUS_STATUS):
        sens = await text_sensor.new_text_sensor(bus_status_config)
        cg.add(parent.add_listener(sens))

    if charge_status_config := config.get(CONF_CHARGE_STATUS):
        sens = await text_sensor.new_text_sensor(charge_status_config)
        cg.add(parent.add_listener(sens))

    if ntc_status_config := config.get(CONF_NTC_STATUS):
        sens = await text_sensor.new_text_sensor(ntc_status_config)
        cg.add(parent.add_listener(sens))
