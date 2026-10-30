#include "multi_graph.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace esphome::multi_graph {

using tessera::Font;
namespace ui = tessera::ui;

static constexpr int SERIES = 4;
static constexpr int HOURLY = 49;        // 48 hours back, and the hour that was half over at the start
static constexpr int POINTS = HOURLY + 1;  // and the live value
static constexpr uint32_t REFRESH = 600;   // seconds between two rounds of asking
static const theme::Role ROLES[SERIES] = {theme::MARK_GREEN, theme::MARK_AMBER, theme::MARK_BLUE, theme::MARK_PURPLE};
static const char *const IDS[SERIES] = {"s1", "s2", "s3", "s4"};

static void copy(char *to, size_t size, const char *from) { snprintf(to, size, "%s", from ? from : ""); }

// 1043, 18.4, 0.52: the digits a reader needs for the size of the number.
static void number(char *to, size_t size, float v) {
  const float a = fabsf(v);
  snprintf(to, size, a >= 100 ? "%.0f" : a >= 10 ? "%.1f" : "%.2f", v);
}

static void show(lv_obj_t *part, bool on) {
  if (!part) return;
  if (on) lv_obj_remove_flag(part, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
}

struct Series {
  char entity[64] = "", name[40] = "", unit[12] = "";
  float live = NAN;
  uint32_t asked = 0;     // the number of the question in flight, 0 for none
  uint8_t kept = 0;       // hours kept from the last answer
  uint32_t hour[HOURLY];  // hours since 1970 of the answer's rows
  float level[HOURLY];    // their mean or highest value
  uint8_t drawn = 0;      // points of the line now: when[] and raw[] hold their moment and value
  uint32_t when[POINTS];
  float raw[POINTS];
  lv_point_precise_t points[POINTS];
  lv_obj_t *line{}, *dot{}, *name_label{}, *value_label{};
};

// The tile: lines of up to four sensors, and a legend of their live values. History comes by tessera::send, one
// question per sensor on a tick of its own, the live value by on_state.
class GraphTile : public tessera::Tile {
 public:
  explicit GraphTile(MultiGraph *plugin) : plugin_(plugin) {}
  ~GraphTile() override { plugin_->forget(this); }

  void create(const tessera::TileContext &c) override {
    plugin_->remember(this);
    width_ = c.width;
    height_ = c.height;
    hours_ = atoi(c.options["hours"] | "24");
    if (hours_ != 6 && hours_ != 12 && hours_ != 48) hours_ = 24;
    use_max_ = strcmp(c.options["stat"] | "mean", "max") == 0;
    for (int i = 0; i < SERIES; ++i) {
      copy(series_[i].entity, sizeof(series_[i].entity), c.options[IDS[i]] | "");
      if (series_[i].entity[0]) active_[count_++] = i;
    }
    stroke_ = ui::px(ui::large() ? 3 : 2);
    line_h_ = ui::line_height(Font::BODY);
    if (!count_) {
      hint_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_width(hint_, width_);
      lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(hint_, 0, (height_ - line_h_) / 2);
      ui::set_text(hint_, plugin_->text("choose"));
      return;
    }
    // The legend: a dot in the line's colour, then the sensor's name over its value on a tile two rows high, the value
    // alone on one row.
    const bool names = c.rows >= 2;
    const int dot = ui::px(8), gap = ui::px(4), entry_w = width_ / count_;
    legend_h_ = names ? 2 * line_h_ : line_h_;
    for (int k = 0; k < count_; ++k) {
      Series &s = series_[active_[k]];
      const int x = k * entry_w, tx = x + dot + gap, tw = std::max(1, entry_w - dot - 2 * gap);
      s.dot = ui::block(c.parent, ROLES[active_[k]]);
      lv_obj_set_size(s.dot, dot, dot);
      lv_obj_set_style_radius(s.dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_pos(s.dot, x, (line_h_ - dot) / 2);
      s.value_label = ui::label(c.parent, Font::BODY, theme::INK);
      lv_obj_set_width(s.value_label, tw);
      lv_obj_set_pos(s.value_label, tx, names ? line_h_ : 0);
      if (names) {
        s.name_label = ui::label(c.parent, Font::BODY, theme::MUTED);
        lv_obj_set_width(s.name_label, tw);
        lv_obj_set_pos(s.name_label, tx, 0);
      }
      s.line = lv_line_create(c.parent);
      lv_obj_remove_flag(s.line, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_line_rounded(s.line, true, 0);
      lv_obj_set_style_line_width(s.line, stroke_, 0);
      lv_obj_set_style_line_color(s.line, ui::color(ROLES[active_[k]]), 0);
      show(s.line, false);
    }
    graph_y_ = legend_h_ + ui::px(4);
    graph_h_ = std::max(1, height_ - graph_y_);
    axis_hi_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    axis_lo_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    labels_ = graph_h_ >= 2 * line_h_ + ui::px(2);
    show(axis_hi_, labels_);
    show(axis_lo_, labels_);
  }

  void on_state(JsonObjectConst data) override {
    for (int i = 0; i < SERIES; ++i) {
      Series &s = series_[i];
      if (!s.entity[0]) continue;
      JsonObjectConst e = data["entities"][IDS[i]];
      s.live = NAN;
      if (e.isNull()) continue;
      copy(s.name, sizeof(s.name), e["name"] | "");
      copy(s.unit, sizeof(s.unit), e["attributes"]["unit_of_measurement"] | "");
      const char *state = e["state"] | "";
      char *end = nullptr;
      const float v = strtof(state, &end);
      if (end != state && *end == '\0' && std::isfinite(v)) s.live = v;
    }
    dirty_ = true;
  }

  void on_tick(uint32_t epoch) override {
    if (!epoch || !count_) return;
    ask_next(epoch);
    if (dirty_ || epoch / 60 != drawn_minute_) draw(epoch);
  }

  void on_theme() override {
    for (int k = 0; k < count_; ++k) lv_obj_set_style_line_color(series_[active_[k]].line, ui::color(ROLES[active_[k]]), 0);
    dirty_ = true;
    if (epoch_) draw(epoch_);
  }

  // The app's answer to a question of this tile: {"re": n, "ok": true, "result": {"start": [ms...], "mean": [...]}}.
  void on_answer(JsonObjectConst m) {
    const uint32_t re = m["re"] | 0u;
    if (!re) return;
    for (Series &s : series_) {
      if (s.asked != re) continue;
      s.asked = 0;
      if (!(m["ok"] | false)) return;
      JsonArrayConst start = m["result"]["start"], level = m["result"][use_max_ ? "max" : "mean"];
      const size_t n = std::min(start.size(), level.size());
      s.kept = 0;
      for (size_t i = n > HOURLY ? n - HOURLY : 0; i < n; ++i) {
        if (!level[i].is<float>()) continue;  // an hour without a value is a gap in the line
        double t = start[i] | 0.0;
        if (t > 1e11) t /= 1000;  // Home Assistant counts milliseconds
        s.hour[s.kept] = static_cast<uint32_t>(t / 3600);
        s.level[s.kept++] = level[i];
      }
      dirty_ = true;
      return;
    }
  }

 private:
  // One question a second, in turn, then a rest until the next round.
  void ask_next(uint32_t epoch) {
    if (!round_ || epoch - round_ >= REFRESH) {
      round_ = epoch;
      next_ = 0;
    }
    while (next_ < SERIES && !series_[next_].entity[0]) ++next_;
    if (next_ >= SERIES) return;
    Series &s = series_[next_++];
    const time_t from = static_cast<time_t>((epoch - hours_ * 3600u) / 3600 * 3600);
    struct tm utc;
    gmtime_r(&from, &utc);
    char when[32];
    strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%S+00:00", &utc);
    JsonDocument q;
    q["ask"] = "recorder/statistics_during_period";
    JsonObject d = q["data"].to<JsonObject>();
    d["start_time"] = when;
    d["statistic_ids"][0] = s.entity;
    d["period"] = "hour";
    d["types"][0] = use_max_ ? "max" : "mean";
    s.asked = tessera::send(plugin_, q.as<JsonObjectConst>());
  }

  void draw(uint32_t epoch) {
    epoch_ = epoch;
    dirty_ = false;
    drawn_minute_ = epoch / 60;
    const uint32_t span = hours_ * 3600u, t0 = epoch - span;
    // The points of each line in time and value, and the range of each and of all.
    float lo = INFINITY, hi = -INFINITY;
    bool shared = true;
    for (int k = 0; k < count_; ++k) {
      Series &s = series_[active_[k]];
      s.drawn = 0;
      for (int j = 0; j < s.kept; ++j) {
        const uint32_t t = s.hour[j] * 3600u + 1800;  // an hour's value stands in the middle of it
        if (t >= t0 && t <= epoch) {
          s.when[s.drawn] = t;
          s.raw[s.drawn++] = s.level[j];
        }
      }
      if (std::isfinite(s.live)) {
        s.when[s.drawn] = epoch;
        s.raw[s.drawn++] = s.live;
      }
      for (int j = 0; j < s.drawn; ++j) {
        lo = std::min(lo, s.raw[j]);
        hi = std::max(hi, s.raw[j]);
      }
      if (strcmp(s.unit, series_[active_[0]].unit) != 0) shared = false;
    }
    // The y axis at the left, as the core's graph has it: the lowest and the highest. Lines of other units each use
    // their own range, and the axis then belongs to the first sensor.
    const Series &ref = series_[active_[0]];
    auto range = [&](const Series &s, float &a, float &b) {
      a = INFINITY;
      b = -INFINITY;
      for (int j = 0; j < s.drawn; ++j) {
        a = std::min(a, s.raw[j]);
        b = std::max(b, s.raw[j]);
      }
    };
    float axis_lo = lo, axis_hi = hi;
    if (!shared) range(ref, axis_lo, axis_hi);
    int graph_x = 0;
    if (labels_ && std::isfinite(axis_lo)) {
      char top[16], bottom[16];
      number(top, sizeof(top), axis_hi);
      number(bottom, sizeof(bottom), axis_lo);
      graph_x = std::max(ui::text_width(top, Font::BODY), ui::text_width(bottom, Font::BODY)) + ui::px(ui::large() ? 6 : 4);
      const theme::Role role = shared ? theme::MUTED : ROLES[active_[0]];
      for (lv_obj_t *label : {axis_hi_, axis_lo_}) {
        lv_obj_set_width(label, graph_x);
        ui::set_color(label, role);
      }
      lv_obj_set_pos(axis_hi_, 0, graph_y_);
      lv_obj_set_pos(axis_lo_, 0, graph_y_ + graph_h_ - line_h_);
      ui::set_text(axis_hi_, top);
      ui::set_text(axis_lo_, bottom);
      show(axis_hi_, true);
      show(axis_lo_, true);
    } else {
      graph_x = 0;
      show(axis_hi_, false);
      show(axis_lo_, false);
    }
    const int w = std::max(2, width_ - graph_x), margin = stroke_, top = stroke_;
    for (int k = 0; k < count_; ++k) {
      Series &s = series_[active_[k]];
      float a = lo, b = hi;
      if (!shared) range(s, a, b);
      for (int j = 0; j < s.drawn; ++j) {
        const float level = b > a ? (s.raw[j] - a) / (b - a) : 0.5f;
        s.points[j] = {static_cast<lv_value_precise_t>(margin / 2 + static_cast<float>(s.when[j] - t0) * (w - margin - 1) / span),
                       static_cast<lv_value_precise_t>(graph_h_ - margin / 2 - 1 - level * (graph_h_ - margin - 1 - top))};
      }
      if (s.drawn > 1) {
        lv_line_set_points(s.line, s.points, s.drawn);
        lv_obj_set_pos(s.line, graph_x, graph_y_);
      }
      show(s.line, s.drawn > 1);
      char text[40] = "-";
      if (std::isfinite(s.live)) {
        number(text, sizeof(text), s.live);
        if (s.unit[0]) snprintf(text + strlen(text), sizeof(text) - strlen(text), " %s", s.unit);
      }
      ui::set_text(s.value_label, text);
      if (s.name_label) ui::set_text(s.name_label, s.name);
    }
  }

  MultiGraph *plugin_;
  Series series_[SERIES];
  int active_[SERIES]{};
  int count_ = 0, width_ = 0, height_ = 0, hours_ = 24, stroke_ = 2, line_h_ = 0, legend_h_ = 0, graph_y_ = 0, graph_h_ = 0;
  bool use_max_ = false, labels_ = false, dirty_ = true;
  uint32_t epoch_ = 0, drawn_minute_ = 0, round_ = 0;
  int next_ = 0;
  lv_obj_t *hint_{}, *axis_hi_{}, *axis_lo_{};
};

void MultiGraph::setup() {
  add_tile("graph", [this]() { return new GraphTile(this); });
}

void MultiGraph::remember(GraphTile *tile) {
  for (GraphTile *&slot : tiles_) {
    if (slot) continue;
    slot = tile;
    return;
  }
}

void MultiGraph::forget(GraphTile *tile) {
  for (GraphTile *&slot : tiles_)
    if (slot == tile) slot = nullptr;
}

void MultiGraph::on_message(JsonObjectConst message) {
  for (GraphTile *tile : tiles_)
    if (tile) tile->on_answer(message);
}

}  // namespace esphome::multi_graph
