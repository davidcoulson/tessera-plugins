#include "alert_feed.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "esphome/core/hal.h"

namespace esphome::alert_feed {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

static constexpr uint32_t ASK_NEAR_MS = 30000;   // while the tile is on the glass
static constexpr uint32_t ASK_FAR_MS = 120000;   // otherwise
static constexpr uint32_t BOOT_WAIT_MS = 8000;   // the app and Home Assistant are not there at once
static constexpr uint32_t RETRY_MS = 15000;      // a question that was not sent, or never answered
static constexpr size_t MAX_ROWS = 8;

static bool after(uint32_t now, uint32_t then) { return static_cast<int32_t>(now - then) >= 0; }

// Days since 1970-01-01 for a date (Howard Hinnant's days_from_civil).
static long days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long>(doe) - 719468;
}

// "2026-10-30T19:59:26.136294+00:00" (Home Assistant's created_at) as seconds since 1970; no offset means UTC; 0 on a text
// that is not one.
static uint32_t epoch_of(const char *text) {
  int y, mo, d, h, mi, s;
  if (sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) != 6 || mo < 1 || mo > 12 || d < 1 || d > 31)
    return 0;
  long at = days_from_civil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400L + h * 3600L + mi * 60L + s;
  const char *tail = text + 19;
  if (*tail == '.') while (*++tail >= '0' && *tail <= '9') {}
  int oh = 0, om = 0;
  if ((*tail == '+' || *tail == '-') && sscanf(tail + 1, "%2d:%2d", &oh, &om) >= 1)
    at -= (*tail == '-' ? -1 : 1) * (oh * 3600L + om * 60L);
  return at > 0 ? static_cast<uint32_t>(at) : 0;
}

static void copy(char *to, size_t size, const char *from) {
  strncpy(to, from, size - 1);
  to[size - 1] = '\0';
}

// ---- The popup: one notification in full, with a Dismiss key; the same card marked ALERT for a new one ----
class Detail : public tessera::Popup {
 public:
  Detail(AlertFeed *plugin, const Item &item, bool alert) : plugin_(plugin), item_(item), alert_(alert) {}

  void open(const tessera::PopupContext &c) override {
    const int w = c.width, gap = ui::px(8);
    const int lh = ui::line_height(Font::BODY), lhl = ui::line_height(Font::BODY_LARGE);
    int y = 0;
    // The first row stands as high as the close key, which sits in the room's top right corner.
    const int row = std::max(c.key, lh);
    head_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_width(head_, w - c.corner);
    lv_obj_set_pos(head_, 0, (row - lh) / 2);
    y += row + gap;
    title_ = ui::text(c.parent, Font::BODY_LARGE, theme::INK);
    lv_obj_set_size(title_, w, 2 * lhl);
    lv_obj_set_pos(title_, 0, y);
    ui::set_text(title_, item_.title[0] ? item_.title : item_.id);
    y += 2 * lhl + gap;
    button_h_ = std::max(ui::mm(10), lh + ui::px(14));
    // Whatever room is left over the key goes to the message: its first lines, it never scrolls.
    const int lines = std::min(4, (c.height - y - button_h_ - gap) / lh);
    message_ = ui::text(c.parent, Font::BODY, theme::MUTED);
    if (lines > 0 && item_.message[0]) {
      lv_obj_set_size(message_, w, lines * lh);
      lv_obj_set_pos(message_, 0, y);
      ui::set_text(message_, item_.message);
      y += lines * lh + gap;
    } else {
      lv_obj_add_flag(message_, LV_OBJ_FLAG_HIDDEN);
    }
    button_y_ = y;
    button_ = ui::block(c.parent, theme::ACCENT);
    lv_obj_set_size(button_, w, button_h_);
    lv_obj_set_pos(button_, 0, button_y_);
    key_ = ui::label(button_, Font::BODY, theme::ON_ACCENT);
    lv_obj_set_width(key_, w);
    lv_obj_set_style_text_align(key_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(key_, 0, (button_h_ - lh) / 2);
    ui::set_text(key_, plugin_->text("dismiss"));
    on_tick(0);
    on_theme();
  }

  void on_tick(uint32_t epoch) override {
    const std::string age = plugin_->age(item_.created, epoch);
    const std::string word = plugin_->text("alert");
    ui::set_text(head_, alert_ ? (age.empty() ? word : word + "  " + age) : age);
  }

  void on_theme() override {
    ui::set_tone(head_, alert_ ? Tone::ALERT : Tone::NORMAL, theme::MUTED);
    ui::set_color(title_, theme::INK);
    ui::set_color(message_, theme::MUTED);
    ui::set_fill(button_, Tone::NORMAL, theme::ACCENT);
    ui::set_color(key_, theme::ON_ACCENT);
  }

  void on_tap() override {
    const tessera::Point p = tessera::tap_point();
    if (p.y < button_y_ || p.y >= button_y_ + button_h_) return;
    plugin_->dismiss(item_.id);
    tessera::close_popup(plugin_);
  }

 private:
  AlertFeed *plugin_;
  Item item_;
  bool alert_;
  int button_y_{0}, button_h_{0};
  lv_obj_t *head_{}, *title_{}, *message_{}, *button_{}, *key_{};
};

// ---- The tile: a badge with the count over up to 8 rows of title and age; a tap on a row opens its popup ----
class FeedTile : public tessera::Tile {
 public:
  explicit FeedTile(AlertFeed *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    const int lh = ui::line_height(Font::BODY), lhl = ui::line_height(Font::BODY_LARGE);
    row_h_ = std::max(lh + ui::px(8), ui::mm(7));
    head_h_ = std::max(lhl, lh + ui::px(6)) + ui::px(6);
    rows_ = static_cast<int>(std::min<size_t>(MAX_ROWS, std::max(1, (height_ - head_h_) / row_h_)));
    badge_w_ = std::max(lh + ui::px(6), ui::text_width("99+", Font::BODY) + ui::px(12));

    title_ = ui::label(c.parent, Font::BODY_LARGE, theme::INK);
    lv_obj_set_width(title_, width_ - badge_w_ - ui::px(8));
    lv_obj_set_pos(title_, 0, 0);
    ui::set_text(title_, plugin_->text("title"));
    badge_ = ui::block(c.parent, theme::ACCENT);
    lv_obj_set_size(badge_, badge_w_, lh + ui::px(6));
    lv_obj_set_pos(badge_, width_ - badge_w_, 0);
    count_ = ui::label(badge_, Font::BODY, theme::ON_ACCENT);
    lv_obj_set_width(count_, badge_w_);
    lv_obj_set_style_text_align(count_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(count_, 0, ui::px(3));

    age_w_ = ui::text_width(tessera::fill(plugin_->text("age_min"), "n", "99"), Font::BODY) + ui::px(4);
    for (int i = 0; i < rows_; ++i) {
      const int y = head_h_ + i * row_h_ + (row_h_ - lh) / 2;
      name_[i] = ui::label(c.parent, Font::BODY, theme::INK);
      lv_obj_set_width(name_[i], width_ - age_w_ - ui::px(8));
      lv_obj_set_pos(name_[i], 0, y);
      age_[i] = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_width(age_[i], age_w_);
      lv_obj_set_style_text_align(age_[i], LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_pos(age_[i], width_ - age_w_, y);
    }
    paint(0);
  }

  void on_tick(uint32_t epoch) override {
    plugin_->tile_on_glass();
    paint(epoch);
  }

  void on_theme() override {
    ui::set_color(title_, theme::INK);
    ui::set_color(count_, theme::ON_ACCENT);
    for (int i = 0; i < rows_; ++i) {
      ui::set_color(name_[i], theme::INK);
      ui::set_color(age_[i], theme::MUTED);
    }
    paint(epoch_);
  }

  void on_tap() override {
    const int y = tessera::tap_point().y - head_h_;
    if (y < 0) return;
    const size_t row = static_cast<size_t>(y / row_h_);
    if (row < static_cast<size_t>(rows_) && row < plugin_->items().size()) plugin_->show(plugin_->items()[row], false);
  }

 private:
  void paint(uint32_t epoch) {
    epoch_ = epoch;
    const tessera::List<Item> &items = plugin_->items();
    const size_t n = items.size();
    ui::set_text(count_, n > 99 ? "99+" : std::to_string(n));
    ui::set_fill(badge_, n ? Tone::ALERT : Tone::NORMAL, theme::TRACK);
    for (int i = 0; i < rows_; ++i) {
      const bool has = static_cast<size_t>(i) < n;
      ui::set_text(name_[i], has ? (items[i].title[0] ? items[i].title : items[i].id) : (i == 0 ? plugin_->text("none") : ""));
      ui::set_color(name_[i], has ? theme::INK : theme::MUTED);
      ui::set_text(age_[i], has ? plugin_->age(items[i].created, epoch) : "");
    }
  }

  AlertFeed *plugin_;
  int width_{0}, height_{0}, row_h_{0}, head_h_{0}, rows_{0}, badge_w_{0}, age_w_{0};
  uint32_t epoch_{0};
  lv_obj_t *title_{}, *badge_{}, *count_{};
  lv_obj_t *name_[MAX_ROWS]{}, *age_[MAX_ROWS]{};
};

void AlertFeed::setup() { add_tile("feed", [this]() { return new FeedTile(this); }); }

void AlertFeed::tile_on_glass() {
  glass_at_ = millis();
  on_glass_ = true;
}

std::string AlertFeed::age(uint32_t created, uint32_t epoch) const {
  if (!created || !epoch) return "";
  const uint32_t s = epoch > created ? epoch - created : 0;
  if (s < 60) return text("now");
  if (s < 3600) return tessera::fill(text("age_min"), "n", std::to_string(s / 60));
  if (s < 86400) return tessera::fill(text("age_hour"), "n", std::to_string(s / 3600));
  return tessera::fill(text("age_day"), "n", std::to_string(s / 86400));
}

// Every 250 ms: ask when it is due. 30 s while a tile showed in the last 3 s, 2 min otherwise.
void AlertFeed::on_interval(uint32_t now_ms) {
  if (on_glass_ && after(now_ms, glass_at_ + 3000)) on_glass_ = false;
  if (now_ms < BOOT_WAIT_MS || !after(now_ms, hold_until_)) return;
  if (asked_) {  // no answer in time: ask again
    if (after(now_ms, asked_at_ + RETRY_MS)) asked_ = 0;
    else return;
  }
  if (force_ || !ever_asked_ || after(now_ms, asked_at_ + (on_glass_ ? ASK_NEAR_MS : ASK_FAR_MS))) ask();
}

void AlertFeed::ask() {
  JsonDocument request;
  request["ask"] = "persistent_notification/get";
  asked_ = tessera::send(this, request.as<JsonObjectConst>());
  asked_at_ = millis();
  ever_asked_ = true;
  force_ = false;
  if (!asked_) asked_at_ = asked_at_ + RETRY_MS - (on_glass_ ? ASK_NEAR_MS : ASK_FAR_MS);  // not sent: try again soon
}

void AlertFeed::on_message(JsonObjectConst message) {
  if (!asked_ || (message["re"] | 0u) != asked_) return;
  asked_ = 0;
  // An answer that failed leaves the list as it was.
  if (!(message["ok"] | false) || !message["result"].is<JsonArrayConst>()) return;

  tessera::List<Item> now;
  for (JsonObjectConst n : message["result"].as<JsonArrayConst>()) {
    Item item{};
    copy(item.id, sizeof item.id, n["notification_id"] | "");
    if (!item.id[0]) continue;
    copy(item.title, sizeof item.title, n["title"] | "");
    copy(item.message, sizeof item.message, n["message"] | "");
    item.created = epoch_of(n["created_at"] | "");
    now.push_back(item);
  }
  // Newest first; one whose time could not be read goes last. Home Assistant lists the oldest first, so a tie keeps
  // the later one in front.
  std::stable_sort(now.begin(), now.end(), [](const Item &a, const Item &b) { return a.created > b.created; });
  if (now.size() > KEEP) now.resize(KEEP);

  // The newest id not in the previous answer, if the switch says to announce it.
  const Item *fresh = nullptr;
  if (primed_) {
    for (const Item &item : now) {
      const bool known = std::any_of(items_.begin(), items_.end(), [&](const Item &o) { return strcmp(o.id, item.id) == 0; });
      if (!known) {
        fresh = &item;
        break;
      }
    }
  }
  const Item announce = fresh ? *fresh : Item{};
  items_ = std::move(now);
  primed_ = true;
  if (fresh && wake_ != nullptr && wake_->state && !tessera::popup_shown(this)) {
    tessera::wake(this);
    show(announce, true);
  }
}

void AlertFeed::show(const Item &item, bool alert) {
  tessera::show_popup(this, [this, item, alert]() { return new Detail(this, item, alert); });
}

void AlertFeed::dismiss(const char *id) {
  JsonDocument fields;
  fields["notification_id"] = id;
  if (!tessera::action("persistent_notification.dismiss", "", fields)) return;
  // Gone from the tile at once; the next answer confirms it. Ask again in a moment, not before Home Assistant has it.
  items_.erase(std::remove_if(items_.begin(), items_.end(), [&](const Item &o) { return strcmp(o.id, id) == 0; }), items_.end());
  force_ = true;
  hold_until_ = millis() + 3000;
}

bool AlertFeed::settings(tessera::SettingsPage &page) {
  if (wake_ == nullptr) return false;
  page.icon = "\U000F009A";  // bell
  switch_::Switch *wake = wake_;
  page.toggle(text("wake"), [wake] { return wake->state; }, [wake](bool on) { on ? wake->turn_on() : wake->turn_off(); });
  return true;
}

}  // namespace esphome::alert_feed
