<script setup lang="ts">
// One of a light's presets as the screen draws its key (light_presets::key): a disc in its colour, a hairline round a pale
// one, its brightness small and quiet in the middle where the tile shows it, and Home Assistant's focus ring a little way
// round the one the light shows. A brightness alone stands on the light's pale amber. Used on the mockup's cards and in
// the inspector's presets, so both look like the glass.
import { computed } from "vue";
import theme from "virtual:esp-screens-theme";
import { darkInk, hex, pale, presetRgb, type Preset } from "../../model/light-presets";

const props = defineProps<{ preset: Preset; lit?: boolean; level?: boolean }>();
const role = (name: string) => hex(theme.roles[name].light);
const look = computed(() => {
  const c = presetRgb(props.preset);
  return { background: c === null ? role("AMBER_TRACK") : hex(c), borderColor: pale(props.preset) ? role("LINE") : "transparent",
    color: role(darkInk(props.preset) ? "PRESET_INK_DARK" : "PRESET_INK_LIGHT"), outlineColor: role("INK") };
});
</script>

<template>
  <span class="preset-disc" :class="{ lit }" :style="look"><span v-if="level" class="preset-level">{{ preset.b }}</span></span>
</template>

<style scoped>
.preset-disc { box-sizing: border-box; display: grid; place-items: center; border-radius: 50%; border: 1px solid; line-height: 1; outline: 0 solid; outline-offset: var(--ring-pad, 3px); }
.preset-disc.lit { outline-width: var(--ring-w, 2px); }
/* Quiet: the colour is the key, its brightness a whisper on it (LV_OPA_40). */
.preset-level { opacity: .4; font-variant-numeric: tabular-nums; }
</style>
