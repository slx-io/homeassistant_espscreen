# A light's presets

Round keys on a light's tile and on its card that each set the light to a colour or a white shade and a brightness with
one tap. They are Home Assistant's favourite colours (the round keys in a light's dialog) with the brightness they go
with, and they work the same on a light group: a tap sets every lamp of the group at once.

Every board with 8 MB of flash or more draws them. The CYD and the other boards with 4 MB have no room left for them
(docs/MODULES.md) and keep the brightness slider.

## Making them

In the editor, select the light's tile and choose **Presets** under **Direct control on the tile**, or **Brightness
slider and presets** on a tile two rows high. A tile has at most four presets. Each is:

- a **white shade** in kelvin, within the light's own range, and a brightness;
- a **colour** (a hue and a saturation, as Home Assistant's `hs_color`) and a brightness;
- or a **brightness** alone, for a light that only dims: it keeps the colour the light has.

The editor offers only what the light can do. A new preset starts from what the light shows now, so the quickest way
to make one is to set the light the way you like it and add a preset.

**Show brightness on presets** writes each preset's brightness, small and quiet, in the middle of its key. It is off by
default: the colour is what you look for. A preset of a brightness alone always shows its number, since it has no
colour to tell it by.

## On the screen

| Where | What it shows |
|---|---|
| A tile one row high, two cells wide | The keys at the right, where the slider stands otherwise. |
| A tile of one cell | Its name with the keys under it. |
| A tile two rows high | **Brightness slider and presets**: the slider with the keys under it. |
| The light's card (a hold on the tile) | A row **Presets** under the colour, the white shade and the brightness, or on glass much wider than high a column at their right, the keys one under the other. A light that only dims shows them under its percentage. |

The key of the preset the light shows now has a ring round it, as Home Assistant marks a favourite colour: its
brightness within a percent, its white shade within 50 K, or its colour within a few degrees. A tap shows the new
colour and brightness at once on every tile and card of the light, and goes to Home Assistant as one
`light.turn_on` with `brightness_pct` and `color_temp_kelvin` or `hs_color` (docs/OPTIMISTIC.md, the field `PRESET`).

As many keys as fit the room are shown, the first first, each at least 5 mm across: four fit a tile of one cell on a
four-inch screen.

## How it is built

- **Catalogue**: `catalogue/light.yaml` has the controls `presets` (laid out by the card's room, a card of one cell
  included) and `brightness_presets` (two rows; one row high it draws the presets). A screen whose hello lacks
  `light_presets` gets the brightness slider (`screen: {feature: light_presets, else: brightness}`).
- **Add-on**: `screen_manager/app/light_presets.py` checks the tile's `presets` and `preset_levels`
  (`core.validate_layout`), the page document keeps them under the tile's `interaction`, and a tile event may set them
  (as a list or JSON). The screen gets them in the tile's options as they are stored. The editor learns whether a
  board draws them from `screen.light_presets` (`server.py`, the board's `modules`).
- **Firmware**: the module `light_presets` (`components/smart_display/module_light_presets.h`): the colours as Home
  Assistant computes them (`temperature2rgb`, `hs2rgb`, its luminance rule for the hairline), which preset the light
  shows, the wish's value and the row of keys, tested on a PC in `tests/test_light_presets.cpp`; and the key itself.
  `runtime_tiles.h` draws the keys as a panel of the tile (`layout_panel`, `preset_keys`, `preset_cell` for a card of
  one cell, `render_tall` for the second row), in the colour card (`colour_presets_draw`) and in the card of a light that
  only dims (`render_light_detail`, `light_card.h`). `page_receiver.cpp` reads the options into `Tile::presets`; a light's
  white shade (`Tile::white`) is read from whether Home Assistant reports a colour temperature.
- **Editor**: the inspector's presets, the mockup's keys and the model's port of the firmware's rules (`web/src`).

A change to the presets touches: `catalogue/light.yaml` (then `tools/generate_catalogue.py`),
`screen_manager/app/light_presets.py`, `core.py` (`TILE_EVENT_OPTIONS`, `tile_options`), `page_layout.py`
(`INTERACTION`), `module_light_presets.h`, `runtime_tiles.h`, `page_receiver.cpp`, `light_controls.h`, `light_card.h`,
`theme.h` (`PRESET_INK_DARK`, `PRESET_INK_LIGHT`), the editor's inspector and mockup, the translations
(`screen.light.presets`, the controls' labels, the editor's words), and the tests `tests/test_light_presets.py`,
`tests/test_light_presets.cpp` and the editor's specs.
