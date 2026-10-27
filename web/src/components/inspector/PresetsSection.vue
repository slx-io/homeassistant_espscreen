<script setup lang="ts">
// A light's presets (module light_presets, docs/LIGHT_PRESETS.md): up to four round keys under the control choice, drawn
// as the screen draws them. A key opens its preset: a white shade, a colour or the brightness alone (what the light
// takes, from its supported_color_modes), its brightness, the light's own state at a click, and a way to take it off.
// The + adds one from what the light shows now, so one click gives a key that does something. Keys are dragged into
// another order. Every change applies live and is a step of undo; a drag of a slider is one step.
import { computed, ref, watch } from "vue";
import { PopoverContent, PopoverPortal, PopoverRoot, PopoverTrigger } from "reka-ui";
import theme from "virtual:esp-screens-theme";
import { t } from "../../i18n";
import { fromLight, hex, hsRgb, kelvinRange, kelvinRgb, kindOf, levelShown, lightModes, lightNow, MAX, presetsOf, shown, startPreset,
  type Preset, type PresetKind } from "../../model/light-presets";
import type { Tile } from "../../types";
import { setTileOption } from "../../editor/tiles";
import { useSortableRows } from "../../composables/useSortableRows";
import { useDocumentStore } from "../../stores/document";
import { useEntitiesStore } from "../../stores/entities";
import PresetDisc from "../tile/PresetDisc.vue";
import Segmented from "../Segmented.vue";
import Icon from "../ui/Icon.vue";
import SwitchRow from "../ui/SwitchRow.vue";
import UiSlider from "../ui/UiSlider.vue";

const props = defineProps<{ tile: Tile }>();
const doc = useDocumentStore();
const entities = useEntitiesStore();
const T = (key: string, values?: Record<string, unknown>) => (values ? t(`editor.tile.presets.${key}`, values) : t(`editor.tile.presets.${key}`));

const live = computed(() => entities.liveOf(props.tile.entity));
const attributes = computed(() => live.value?.a || {});
const modes = computed(() => lightModes(attributes.value));
const range = computed(() => kelvinRange(attributes.value));
const presets = computed(() => presetsOf(props.tile.options));
const levels = computed(() => props.tile.options?.preset_levels === true);
const lit = computed(() => shown(presets.value, lightNow(live.value)));

// Which preset is open in its popover (-1: none).
const open = ref(-1);
watch(() => presets.value.length, (n) => { if (open.value >= n) open.value = -1; });
const field = (i: number) => `presets:${props.tile.id}:${i}`;
function write(list: Preset[], edit?: string) { setTileOption(props.tile, "presets", list, edit); }
function change(i: number, preset: Preset, edit?: string) {
  const list = presets.value.map((p, n) => (n === i ? preset : p));
  write(list, edit);
}

// ---- Adding, taking off and moving ----
function add() {
  if (presets.value.length >= MAX) return;
  // What the light shows now, or full brightness in its own warm white: a key that does something at once.
  let next = live.value?.state === "on" ? fromLight(live.value) : startPreset(live.value);
  // The light as it is is a preset already: the same colour at another brightness, so the new key does something else.
  const same = (p: Preset) => JSON.stringify({ ...p, b: 0 }) === JSON.stringify({ ...next, b: 0 });
  if (presets.value.some((p) => same(p) && p.b === next.b)) {
    const b = [100, 50, 20, 75, 10].find((level) => !presets.value.some((p) => same(p) && p.b === level));
    if (b) next = { ...next, b };
  }
  const index = presets.value.length;
  write([...presets.value, next]);
  // Its popover opens once its key is drawn, after the click that added it has gone by (or it would close it again).
  setTimeout(() => { open.value = index; });
}
function remove(i: number) {
  open.value = -1;
  write(presets.value.filter((_, n) => n !== i));
}
const sort = useSortableRows<Preset>({ rows: ".lp-row .lp-swatch[data-index]", items: () => presets.value, commit: (list) => write(list), axis: "x",
  enabled: () => open.value < 0 });
const shownList = computed(() => sort.live.value ?? presets.value);

// ---- One preset ----
const kinds = computed(() => {
  const out: [PresetKind, string][] = [];
  if (modes.value.white) out.push(["white", T("white")]);
  if (modes.value.colour) out.push(["colour", T("colour")]);
  out.push(["level", T("level")]);
  return out;
});
function pickKind(i: number, kind: PresetKind) {
  const p = presets.value[i], now = lightNow(live.value);
  if (!p || kind === kindOf(p)) return;
  const [lo, hi] = range.value;
  if (kind === "white") change(i, { b: p.b, k: Math.min(hi, Math.max(lo, now.white ? now.kelvin : 2700)) });
  else if (kind === "colour") change(i, { b: p.b, h: !now.white && now.saturation ? now.hue % 360 : 30, s: !now.white && now.saturation ? now.saturation : 80 });
  else change(i, { b: p.b });
}
const set = (i: number, key: "b" | "k" | "h" | "s", value: number) => change(i, { ...presets.value[i], [key]: Math.round(value) }, field(i));
function useLight(i: number) { change(i, fromLight(live.value)); }
const lightOn = computed(() => live.value?.state === "on");

// ---- The tracks, painted with the colours they set (the light's own whites, Home Assistant's hues) ----
const stops = (colours: number[]) => `linear-gradient(to right, ${colours.map(hex).join(", ")})`;
const whiteTrack = computed(() => {
  const [lo, hi] = range.value;
  return stops(Array.from({ length: 7 }, (_, n) => kelvinRgb(lo + ((hi - lo) * n) / 6)));
});
const hueTrack = stops(Array.from({ length: 13 }, (_, n) => hsRgb(n * 30, 100)));
const saturationTrack = (h: number) => stops([hsRgb(h, 0), hsRgb(h, 100)]);
const amberTrack = stops([theme.roles.AMBER_TRACK.light, theme.ha.AMBER]);
const knob = (p: Preset) => { const c = p.k !== undefined ? kelvinRgb(p.k) : p.h !== undefined ? hsRgb(p.h, p.s ?? 0) : null; return c === null ? undefined : hex(c); };
</script>

<template>
  <div class="lp" id="tile-presets">
    <div class="lp-row" role="list" :aria-label="T('label')" @click.capture="sort.click">
      <PopoverRoot v-for="(preset, i) in shownList" :key="i" :open="open === i" @update:open="(on) => (open = on ? i : open === i ? -1 : open)">
        <PopoverTrigger as-child>
          <button type="button" class="lp-swatch" role="listitem" :data-index="i" :class="{ dragging: sort.drag.value.active && sort.drag.value.index === i }"
            :aria-label="T('swatch_aria', { n: i + 1, b: preset.b })" @pointerdown="(e) => sort.down(e, i)">
            <PresetDisc :preset="preset" :lit="i === lit" :level="levelShown(preset, levels)" />
          </button>
        </PopoverTrigger>
        <PopoverPortal>
          <PopoverContent class="ui-popover lp-pop" align="start" :side-offset="8" :collision-padding="10" :aria-label="T('preset', { n: i + 1 })">
            <div class="lp-pop-head">
              <PresetDisc class="lp-pop-disc" :preset="preset" :level="true" />
              <b>{{ T("preset", { n: i + 1 }) }}</b>
            </div>
            <Segmented v-if="kinds.length > 1" :choices="kinds" :value="kindOf(preset)" @pick="(k: PresetKind) => pickKind(i, k)" />
            <label v-if="kindOf(preset) === 'white'" class="lp-field">
              <span>{{ T("white_shade") }}<small>{{ T("kelvin", { k: preset.k }) }}</small></span>
              <UiSlider :model-value="preset.k!" :min="range[0]" :max="range[1]" :step="10" :track="whiteTrack" :knob="knob(preset)" :label="T('white_shade')"
                @start="doc.beginFieldEdit(field(i))" @commit="doc.endFieldEdit()" @update:model-value="(v) => set(i, 'k', v)" />
            </label>
            <template v-if="kindOf(preset) === 'colour'">
              <label class="lp-field">
                <span>{{ T("hue") }}<small>{{ T("degrees", { h: preset.h }) }}</small></span>
                <UiSlider :model-value="preset.h!" :min="0" :max="359" :track="hueTrack" :knob="knob(preset)" :label="T('hue')"
                  @start="doc.beginFieldEdit(field(i))" @commit="doc.endFieldEdit()" @update:model-value="(v) => set(i, 'h', v)" />
              </label>
              <label class="lp-field">
                <span>{{ T("saturation") }}<small>{{ T("percent", { n: preset.s }) }}</small></span>
                <UiSlider :model-value="preset.s ?? 0" :min="0" :max="100" :track="saturationTrack(preset.h ?? 0)" :knob="knob(preset)" :label="T('saturation')"
                  @start="doc.beginFieldEdit(field(i))" @commit="doc.endFieldEdit()" @update:model-value="(v) => set(i, 's', v)" />
              </label>
            </template>
            <label class="lp-field">
              <span>{{ T("brightness") }}<small>{{ T("percent", { n: preset.b }) }}</small></span>
              <UiSlider :model-value="preset.b" :min="1" :max="100" :track="amberTrack" :knob="hex(theme.ha.AMBER)" :label="T('brightness')"
                @start="doc.beginFieldEdit(field(i))" @commit="doc.endFieldEdit()" @update:model-value="(v) => set(i, 'b', v)" />
            </label>
            <div class="lp-actions">
              <button type="button" class="btn quiet mini lp-use" :disabled="!lightOn" :title="lightOn ? undefined : T('use_light_off')" @click="useLight(i)">
                <Icon name="crosshairs" />{{ T("use_light") }}</button>
              <button type="button" class="btn quiet mini danger-soft lp-remove" @click="remove(i)"><Icon name="delete-outline" />{{ t("editor.common.remove") }}</button>
            </div>
          </PopoverContent>
        </PopoverPortal>
      </PopoverRoot>
      <button v-if="presets.length < MAX" type="button" class="lp-add" :class="{ first: !presets.length }" :aria-label="T('add')" :title="T('add')" @click="add">
        <span class="lp-add-disc"><Icon name="plus" /></span><span v-if="!presets.length" class="lp-add-words">{{ T("add_first") }}</span>
      </button>
    </div>
    <SwitchRow v-if="presets.length" class="lp-levels" :label="T('levels')" :description="T('levels_hint')" :model-value="levels"
      @update:model-value="(on: boolean) => setTileOption(tile, 'preset_levels', on)" />
    <small class="help">{{ presets.length ? T("hint") : T("hint_empty") }}</small>
  </div>
</template>

<style scoped>
.lp { display: grid; gap: 8px; padding: 2px 0 6px; }
.lp-row { display: flex; flex-wrap: wrap; align-items: center; gap: 10px; padding: 6px 4px; }
.lp-swatch { padding: 0; border: 0; background: none; border-radius: 50%; cursor: pointer; touch-action: none; }
.lp-swatch .preset-disc { width: 36px; height: 36px; font-size: 12px; --ring-pad: 3px; --ring-w: 2px; }
.lp-swatch:focus-visible { outline: 2px solid var(--focus); outline-offset: 6px; }
.lp-swatch.dragging { opacity: .6; }
.lp-add { display: inline-flex; align-items: center; gap: 8px; padding: 0; border: 0; background: none; color: var(--muted); cursor: pointer; font: inherit; }
.lp-add-disc { box-sizing: border-box; width: 36px; height: 36px; border-radius: 50%; border: 1.5px dashed var(--line-strong); display: grid; place-items: center; font-size: 18px; }
.lp-add:hover .lp-add-disc, .lp-add:focus-visible .lp-add-disc { border-color: var(--accent); color: var(--accent); }
.lp-add.first { color: var(--accent); font-weight: 600; }
.lp-add.first .lp-add-disc { border-color: var(--accent); background: var(--accent-soft); }
.lp-pop { width: 260px; padding: 12px; display: grid; gap: 12px; }
.lp-pop-head { display: flex; align-items: center; gap: 10px; }
.lp-pop-disc { width: 28px; height: 28px; font-size: 10px; }
.lp-field { display: grid; gap: 6px; }
.lp-field > span { display: flex; justify-content: space-between; font-size: 12px; font-weight: 600; }
.lp-field small { font-weight: 400; color: var(--muted); font-variant-numeric: tabular-nums; }
.lp-actions { display: flex; justify-content: space-between; gap: 6px; }
</style>
