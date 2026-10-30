#include "ble_filter.h"

#include <string>

namespace esphome::ble_filter {

bool BleFilter::settings(tessera::SettingsPage &page) {
  if (filter_ == nullptr) return false;
  page.icon = "\U000F00AF";  // bluetooth
  ble_advert_filter::BLEAdvertFilter *filter = filter_;
  page.info(text("threshold"), [filter] { return std::to_string(filter->get_rssi_threshold()) + " dBm"; });
  page.info(text("forwarded"), [filter] { return std::to_string(filter->get_adv_forwarded()); });
  page.info(text("dropped"), [filter] { return std::to_string(filter->get_adv_dropped()); });
  return true;
}

}  // namespace esphome::ble_filter
