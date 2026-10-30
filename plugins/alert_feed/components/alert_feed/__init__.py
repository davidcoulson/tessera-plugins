"""Alert feed: the ESPHome side. The tile, the popup and the questions to Home Assistant are in alert_feed.cpp;
register_plugin() takes the id, version, tile memory and the screen's texts from the manifest and translations/ beside this
folder (docs/FIRMWARE_API.md). The only configuration is the switch "Wake on new alert" of plugin.yaml."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display, switch
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

CONF_WAKE = "wake_on_new_alert"

alert_feed_ns = cg.esphome_ns.namespace("alert_feed")
AlertFeed = alert_feed_ns.class_("AlertFeed", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AlertFeed),
        cv.Required(CONF_WAKE): cv.use_id(switch.Switch),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_wake_switch(await cg.get_variable(config[CONF_WAKE])))
