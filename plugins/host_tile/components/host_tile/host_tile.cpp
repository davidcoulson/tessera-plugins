#include "host_tile.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace esphome::host_tile {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

using Rows = std::vector<std::pair<std::string, std::string>>;

// The tap popup: the host's name, then a row of label and value for each fact.
class RowsPopup : public tessera::Popup {
 public:
  RowsPopup(std::string title, Rows rows) : title_(std::move(title)), rows_(std::move(rows)) {}

  void open(const tessera::PopupContext &c) override {
    lv_obj_t *head = ui::label(c.parent, Font::HEADLINE, theme::INK);
    lv_obj_set_width(head, c.width - c.corner);
    ui::set_text(head, title_);
    int y = std::max(c.key, ui::line_height(Font::HEADLINE)) + ui::px(8);
    for (const auto &row : rows_) {
      lv_obj_t *k = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_width(k, c.width / 3);
      lv_obj_set_pos(k, 0, y);
      ui::set_text(k, row.first);
      lv_obj_t *v = ui::label(c.parent, Font::BODY_LARGE, theme::INK);
      lv_obj_set_width(v, c.width - c.width / 3);
      lv_obj_set_pos(v, c.width / 3, y);
      lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
      ui::set_text(v, row.second);
      y += ui::line_height(Font::BODY_LARGE) + ui::px(6);
    }
  }

 private:
  std::string title_;
  Rows rows_;
};

// Days since 1970-01-01 for a date (Howard Hinnant's days_from_civil).
static long days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long>(doe) - 719468;
}

// "2026-10-01T08:30:00+02:00" or "...Z" -> seconds since 1970; 0 when it is not a timestamp.
static long parse_timestamp(const std::string &s) {
  int y, mo, d, h, mi, sec = 0;
  if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) < 5) return 0;
  long t = days_from_civil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400L + h * 3600L + mi * 60L + sec;
  const size_t sign = s.find_last_of("+-");
  int oh = 0, om = 0;
  if (sign != std::string::npos && sign > 10 && sscanf(s.c_str() + sign + 1, "%d:%d", &oh, &om) >= 1)
    t -= (s[sign] == '-' ? -1 : 1) * (oh * 3600L + om * 60L);
  return t;
}

static std::string duration(long s) {
  char buf[24];
  if (s >= 86400) snprintf(buf, sizeof buf, "%ldd %ldh", s / 86400, s % 86400 / 3600);
  else if (s >= 3600) snprintf(buf, sizeof buf, "%ldh %ldm", s / 3600, s % 3600 / 60);
  else snprintf(buf, sizeof buf, "%ldm", s / 60);
  return buf;
}

constexpr int BARS = 3;

class HostTileView : public tessera::Tile {
 public:
  explicit HostTileView(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    wide_ = c.rows >= 2;
    name_ = c.options["host_name"] | "";
    if (name_.empty()) name_ = c.name;
    title_ = ui::label(c.parent, Font::BODY_LARGE, theme::INK);
    dot_ = ui::block(c.parent, theme::MUTED);
    lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
    foot_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    for (int i = 0; i < BARS; i++) {
      bars_[i].label = ui::label(c.parent, Font::BODY, theme::MUTED);
      bars_[i].track = ui::block(c.parent, theme::TRACK);
      bars_[i].fill = ui::block(c.parent, theme::INK_SOFT);
    }
    hint_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    ui::set_text(hint_, plugin_->text("choose"));
    layout();
  }

  void on_state(JsonObjectConst data) override {
    JsonVariantConst ents = data["entities"];
    static const char *const keys[BARS] = {"cpu", "mem", "disk"};
    any_ = false;
    for (int i = 0; i < BARS; i++) {
      JsonVariantConst e = ents[keys[i]];
      Bar &b = bars_[i];
      b.has = !e.isNull();
      any_ |= b.has;
      if (!b.has) continue;
      const std::string state = e["state"] | "";
      char *end = nullptr;
      b.pct = strtod(state.c_str(), &end);
      b.ok = !state.empty() && end && *end == 0;
    }
    const std::string temp_state = text_of(ents["temp"]);
    temp_ = temp_state.empty() ? "" : temp_state + unit_of(ents["temp"]);
    any_ |= !temp_.empty();
    JsonVariantConst up = ents["uptime"];
    uptime_raw_ = text_of(up);
    uptime_unit_ = unit_of(up);
    any_ |= !uptime_raw_.empty();
    JsonVariantConst on = ents["online"];
    has_online_ = !on.isNull();
    any_ |= has_online_;
    const std::string s = text_of(on);
    online_ = s == "on" || s == "home" || s == "online";
    for (int i = 0; i < BARS; i++) full_[i] = text_of(ents[keys[i]]) + unit_of(ents[keys[i]]);
    paint();
  }

  void on_tick(uint32_t epoch) override {
    now_ = epoch;
    paint();
  }
  void on_theme() override { paint(); }

  void on_tap() override {
    static const char *const keys[BARS] = {"cpu", "mem", "disk"};
    Rows rows;
    for (int i = 0; i < BARS; i++)
      if (bars_[i].has) rows.push_back({plugin_->text(keys[i]), full_[i]});
    if (!temp_.empty()) rows.push_back({plugin_->text("temp"), temp_});
    const std::string up = uptime_text();
    if (!up.empty()) rows.push_back({plugin_->text("uptime"), up});
    if (has_online_) rows.push_back({plugin_->text("online"), plugin_->text(online_ ? "yes" : "no")});
    std::string title = name_;
    tessera::show_popup(plugin_, [title, rows]() -> tessera::Popup * { return new RowsPopup(title, rows); });
  }

 private:
  struct Bar {
    lv_obj_t *label{}, *track{}, *fill{};
    bool has = false, ok = false;
    double pct = 0;
    int x = 0, w = 0;
  };

  static std::string text_of(JsonVariantConst e) {
    std::string s = e["state"] | "";
    return s == "unavailable" || s == "unknown" ? "" : s;
  }
  static std::string unit_of(JsonVariantConst e) {
    const char *u = e["attributes"]["unit_of_measurement"] | "";
    return *u ? std::string(" ") + u : "";
  }

  // Seconds up: a sensor of seconds (or minutes, hours, days by its unit), or a timestamp of the last boot.
  std::string uptime_text() const {
    if (uptime_raw_.empty()) return "";
    char *end = nullptr;
    const double v = strtod(uptime_raw_.c_str(), &end);
    long secs;
    if (end && *end == 0) {
      const std::string u = uptime_unit_;
      secs = static_cast<long>(v * (u == " min" ? 60 : u == " h" ? 3600 : u == " d" ? 86400 : 1));
    } else {
      const long boot = parse_timestamp(uptime_raw_);
      if (!boot || !now_) return "";
      secs = std::max(0L, static_cast<long>(now_) - boot);
    }
    return duration(secs);
  }

  // 2x1: name over three side-by-side bars. 2x2: name, three bar rows, a bottom line.
  void layout() {
    const int lh = ui::line_height(Font::BODY), gap = ui::px(4), dot = ui::px(8);
    const int head = ui::line_height(Font::BODY_LARGE);
    lv_obj_set_pos(title_, 0, 0);
    lv_obj_set_width(title_, width_ - dot - gap);
    lv_obj_set_size(dot_, dot, dot);
    lv_obj_set_pos(dot_, width_ - dot, (head - dot) / 2);
    lv_obj_set_width(hint_, width_);
    lv_obj_set_pos(hint_, 0, head + gap);
    const int bar_h = ui::px(6);
    if (wide_) {
      const int rows = 3;
      const int top = head + gap;
      const int row_h = std::max(lh, (height_ - top - lh - gap) / rows);
      const int label_w = ui::text_width("100%", Font::BODY) + ui::text_width("CPU", Font::BODY);
      for (int i = 0; i < BARS; i++) {
        Bar &b = bars_[i];
        const int y = top + i * row_h;
        lv_obj_set_pos(b.label, 0, y);
        lv_obj_set_width(b.label, label_w);
        b.x = label_w + gap;
        b.w = width_ - b.x;
        lv_obj_set_size(b.track, b.w, bar_h);
        lv_obj_set_pos(b.track, b.x, y + (lh - bar_h) / 2);
        lv_obj_set_pos(b.fill, b.x, y + (lh - bar_h) / 2);
      }
      lv_obj_set_width(foot_, width_);
      lv_obj_set_pos(foot_, 0, height_ - lh);
    } else {
      const int cw = (width_ - 2 * gap) / BARS;
      for (int i = 0; i < BARS; i++) {
        Bar &b = bars_[i];
        b.x = i * (cw + gap);
        b.w = cw;
        lv_obj_set_width(b.label, cw);
        lv_obj_set_pos(b.label, b.x, head + gap);
        lv_obj_set_size(b.track, cw, bar_h);
        lv_obj_set_pos(b.track, b.x, head + gap + lh + ui::px(2));
        lv_obj_set_pos(b.fill, b.x, head + gap + lh + ui::px(2));
      }
      // The bottom line shares the name's row, at the right of it.
      lv_obj_set_width(foot_, width_ / 2);
      lv_obj_set_pos(foot_, width_ / 2 - dot - gap, 0);
      lv_obj_set_style_text_align(foot_, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_width(title_, width_ / 2 - dot - gap);
    }
  }

  void paint() {
    ui::set_text(title_, name_);
    ui::set_color(title_, theme::INK);
    if (has_online_) {
      lv_obj_remove_flag(dot_, LV_OBJ_FLAG_HIDDEN);
      ui::set_fill(dot_, online_ ? Tone::GOOD : Tone::ALERT, theme::MUTED);
    } else {
      lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);
    }
    static const char *const names[BARS] = {"cpu", "mem", "disk"};
    for (int i = 0; i < BARS; i++) {
      Bar &b = bars_[i];
      const bool show = b.has && b.ok;
      for (lv_obj_t *o : {b.label, b.track, b.fill}) {
        if (show) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
      }
      if (!show) continue;
      const double pct = std::min(100.0, std::max(0.0, b.pct));
      char buf[24];
      snprintf(buf, sizeof buf, "%s %.0f%%", plugin_->text(names[i]), b.pct);
      // A short label for a narrow bar: the first three letters when the whole word does not fit.
      std::string label = buf;
      if (ui::text_width(label, Font::BODY) > (wide_ ? ui::text_width("100%", Font::BODY) + ui::text_width("CPU", Font::BODY) : b.w))
        snprintf(buf, sizeof buf, "%.3s %.0f%%", plugin_->text(names[i]), b.pct), label = buf;
      ui::set_text(b.label, label);
      lv_obj_set_width(b.fill, std::max(1, static_cast<int>(b.w * pct / 100)));
      lv_obj_set_height(b.fill, lv_obj_get_height(b.track));
      ui::set_fill(b.fill, b.pct > 90 ? Tone::ALERT : b.pct > 80 ? Tone::ACCENT : Tone::NORMAL, theme::INK_SOFT);
    }
    std::string foot = temp_;
    const std::string up = uptime_text();
    if (!up.empty()) foot += (foot.empty() ? "" : "  ") + up;
    ui::set_text(foot_, foot);
    if (any_) lv_obj_add_flag(hint_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(hint_, LV_OBJ_FLAG_HIDDEN);
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;
  bool wide_ = false, any_ = false, has_online_ = false, online_ = false;
  uint32_t now_ = 0;
  std::string name_, temp_, uptime_raw_, uptime_unit_, full_[BARS];
  Bar bars_[BARS];
  lv_obj_t *title_{}, *dot_{}, *foot_{}, *hint_{};
};

void HostTile::setup() {
  add_tile("host", [this]() { return new HostTileView(this); });
}

}  // namespace esphome::host_tile
