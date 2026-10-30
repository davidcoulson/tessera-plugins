#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::multi_graph {

class GraphTile;

// The plugin: registers the tile, and hands the app's answers (on_message) to the tiles that asked.
class MultiGraph : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_message(JsonObjectConst message) override;
  void remember(GraphTile *tile);
  void forget(GraphTile *tile);

 private:
  GraphTile *tiles_[16]{};  // the tiles on the glass; a layout has few of these
};

}  // namespace esphome::multi_graph
