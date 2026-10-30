#pragma once
#include <string>

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::alert_feed {

// One persistent notification, in fixed fields: Home Assistant's answer is cut to 48 bytes a text, and the list is one
// block in PSRAM instead of many small strings in the memory Wi-Fi needs.
struct Item {
  char id[49], title[49], message[49];
  uint32_t created;  // seconds since 1970, 0 when Home Assistant's time could not be read
};

// The plugin: it asks Home Assistant for the notifications (tessera::send, persistent_notification/get), keeps the list
// newest first, and shows the tile and the popup from it. With the switch "Wake on new alert" on, an id it had not seen in
// the previous answer wakes the screen and opens a popup. The first answer after a start is never announced.
class AlertFeed : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_interval(uint32_t now_ms) override;
  void on_message(JsonObjectConst message) override;
  bool settings(tessera::SettingsPage &page) override;

  void set_wake_switch(switch_::Switch *s) { wake_ = s; }

  const tessera::List<Item> &items() const { return items_; }
  // A tile is on the glass right now: ask every 30 seconds instead of every 2 minutes.
  void tile_on_glass();
  void show(const Item &item, bool alert);
  void dismiss(const char *id);
  // "now", "5 min", "3 h", "2 d" for a moment; "" while the clock or the moment is unknown.
  std::string age(uint32_t created, uint32_t epoch) const;

 protected:
  static constexpr size_t KEEP = 16;
  void ask();
  switch_::Switch *wake_{nullptr};
  tessera::List<Item> items_;
  uint32_t asked_{0};          // the number of the question on its way, 0 for none
  uint32_t asked_at_{0}, glass_at_{0}, hold_until_{0};
  bool ever_asked_{false}, primed_{false}, on_glass_{false}, force_{false};
};

}  // namespace esphome::alert_feed
