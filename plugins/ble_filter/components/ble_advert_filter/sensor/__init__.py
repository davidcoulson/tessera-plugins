"""Filter counters as Home Assistant sensors, so no lambdas are needed."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from .. import CONF_BLE_ADVERT_FILTER_ID, BLEAdvertFilter, ble_advert_filter_ns

DEPENDENCIES = ["ble_advert_filter"]

BLEAdvertFilterSensor = ble_advert_filter_ns.class_(
    "BLEAdvertFilterSensor", cg.PollingComponent
)
Counter = BLEAdvertFilterSensor.enum("Counter")

UNIT_ADVERTS_PER_MINUTE = "adv/min"
CONF_DROP_RATE = "drop_rate"
CONF_IRK_COUNT = "irk_count"

# Option name -> (C++ counter, icon). Each reports a per-minute rate.
_RATES = {
    "forwarded": ("FORWARDED", "mdi:bluetooth-transfer"),
    "dropped": ("DROPPED", "mdi:bluetooth-off"),
    "dropped_rpa": ("DROPPED_RPA", "mdi:cellphone-remove"),
    "forwarded_irk": ("FORWARDED_IRK", "mdi:cellphone-key"),
    "allowed_service_uuid": ("ALLOWED_SERVICE_UUID", "mdi:key-wireless"),
    "dropped_floor": ("DROPPED_FLOOR", "mdi:signal-off"),
    "dropped_gate": ("DROPPED_GATE", "mdi:filter-remove"),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEAdvertFilterSensor),
        cv.GenerateID(CONF_BLE_ADVERT_FILTER_ID): cv.use_id(BLEAdvertFilter),
        **{
            cv.Optional(key): sensor.sensor_schema(
                unit_of_measurement=UNIT_ADVERTS_PER_MINUTE,
                icon=icon,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            )
            for key, (_, icon) in _RATES.items()
        },
        cv.Optional(CONF_DROP_RATE): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:filter-variant",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_IRK_COUNT): sensor.sensor_schema(
            icon="mdi:key-chain",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_BLE_ADVERT_FILTER_ID])

    for key, (counter, _) in _RATES.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(var.set_rate_sensor(getattr(Counter, counter), sens))
    if CONF_DROP_RATE in config:
        sens = await sensor.new_sensor(config[CONF_DROP_RATE])
        cg.add(var.set_drop_rate_sensor(sens))
    if CONF_IRK_COUNT in config:
        sens = await sensor.new_sensor(config[CONF_IRK_COUNT])
        cg.add(var.set_irk_count_sensor(sens))
