#include "rssi_threshold_number.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::ble_advert_filter {

static const char *const TAG = "ble_advert_filter.number";

void RSSIThresholdNumber::setup() {
  // Start from what the filter was built with; a value saved from this number
  // on an earlier boot takes over from it.
  float value = this->parent_->get_rssi_threshold();
  if (this->restore_value_) {
    this->pref_ = this->make_entity_preference<float>();
    float saved;
    if (this->pref_.load(&saved) && !std::isnan(saved))
      value = std::clamp(saved, this->traits.get_min_value(), this->traits.get_max_value());
  }
  // Applied on boot as well as on change. Restoring only the displayed value
  // would show the saved threshold in Home Assistant while the filter kept
  // running on the YAML one.
  this->apply_(value);
}

void RSSIThresholdNumber::control(float value) {
  this->apply_(value);
  if (this->restore_value_)
    this->pref_.save(&value);
}

void RSSIThresholdNumber::apply_(float value) {
  this->parent_->set_rssi_threshold(static_cast<int8_t>(std::lround(value)));
  this->publish_state(value);
}

void RSSIThresholdNumber::dump_config() {
  LOG_NUMBER("", "BLE Advertisement Filter RSSI Threshold", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::ble_advert_filter
