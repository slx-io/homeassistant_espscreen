#pragma once
// A light's presets (module light_presets, docs/LIGHT_PRESETS.md): up to four round keys on its tile and its card, each
// setting the light to a brightness and, where the preset has one, a white shade or a colour, in one light.turn_on. They
// are Home Assistant's favourite colours (ha-favorite-color-button: a disc of the colour, a hairline round a very pale
// one, a ring round the one in focus) with the brightness they go with, chosen per tile in the editor.
//
// The values part stays free of LVGL for tests/test_light_presets.cpp; the module's drawing is under it.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace light_presets {
constexpr unsigned MAX = 4;
// A preset as the tile's options keep it ({"b": 80, "k": 2700}, {"b": 60, "h": 280, "s": 41}, {"b": 30}).
struct Preset {
  enum Kind : uint8_t { LEVEL, WHITE, COLOUR };
  uint8_t level = 0;       // brightness 1-100
  Kind kind = LEVEL;       // a brightness alone keeps the colour the light has
  uint8_t saturation = 0;  // 0-100, COLOUR
  uint16_t hue = 0;        // 0-359, COLOUR
  uint16_t kelvin = 0;     // WHITE
};
struct Presets {
  std::array<Preset, MAX> items{};
  uint8_t count = 0;
  bool levels = false;  // each disc shows its brightness (the tile's preset_levels); one of a brightness alone always does
  bool level_shown(const Preset &p) const { return levels || p.kind == Preset::LEVEL; }
};
// A preset as it comes from the app, or a level of 0 for one the screen cannot take.
inline Preset make(int level, int kelvin, int hue, int saturation, bool white, bool colour) {
  Preset p;
  if (level < 1 || level > 100) return p;
  if (white && kelvin >= 1000 && kelvin <= 15000) { p.kind = Preset::WHITE; p.kelvin = (uint16_t) kelvin; }
  else if (colour && hue >= 0 && hue <= 360 && saturation >= 0 && saturation <= 100) {
    p.kind = Preset::COLOUR; p.hue = (uint16_t) (hue % 360); p.saturation = (uint8_t) saturation;
  } else if (white || colour) return p;
  p.level = (uint8_t) level;
  return p;
}

// ---- Its colour, as Home Assistant draws a favourite colour ----
inline uint8_t clamp8(double v) { return (uint8_t) std::lround(std::clamp(v, 0.0, 255.0)); }
// convert-light-color.ts temperature2rgb.
inline uint32_t kelvin_rgb(int kelvin) {
  const double t = kelvin / 100.0;
  const double r = t <= 66 ? 255 : 329.698727446 * std::pow(t - 60, -0.1332047592);
  const double g = t <= 66 ? 99.4708025861 * std::log(t) - 161.1195681661 : 288.1221695283 * std::pow(t - 60, -0.0755148492);
  const double b = t >= 66 ? 255 : t <= 19 ? 0 : 138.5177312231 * std::log(t - 10) - 305.0447927307;
  return (uint32_t) clamp8(r) << 16 | (uint32_t) clamp8(g) << 8 | clamp8(b);
}
// convert-color.ts hs2rgb: the hue at the saturation, full value.
inline uint32_t hs_rgb(int hue, int saturation) {
  const double s = saturation / 100.0;
  auto f = [&](double n) {
    const double k = std::fmod(n + hue / 60.0, 6.0);
    return 255.0 - 255.0 * s * std::max(std::min({k, 4 - k, 1.0}), 0.0);
  };
  return (uint32_t) clamp8(f(5)) << 16 | (uint32_t) clamp8(f(3)) << 8 | clamp8(f(1));
}
// The disc's colour; a brightness alone has none (the key shows the level instead).
inline uint32_t rgb(const Preset &p) {
  return p.kind == Preset::WHITE ? kelvin_rgb(p.kelvin) : p.kind == Preset::COLOUR ? hs_rgb(p.hue, p.saturation) : 0;
}
// WCAG relative luminance (rgb.ts luminosity); Home Assistant gives a disc over 0.8 a hairline.
inline double luminosity(uint32_t c) {
  auto lin = [](uint32_t v) { const double s = v / 255.0; return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4); };
  return 0.2126 * lin(c >> 16 & 255) + 0.7152 * lin(c >> 8 & 255) + 0.0722 * lin(c & 255);
}
inline bool pale(const Preset &p) { return p.kind == Preset::LEVEL || luminosity(rgb(p)) > 0.8; }
// The brightness on the disc: dark ink where it reads better than white (WCAG contrast, 0.179 the luminance where both are
// equal), white on a deep colour. A brightness alone stands on the light's pale amber.
inline bool dark_ink(const Preset &p) { return p.kind == Preset::LEVEL || luminosity(rgb(p)) > 0.179; }

// ---- Which preset the light shows now ----
// The light as its tile holds it: `white` while Home Assistant reports a colour temperature (it names none in a colour
// mode), `level` its brightness 1-100.
struct Light {
  bool on = false, white = false;
  int level = 0, kelvin = 0, hue = 0, saturation = 0;
};
// The preset the light shows, within what a light rounds to (a percent, a few kelvin, a degree of hue).
inline bool shows(const Preset &p, const Light &l) {
  if (!l.on || p.level == 0 || std::abs(l.level - p.level) > 1) return false;
  if (p.kind == Preset::WHITE) return l.white && std::abs(l.kelvin - p.kelvin) <= 50;
  if (p.kind == Preset::COLOUR) {
    const int d = std::abs(l.hue - p.hue) % 360;
    return !l.white && std::min(d, 360 - d) <= 3 && std::abs(l.saturation - p.saturation) <= 3;
  }
  return true;
}
// The first preset the light shows, or -1.
inline int shown(const Presets &ps, const Light &l) {
  for (unsigned i = 0; i < ps.count; ++i) if (shows(ps.items[i], l)) return (int) i;
  return -1;
}

// ---- The action and its wish (docs/OPTIMISTIC.md, Field::PRESET) ----
// What a wish shows and compares: "80|k2700", "60|h280,41", "30|" (a brightness alone), "0|" for off. `colours`: the light
// has a colour or a white shade at all; a light that only dims keeps the brightness alone.
inline std::string value(const Preset &p) {
  std::string v = std::to_string(p.level) + "|";
  if (p.kind == Preset::WHITE) v += "k" + std::to_string(p.kelvin);
  if (p.kind == Preset::COLOUR) v += "h" + std::to_string(p.hue) + "," + std::to_string(p.saturation);
  return v;
}
inline std::string read(const Light &l, bool colours, Preset::Kind kind) {
  if (!l.on) return "0|";
  std::string v = std::to_string(l.level) + "|";
  if (!colours || kind == Preset::LEVEL) return v;
  if (l.white) return v + "k" + std::to_string(l.kelvin);
  return v + "h" + std::to_string(l.hue) + "," + std::to_string(l.saturation);
}
// A wish's value back as a preset (wish_write).
inline Preset parse(const std::string &v) {
  Preset p;
  const auto bar = v.find('|');
  if (bar == std::string::npos) return p;
  p.level = (uint8_t) std::clamp(std::atoi(v.c_str()), 0, 100);
  const std::string rest = v.substr(bar + 1);
  if (!rest.empty() && rest[0] == 'k') { p.kind = Preset::WHITE; p.kelvin = (uint16_t) std::atoi(rest.c_str() + 1); }
  else if (!rest.empty() && rest[0] == 'h') {
    p.kind = Preset::COLOUR; p.hue = (uint16_t) std::atoi(rest.c_str() + 1);
    const auto comma = rest.find(',');
    if (comma != std::string::npos) p.saturation = (uint8_t) std::atoi(rest.c_str() + comma + 1);
  }
  return p;
}
// light.turn_on's data, each rendered by Home Assistant (a number, a list): brightness_pct and the colour, if any.
struct Data { std::string key2, value2; };
inline Data colour_data(const Preset &p) {
  if (p.kind == Preset::WHITE) return {"color_temp_kelvin", std::to_string(p.kelvin)};
  if (p.kind == Preset::COLOUR) return {"hs_color", "[" + std::to_string(p.hue) + ", " + std::to_string(p.saturation) + "]"};
  return {};
}

// ---- Where the keys go ----
// A row (or a column) of `count` round keys in `length` x `across`: each `wanted` across where that fits, smaller down
// to `least`, `gap` apart at least; as many as fit, the first first. A key's touch area reaches half the gap round it.
struct Row {
  int count = 0, size = 0, gap = 0;
  int length() const { return count ? count * size + (count - 1) * gap : 0; }
};
inline Row row(int length, int across, int count, int wanted, int least, int gap) {
  Row r;
  if (count <= 0 || length <= 0 || across <= 0) return r;
  const int size = std::min(wanted, across);
  for (int n = std::min<int>(count, MAX); n > 0; --n) {
    const int fit = std::min(size, (length - (n - 1) * gap) / n);
    if (fit >= std::min(least, size)) { r.count = n; r.size = fit; r.gap = gap; return r; }
  }
  return r;
}
}  // namespace light_presets

#if defined(SD_MOD_LIGHT_PRESETS) && !defined(LIGHT_PRESETS_TEST)
#include "lvgl.h"
#include "screen_text.h"
#include "theme.h"
#include "ui_scale.h"
namespace light_presets {
// The keys' sizes on this glass: Home Assistant's 40 px disc is 6 mm here, never below 5 mm, and a key on a card a little
// larger, a finger wide.
inline int tile_key() { return ui::mm(6); }
inline int card_key() { return ui::touch_min(); }
inline int least() { return ui::mm(5); }
inline int gap() { return ui::px(ui::large() ? 10 : 6); }
// How far the ring round a key reaches outside it: its pad and its width (mark).
inline int ring_pad() { return std::max(2, ui::px(3)); }
inline int ring() { return ring_pad() + std::max(2, ui::px(2)); }

// A key's colours that follow the look (a change of look paints them again): the amber of a brightness alone, the hairline
// and the ring; the disc of a colour is the light's and stays.
inline void paint(lv_obj_t *k, const Preset &p) {
  if (!k) return;
  const lv_color_t fill = p.kind == Preset::LEVEL ? theme::color(theme::AMBER_TRACK) : lv_color_hex(rgb(p));
  lv_obj_set_style_bg_color(k, fill, 0);
  // Pressed: the disc a shade darker at once, as a tile is (firmware 0.2.111).
  lv_obj_set_style_bg_color(k, lv_color_mix(lv_color_black(), fill, 31), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(k, theme::color(theme::LINE), 0);
  lv_obj_set_style_outline_color(k, theme::color(theme::INK), 0);
}
// One round key: the disc in the preset's colour, a hairline round a pale one, its brightness in the middle where `level`
// asks for it, and a ring round the preset the light shows now. `font` writes the brightness.
inline lv_obj_t *key(lv_obj_t *parent, const Preset &p, int size, const lv_font_t *font, lv_event_cb_t tapped, void *user,
                     bool level = true) {
  auto *k = lv_obj_create(parent);
  lv_obj_remove_style_all(k);
  lv_obj_remove_flag(k, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(k, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(k, size, size);
  lv_obj_set_style_radius(k, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(k, pale(p) ? 1 : 0, 0);
  // The one the light shows: Home Assistant's focus ring, 2 px a little way round the disc.
  lv_obj_set_style_outline_width(k, 0, 0);
  lv_obj_set_style_outline_pad(k, ring_pad(), 0);
  // Its brightness, small and quiet in the middle of the disc, where the tile asks for it (preset_levels).
  if (font && level) {
    auto *l = lv_label_create(k);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, theme::color(dark_ink(p) ? theme::PRESET_INK_DARK : theme::PRESET_INK_LIGHT), 0);
    // Quiet: the colour is the key, its brightness a whisper on it.
    lv_obj_set_style_text_opa(l, LV_OPA_40, 0);
    lv_label_set_text(l, std::to_string(p.level).c_str());
    lv_obj_center(l);
  }
  if (tapped) lv_obj_add_event_cb(k, tapped, LV_EVENT_SHORT_CLICKED, user);
  paint(k, p);
  return k;
}
// The ring round the key of the preset the light shows.
inline void mark(lv_obj_t *k, bool shown) {
  if (k) lv_obj_set_style_outline_width(k, shown ? std::max(2, ui::px(2)) : 0, 0);
}
// Its touch area: half the gap round it, and never less than a finger.
inline void reach(lv_obj_t *k, int size, int gap) {
  lv_obj_set_ext_click_area(k, std::max(gap / 2, (ui::touch_min() - size) / 2));
}
}  // namespace light_presets
#endif
