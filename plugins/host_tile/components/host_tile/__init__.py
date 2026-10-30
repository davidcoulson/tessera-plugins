"""Host summary: the ESPHome side. register_plugin() reads the id, version, tile memory and the screen's texts from the
manifest and translations/ beside this folder."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

host_tile_ns = cg.esphome_ns.namespace("host_tile")
HostTile = host_tile_ns.class_("HostTile", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(HostTile)}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
