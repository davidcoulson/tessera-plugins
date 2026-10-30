#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include "../ble_advert_filter.h"

namespace esphome::ble_advert_filter {

/// rssi_threshold as a Home Assistant number, so the filter can be tuned per
/// proxy without reflashing.
///
/// With restore_value (the default) the last value set here survives a reboot
/// and REPLACES the YAML rssi_threshold from then on; the YAML value is only the
/// starting point. A restored value outside the current min/max (the range was
/// narrowed since) is clamped into it.
class RSSIThresholdNumber : public number::Number, public Component, public Parented<BLEAdvertFilter> {
 public:
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

  void setup() override;
  void dump_config() override;
  // After the filter itself, which runs at AFTER_BLUETOOTH.
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH - 1.0f; }

 protected:
  void control(float value) override;
  void apply_(float value);

  bool restore_value_{true};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::ble_advert_filter
