"""BLE advert filter: the plugin side of ble_advert_filter (components/ble_advert_filter, David Coulson's component,
v2.0.4, vendored because a plugin cannot add external components). It shows the filter's threshold and counters on the
screen's settings page. The filter itself and its number are configured in plugin.yaml; register_plugin() takes the id,
version and the screen's texts from the manifest and translations/."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_advert_filter, smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display", "ble_advert_filter"]

CONF_FILTER = "filter"

ble_filter_ns = cg.esphome_ns.namespace("ble_filter")
BleFilter = ble_filter_ns.class_("BleFilter", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BleFilter),
        cv.Required(CONF_FILTER): cv.use_id(ble_advert_filter.BLEAdvertFilter),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_filter(await cg.get_variable(config[CONF_FILTER])))
