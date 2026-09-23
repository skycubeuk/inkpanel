# Designing for a 1-bit e-paper panel

What LVGL can and cannot do on a two-colour electrophoretic display, written
against firmware that actually runs. Most of this is not in the ESPHome or LVGL
docs, because neither assumes a display with no grey and a two-thirds-of-a-second
refresh.

Applies to the X4 Pro (480 x 800 portrait, UC8179) but almost all of it holds for
any 1-bit e-paper panel driven by LVGL.

## The canvas

| | |
|---|---|
| Pixels | 480 x 800 (panel is 800 x 480; LVGL rotates 270°) |
| Colours | **2.** Black ink, white paper. Nothing between |
| Input | Capacitive touch. Taps fire on **finger-down**, not release |
| Memory | 8 MB PSRAM; a full-screen LVGL buffer is 768 KB. Assets are not the constraint |

Colours are inverted: the display treats any nonzero value as ink, so in LVGL
`0x000000` is **paper** and `0xFFFFFF` is **ink**.

## Hard limits

**1. No grey, ever.** The framebuffer is 1 bit and conversion is "any non-zero
pixel becomes ink". So all of these collapse to solid black: `opa` and `bg_opa`
below 100 %, `bg_grad`, shadows, blurs, `blend_mode`, tints, "subtle" separators,
10 %-grey panels. A 1 %-alpha pixel is as black as a 100 % one. Do not design with
tone - design with inversion.

**2. Anti-aliasing becomes fat black.** Every partially covered edge pixel goes
full ink, never paper. Curves, rounded corners and thin diagonals come out about a
pixel heavier and visibly stair-stepped. Keep `radius` at 8 or below. LVGL's
default theme gives sliders a full stadium radius, which is worth overriding.

**3. Fonts must stay 1-bit** (`bpp: 1`, ESPHome's default). Anti-aliased fonts are
*worse* here: the grey coverage all becomes ink and glyphs go blobby. Below about
16-18 px a 1-bit face loses its shape, so there is no elegant small type. This
project uses 18 / 22 / 28 / 34 / 48 px.

**4. Every refresh is the whole screen.** Windowed update is not implemented for
this controller, so changing one pixel costs the same as changing all of them.
Designing "a small area that updates often" buys nothing.

**5. No animation, no transitions, no smooth scrolling, no drag.** Each frame
would be a full refresh. LVGL's default theme animations are stripped at boot
(`esphome/ui_helpers.h`) because otherwise a single tap costs two refreshes: one
for the pressed state, one for the result.

**6. Widget trees are fixed at compile time.** Text, values, visibility and styles
can change at runtime; the *number* of widgets cannot follow Home Assistant.
(`lvgl.list.add/remove` is the one exception.) Design for a fixed set of slots -
which is why the generator paginates at build time.

**7. Ghosting is real.** Fast differential refreshes leave residue. Large solid
black areas ghost worst, and are also the most expensive thing to flip.

## The time budget

| Step | Cost |
|---|---|
| LVGL render, one tile's state | ~42 ms |
| LVGL render, whole page | ~205-240 ms |
| Fast panel refresh | **~670 ms**, whole screen |
| Half refresh (ghost clean) | **~1.65 s** |
| Tap -> refresh starts | ~45 ms |
| Tap -> settled pixels | ~0.7 s tile, ~0.9 s page change |

**Aim for one refresh per user action.** Two-stage feedback doubles the wait. This
project flips a tile optimistically on finger-down so the single refresh carries
both the press feedback and the result, then reconciles when Home Assistant
confirms.

**Clear ghosts on idle, not on tap.** Rather than interrupt a tap with a 1.65 s
clearing pass, count the fast refreshes and do the clean 4 seconds after the last
touch. Exclude redraws that do not really ghost - a changing battery number
should not eventually trigger a flash on an idle panel.

## Things that will bite you

**`glyphs:` replaces ESPHome's default ASCII set; `extras:` adds to it.** Adding a
degree sign with `glyphs: ["°"]` silently removes every letter and digit from
that font. To add characters to a normal text font, point `extras` back at the
same font:

```yaml
- file: "gfonts://Roboto"
  id: font_m
  size: 28
  extras:
    - file: "gfonts://Roboto"
      glyphs: ["°", "−"]   # degree sign, real minus
```

**A glyph must be in the font it is drawn with.** Compile one icon into `icon_l`
and draw it with `icon_m` and you get an empty box - no build error, no warning,
just a wrong panel. Generating glyph lists from the same source that lays out the
pages removes the possibility.

**`flex_align_cross` is not enough to centre a column.** It positions items
*within* the track; a single-column track still sits at the start of the
container unless you also set `flex_align_track: CENTER`. Symptom: icon and label
hug the left edge of a tile that is obviously wide enough.

**A slider knob and a flush fill are mutually exclusive.** The knob needs half its
width of travel at each end, and the padding that buys it is the same padding that
stops the indicator reaching the left edge. Give the knob room and the fill
detaches from the end; take it away and the knob overhangs onto whatever contains
it. On a 1-bit panel the answer is to stop drawing the knob: the fill alone reads
as a level, and there is nothing left to collide.

**A status bar on `top_layer` shows on every page** - one widget instead of one
per page. But anything that counted on it being page-specific has to change too:
a ghost-clean counter conditioned on "are we on Home" is now wrong, because that
redraw happens everywhere.

**Refresh on the displayed value, not the underlying one.** A sensor moving 1.42
-> 1.44 kW shown as `%.1f` is not a change. Compare what the format would print
and skip the redraw otherwise. On a panel that is idle most of the time this is
the difference between zero refreshes an hour and dozens.

**`on_draw_end` fires every tick,** not only after a flush, so gate the panel
update behind a dirty flag set in `on_draw_start`.

**Do not enable `update_when_display_idle`.** A polling display never reports
idle, so LVGL never renders.

## What you do have - do not under-use it

The LVGL binding is nearly complete. The poverty is in the *output*, not the
toolkit.

- **Widgets**: label, button, buttonmatrix, checkbox, switch, slider, arc, bar,
  dropdown, roller, spinbox, table, list, tileview, tabview, msgbox, meter, chart,
  canvas, qrcode, img, animimg, keyboard, textarea, led, container.
- **Layout**: full flex and grid, padding, margins, min/max, percentages.
- **Styling**: per-side borders, outlines, radius, letter/line spacing, transforms,
  and per-state styles (`checked`, `pressed`, `disabled`) plus reusable
  `style_definitions`.
- **Icons**: Material Design Icons compiled into a font at any size, crisp. The
  cheapest way to add visual language to a two-colour screen.
- **Interaction**: tap, long press, double click, and swipe.
- **Inversion is free.** White-on-black costs exactly what black-on-white costs,
  which makes "this one is on" readable across a room.

## Rules of thumb

1. **Emphasis is inversion.** There is no third weight. A thing is ink-on-paper or
   paper-on-ink, and that is the whole state system.
2. **Solid shapes over outlines and tone.** On = filled. Off = a 2-3 px ring.
3. **Nothing smaller than 64 px is tappable**, and nothing below 18 px is legible.
4. **Prefer chunky icons.** `mdi:thermostat` is a dial with fine interior detail
   and turns to mush at 36 px; `mdi:thermometer-lines` survives.
5. **One tap, one refresh.** If a design needs two, redesign it.
6. **Anything that changes on its own is a decision, not a freebie.** A live clock
   is 60 refreshes an hour plus ghost cleans.

## If a design needs more

**4-level greyscale** is supported by the hardware and the FreeInk SDK
(`displayGrayscaleBase` / `displayGray`, 2-bitplane). Using it means teaching the
display component a 2-bit path and mapping LVGL's output onto four levels, plus a
slower waveform per frame. Days of work, and it slows every refresh. Only worth
raising if a design genuinely depends on tone.

**Windowed/partial refresh** would need a `displayWindow` implementation for this
controller and would make small updates far faster than 670 ms. Not currently
possible.
