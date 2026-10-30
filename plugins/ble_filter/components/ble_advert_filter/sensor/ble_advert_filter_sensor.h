#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../ble_advert_filter.h"

#include <array>

namespace esphome::ble_advert_filter {

/// Publishes the filter's counters as per-minute rates, plus the share of
/// adverts dropped and the number of IRKs loaded.
///
/// The counters are free-running uint32 totals. Each update reports the delta
/// since the previous one, scaled by the real elapsed time, so the rate stays
/// right whatever update_interval is and however late the loop ran. Unsigned
/// subtraction stays correct across the counter wrapping.
class BLEAdvertFilterSensor : public PollingComponent, public Parented<BLEAdvertFilter> {
 public:
  enum Counter : uint8_t {
    FORWARDED,
    DROPPED,
    DROPPED_RPA,
    FORWARDED_IRK,
    ALLOWED_SERVICE_UUID,
    DROPPED_FLOOR,
    DROPPED_GATE,
    COUNTER_COUNT,
  };

  void set_rate_sensor(Counter counter, sensor::Sensor *sensor) { this->rates_[counter].sensor = sensor; }
  void set_drop_rate_sensor(sensor::Sensor *sensor) { this->drop_rate_sensor_ = sensor; }
  void set_irk_count_sensor(sensor::Sensor *sensor) { this->irk_count_sensor_ = sensor; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  uint32_t read_(Counter counter) const;

  struct Rate {
    sensor::Sensor *sensor{nullptr};
    uint32_t last{0};
  };
  std::array<Rate, COUNTER_COUNT> rates_{};
  sensor::Sensor *drop_rate_sensor_{nullptr};
  sensor::Sensor *irk_count_sensor_{nullptr};
  // drop_rate keeps its own baselines: it must work without the forwarded /
  // dropped rate sensors being configured.
  uint32_t drop_rate_last_forwarded_{0};
  uint32_t drop_rate_last_dropped_{0};
  uint32_t last_ms_{0};
};

}  // namespace esphome::ble_advert_filter
