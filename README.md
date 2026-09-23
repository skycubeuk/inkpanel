# inkpanel

Turn an **Xteink X4 Pro** e-reader into a Home Assistant wall panel that runs
about **four days on a charge**, described entirely in one config file.

The stock reader firmware stays in the other flash slot, so it is reversible with
one command.

```yaml
# panel.yaml
panel:
  name: hallway-panel
  friendly_name: Hallway Panel

pages:
  - type: lights
    title: Lights
    entities:
      - { entity: light.kitchen, label: "Kitchen & Dining", dim: true }
      - { entity: light.porch,   label: "Porch" }

  - type: readouts
    title: Solar
    entities:
      - { entity: sensor.pv_power, label: "Solar now",
          icon: mdi:solar-power, format: "%.2f kW" }
```

```
tools/panel build && tools/panel flash
```

Then adopt it in Home Assistant (**Settings > Devices & services**, it is
discovered over mDNS) and tick *"Allow the device to perform Home Assistant
actions"* so taps do something -
[getting-started.md](docs/getting-started.md) walks through both.

No coordinates, no font sizes, no icon codepoints. The generator lays out the
pages, splits anything that does not fit, and compiles in exactly the Material
Design Icons glyphs you referenced - at the sizes they are drawn at.

## Why bother

E-ink is a genuinely good surface for a wall panel: no backlight to glow at night,
holds its image at zero power, readable across a room. The costs are that
everything is black or white, and every refresh takes two-thirds of a second.

Most of the work here is in living with that well - see
**[docs/design-constraints.md](docs/design-constraints.md)**, which is the part
most likely to be useful even if you never touch this hardware.

## Battery

| Setup | Drain | From full |
|---|---|---|
| Awake, no Wi-Fi power save | 9.7 %/h | ~10 h |
| Awake, Wi-Fi light power save | 4.5 %/h | ~22 h |
| Processor light sleep when idle | 1.5 %/h | ~2.7 days |
| Plus Wi-Fi max modem sleep when idle | **~1.07 %/h** | **~3.9 days** |

Measured from six days of Home Assistant recorder history, taking three
undisturbed discharge runs totalling 90 hours that agreed to within 0.03 %/h -
and with *Keep awake* on, so it is a worst case. Method in
[docs/power.md](docs/power.md).

## How it feels

Pick it up, tap a tile, the light changes: about 0.7 s to settled pixels, even
after hours asleep.

- Taps act on **finger-down** and the tile inverts optimistically, so one refresh
  carries both the feedback and the result.
- The e-paper refresh is **asynchronous**: the frame is pushed in ~50 ms and touch
  keeps working while the panel finishes its waveform.
- Ghost cleanup waits until the panel has been left alone for 4 s, rather than
  interrupting a tap with a 1.65 s clearing pass.

## Documentation

| | |
|---|---|
| [getting-started.md](docs/getting-started.md) | Back up the stock firmware, flash, first config |
| [configuration.md](docs/configuration.md) | Every `panel.yaml` option |
| [design-constraints.md](docs/design-constraints.md) | Designing for 1-bit e-paper with LVGL |
| [power.md](docs/power.md) | What each optimisation is worth, and how to measure your own |

## Commands

```
tools/panel validate    # check panel.yaml without building
tools/panel generate    # emit the ESPHome YAML only
tools/panel build       # generate, compile, copy the image to firmware/
tools/panel flash       # write to ota_0 over USB
tools/panel logs        # stream logs over Wi-Fi
```

Plus `scripts/backup-stock.sh` (do this first), `scripts/select-slot.sh` to swap
back to the reader firmware, and `scripts/sync-xteink.sh` to re-vendor the
upstream component.

## Requirements

Xteink X4 Pro, a 4-pin pogo adapter, Python 3.10+, ESPHome 2026.9+, `esptool`,
and Home Assistant.

## Built on

[vjFaLk/esphome-xteink](https://github.com/vjFaLk/esphome-xteink) and the
[FreeInk SDK](https://github.com/Free-Ink/freeink-sdk), vendored with the patches
in `patches/`. See [NOTICE](NOTICE) for the full chain. MIT.

## Tests

```
python3 tools/test_panelgen.py
```
