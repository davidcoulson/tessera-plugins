#include "ble_advert_filter_sensor.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::ble_advert_filter {

static const char *const TAG = "ble_advert_filter.sensor";

uint32_t BLEAdvertFilterSensor::read_(Counter counter) const {
  switch (counter) {
    case FORWARDED:
      return this->parent_->get_adv_forwarded();
    case DROPPED:
      return this->parent_->get_adv_dropped();
    case DROPPED_RPA:
      return this->parent_->get_adv_dropped_rpa();
    case FORWARDED_IRK:
      return this->parent_->get_adv_forwarded_irk();
    case ALLOWED_SERVICE_UUID:
      return this->parent_->get_adv_allowed_service_uuid();
    case DROPPED_FLOOR:
      return this->parent_->get_adv_dropped_floor();
    case DROPPED_GATE:
      return this->parent_->get_adv_dropped_gate();
    default:
      return 0;
  }
}

void BLEAdvertFilterSensor::setup() {
  // Baselines, so the first update reports the rate since boot rather than
  // whatever had accumulated before this component started.
  for (uint8_t i = 0; i < COUNTER_COUNT; i++)
    this->rates_[i].last = this->read_(static_cast<Counter>(i));
  this->drop_rate_last_forwarded_ = this->parent_->get_adv_forwarded();
  this->drop_rate_last_dropped_ = this->parent_->get_adv_dropped();
  this->last_ms_ = millis();
}

void BLEAdvertFilterSensor::update() {
  const uint32_t now = millis();
  const uint32_t elapsed = now - this->last_ms_;
  if (elapsed == 0)
    return;
  this->last_ms_ = now;
  const float per_minute = 60000.0f / static_cast<float>(elapsed);

  for (uint8_t i = 0; i < COUNTER_COUNT; i++) {
    Rate &rate = this->rates_[i];
    if (rate.sensor == nullptr)
      continue;
    const uint32_t current = this->read_(static_cast<Counter>(i));
    const uint32_t delta = current - rate.last;
    rate.last = current;
    rate.sensor->publish_state(static_cast<float>(delta) * per_minute);
  }

  if (this->drop_rate_sensor_ != nullptr) {
    const uint32_t forwarded = this->parent_->get_adv_forwarded();
    const uint32_t dropped = this->parent_->get_adv_dropped();
    const uint32_t df = forwarded - this->drop_rate_last_forwarded_;
    const uint32_t dd = dropped - this->drop_rate_last_dropped_;
    this->drop_rate_last_forwarded_ = forwarded;
    this->drop_rate_last_dropped_ = dropped;
    const uint64_t total = static_cast<uint64_t>(df) + dd;
    // Unknown, not 0%, when nothing was heard: an idle proxy is not a proxy
    // that dropped nothing.
    this->drop_rate_sensor_->publish_state(total == 0 ? NAN
                                                      : 100.0f * static_cast<float>(dd) / static_cast<float>(total));
  }

  if (this->irk_count_sensor_ != nullptr)
    this->irk_count_sensor_->publish_state(static_cast<float>(this->parent_->get_irk_count()));
}

void BLEAdvertFilterSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Advertisement Filter Sensors:");
  LOG_UPDATE_INTERVAL(this);
  // Same order as the Counter enum and the YAML keys.
  static const char *const NAMES[COUNTER_COUNT] = {
      "Forwarded", "Dropped", "Dropped RPA", "Forwarded IRK", "Allowed service UUID", "Dropped floor", "Dropped gate",
  };
  for (uint8_t i = 0; i < COUNTER_COUNT; i++) {
    if (this->rates_[i].sensor != nullptr)
      LOG_SENSOR("  ", NAMES[i], this->rates_[i].sensor);
  }
  LOG_SENSOR("  ", "Drop rate", this->drop_rate_sensor_);
  LOG_SENSOR("  ", "IRK count", this->irk_count_sensor_);
}

}  // namespace esphome::ble_advert_filter
