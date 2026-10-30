#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/bluetooth_proxy/bluetooth_proxy.h"

#include <array>
#include <string>
#include <vector>

namespace esphome::ble_advert_filter {

/// On-device advertisement filtering for bluetooth_proxy.
///
/// Installs itself into the proxy's AdvertisementFilter slot, so advertisements
/// are dropped before they are queued for the API rather than being shipped
/// across the network for Home Assistant to discard.
///
/// Filters run cheapest-first; see should_forward().
class BLEAdvertFilter : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void set_parent(bluetooth_proxy::BluetoothProxy *parent) { this->parent_ = parent; }

  /// The limit for the DEFAULT category - everything not matched by the MAC
  /// allowlist, an IRK, or an allowlisted service UUID. Also what the irk and
  /// service_uuid categories inherit when their own limit is unset.
  ///
  /// Runtime-tunable: the matching number entity restores on boot and overwrites
  /// whatever the YAML set, so tune there rather than reflashing.
  void set_rssi_threshold(int8_t rssi) {
    this->rssi_threshold_ = rssi;
    this->recompute_gate_();
  }
  int8_t get_rssi_threshold() const { return this->rssi_threshold_; }

  /// Absolute reception floor, applied to EVERY advertisement including ones
  /// mac_allowlist / service_uuid_allowlist would otherwise protect. Where
  /// rssi_threshold answers "is this close enough to be interesting", this
  /// answers "is this reading usable at all" - below it the RSSI is dominated
  /// by noise and a tracker would only be misled by it.
  ///
  /// Runs BEFORE categorisation, which is the whole point: an allowlisted tag
  /// heard at -100 dBm is still dropped. -127 (the default) disables it.
  void set_rssi_floor(int8_t rssi) {
    this->rssi_floor_ = rssi;
    this->recompute_gate_();
  }
  int8_t get_rssi_floor() const { return this->rssi_floor_; }

  /// Per-category RSSI limits. Every advertisement is categorised first (MAC
  /// allowlist / IRK match / allowlisted service UUID / everything else) and
  /// then measured against that category's own limit, so all four are fully
  /// independent - any one can be looser or stricter than any other.
  ///
  /// -127 (the default) means INHERIT, chosen so a config that sets none of
  /// these behaves exactly as it did before they existed:
  ///   mac_allowlist  -> no limit beyond rssi_floor (it has always been a full
  ///                     bypass of rssi_threshold)
  ///   irk            -> rssi_threshold (they were resolved after it ran, so it
  ///   service_uuid      always applied to them)
  ///
  /// The categories want different distances. Beacon tags (mac_allowlist) are
  /// the reason the bypass exists: a weak reading at one proxy is exactly what
  /// places the tag nearer another, so they want the loosest limit. Phones
  /// (irks) are tracked the same way but are far chattier. A device in pairing
  /// mode (service_uuid_allowlist) is in your hand, so it can afford the
  /// strictest limit of the three.
  /// Exempt iBeacon advertisements (Apple company id, subtype 0x02) from
  /// manufacturer_blocklist, the same way allow_homekit exempts HAP. Needed
  /// whenever 0x004C is blocklisted and something you care about advertises an
  /// iBeacon - notably ESPHome proxies beaconing for BLE positioning
  /// self-calibration, which advertise from a public Espressif MAC and so have
  /// no IRK to protect them.
  ///
  /// Two scopes, mirroring the YAML:
  ///   allow_ibeacon: true   -> set_allow_ibeacon(true), every iBeacon exempt
  ///   allow_ibeacon: [...]  -> the major/minor filters added below
  ///
  /// These NARROW the exemption; they never add a drop rule. An iBeacon
  /// matching nothing falls through to the normal manufacturer test, which is
  /// what lets one fleet-wide major exempt your own probes while every other
  /// iBeacon in range stays blocked. Each filter carries its own RSSI limit,
  /// which OVERRIDES both rssi_threshold and rssi_floor for adverts it matches.
  ///
  /// Sentinel for "this rule brought no RSSI limit of its own". Outside the
  /// valid -127..0 range, so it cannot collide with a real setting - unlike
  /// -127, which means the opposite (forward at any strength).
  static constexpr int8_t IBEACON_RSSI_INHERIT = -128;

  void set_allow_ibeacon(bool allow) {
    this->allow_ibeacon_ = allow;
    this->recompute_gate_();
  }
  void set_ibeacon_any_rssi(int8_t rssi) {
    this->ibeacon_any_rssi_ = rssi;
    this->recompute_gate_();
  }
  /// One iBeacon rule. Any of uuid / major / minor may be left out (nullptr /
  /// -1), but not all three, and a minor needs a major. A rule matches when
  /// every field it names matches; when several match, the most specific wins
  /// (minor over major over uuid-only, a uuid-scoped rule over an unscoped one),
  /// so a single probe can be given its own limit inside a fleet that shares one.
  ///
  /// SCOPE RULES BY UUID. major/minor are small integers every vendor starts
  /// counting from 1, so `major: 1` on its own admits anybody's beacon that was
  /// left on its defaults - at this rule's RSSI limit, which usually overrides
  /// the floor. The UUID is the only field that identifies whose beacon it is.
  ///
  /// uuid_hex is 32 hex characters, canonical order (as transmitted). A value
  /// that does not parse drops the rule rather than widening it to "any UUID".
  void add_ibeacon_rule(const char *uuid_hex, int32_t major, int32_t minor, int8_t rssi);
  void add_ibeacon_major(uint16_t major, int8_t rssi) { this->add_ibeacon_rule(nullptr, major, -1, rssi); }
  void add_ibeacon_major_minor(uint16_t major, uint16_t minor, int8_t rssi) {
    this->add_ibeacon_rule(nullptr, major, minor, rssi);
  }
  /// Cheap pre-gate: the loosest RSSI limit any rule in this config could
  /// apply. Anything weaker is dropped before the categoriser runs, so a fleet
  /// that lets its own beacons through at a low RSSI still does not pay an AES
  /// resolve and a payload walk for every distant advert in the neighbourhood.
  ///
  /// Derived, never set: recompute_gate_() runs whenever a limit or a list it
  /// depends on changes, INCLUDING at runtime. It used to be computed once at
  /// codegen, so a number entity lowering rssi_threshold below the compiled
  /// gate silently stopped working - the gate had already dropped the adverts
  /// the new threshold was meant to admit.
  int8_t get_min_rssi_gate() const { return this->min_rssi_gate_; }

  void set_rssi_mac_allowlist(int8_t rssi) {
    this->rssi_mac_allowlist_ = rssi;
    this->recompute_gate_();
  }
  void set_rssi_irk(int8_t rssi) {
    this->rssi_irk_ = rssi;
    this->recompute_gate_();
  }
  void set_rssi_service_uuid(int8_t rssi) {
    this->rssi_service_uuid_ = rssi;
    this->recompute_gate_();
  }

  void set_allow_espressif(bool allow) { this->allow_espressif_ = allow; }
  void set_drop_non_resolvable(bool drop) { this->drop_non_resolvable_ = drop; }
  void set_allow_homekit(bool allow) { this->allow_homekit_ = allow; }
  /// Exempt Apple FindMy (Offline Finding) advertisements from
  /// manufacturer_blocklist: AirTags, AirPods and licensed third-party tags
  /// advertise Apple's company id with subtype 0x12 (or, for AirPods near
  /// their owner, proximity-pairing subtype 0x07 on the same rotated address)
  /// from a random static address, so neither the IRK test nor
  /// drop_non_resolvable sees them and only the Apple blocklist entry stands
  /// in their way. Needed
  /// by a tracker that knows an accessory's pairing keys (Bermuda's FindMy
  /// support) and can therefore follow its address rotation. Every passing
  /// AirTag comes through too, which is why the rule can carry its own RSSI
  /// limit (set_findmy_rssi), resolved exactly like an iBeacon rule's.
  void set_allow_findmy(bool allow) {
    this->allow_findmy_ = allow;
    this->recompute_gate_();
  }
  void set_findmy_rssi(int8_t rssi) {
    this->findmy_rssi_ = rssi;
    this->recompute_gate_();
  }
  /// Replace the IRK list with concatenated 32-hex-char keys (what the codegen passes).
  void set_irks_hex(const char *hex);
  /// Replace the IRK list at runtime - typically from a Home Assistant entity,
  /// so a new phone does not mean reflashing every proxy.
  ///
  /// Every run of EXACTLY 32 hex characters in `text` is taken as a key and
  /// everything else is ignored. That makes the format forgiving on purpose:
  /// commas, newlines, quotes, "label: key" pairs and the braces of a
  /// stringified dict all work, which lets the Home Assistant side keep a name
  /// next to each key. The one rule is that no label may itself contain 32
  /// consecutive hex digits. Duplicates are dropped.
  ///
  /// Returns the number of keys installed. If `text` contains NO valid key the
  /// current list is left untouched and -1 is returned: an entity that is
  /// briefly unavailable (e.g. during a Home Assistant restart) must not be
  /// able to wipe the list, because with a manufacturer blocklist active that
  /// would silently drop our own phones. Use clear_irks() to empty it on purpose.
  ///
  /// Safe to call at any time: advertisements and API state updates are both
  /// dispatched from the main loop, so the list is never swapped mid-lookup.
  /// At most MAX_RUNTIME_IRKS keys are taken; the rest are ignored with a warning.
  static constexpr size_t MAX_RUNTIME_IRKS = 32;
  int set_irks(const std::string &text);
  /// Deliberately empty the IRK list, which turns IRK gating off entirely.
  void clear_irks() {
    this->irks_.clear();
    this->clear_rpa_cache_();
    this->recompute_gate_();
  }
  size_t get_irk_count() const { return this->irks_.size(); }
  /// Verdicts remembered for recently seen RPAs, so a phone advertising several
  /// times a second costs one IRK resolution per address rotation (~15 min)
  /// instead of one per advertisement. Each slot is 8 bytes; allocated on the
  /// zero-filled, and emptied whenever the key list
  /// changes.
  static constexpr size_t RPA_CACHE_SIZE = 64;
  /// IRK resolutions actually computed (cache misses). Each costs one AES
  /// key schedule plus one block per loaded key.
  uint32_t get_irk_resolutions() const { return this->irk_resolutions_; }
  /// The live list, read-only - so a YAML lambda can persist the last good
  /// list to flash and restore it before Home Assistant connects.
  const std::vector<std::array<uint8_t, 16>> &get_irks() const { return this->irks_; }
  void add_blocked_name(const char *needle) { this->name_blocklist_.push_back(needle); }
  void add_blocked_manufacturer(uint16_t company) { this->manufacturer_blocklist_.push_back(company); }
  /// Address that bypasses every filter.
  void add_allowed_mac(uint64_t addr) {
    this->mac_allowlist_.push_back(addr);
    this->recompute_gate_();
  }
  /// Address this proxy ignores entirely, everything else proceeding normally.
  ///
  /// For multi-proxy bonding conflicts: when several proxies are in range of a
  /// device that requires bonding, only one can hold the bond cleanly and the
  /// rest cause connection thrashing. Home Assistant picks a proxy from the ones
  /// reporting the device, so dropping its advertisements here takes this proxy
  /// out of the running without turning it into a single-purpose bridge.
  void add_blocked_mac(uint64_t addr) { this->mac_blocklist_.push_back(addr); }
  /// Turn the allow rules from a bypass into the only way through: an advert is
  /// forwarded only if mac_allowlist, an IRK, service_uuid_allowlist or an
  /// iBeacon / FindMy rule matched it.
  ///
  /// Reproduces the observable behaviour of the ESP-IDF controller whitelist
  /// (esphome/esphome#14353) on platforms that have no such hardware path -
  /// rp2, bk72xx, ln882x. On ESP32 prefer that PR's esp32_ble filter when it
  /// lands: filtering in the controller also saves the CPU and power spent
  /// parsing packets, which a host-side filter like this one cannot.
  void set_allowlist_exclusive(bool exclusive) { this->allowlist_exclusive_ = exclusive; }
  void add_allowed_service_uuid(uint16_t uuid) {
    this->service_uuid_allowlist_.push_back(uuid);
    this->recompute_gate_();
  }
  void add_allowed_service_uuid128(const char *hex);

  uint32_t get_adv_forwarded() const { return this->adv_forwarded_; }
  uint32_t get_adv_dropped() const { return this->adv_dropped_; }
  uint32_t get_adv_dropped_rpa() const { return this->adv_dropped_rpa_; }
  /// Advertisements forwarded because their RPA resolved to one of our IRKs.
  /// The counterpart of get_adv_dropped_rpa(), and the only way to tell a wrong
  /// key from an absent phone: a key that never matches leaves this flat while
  /// its owner's adverts are counted as somebody else's and dropped.
  uint32_t get_adv_forwarded_irk() const { return this->adv_forwarded_irk_; }
  uint32_t get_adv_allowed_service_uuid() const { return this->adv_allowed_service_uuid_; }
  /// Subset of get_adv_dropped(): advertisements discarded because rssi_floor
  /// was the binding limit for their category. It can include otherwise
  /// protected devices, so a rising value means a tracked tag is being cut -
  /// exactly when the floor needs revisiting.
  uint32_t get_adv_dropped_floor() const { return this->adv_dropped_floor_; }
  /// Subset of get_adv_dropped(): rejected by the cheap pre-gate before
  /// categorisation. Distinct from the floor counter - the gate is the loosest
  /// limit in the whole config, so this is "too weak for ANY rule", not "too
  /// weak for the floor".
  uint32_t get_adv_dropped_gate() const { return this->adv_dropped_gate_; }

  /// The predicate itself. False drops the advertisement.
  bool should_forward(const ble_device_base::RawAdvertisement &adv);

 protected:
  // RawAdvertisement::addr_type, as the hubs report it: 0 public, 1 random.
  // ESP-IDF adds 2/3 for identity addresses of RPAs its controller resolved.
  static constexpr uint8_t ADDR_TYPE_RANDOM = 1;
  static bool address_is_rpa_(uint64_t addr, uint8_t addr_type);
  static bool address_is_non_resolvable_(uint64_t addr, uint8_t addr_type);
  static bool is_espressif_oui_(uint64_t addr);
  bool irk_matches_(uint64_t addr) const;
  /// irk_matches_() behind the RPA verdict cache.
  bool irk_matches_cached_(uint64_t addr);
  void clear_rpa_cache_();
  bool payload_blocked_(const uint8_t *data, uint16_t len) const;
  /// True when the advert is an iBeacon accepted by a configured filter;
  /// writes that filter's RSSI limit to limit_out.
  bool ibeacon_match_(const uint8_t *data, uint16_t len, int8_t *limit_out) const;
  bool findmy_match_(const uint8_t *data, uint16_t len) const;
  bool payload_has_allowed_service_uuid_(const uint8_t *data, uint16_t len) const;
  bool uuid128_matches_(const uint8_t *le_bytes) const;
  void recompute_gate_();

  bluetooth_proxy::BluetoothProxy *parent_{nullptr};

  uint32_t adv_forwarded_{0};
  uint32_t adv_dropped_{0};
  uint32_t adv_dropped_rpa_{0};
  uint32_t adv_forwarded_irk_{0};
  uint32_t adv_allowed_service_uuid_{0};
  uint32_t adv_dropped_floor_{0};
  uint32_t adv_dropped_gate_{0};
  uint32_t irk_resolutions_{0};

  std::vector<std::array<uint8_t, 16>> irks_;
  // Each slot is a 48-bit address with the verdict in bit 63; 0 is empty (an
  // RPA always has bit 46 set, so a real entry is never 0). Filled round-robin:
  // the oldest verdict goes first, which for rotating addresses is the one
  // least likely to be heard again.
  std::array<uint64_t, RPA_CACHE_SIZE> rpa_cache_{};
  uint8_t rpa_cache_next_{0};
  std::vector<const char *> name_blocklist_;
  std::vector<uint64_t> mac_allowlist_;
  std::vector<uint64_t> mac_blocklist_;
  std::vector<uint16_t> service_uuid_allowlist_;
  // Kept sorted most-specific-first (see add_ibeacon_rule), so the matcher can
  // stop at the first hit.
  struct IBeaconRule {
    std::array<uint8_t, 16> uuid;
    uint16_t major;
    uint16_t minor;
    int8_t rssi;  // IBEACON_RSSI_INHERIT = no limit of its own
    bool has_uuid;
    bool has_major;
    bool has_minor;
    uint8_t specificity() const {
      return (this->has_minor ? 4 : 0) + (this->has_major ? 2 : 0) + (this->has_uuid ? 1 : 0);
    }
  };
  std::vector<IBeaconRule> ibeacon_rules_;
  std::vector<std::array<uint8_t, 16>> service_uuid128_;
  std::vector<uint16_t> manufacturer_blocklist_;

  int8_t rssi_threshold_{-127};
  // Absolute floor applied ahead of categorisation. -127 disables it.
  int8_t rssi_floor_{-127};
  // Per-category limits; -127 means "inherit", see the setters above.
  int8_t rssi_mac_allowlist_{-127};
  int8_t rssi_irk_{-127};
  int8_t rssi_service_uuid_{-127};
  bool allow_espressif_{true};
  bool drop_non_resolvable_{false};
  bool allow_homekit_{true};
  bool allow_ibeacon_{false};
  int8_t ibeacon_any_rssi_{-127};
  bool allow_findmy_{false};
  int8_t findmy_rssi_{IBEACON_RSSI_INHERIT};
  int8_t min_rssi_gate_{-127};
  bool allowlist_exclusive_{false};
};

}  // namespace esphome::ble_advert_filter
