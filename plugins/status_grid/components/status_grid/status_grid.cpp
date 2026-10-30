#include "status_grid.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace esphome::status_grid {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

constexpr int CELLS = 8;
using Rows = std::vector<std::pair<std::string, std::string>>;

// The tap popup: the entity's name, then a row of label and value for each of its facts.
class RowsPopup : public tessera::Popup {
 public:
  RowsPopup(std::string title, Rows rows) : title_(std::move(title)), rows_(std::move(rows)) {}

  void open(const tessera::PopupContext &c) override {
    title_label_ = ui::label(c.parent, Font::HEADLINE, theme::INK);
    lv_obj_set_width(title_label_, c.width - c.corner);
    ui::set_text(title_label_, title_);
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
  lv_obj_t *title_label_{};
};

struct Cell {
  lv_obj_t *box{}, *name{}, *state{};
  bool active = false;
  Tone tone = Tone::NORMAL;
  std::string title, value;
  uint32_t updated = 0;
  int x = 0, y = 0, w = 0, h = 0;
};

class GridTile : public tessera::Tile {
 public:
  explicit GridTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    cols_ = c.columns >= 4 ? 4 : 2;
    const char *limits = c.options["limits"] | "80,90";
    have_limits_ = sscanf(limits, "%lf , %lf", &warn_, &crit_) == 2;
    low_is_bad_ = std::string(c.options["direction"] | "high") == "low";
    for (Cell &cell : cells_) {
      cell.box = ui::block(c.parent, theme::TRACK);
      lv_obj_add_flag(cell.box, LV_OBJ_FLAG_HIDDEN);
      cell.name = ui::label(cell.box, Font::BODY, theme::MUTED);
      cell.state = ui::label(cell.box, Font::TITLE, theme::INK);
    }
    hint_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_width(hint_, width_);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(hint_, 0, (height_ - ui::line_height(Font::BODY)) / 2);
    ui::set_text(hint_, plugin_->text("choose"));
  }

  void on_state(JsonObjectConst data) override {
    JsonVariantConst ents = data["entities"];
    bool changed = false;
    for (int i = 0; i < CELLS; i++) {
      char key[3] = {'e', static_cast<char>('1' + i), 0};
      JsonVariantConst e = ents[key];
      Cell &cell = cells_[i];
      const bool active = !e.isNull();
      changed |= active != cell.active;
      cell.active = active;
      if (!active) continue;
      cell.title = e["name"] | key;
      std::string state = e["state"] | "unavailable";
      cell.updated = e["updated"] | 0u;
      const char *unit = e["attributes"]["unit_of_measurement"] | "";
      char *end = nullptr;
      const double v = strtod(state.c_str(), &end);
      if (!state.empty() && end && *end == 0) {
        cell.tone = number_tone(v);
        cell.value = *unit ? state + " " + unit : state;
      } else {
        cell.tone = state == "on" || state == "problem" || state == "unavailable" ? Tone::ALERT : Tone::GOOD;
        cell.value = state;
      }
    }
    if (changed) layout();
    paint();
  }

  void on_tick(uint32_t epoch) override { now_ = epoch; }
  void on_theme() override { paint(); }

  void on_tap() override {
    const tessera::Point p = tessera::tap_point();
    for (const Cell &cell : cells_) {
      if (!cell.active || p.x < cell.x || p.x >= cell.x + cell.w || p.y < cell.y || p.y >= cell.y + cell.h) continue;
      Rows rows = {{plugin_->text("state"), cell.value}, {plugin_->text("changed"), ago(cell.updated)}};
      std::string title = cell.title;
      tessera::show_popup(plugin_, [title, rows]() -> tessera::Popup * { return new RowsPopup(title, rows); });
      return;
    }
  }

 private:
  Tone number_tone(double v) const {
    if (!have_limits_) return Tone::NORMAL;
    const double sign = low_is_bad_ ? -1 : 1;
    if (sign * v >= sign * crit_) return Tone::ALERT;
    if (sign * v >= sign * warn_) return Tone::ACCENT;
    return Tone::NORMAL;
  }

  std::string ago(uint32_t then) const {
    if (!now_ || !then) return "";
    const uint32_t s = now_ > then ? now_ - then : 0;
    if (s < 60) return plugin_->text("just_now");
    if (s < 3600) return tessera::format(plugin_->text("min_ago"), static_cast<long>(s / 60));
    if (s < 86400) return tessera::format(plugin_->text("hour_ago"), static_cast<long>(s / 3600));
    return tessera::format(plugin_->text("day_ago"), static_cast<long>(s / 86400));
  }

  // Cells fill the tile in order, only for the entities that are chosen.
  void layout() {
    int n = 0;
    for (const Cell &cell : cells_) n += cell.active;
    if (n == 0) {
      lv_obj_remove_flag(hint_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(hint_, LV_OBJ_FLAG_HIDDEN);
    }
    const int cols = n < cols_ ? std::max(n, 1) : cols_;
    const int rows = (n + cols - 1) / cols;
    const int gap = ui::px(4), pad = ui::px(6);
    const int cw = (width_ - gap * (cols - 1)) / cols;
    const int ch = rows ? (height_ - gap * (rows - 1)) / rows : 0;
    const bool two_lines = ch >= 2 * ui::line_height(Font::BODY) + pad;
    int k = 0;
    for (Cell &cell : cells_) {
      if (!cell.active) {
        lv_obj_add_flag(cell.box, LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      cell.x = (k % cols) * (cw + gap);
      cell.y = (k / cols) * (ch + gap);
      cell.w = cw;
      cell.h = ch;
      k++;
      lv_obj_remove_flag(cell.box, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_size(cell.box, cw, ch);
      lv_obj_set_pos(cell.box, cell.x, cell.y);
      const int inner = cw - 2 * pad;
      if (two_lines) {
        lv_obj_set_width(cell.name, inner);
        lv_obj_set_width(cell.state, inner);
        lv_obj_set_pos(cell.name, pad, (ch - ui::line_height(Font::BODY) - ui::line_height(Font::TITLE)) / 2);
        lv_obj_set_pos(cell.state, pad, lv_obj_get_y(cell.name) + ui::line_height(Font::BODY));
      } else {
        lv_obj_set_pos(cell.name, pad, (ch - ui::line_height(Font::BODY)) / 2);
        lv_obj_set_pos(cell.state, pad, (ch - ui::line_height(Font::BODY)) / 2);
      }
      two_lines_ = two_lines;
    }
  }

  void paint() {
    for (Cell &cell : cells_) {
      if (!cell.active) continue;
      const bool plain = cell.tone == Tone::NORMAL;
      ui::set_fill(cell.box, cell.tone, theme::TRACK);
      ui::set_color(cell.name, plain ? theme::MUTED : theme::ON_ACCENT);
      ui::set_color(cell.state, plain ? theme::INK : theme::ON_ACCENT);
      ui::set_text(cell.name, cell.title);
      ui::set_text(cell.state, cell.value);
      const int inner = cell.w - 2 * ui::px(6);
      if (two_lines_) {
        Font face = Font::TITLE;
        for (Font f : {Font::TITLE, Font::BODY_LARGE, Font::BODY}) {
          face = f;
          if (ui::text_width(cell.value, f) <= inner) break;
        }
        ui::set_font(cell.state, face);
      } else {
        // One line: the state on the right, the name takes what is left.
        const int sw = std::min(ui::text_width(cell.value, Font::BODY), inner * 2 / 3);
        lv_obj_set_width(cell.state, sw);
        lv_obj_set_x(cell.state, cell.w - ui::px(6) - sw);
        lv_obj_set_style_text_align(cell.state, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_width(cell.name, inner - sw - ui::px(4));
      }
    }
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0, cols_ = 2;
  bool have_limits_ = false, low_is_bad_ = false, two_lines_ = true;
  double warn_ = 0, crit_ = 0;
  uint32_t now_ = 0;
  Cell cells_[CELLS];
  lv_obj_t *hint_{};
};

void StatusGrid::setup() {
  add_tile("grid", [this]() { return new GridTile(this); });
}

}  // namespace esphome::status_grid
