#include "ble_advert_filter.h"

#include "esphome/components/ble_device_base/ble_aes_ccm.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstring>

namespace esphome::ble_advert_filter {

static const char *const TAG = "ble_advert_filter";

static const uint32_t ESPRESSIF_OUIS[] = {
    0x004B12, 0x007007, 0x00FB4A, 0x048308, 0x04B247, 0x083A8D, 0x083AF2, 0x089272, 0x08A6F7, 0x08AD0A, 0x08B61F,
    0x08D1F9, 0x08F9E0, 0x0C4EA0, 0x0C8B95, 0x0CB815, 0x0CDC7E, 0x10003B, 0x10061C, 0x1020BA, 0x1051DB, 0x10521C,
    0x1091A8, 0x1097BD, 0x10B41D, 0x10BDA3, 0x140808, 0x142B2F, 0x14335C, 0x146393, 0x14C19F, 0x188B0E, 0x18FE34,
    0x1C2904, 0x1C6920, 0x1C8B84, 0x1C8F57, 0x1C9DC2, 0x1CC3AB, 0x1CDBD4, 0x1CE4CB, 0x202565, 0x2043A8, 0x20500D,
    0x206EF1, 0x209BA9, 0x20D5C2, 0x20E7C8, 0x240AC4, 0x244CAB, 0x24587C, 0x2462AB, 0x246F28, 0x24A160, 0x24B2DE,
    0x24D7EB, 0x24DCC3, 0x24EC4A, 0x2805A5, 0x28372F, 0x28562F, 0x288485, 0x2C3AE8, 0x2CBCBB, 0x2CF432, 0x3030F9,
    0x3076F5, 0x308398, 0x30AEA4, 0x30C6F7, 0x30C922, 0x30EDA0, 0x345F45, 0x348518, 0x34865D, 0x349454, 0x34987A,
    0x34AB95, 0x34B472, 0x34B7DA, 0x34CDB0, 0x38182B, 0x383E51, 0x3844BE, 0x3C0D0D, 0x3C0F02, 0x3C6105, 0x3C71BF,
    0x3C8427, 0x3C8A1F, 0x3CDC75, 0x3CE90E, 0x4022D8, 0x404CCA, 0x409151, 0x40F520, 0x441793, 0x441BF6, 0x441D64,
    0x447B30, 0x44B176, 0x44BD8D, 0x4827E2, 0x4831B7, 0x483FDA, 0x485519, 0x489D31, 0x48AFF3, 0x48CA43, 0x48E729,
    0x48F6EE, 0x4C11AE, 0x4C7525, 0x4CC382, 0x4CEBD6, 0x500291, 0x50787D, 0x543204, 0x5443B2, 0x545AA6, 0x549DEA,
    0x582ABD, 0x588C81, 0x58BF25, 0x58CF79, 0x58E6C5, 0x5C013B, 0x5CCF7F, 0x600194, 0x6055F9, 0x648914, 0x64B708,
    0x64E833, 0x680947, 0x6825DD, 0x686725, 0x689DD2, 0x68B6B3, 0x68C63A, 0x68EE8F, 0x68FE71, 0x6C3DD8, 0x6CB456,
    0x6CC840, 0x70039F, 0x70041D, 0x704BCA, 0x70AF09, 0x70B8F6, 0x744DBD, 0x781C3C, 0x782184, 0x78421C, 0x78E36D,
    0x78EE4C, 0x7C0C5F, 0x7C2C67, 0x7C4FAD, 0x7C7398, 0x7C87CE, 0x7C9EBD, 0x7CD544, 0x7CDFA1, 0x7CE8B1, 0x80456B,
    0x8053E0, 0x80646F, 0x806599, 0x807D3A, 0x80B54E, 0x80F1B2, 0x80F3DA, 0x840D8E, 0x841FE8, 0x84C7BB, 0x84CCA8,
    0x84F3EB, 0x84F703, 0x84FCE6, 0x8813BF, 0x8856A6, 0x885721, 0x88F155, 0x8C4B14, 0x8C4F00, 0x8C8C29, 0x8C94DF,
    0x8CAAB5, 0x8CBFEA, 0x8CCE4E, 0x8CFD49, 0x901506, 0x90380C, 0x90649B, 0x907069, 0x9097D5, 0x90B339, 0x90DA72,
    0x90E5B1, 0x943CC6, 0x9451DC, 0x9454C5, 0x94A990, 0x94B555, 0x94B97E, 0x94E686, 0x983DAE, 0x9888E0, 0x98A316,
    0x98C377, 0x98CDAC, 0x98F4AB, 0x9C139E, 0x9C96D5, 0x9C9C1F, 0x9C9E6E, 0x9CCC01, 0xA020A6, 0xA0764E, 0xA085E3,
    0xA0A3B3, 0xA0B765, 0xA0DD6C, 0xA0F262, 0xA47B9D, 0xA4CB8F, 0xA4CF12, 0xA4E57C, 0xA4F00F, 0xA8032A, 0xA842E3,
    0xA84674, 0xA848FA, 0xA8FD07, 0xAC0BFB, 0xAC1518, 0xAC276E, 0xAC67B2, 0xACA704, 0xACD074, 0xACEBE6, 0xB03FD3,
    0xB08184, 0xB0A604, 0xB0A732, 0xB0B21C, 0xB0CBD8, 0xB43A45, 0xB48A0A, 0xB4A64A, 0xB4BFE9, 0xB4E62D, 0xB81F3F,
    0xB87B4D, 0xB8BB11, 0xB8D61A, 0xB8F009, 0xB8F862, 0xBCDDC2, 0xBCFF4D, 0xC049EF, 0xC04E30, 0xC05D89, 0xC0CDD6,
    0xC44F33, 0xC45BBE, 0xC49E7E, 0xC4D8D5, 0xC4DD57, 0xC4DEE2, 0xC82B96, 0xC82E18, 0xC88541, 0xC88A7B, 0xC8C9A3,
    0xC8DA29, 0xC8F09E, 0xCC50E3, 0xCC68C7, 0xCC7B5C, 0xCC7E1F, 0xCC8DA2, 0xCCBA97, 0xCCDBA7, 0xD09AAF, 0xD0CF13,
    0xD0EF76, 0xD40592, 0xD48AFC, 0xD48C49, 0xD4D4DA, 0xD4E9F4, 0xD4F98D, 0xD8132A, 0xD83BDA, 0xD885AC, 0xD8A01D,
    0xD8BC38, 0xD8BFC0, 0xD8F15B, 0xDC0675, 0xDC0A69, 0xDC1ED5, 0xDC4F22, 0xDC5475, 0xDC55B1, 0xDCB4D9, 0xDCDA0C,
    0xE05A1B, 0xE072A1, 0xE08CFE, 0xE09806, 0xE0E2E6, 0xE465B8, 0xE4B063, 0xE4B323, 0xE80690, 0xE831CD, 0xE83DC1,
    0xE868E7, 0xE86BEA, 0xE89F6D, 0xE8DB84, 0xE8F60A, 0xEC6260, 0xEC64C9, 0xEC94CB, 0xECC9FF, 0xECD61B, 0xECDA3B,
    0xECE334, 0xECFABC, 0xF008D1, 0xF0161D, 0xF024F9, 0xF09E9E, 0xF0F5BD, 0xF412FA, 0xF42DC9, 0xF4650B, 0xF4CFA2,
    0xF843EE, 0xF85B1B, 0xF8B3B7, 0xFC012C, 0xFCB467, 0xFCE8C0, 0xFCF5C4,
};
static constexpr size_t ESPRESSIF_OUI_COUNT = sizeof(ESPRESSIF_OUIS) / sizeof(ESPRESSIF_OUIS[0]);

bool BLEAdvertFilter::is_espressif_oui_(uint64_t addr) {
  const uint32_t oui = static_cast<uint32_t>((addr >> 24) & 0xFFFFFF);
  size_t lo = 0, hi = ESPRESSIF_OUI_COUNT;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    if (ESPRESSIF_OUIS[mid] < oui) {
      lo = mid + 1;
    } else if (ESPRESSIF_OUIS[mid] > oui) {
      hi = mid;
    } else {
      return true;
    }
  }
  return false;
}

bool BLEAdvertFilter::findmy_match_(const uint8_t *data, uint16_t len) const {
  // Apple Offline Finding: manufacturer data, company 0x004C, subtype 0x12.
  // The rest of the payload (status byte, 22 bytes of public key) carries no
  // identity a proxy could act on - the address rotation is only resolvable
  // with the accessory's keys, which live in the tracker, not here.
  //
  // Subtype 0x07 is AirPods proximity pairing - the advert that raises the
  // AirPods card on a nearby iPhone. AirPods near their owner send that
  // instead of 0x12, but from the SAME FindMy-rotated address, so a tracker
  // holding the keys resolves it just as well. Without this an owned AirPods
  // case is heard only by receivers that do not run the Apple blocklist.
  uint16_t i = 0;
  while (i < len) {
    const uint8_t field_len = data[i];
    if (field_len == 0) {
      // Padding, not the end: ESPHome passes the advert and its scan response
      // as one buffer, so stopping here would skip the whole scan response.
      i++;
      continue;
    }
    if (static_cast<uint32_t>(i) + 1u + field_len > len)
      break;
    if (data[i + 1] == 0xFF && field_len >= 4) {
      const uint16_t company = static_cast<uint16_t>(data[i + 2]) | (static_cast<uint16_t>(data[i + 3]) << 8);
      // Apple continuity messages are TLV: subtype, length L, L bytes. A frame
      // that is cut short (or declares less than 2 bytes) is not an advert a
      // tracker could have sent.
      if (company == 0x004C && (data[i + 4] == 0x12 || data[i + 4] == 0x07) && field_len >= 6 &&
          data[i + 5] >= 2 && field_len >= 5u + data[i + 5])
        return true;
    }
    i += field_len + 1;
  }
  return false;
}

void BLEAdvertFilter::add_ibeacon_rule(const char *uuid_hex, int32_t major, int32_t minor, int8_t rssi) {
  IBeaconRule rule{};
  rule.rssi = rssi;
  if (uuid_hex != nullptr) {
    // Fail closed: a UUID that does not parse must not become "any UUID".
    if (strlen(uuid_hex) != 32 || parse_hex(uuid_hex, 32, rule.uuid.data(), 16) != 32) {
      ESP_LOGE(TAG, "iBeacon rule dropped: uuid is not 32 hex characters");
      return;
    }
    rule.has_uuid = true;
  }
  if (major >= 0) {
    rule.has_major = true;
    rule.major = static_cast<uint16_t>(major);
  }
  if (minor >= 0 && rule.has_major) {
    rule.has_minor = true;
    rule.minor = static_cast<uint16_t>(minor);
  }
  if (rule.specificity() == 0)
    return;  // names nothing - that is allow_ibeacon: true, not a rule
  // Insert before the first strictly less specific rule: most specific first,
  // config order preserved among equals.
  auto pos = this->ibeacon_rules_.begin();
  while (pos != this->ibeacon_rules_.end() && pos->specificity() >= rule.specificity())
    ++pos;
  this->ibeacon_rules_.insert(pos, rule);
  this->recompute_gate_();
}

bool BLEAdvertFilter::ibeacon_match_(const uint8_t *data, uint16_t len, int8_t *limit_out) const {
  // Walks the AD structures looking for an iBeacon (Apple company id, subtype
  // 0x02) that one of the configured filters accepts. Writes that filter's RSSI
  // limit to limit_out (-127 = no limit) and returns true.
  //
  // Layout, indices relative to the length byte at data[i]:
  //   i+1  0xFF          i+2..3  company (LE)   i+4  subtype 0x02
  //   i+5  0x15          i+6..21 UUID (16)      i+22..23 major (BE)
  //   i+24..25 minor (BE)                       i+26 measured power
  // field_len covers type+payload, so a complete iBeacon is 26.
  uint16_t i = 0;
  while (i < len) {
    const uint8_t field_len = data[i];
    if (field_len == 0) {
      // Padding, not the end: ESPHome passes the advert and its scan response
      // as one buffer, so stopping here would skip the whole scan response.
      i++;
      continue;
    }
    if (static_cast<uint32_t>(i) + 1u + field_len > len)
      break;
    if (data[i + 1] == 0xFF && field_len >= 4) {
      const uint16_t company = static_cast<uint16_t>(data[i + 2]) | (static_cast<uint16_t>(data[i + 3]) << 8);
      // Only a complete iBeacon (subtype 0x02, length 0x15) counts: a truncated
      // or malformed one would otherwise buy Apple data the blocklist exemption.
      // A later one in the same payload (e.g. in the scan response) still can.
      if (company == 0x004C && field_len >= 26 && data[i + 4] == 0x02 && data[i + 5] == 0x15) {
        if (this->allow_ibeacon_) {
          *limit_out = this->ibeacon_any_rssi_;
          return true;
        }
        const uint8_t *uuid = &data[i + 6];
        const uint16_t major = (static_cast<uint16_t>(data[i + 22]) << 8) | data[i + 23];
        const uint16_t minor = (static_cast<uint16_t>(data[i + 24]) << 8) | data[i + 25];
        // Sorted most-specific-first, so the first hit is the one that wins.
        for (const auto &rule : this->ibeacon_rules_) {
          if (rule.has_minor && rule.minor != minor)
            continue;
          if (rule.has_major && rule.major != major)
            continue;
          if (rule.has_uuid && memcmp(uuid, rule.uuid.data(), 16) != 0)
            continue;
          *limit_out = rule.rssi;
          return true;
        }
        // No rule for this one; keep looking for another iBeacon.
      }
    }
    i += field_len + 1;
  }
  return false;
}

bool BLEAdvertFilter::payload_blocked_(const uint8_t *data, uint16_t len) const {
  // AD structures are [length][type][payload...], length covering type+payload.
  // One pass checks both the local name and the manufacturer id so a dropped
  // advertisement is never walked twice.
  uint16_t i = 0;
  while (i < len) {
    const uint8_t field_len = data[i];
    if (field_len == 0) {
      // Padding, not the end: ESPHome passes the advert and its scan response
      // as one buffer, so stopping here would skip the whole scan response.
      i++;
      continue;
    }
    // Reject a structure that claims to run past the buffer (truncated packet).
    if (static_cast<uint32_t>(i) + 1u + field_len > len)
      break;
    const uint8_t type = data[i + 1];
    // 0xFF manufacturer specific data: first two payload bytes are the
    // Bluetooth SIG company identifier, little-endian.
    if (type == 0xFF && field_len >= 3) {
      const uint16_t company = static_cast<uint16_t>(data[i + 2]) | (static_cast<uint16_t>(data[i + 3]) << 8);
      // HomeKit accessories advertise under Apple's company id with subtype
      // 0x06. Blocklisting Apple to kill phone/AirPods/AirTag noise would take
      // them with it, and they carry no IRK to rescue them, so exempt HAP
      // explicitly rather than forcing users to choose between the two.
      const bool is_hap = this->allow_homekit_ && company == 0x004C && field_len >= 4 && data[i + 4] == 0x06;
      if (!is_hap) {
        for (const uint16_t blocked : this->manufacturer_blocklist_) {
          if (blocked == company)
            return true;
        }
      }
    }
    // 0x09 complete local name, 0x08 shortened local name.
    if ((type == 0x09 || type == 0x08) && field_len > 1) {
      const char *name = reinterpret_cast<const char *>(&data[i + 2]);
      const uint8_t name_len = field_len - 1;
      for (const char *needle : this->name_blocklist_) {
        const size_t needle_len = strlen(needle);
        if (needle_len == 0 || needle_len > name_len)
          continue;
        // Case-insensitive substring search; the name is not NUL-terminated,
        // so this walks it by length rather than using strstr().
        for (uint8_t start = 0; start + needle_len <= name_len; start++) {
          size_t k = 0;
          while (k < needle_len && static_cast<char>(tolower(static_cast<unsigned char>(name[start + k]))) == needle[k])
            k++;
          if (k == needle_len)
            return true;
        }
      }
    }
    i += field_len + 1;
  }
  return false;
}

// The Bluetooth Base UUID, big-endian, with the 16-bit slot (bytes 2-3) zeroed.
// A SIG-allocated short UUID advertised in 128-bit form is this with those two
// bytes filled in, so a device using the long form of 0xFFF6 still matches a
// 16-bit allowlist entry.
static const uint8_t BT_BASE_UUID[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                         0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};

bool BLEAdvertFilter::uuid128_matches_(const uint8_t *le_bytes) const {
  // Advertisements carry 128-bit UUIDs little-endian; flip to canonical order
  // once, then compare.
  uint8_t be[16];
  for (uint8_t k = 0; k < 16; k++)
    be[k] = le_bytes[15 - k];

  for (const auto &allowed : this->service_uuid128_) {
    if (memcmp(be, allowed.data(), 16) == 0)
      return true;
  }

  // Long form of a SIG short UUID: everything but bytes 2-3 matches the base.
  if (!this->service_uuid_allowlist_.empty() && memcmp(be, BT_BASE_UUID, 2) == 0 &&
      memcmp(be + 4, BT_BASE_UUID + 4, 12) == 0) {
    const uint16_t shortened = static_cast<uint16_t>(be[3]) | (static_cast<uint16_t>(be[2]) << 8);
    for (const uint16_t allowed : this->service_uuid_allowlist_) {
      if (allowed == shortened)
        return true;
    }
  }
  return false;
}

bool BLEAdvertFilter::payload_has_allowed_service_uuid_(const uint8_t *data, uint16_t len) const {
  // Same [length][type][payload...] walk as payload_blocked_. A service UUID can
  // appear in several places and a device in pairing mode does not consistently
  // use one, so every form is checked:
  //   0x02/0x03 incomplete/complete 16-bit UUID list   (n * 2 bytes)
  //   0x14      16-bit solicitation list               (n * 2 bytes)
  //   0x16      service data, 16-bit UUID              (2 bytes + data)
  //   0x06/0x07 incomplete/complete 128-bit UUID list  (n * 16 bytes)
  //   0x15      128-bit solicitation list              (n * 16 bytes)
  //   0x21      service data, 128-bit UUID             (16 bytes + data)
  const bool have16 = !this->service_uuid_allowlist_.empty();
  const bool have128 = !this->service_uuid128_.empty();
  uint16_t i = 0;
  while (i < len) {
    const uint8_t field_len = data[i];
    if (field_len == 0) {
      // Padding, not the end: ESPHome passes the advert and its scan response
      // as one buffer, so stopping here would skip the whole scan response.
      i++;
      continue;
    }
    // Reject a structure that claims to run past the buffer (truncated packet).
    if (static_cast<uint32_t>(i) + 1u + field_len > len)
      break;
    const uint8_t type = data[i + 1];
    // Payload is the field minus its type byte. The list types carry a packed
    // array; the service-data types carry exactly one UUID then opaque bytes,
    // so those stop after the first entry.
    const uint8_t payload_len = field_len - 1;

    if (have16 && (type == 0x02 || type == 0x03 || type == 0x14 || type == 0x16)) {
      const uint8_t entries = payload_len / 2;
      const uint8_t limit = (type == 0x16) ? (entries > 0 ? 1 : 0) : entries;
      for (uint8_t p = 0; p < limit; p++) {
        const uint16_t uuid =
            static_cast<uint16_t>(data[i + 2 + p * 2]) | (static_cast<uint16_t>(data[i + 3 + p * 2]) << 8);
        for (const uint16_t allowed : this->service_uuid_allowlist_) {
          if (allowed == uuid)
            return true;
        }
      }
    }

    // The long form can match a 16-bit entry via the base UUID, so this arm runs
    // whenever either list is configured.
    if ((have128 || have16) && (type == 0x06 || type == 0x07 || type == 0x15 || type == 0x21)) {
      const uint8_t entries = payload_len / 16;
      const uint8_t limit = (type == 0x21) ? (entries > 0 ? 1 : 0) : entries;
      for (uint8_t p = 0; p < limit; p++) {
        if (this->uuid128_matches_(&data[i + 2 + p * 16]))
          return true;
      }
    }

    i += field_len + 1;
  }
  return false;
}

bool BLEAdvertFilter::address_is_rpa_(uint64_t addr, uint8_t addr_type) {
  // Only a RANDOM address (type 1) has the sub-type bits this tests. Type 0 is
  // public. Types 2 and 3 are what the ESP-IDF controller reports once it has
  // resolved an RPA itself - i.e. for a device this proxy is bonded to - and
  // carry the IDENTITY address: for type 2 that is a public address, whose
  // leading OUI byte means nothing here. Treating "not 0" as random dropped a
  // bonded device with a 40:-7F: OUI as an unresolved RPA.
  if (addr_type != ADDR_TYPE_RANDOM)
    return false;
  return (((addr >> 40) & 0xC0) == 0x40);
}

bool BLEAdvertFilter::address_is_non_resolvable_(uint64_t addr, uint8_t addr_type) {
  // Same guard as address_is_rpa_, and here it bites harder: 00:-3F: covers a
  // large share of real OUIs, so a bonded device reported by its public
  // identity address (type 2) would be discarded by drop_non_resolvable.
  if (addr_type != ADDR_TYPE_RANDOM)
    return false;
  return (((addr >> 40) & 0xC0) == 0x00);
}

int BLEAdvertFilter::set_irks(const std::string &text) {
  // Parse into a scratch vector first so a bad input never leaves the live
  // list half-replaced.
  std::vector<std::array<uint8_t, 16>> parsed;
  const size_t n = text.size();
  size_t i = 0;
  while (i < n) {
    if (!isxdigit(static_cast<unsigned char>(text[i]))) {
      i++;
      continue;
    }
    size_t j = i;
    while (j < n && isxdigit(static_cast<unsigned char>(text[j])))
      j++;
    // Exactly 32: a longer run is not a key with a suffix, it is something
    // else entirely (a hash, a UUID without dashes) and must not be truncated
    // into one.
    if (j - i == 32) {
      std::array<uint8_t, 16> irk{};
      if (parse_hex(text.c_str() + i, 32, irk.data(), 16) == 32 &&
          std::find(parsed.begin(), parsed.end(), irk) == parsed.end()) {
        // Every key costs an AES block per unresolved advertisement, and the
        // text arrives over the API from outside the device. Bound both.
        if (parsed.size() >= MAX_RUNTIME_IRKS) {
          ESP_LOGW(TAG, "set_irks: more than %u keys, ignoring the rest", static_cast<unsigned>(MAX_RUNTIME_IRKS));
          break;
        }
        parsed.push_back(irk);
      }
    }
    i = j;
  }
  if (parsed.empty()) {
    ESP_LOGW(TAG, "set_irks: no 32-hex-character keys found, keeping the current %u",
             static_cast<unsigned>(this->irks_.size()));
    return -1;
  }
  this->irks_ = std::move(parsed);
  this->clear_rpa_cache_();
  this->recompute_gate_();
  ESP_LOGI(TAG, "Loaded %u IRK(s) at runtime", static_cast<unsigned>(this->irks_.size()));
  return static_cast<int>(this->irks_.size());
}

void BLEAdvertFilter::recompute_gate_() {
  // The loosest limit any REACHABLE category could apply. A category nothing
  // can land in contributes nothing: with no mac_allowlist there is no CAT_MAC
  // advert for the gate to protect, so its (possibly unbounded) limit must not
  // hold the gate open.
  //
  // -127 on a category key means "inherit", not "forward everything", so each
  // category resolves to its effective bound first. A category whose effective
  // bound really is -127 (e.g. a MAC allowlist with no floor) disables the gate.
  auto bound = [](int8_t explicit_limit, int8_t fallback) -> int8_t {
    return explicit_limit != -127 ? explicit_limit : fallback;
  };
  // Every limit below is floor-bounded at the point of use unless the rule
  // brought its own, so the floor bounds them here too.
  auto floored = [this](int8_t limit) -> int8_t {
    if (this->rssi_floor_ == -127)
      return limit;
    return (limit == -127 || this->rssi_floor_ > limit) ? this->rssi_floor_ : limit;
  };

  int8_t gate = floored(this->rssi_threshold_);  // DEFAULT, and every inheriting rule
  auto loosen = [&gate](int8_t limit) {
    if (gate != -127 && (limit == -127 || limit < gate))
      gate = limit;
  };
  if (!this->mac_allowlist_.empty())
    loosen(floored(this->rssi_mac_allowlist_));
  if (!this->irks_.empty())
    loosen(floored(bound(this->rssi_irk_, this->rssi_threshold_)));
  if (!this->service_uuid_allowlist_.empty() || !this->service_uuid128_.empty())
    loosen(floored(bound(this->rssi_service_uuid_, this->rssi_threshold_)));
  // A rule with its own RSSI overrides the floor as well, so it is NOT floored.
  if (this->allow_ibeacon_ && this->ibeacon_any_rssi_ != IBEACON_RSSI_INHERIT)
    loosen(this->ibeacon_any_rssi_);
  for (const auto &rule : this->ibeacon_rules_) {
    if (rule.rssi != IBEACON_RSSI_INHERIT)
      loosen(rule.rssi);
  }
  if (this->allow_findmy_ && this->findmy_rssi_ != IBEACON_RSSI_INHERIT)
    loosen(this->findmy_rssi_);
  this->min_rssi_gate_ = gate;
}

bool BLEAdvertFilter::irk_matches_(uint64_t addr) const {
  // Bluetooth Core "ah": hash = e(IRK, 0-padding | prand)[low 24 bits], where
  // the RPA is prand (top 3 bytes) | hash (bottom 3 bytes).
  uint8_t plaintext[16] = {0};
  uint8_t ciphertext[16];
  plaintext[13] = (addr >> 40) & 0xff;
  plaintext[14] = (addr >> 32) & 0xff;
  plaintext[15] = (addr >> 24) & 0xff;
  for (const auto &irk : this->irks_) {
    ble_device_base::aes128_encrypt_block(irk.data(), plaintext, ciphertext);
    if (ciphertext[15] == (addr & 0xff) && ciphertext[14] == ((addr >> 8) & 0xff) &&
        ciphertext[13] == ((addr >> 16) & 0xff))
      return true;
  }
  return false;
}

bool BLEAdvertFilter::irk_matches_cached_(uint64_t addr) {
  // An RPA is stable for its rotation period (about 15 minutes on phones) and
  // is advertised several times a second meanwhile, so nearly every lookup is a
  // repeat. Without this, each repeat from somebody else's phone re-runs one
  // AES key schedule and block per loaded key - the whole cost of IRK gating.
  constexpr uint64_t ADDR_MASK = 0xFFFFFFFFFFFFULL;
  constexpr uint64_t OURS = 1ULL << 63;
  for (const uint64_t entry : this->rpa_cache_) {
    if (entry != 0 && (entry & ADDR_MASK) == addr)
      return (entry & OURS) != 0;
  }
  this->irk_resolutions_++;
  const bool ours = this->irk_matches_(addr);
  this->rpa_cache_[this->rpa_cache_next_] = (addr & ADDR_MASK) | (ours ? OURS : 0);
  this->rpa_cache_next_ = static_cast<uint8_t>((this->rpa_cache_next_ + 1) % RPA_CACHE_SIZE);
  return ours;
}

void BLEAdvertFilter::clear_rpa_cache_() {
  // Every verdict was computed against the old key list, so all of them go -
  // a cached "not ours" would otherwise keep hiding a phone whose key was
  // just added.
  std::fill(this->rpa_cache_.begin(), this->rpa_cache_.end(), 0);
  this->rpa_cache_next_ = 0;
}

void BLEAdvertFilter::set_irks_hex(const char *hex) {
  // Concatenated 32-hex-char keys, unpacked once so the advertisement path only
  // ever touches the parsed vector.
  this->irks_.clear();
  const size_t len = strlen(hex);
  this->irks_.reserve(len / 32);
  for (size_t i = 0; i + 32 <= len; i += 32) {
    std::array<uint8_t, 16> irk{};
    if (parse_hex(hex + i, 32, irk.data(), 16) == 32)
      this->irks_.push_back(irk);
  }
  this->clear_rpa_cache_();
  this->recompute_gate_();
}

void BLEAdvertFilter::add_allowed_service_uuid128(const char *hex) {
  std::array<uint8_t, 16> uuid{};
  if (strlen(hex) != 32 || parse_hex(hex, 32, uuid.data(), 16) != 32)
    return;
  // 0000xxxx-0000-1000-8000-00805f9b34fb is the 16-bit UUID xxxx written
  // out in full. Devices advertise the short form, so match that too -
  // the reverse (short configured, long advertised) already works in
  // uuid128_matches_().
  if (memcmp(uuid.data(), BT_BASE_UUID, 2) == 0 && memcmp(uuid.data() + 4, BT_BASE_UUID + 4, 12) == 0) {
    this->service_uuid_allowlist_.push_back(static_cast<uint16_t>((uuid[2] << 8) | uuid[3]));
  }
  this->service_uuid128_.push_back(uuid);
  this->recompute_gate_();
}

void BLEAdvertFilter::setup() {
  this->recompute_gate_();

  this->parent_->set_advertisement_filter({this, [](void *self, const ble_device_base::RawAdvertisement &adv) {
                                             return static_cast<BLEAdvertFilter *>(self)->should_forward(adv);
                                           }});
}

void BLEAdvertFilter::dump_config() {
  // -127 means "off" for the threshold and floor and "inherit" for the
  // per-category limits; print that rather than a number nobody configured.
  auto log_limit = [](const char *label, int8_t limit, const char *unset) {
    if (limit == -127) {
      ESP_LOGCONFIG(TAG, "  %s: %s", label, unset);
    } else {
      ESP_LOGCONFIG(TAG, "  %s: %d dBm", label, limit);
    }
  };
  ESP_LOGCONFIG(TAG, "BLE Advertisement Filter:");
  log_limit("RSSI threshold", this->rssi_threshold_, "off");
  log_limit("RSSI floor", this->rssi_floor_, "off");
  // Per-category limits only for categories something can land in, so the log
  // does not suggest a feature is in use when it is not.
  if (!this->mac_allowlist_.empty())
    log_limit("RSSI limit, mac_allowlist", this->rssi_mac_allowlist_, "floor only");
  if (!this->irks_.empty())
    log_limit("RSSI limit, irk", this->rssi_irk_, "inherit threshold");
  if (!this->service_uuid_allowlist_.empty() || !this->service_uuid128_.empty())
    log_limit("RSSI limit, service_uuid", this->rssi_service_uuid_, "inherit threshold");
  if (this->min_rssi_gate_ != -127)
    ESP_LOGCONFIG(TAG, "  Pre-gate drops anything below %d dBm", this->min_rssi_gate_);
  ESP_LOGCONFIG(TAG, "  Drop non-resolvable: %s", YESNO(this->drop_non_resolvable_));
  if (this->irks_.empty()) {
    ESP_LOGCONFIG(TAG, "  IRKs: none (rotating addresses are not checked)");
  } else {
    ESP_LOGCONFIG(TAG, "  IRKs: %u (RPA verdict cache: %u slots)", static_cast<unsigned>(this->irks_.size()),
                  static_cast<unsigned>(RPA_CACHE_SIZE));
  }
  ESP_LOGCONFIG(TAG, "  Allow Espressif: %s", YESNO(this->allow_espressif_));
  ESP_LOGCONFIG(TAG, "  MAC allowlist: %u, blocklist: %u", static_cast<unsigned>(this->mac_allowlist_.size()),
                static_cast<unsigned>(this->mac_blocklist_.size()));
  ESP_LOGCONFIG(TAG, "  Allowlist exclusive: %s", YESNO(this->allowlist_exclusive_));
  ESP_LOGCONFIG(TAG, "  Service UUID allowlist: %u short, %u long",
                static_cast<unsigned>(this->service_uuid_allowlist_.size()),
                static_cast<unsigned>(this->service_uuid128_.size()));
  ESP_LOGCONFIG(TAG, "  Name blocklist: %u", static_cast<unsigned>(this->name_blocklist_.size()));
  for (const uint16_t company : this->manufacturer_blocklist_)
    ESP_LOGCONFIG(TAG, "  Manufacturer blocklist: 0x%04X", company);
  ESP_LOGCONFIG(TAG, "  Allow HomeKit: %s", YESNO(this->allow_homekit_));
  if (this->allow_findmy_) {
    if (this->findmy_rssi_ != IBEACON_RSSI_INHERIT) {
      ESP_LOGCONFIG(TAG, "  FindMy exempt from manufacturer_blocklist (RSSI limit %d dB)", this->findmy_rssi_);
    } else {
      ESP_LOGCONFIG(TAG, "  FindMy exempt from manufacturer_blocklist");
    }
  }
  if (this->allow_ibeacon_) {
    ESP_LOGCONFIG(TAG, "  All iBeacons exempt from manufacturer_blocklist (RSSI limit %d dB)", this->ibeacon_any_rssi_);
  } else {
    for (const auto &rule : this->ibeacon_rules_) {
      if (rule.has_uuid) {
        ESP_LOGCONFIG(
            TAG, "  iBeacon rule: uuid %02X%02X%02X%02X-...-%02X%02X%02X%02X, major %d, minor %d: RSSI limit %d dB",
            rule.uuid[0], rule.uuid[1], rule.uuid[2], rule.uuid[3], rule.uuid[12], rule.uuid[13], rule.uuid[14],
            rule.uuid[15], rule.has_major ? rule.major : -1, rule.has_minor ? rule.minor : -1, rule.rssi);
      } else {
        ESP_LOGCONFIG(TAG, "  iBeacon rule: ANY uuid, major %d, minor %d: RSSI limit %d dB",
                      rule.has_major ? rule.major : -1, rule.has_minor ? rule.minor : -1, rule.rssi);
      }
    }
  }
}

bool BLEAdvertFilter::should_forward(const ble_device_base::RawAdvertisement &adv) {
  // This is the only point where a packet can be suppressed without it crossing
  // the network, so every filter lives here.
  //
  // Structure, changed in v1.1.0: the advertisement is CATEGORISED first, then
  // measured against that category's own RSSI limit. Before v1.1.0 the single
  // rssi_threshold ran partway down the chain, which meant only mac_allowlist
  // could be given a looser limit - irks and service UUIDs were resolved after
  // the threshold had already dropped the advertisement, so their limits could
  // only ever tighten. Categorising first makes all four limits genuinely
  // independent: any category can be looser OR stricter than any other.
  //
  // The cost is that IRK resolution and the service-UUID payload walk now run
  // for advertisements between rssi_floor and rssi_threshold that the old order
  // discarded before reaching them. rssi_floor is the mitigation - it is the
  // cheapest test there is, it runs first, and it applies to everything, so the
  // bulk of distant noise never reaches the categoriser at all.

  // The payload walkers below index `data` directly. ESPHome always passes a
  // pointer into a scan buffer, but treat a null one as an empty payload
  // rather than trust every producer of RawAdvertisement to keep doing so.
  const uint8_t *const data = adv.data;
  const uint16_t data_len = adv.data != nullptr ? adv.data_len : 0;

  // 1. Ignored outright, ahead of every allow rule: the point of this list is
  //    "this proxy does not handle this device", which a broader allowlist
  //    entry must not be able to override.
  for (const uint64_t blocked : this->mac_blocklist_) {
    if (blocked == adv.address) {
      this->adv_dropped_++;
      ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": blocklisted address", adv.address);
      return false;
    }
  }

  // 2. Absolute reception floor, applied to EVERYTHING including every
  //    protected category. Below this an RSSI carries no usable distance
  //    information - it is receiver noise, and forwarding it does not help a
  //    tracker triangulate, it misleads it. Deliberately first among the
  //    distance tests: it is the cheapest, and it keeps the categoriser below
  //    from running on traffic no category would have kept anyway.
  //    -127 (default) disables it.
  //    This gate is the LOOSEST limit any rule could apply (computed at codegen
  //    from rssi_floor, rssi_threshold and every per-category limit), not
  //    rssi_floor itself. rssi_floor is applied per-category below, because a
  //    category with an explicit limit is allowed to override it - that is what
  //    lets our own beacons through at any strength while tracked tags stay
  //    bounded at the floor.
  if (this->min_rssi_gate_ != -127 && adv.rssi < this->min_rssi_gate_) {
    this->adv_dropped_++;
    this->adv_dropped_gate_++;
    ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": RSSI %d dB below pre-gate %d dB", adv.address, adv.rssi,
              this->min_rssi_gate_);
    return false;
  }

  // 3. Categorise. Tests run cheapest-first and the first hit wins, so a device
  //    on the MAC allowlist is never also charged for an IRK resolution.
  //    protected_addr means "exempt from the payload filters at the end of the
  //    chain" and is set by every category except DEFAULT.
  enum : uint8_t { CAT_DEFAULT, CAT_MAC, CAT_IRK, CAT_UUID, CAT_IBEACON, CAT_FINDMY } category = CAT_DEFAULT;
  bool protected_addr = false;

  for (const uint64_t allowed : this->mac_allowlist_) {
    if (allowed == adv.address) {
      category = CAT_MAC;
      protected_addr = true;
      break;
    }
  }

  // An RPA is identified from two bits of the address, so the expensive part
  // (irk_matches_, which runs AES per key) is reached only by actual RPAs.
  bool unresolved_rpa = false;
  if (category == CAT_DEFAULT && !this->irks_.empty() && this->address_is_rpa_(adv.address, adv.addr_type)) {
    if (this->irk_matches_cached_(adv.address)) {
      // One of ours. Protected so the payload filters below cannot discard it -
      // our phones and watches advertise Apple manufacturer data, which a
      // manufacturer_blocklist entry would otherwise match.
      category = CAT_IRK;
      protected_addr = true;
    } else {
      // Probably somebody else's phone or watch - but NOT dropped yet. The
      // payload rules below exist for devices whose address cannot be known in
      // advance, and a device in pairing mode may well advertise from an RPA.
      // Dropping here made service_uuid_allowlist unable to rescue one, which
      // is the case it is documented to cover. Decided after categorisation.
      //
      // allow_espressif spares an RPA whose random part happens to look like an
      // Espressif OUI. It is a safety net, not load-bearing: ESPHome advertises
      // on the public MAC, which is never an RPA. It is applied only AFTER the
      // IRK test: before it, a phone whose rotating address landed on one of
      // those prefixes (~1 rotation in 58,000) skipped resolution and was
      // dropped as somebody else's despite a correct key.
      unresolved_rpa = !(this->allow_espressif_ && this->is_espressif_oui_(adv.address));
    }
  }

  // Our own beacons. Placed after the RPA/IRK branch because that one is gated
  // on two address bits and rejects most traffic before this payload walk runs;
  // placed before the service-UUID walk because a matched iBeacon can carry its
  // own RSSI limit and should not be measured against the fleet threshold.
  int8_t ibeacon_limit = IBEACON_RSSI_INHERIT;
  bool ibeacon_has_limit = false;
  if (category == CAT_DEFAULT && (this->allow_ibeacon_ || !this->ibeacon_rules_.empty()) &&
      this->ibeacon_match_(data, data_len, &ibeacon_limit)) {
    category = CAT_IBEACON;
    // A rule with no rssi of its own INHERITS: it only exempts the advert from
    // the manufacturer blocklist and leaves the distance rules alone. Omitting
    // a value must not silently be the most permissive setting.
    ibeacon_has_limit = (ibeacon_limit != IBEACON_RSSI_INHERIT);
    protected_addr = true;
  }

  // FindMy accessories: same shape of rule as an iBeacon (a manufacturer-data
  // subtype exempted from the Apple blocklist, optionally with its own RSSI
  // limit), same place in the chain.
  bool findmy_has_limit = false;
  if (category == CAT_DEFAULT && this->allow_findmy_ && this->findmy_match_(data, data_len)) {
    category = CAT_FINDMY;
    findmy_has_limit = (this->findmy_rssi_ != IBEACON_RSSI_INHERIT);
    protected_addr = true;
  }

  // Walks the payload, so it is last and is skipped entirely when unconfigured.
  // Exists for devices whose address cannot be known ahead of time: a device in
  // pairing mode advertises from a rotating private address, which the
  // non-resolvable test below would otherwise discard.
  if (category == CAT_DEFAULT && (!this->service_uuid_allowlist_.empty() || !this->service_uuid128_.empty()) &&
      this->payload_has_allowed_service_uuid_(data, data_len)) {
    category = CAT_UUID;
    protected_addr = true;
  }

  // An RPA that resolved to none of our IRKs and that no other allow rule
  // claimed: it rotates, so it can never be tracked here and is pure noise.
  // Dropped regardless of RSSI - proximity does not make an unidentifiable
  // device identifiable.
  if (category == CAT_DEFAULT && unresolved_rpa) {
    this->adv_dropped_++;
    this->adv_dropped_rpa_++;
    ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": unresolved RPA", adv.address);
    return false;
  }

  // 4. Apply the category's own RSSI limit.
  //
  //    An unset (-127) limit INHERITS, and what it inherits from is chosen to
  //    reproduce pre-v1.1.0 behaviour exactly on an upgrade that sets nothing:
  //      CAT_MAC   -> no further limit; bounded only by rssi_floor. The MAC
  //                   allowlist has always been a full bypass of the threshold.
  //      CAT_IRK   -> rssi_threshold. IRK-matched devices have always been
  //      CAT_UUID     subject to it, because it used to run before they were
  //                   resolved. Set these explicitly to give either category
  //                   more range than the fleet default.
  //      CAT_DEFAULT -> rssi_threshold.
  int8_t limit;
  const char *limit_name;
  switch (category) {
    case CAT_IBEACON:
      // The filter's own value, verbatim - it overrides rssi_floor too. With no
      // value of its own the category falls back to rssi_threshold, and the
      // floor below still applies, exactly like an unqualified device.
      limit = ibeacon_has_limit ? ibeacon_limit : this->rssi_threshold_;
      limit_name = "ibeacon";
      break;
    case CAT_FINDMY:
      limit = findmy_has_limit ? this->findmy_rssi_ : this->rssi_threshold_;
      limit_name = "findmy";
      break;
    case CAT_MAC:
      limit = this->rssi_mac_allowlist_;
      limit_name = "mac_allowlist";
      break;
    case CAT_IRK:
      limit = this->rssi_irk_ != -127 ? this->rssi_irk_ : this->rssi_threshold_;
      limit_name = "irk";
      break;
    case CAT_UUID:
      limit = this->rssi_service_uuid_ != -127 ? this->rssi_service_uuid_ : this->rssi_threshold_;
      limit_name = "service_uuid";
      break;
    default:
      limit = this->rssi_threshold_;
      limit_name = "threshold";
      break;
  }
  // rssi_floor still bounds every category that did NOT bring its own limit.
  if (!ibeacon_has_limit && !findmy_has_limit && this->rssi_floor_ != -127 &&
      (limit == -127 || this->rssi_floor_ > limit))
    limit = this->rssi_floor_;
  if (limit != -127 && adv.rssi < limit) {
    this->adv_dropped_++;
    if (limit == this->rssi_floor_ && this->rssi_floor_ != -127)
      this->adv_dropped_floor_++;
    ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": RSSI %d dB below %s limit %d dB", adv.address, adv.rssi,
              limit_name, limit);
    return false;
  }

  // 5. Exclusive mode: the allowlist stops being a bypass and becomes the only
  //    way through. Every category above except CAT_DEFAULT counts as allowed.
  if (this->allowlist_exclusive_ && !protected_addr) {
    this->adv_dropped_++;
    ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": not on the exclusive allowlist", adv.address);
    return false;
  }

  // 6. A non-resolvable private address rotates and carries no identity, so it
  //    can never be matched to a device - not even with an IRK. Each rotation
  //    looks like a brand new device downstream.
  if (!protected_addr && this->drop_non_resolvable_ && this->address_is_non_resolvable_(adv.address, adv.addr_type)) {
    this->adv_dropped_++;
    ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": non-resolvable private address", adv.address);
    return false;
  }

  // 7. Payload-based filters last: by here most traffic has already gone.
  if (!protected_addr && (!this->name_blocklist_.empty() || !this->manufacturer_blocklist_.empty()) &&
      this->payload_blocked_(data, data_len)) {
    this->adv_dropped_++;
    ESP_LOGVV(TAG, "Dropping packet from %012" PRIX64 ": blocklisted name or manufacturer", adv.address);
    return false;
  }

  this->adv_forwarded_++;
  if (category == CAT_IRK)
    this->adv_forwarded_irk_++;
  if (category == CAT_UUID)
    this->adv_allowed_service_uuid_++;
  return true;
}

}  // namespace esphome::ble_advert_filter
