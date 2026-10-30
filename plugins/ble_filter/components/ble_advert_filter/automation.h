#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "ble_advert_filter.h"

#include <string>

namespace esphome::ble_advert_filter {

/// ble_advert_filter.set_irks: replace the IRK list from YAML, typically from a
/// Home Assistant entity's on_value. Same rules as BLEAdvertFilter::set_irks():
/// input with no valid key leaves the current list in place.
template<typename... Ts> class SetIrksAction : public Action<Ts...>, public Parented<BLEAdvertFilter> {
 public:
  TEMPLATABLE_VALUE(std::string, irks)

  void play(const Ts &...x) override { this->parent_->set_irks(this->irks_.value(x...)); }
};

/// ble_advert_filter.clear_irks: empty the list on purpose, which turns IRK
/// gating off.
template<typename... Ts> class ClearIrksAction : public Action<Ts...>, public Parented<BLEAdvertFilter> {
 public:
  void play(const Ts &.../*x*/) override { this->parent_->clear_irks(); }
};

}  // namespace esphome::ble_advert_filter
