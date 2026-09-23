# Configuring the panel

Everything the panel shows lives in `panel.yaml`. There are no coordinates, font
sizes or icon codepoints in it: `tools/panel build` works those out, and splits a
page that does not fit onto a second page rather than squeezing it.

```
panel.yaml  ->  tools/panel build  ->  esphome/<name>.yaml  ->  firmware
```

Never edit `esphome/<name>.yaml`. It is generated and git-ignored, and the next
build overwrites it.

## Top level

```yaml
panel:
  name: hallway-panel          # lowercase, digits, hyphens. Becomes the hostname.
  friendly_name: Hallway Panel # shown in Home Assistant
```

## Pages

Each entry under `pages:` becomes one destination tile on Home. Four types:

| type | what it is | per page |
|---|---|---|
| `lights` | a tile per light, tap toggles, optional `-`/`+` brightness | 8 with steppers, 12 without |
| `climate` | a card per thermostat: current, target, step buttons | 2 |
| `media` | now-playing card plus transport and volume | 1 |
| `readouts` | a card per sensor: icon, label, value | 6, or 12 with `columns: 2` |

Anything past those limits moves to a second page automatically, titled
`Solar 2`, `Lights 2` and so on, each with its own tile on Home.

### lights

```yaml
- type: lights
  title: Lights
  icon: mdi:lightbulb
  entities:
    - { entity: light.kitchen, label: "Kitchen & Dining", dim: true }
    - { entity: light.office,  label: "Office", dim: true, step: 10 }
    - { entity: light.porch,   label: "Porch",
        icon: mdi:outdoor-lamp, icon_off: mdi:outdoor-lamp-off }
```

`dim: true` adds a `-`/`+` pair under the tile, stepping `step` percent (default
20). Without it the tile is toggle-only and the row is shorter, so more fit.

The tile inverts when the light is on and the icon swaps from `icon_off` to
`icon` in the same handler, so both changes land in one screen refresh.

### climate

```yaml
- type: climate
  title: Climate
  entities:
    - { entity: climate.hall, label: "Hall thermostat", step: 0.5 }
    - { entity: climate.bedroom, label: "Bedroom", icon: mdi:radiator }
```

Reads the `current_temperature` and `temperature` attributes and calls
`climate.set_temperature`.

### media

```yaml
- type: media
  title: Media
  entity: media_player.living_room_tv
  label: "Living Room TV"
```

One player per page. Mute reads the player's `is_volume_muted` and sends the
inverse, so it genuinely toggles.

### readouts

```yaml
- type: readouts
  title: Solar
  entities:
    - { entity: sensor.pv_power, label: "Solar now",
        icon: mdi:solar-power, format: "%.2f kW" }
```

`format` is a printf format taking one float. Use `columns: 2` for short values
like temperatures - twice as many per page, at 28 px instead of 48 px.

Values refresh on page load and every 60 seconds **while that page is on
screen**. A page you are not looking at costs nothing.

## The Home summary card

Optional. Up to four cells across the lower third of Home:

```yaml
home:
  summary:
    - { label: "Lights on", source: lights_on }
    - { label: "Hall", entity: sensor.hall_temperature, format: "%.1f°" }
```

`source: lights_on` counts the lights that are on across every `lights` page.
Everything else takes an `entity` and a `format`.

This card is polled every 30 seconds while Home is on screen and redrawn **only
when the value as displayed changes**. Solar drifting 1.42 -> 1.44 kW does not
redraw anything if the format is `%.1f`; a quiet house costs no refreshes at all.
That matters because every redraw is a ~670 ms whole-screen refresh.

## Icons

Any [Material Design Icons](https://pictogrammers.com/library/mdi/) name, with or
without the `mdi:` prefix. Only the glyphs you actually use are compiled in, at
the sizes they are drawn at.

An unknown name fails the build with a suggestion:

```
error in panel.yaml: unknown Material Design Icon 'mdi:lightbulb-outlin'.
Did you mean: lightbulb-outline, lightbulb-on-outline?
```

That is deliberate. The alternative is discovering it as an empty box on a panel
you have already flashed.

Prefer chunky shapes. The display is 1-bit, so fine interior detail turns to mush
- `mdi:thermostat` (a dial with ticks) reads far worse at 36 px than
`mdi:thermometer-lines`. See [design-constraints.md](design-constraints.md).

## The Settings page

Always generated, not configurable: frontlight brightness and warmth, sleep
timeout, Keep awake, and a manual full refresh.

## Commands

```
tools/panel validate    # check panel.yaml, resolve every icon, lay out every page
tools/panel generate    # emit the ESPHome YAML and stop
tools/panel build       # generate, compile, copy the image to firmware/
tools/panel flash       # write the image to ota_0 over USB
tools/panel logs        # stream logs over Wi-Fi
```

`validate` does everything `build` does except invoke the compiler, so it catches
bad entities, unknown icons and pages that cannot fit in under a second.
