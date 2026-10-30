#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_advert_filter/ble_advert_filter.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::ble_filter {

// The plugin: a settings page that says what the filter does (its threshold, what it forwarded and dropped since the
// start). The filter is ble_advert_filter's; its number is a Home Assistant entity of plugin.yaml.
class BleFilter : public Component, public tessera::Plugin {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  bool settings(tessera::SettingsPage &page) override;

  void set_filter(ble_advert_filter::BLEAdvertFilter *filter) { filter_ = filter; }

 protected:
  ble_advert_filter::BLEAdvertFilter *filter_{nullptr};
};

}  // namespace esphome::ble_filter
