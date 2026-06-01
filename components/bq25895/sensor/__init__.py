import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIAMP,
    UNIT_VOLT,
)

from .. import CONF_BQ25895_ID, BQ25895Component, bq25895_ns

DEPENDENCIES = ["bq25895"]

CONF_VBUS_VOLTAGE = "vbus_voltage"
CONF_SYSTEM_VOLTAGE = "system_voltage"
CONF_CHARGE_CURRENT = "charge_current"
CONF_PRECHARGE_CURRENT = "precharge_current"

BQ25895VbusVoltageSensor = bq25895_ns.class_("BQ25895VbusVoltageSensor", sensor.Sensor)
BQ25895BatteryVoltageSensor = bq25895_ns.class_(
    "BQ25895BatteryVoltageSensor", sensor.Sensor
)
BQ25895SystemVoltageSensor = bq25895_ns.class_("BQ25895SystemVoltageSensor", sensor.Sensor)
BQ25895ChargeCurrentSensor = bq25895_ns.class_("BQ25895ChargeCurrentSensor", sensor.Sensor)
BQ25895PrechargeCurrentSensor = bq25895_ns.class_(
    "BQ25895PrechargeCurrentSensor", sensor.Sensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25895_ID): cv.use_id(BQ25895Component),
        cv.Optional(CONF_VBUS_VOLTAGE): sensor.sensor_schema(
            BQ25895VbusVoltageSensor,
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            BQ25895BatteryVoltageSensor,
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SYSTEM_VOLTAGE): sensor.sensor_schema(
            BQ25895SystemVoltageSensor,
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CHARGE_CURRENT): sensor.sensor_schema(
            BQ25895ChargeCurrentSensor,
            unit_of_measurement=UNIT_MILLIAMP,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PRECHARGE_CURRENT): sensor.sensor_schema(
            BQ25895PrechargeCurrentSensor,
            unit_of_measurement=UNIT_MILLIAMP,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25895_ID])

    if vbus_voltage_config := config.get(CONF_VBUS_VOLTAGE):
        sens = await sensor.new_sensor(vbus_voltage_config)
        cg.add(parent.add_listener(sens))

    if battery_voltage_config := config.get(CONF_BATTERY_VOLTAGE):
        sens = await sensor.new_sensor(battery_voltage_config)
        cg.add(parent.add_listener(sens))

    if system_voltage_config := config.get(CONF_SYSTEM_VOLTAGE):
        sens = await sensor.new_sensor(system_voltage_config)
        cg.add(parent.add_listener(sens))

    if charge_current_config := config.get(CONF_CHARGE_CURRENT):
        sens = await sensor.new_sensor(charge_current_config)
        cg.add(parent.add_listener(sens))

    if precharge_current_config := config.get(CONF_PRECHARGE_CURRENT):
        sens = await sensor.new_sensor(precharge_current_config)
        cg.add(parent.add_listener(sens))
