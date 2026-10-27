// A light's presets (module light_presets, docs/LIGHT_PRESETS.md) in the editor: up to four round keys on a light's tile,
// each a brightness with a white shade or a colour, kept in the tile's options as `presets` and checked by the add-on's
// light_presets.py. The colours, the match with the light's state and the row the keys stand in are the firmware's
// (module_light_presets.h), ported here number for number; tests/test_editor_parity.py compares the two.
import { touchMin, uiScale, type Shape } from "./ui-scale";

export const MAX = 4;
/** The white shades a preset may name, in kelvin (light_presets.KELVIN). */
export const KELVIN = [1000, 15000] as const;
export const OPTION = "presets";
/** Whether each disc shows its brightness (light_presets.LEVELS): stored only when it does. */
export const LEVELS = "preset_levels";

/** A preset as the tile's options keep it: {"b": 80, "k": 2700}, {"b": 60, "h": 280, "s": 41}, {"b": 30}. */
export type Preset = { b: number; k?: number; h?: number; s?: number };
export type PresetKind = "level" | "white" | "colour";
export const kindOf = (p: Preset): PresetKind => p.k !== undefined ? "white" : p.h !== undefined ? "colour" : "level";

const whole = (value: unknown, low: number, high: number) =>
  typeof value === "number" && Number.isInteger(value) && value >= low && value <= high ? value : null;
/** One preset in its stored form, or null where the add-on refuses it (light_presets.preset). */
export function cleanPreset(item: unknown): Preset | null {
  if (!item || typeof item !== "object" || Array.isArray(item)) return null;
  const o = item as Record<string, unknown>;
  if (Object.keys(o).some((key) => !["b", "k", "h", "s"].includes(key))) return null;
  const b = whole(o.b, 1, 100);
  if (b === null) return null;
  if ("k" in o) {
    const k = whole(o.k, KELVIN[0], KELVIN[1]);
    return k === null || "h" in o || "s" in o ? null : { b, k };
  }
  if ("h" in o || "s" in o) {
    const h = whole(o.h, 0, 360), s = whole(o.s, 0, 100);
    return h === null || s === null ? null : { b, h: h % 360, s };
  }
  return { b };
}
/** The presets a tile keeps, those the screen can take. */
export function presetsOf(options: Record<string, unknown> | undefined): Preset[] {
  const list = options?.[OPTION];
  return Array.isArray(list) ? list.slice(0, MAX).map(cleanPreset).filter((p): p is Preset => p !== null) : [];
}

// ---- Its colour, as Home Assistant draws a favourite colour ----
const clamp8 = (v: number) => Math.round(Math.min(255, Math.max(0, v)));
/** convert-light-color.ts temperature2rgb (light_presets::kelvin_rgb), as 0xRRGGBB. */
export function kelvinRgb(kelvin: number) {
  const t = kelvin / 100;
  const r = t <= 66 ? 255 : 329.698727446 * Math.pow(t - 60, -0.1332047592);
  const g = t <= 66 ? 99.4708025861 * Math.log(t) - 161.1195681661 : 288.1221695283 * Math.pow(t - 60, -0.0755148492);
  const b = t >= 66 ? 255 : t <= 19 ? 0 : 138.5177312231 * Math.log(t - 10) - 305.0447927307;
  return (clamp8(r) << 16) | (clamp8(g) << 8) | clamp8(b);
}
/** convert-color.ts hs2rgb (light_presets::hs_rgb): the hue at the saturation, full value. */
export function hsRgb(hue: number, saturation: number) {
  const s = saturation / 100;
  const f = (n: number) => {
    const k = (n + hue / 60) % 6;
    return 255 - 255 * s * Math.max(Math.min(k, 4 - k, 1), 0);
  };
  return (clamp8(f(5)) << 16) | (clamp8(f(3)) << 8) | clamp8(f(1));
}
/** The disc's colour; a brightness alone has none (the key stands on the light's pale amber). */
export const presetRgb = (p: Preset): number | null =>
  p.k !== undefined ? kelvinRgb(p.k) : p.h !== undefined ? hsRgb(p.h, p.s ?? 0) : null;
/** WCAG relative luminance (rgb.ts luminosity). */
export function luminosity(c: number) {
  const lin = (v: number) => { const s = v / 255; return s <= 0.04045 ? s / 12.92 : Math.pow((s + 0.055) / 1.055, 2.4); };
  return 0.2126 * lin((c >> 16) & 255) + 0.7152 * lin((c >> 8) & 255) + 0.0722 * lin(c & 255);
}
/** Home Assistant gives a disc over 0.8 a hairline; a brightness alone stands on pale amber and has one too. */
export const pale = (p: Preset) => { const c = presetRgb(p); return c === null || luminosity(c) > 0.8; };
/** Dark ink where it reads better than white (0.179: where both are equal), white on a deep colour. */
export const darkInk = (p: Preset) => { const c = presetRgb(p); return c === null || luminosity(c) > 0.179; };
/** Whether a disc writes its brightness (Presets::level_shown): where the tile asks for it, and always on a brightness alone,
 * which has no colour to tell it by. */
export const levelShown = (p: Preset, levels: boolean) => levels || kindOf(p) === "level";
export const hex = (c: number) => `#${c.toString(16).padStart(6, "0")}`;

// ---- Which preset the light shows now ----
/** The light as its tile holds it (runtime_tiles preset_light): `white` while Home Assistant reports a colour
 * temperature, `level` its brightness 1-100 (100 where it reports none). */
export type LightNow = { on: boolean; white: boolean; level: number; kelvin: number; hue: number; saturation: number };
export function lightNow(live: { state?: string; a?: Record<string, any> } | null | undefined): LightNow {
  const a = live?.a || {}, brightness = Number(a.brightness), kelvin = Number(a.color_temp_kelvin);
  const hs = Array.isArray(a.hs_color) ? a.hs_color.map(Number) : [];
  const white = a.color_temp_kelvin != null && Number.isFinite(kelvin);
  return { on: live?.state === "on", white,
    level: a.brightness != null && Number.isFinite(brightness) ? Math.min(100, Math.max(1, Math.round((brightness * 100) / 255))) : 100,
    kelvin: white ? Math.round(Math.min(15000, Math.max(1000, kelvin))) : 0,
    hue: Number.isFinite(hs[0]) ? Math.round(Math.min(360, Math.max(0, hs[0]))) : 0,
    saturation: Number.isFinite(hs[0]) && Number.isFinite(hs[1]) ? Math.round(Math.min(100, Math.max(0, hs[1]))) : 0 };
}
/** The preset the light shows (light_presets::shows): a percent of brightness, 50 K of white, 3 of hue and saturation. */
export function shows(p: Preset, l: LightNow) {
  if (!l.on || Math.abs(l.level - p.b) > 1) return false;
  if (p.k !== undefined) return l.white && Math.abs(l.kelvin - p.k) <= 50;
  if (p.h !== undefined) {
    const d = Math.abs(l.hue - p.h) % 360;
    return !l.white && Math.min(d, 360 - d) <= 3 && Math.abs(l.saturation - (p.s ?? 0)) <= 3;
  }
  return true;
}
/** The first preset the light shows, or -1. */
export const shown = (list: readonly Preset[], l: LightNow) => list.findIndex((p) => shows(p, l));

// ---- Where the keys go ----
export type Row = { count: number; size: number; gap: number; length: number };
/** A row of `count` round keys in `length` x `across` (light_presets::row): each `wanted` where that fits, smaller down to
 * `least`, `gap` apart; as many as fit, the first first. */
export function row(length: number, across: number, count: number, wanted: number, least: number, gap: number): Row {
  const none = { count: 0, size: 0, gap: 0, length: 0 };
  if (count <= 0 || length <= 0 || across <= 0) return none;
  const size = Math.min(wanted, across);
  for (let n = Math.min(count, MAX); n > 0; --n) {
    const fit = Math.min(size, Math.trunc((length - (n - 1) * gap) / n));
    if (fit >= Math.min(least, size)) return { count: n, size: fit, gap, length: n * fit + (n - 1) * gap };
  }
  return none;
}
/** ui::mm on this glass. */
export const mm = (shape: Shape, n: number) => Math.floor(((shape.dpi || (shape.look === "compact" ? 143 : 170)) * n + 12) / 25);
/** The keys' sizes on this glass (module_light_presets.h): Home Assistant's 40 px disc is 6 mm, never below 5 mm, a card's a
 * finger wide; the ring stands its pad outside the disc and is `ringWidth` wide, `ring` the room it takes round a key. */
export function presetSizes(shape: Shape) {
  const { large, px } = uiScale(shape), ringPad = Math.max(2, px(3)), ringWidth = Math.max(2, px(2));
  return { tileKey: mm(shape, 6), cardKey: touchMin(shape), least: mm(shape, 5), gap: px(large ? 10 : 6), ringPad, ringWidth, ring: ringPad + ringWidth };
}
/** The keys a light's panel lays out in `room` x `across` (runtime_tiles layout_panel, presets): the row inside the ring's
 * room, its width and height with that room round it, and each key's place in it. */
export function presetPanel(shape: Shape, count: number, room: number, across: number) {
  const z = presetSizes(shape), r = row(room - 2 * z.ring, across, count, z.tileKey, z.least, z.gap), ring = r.count ? z.ring : 0;
  return { row: r, ring, width: r.length + 2 * ring, height: r.count ? r.size + 2 * ring : 0,
    keys: Array.from({ length: r.count }, (_, n) => ({ x: ring + n * (r.size + r.gap), y: ring, size: r.size })) };
}

// ---- What the light can be set to ----
/** What a light's supported_color_modes allow a preset: a white shade (color_temp) and a colour (hs, rgb, rgbw, rgbww, xy). */
export function lightModes(a: Record<string, any> | undefined) {
  const modes: string[] = Array.isArray(a?.supported_color_modes) ? a!.supported_color_modes.map(String) : [];
  return { white: modes.includes("color_temp"), colour: modes.some((m) => ["hs", "rgb", "rgbw", "rgbww", "xy"].includes(m)) };
}
/** The white shades the light takes (its min and max colour temperature), within what a screen keeps. */
export function kelvinRange(a: Record<string, any> | undefined): [number, number] {
  const low = Number(a?.min_color_temp_kelvin), high = Number(a?.max_color_temp_kelvin);
  const lo = Number.isFinite(low) && low > 0 ? Math.max(KELVIN[0], Math.round(low)) : 2000;
  const hi = Number.isFinite(high) && high > 0 ? Math.min(KELVIN[1], Math.round(high)) : 6500;
  return lo < hi ? [lo, hi] : [2000, 6500];
}
/** A preset of what the light shows now: its brightness, and its white shade or colour where it has one to set. */
export function fromLight(live: { state?: string; a?: Record<string, any> } | null | undefined): Preset {
  const a = live?.a || {}, l = lightNow(live), modes = lightModes(a), b = live?.state === "on" ? l.level : 100;
  if (live?.state === "on" && l.white && modes.white) return { b, k: Math.min(KELVIN[1], Math.max(KELVIN[0], l.kelvin)) };
  if (live?.state === "on" && Array.isArray(a.hs_color) && modes.colour) return { b, h: l.hue % 360, s: l.saturation };
  return startPreset(live);
}
/** A sensible first preset where the light is off: full brightness, in the light's own warm white where it has whites. */
export function startPreset(live: { state?: string; a?: Record<string, any> } | null | undefined): Preset {
  const a = live?.a || {}, modes = lightModes(a);
  if (modes.white) { const [lo, hi] = kelvinRange(a); return { b: 100, k: Math.min(hi, Math.max(lo, 2700)) }; }
  return { b: 100 };
}
