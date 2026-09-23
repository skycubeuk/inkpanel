# Getting started

## What you need

- An **Xteink X4 Pro** e-reader (ESP32-S3, 800x480 e-ink, GT911 touch, frontlight).
- A 4-pin pogo adapter for native USB. The 2-pin charge cable will not flash it.
- Python 3.10+, [ESPHome](https://esphome.io) 2026.9 or newer, and `esptool`.
- Home Assistant, with the ESPHome integration.

## 1. Back up the stock firmware first

```
scripts/backup-stock.sh
```

Dumps all 16 MB to `backup/`. **Do this before anything else** - it is the only
way back to the reader firmware if you change your mind. Keep the file somewhere
other than the machine you are working on.

## 2. Secrets

```
cp esphome/secrets.yaml.example esphome/secrets.yaml
$EDITOR esphome/secrets.yaml
```

Generate the API key with `openssl rand -base64 32`.

## 3. Describe your panel

```
cp panel.yaml.example panel.yaml
$EDITOR panel.yaml
```

Put in your own entity ids. See [configuration.md](configuration.md) for the full
reference. Check it before building:

```
tools/panel validate
```

## 4. Build and flash

```
tools/panel build
tools/panel flash            # or: tools/panel flash --port /dev/ttyACM1
```

If nothing appears at `/dev/ttyACM0`, hold the left key (GPIO0), plug in, release.

**Flash over USB, not OTA.** `ota_0` holds this firmware and `ota_1` holds the
stock reader firmware. An ESPHome OTA writes whichever slot is *not* running, so
the first OTA would overwrite the reader. `tools/panel flash` always writes
`ota_0` and selects it.

## 5. Let it act on Home Assistant

In Home Assistant: **Settings > Devices & services > ESPHome >** your panel **>
Configure >** tick *"Allow the device to perform Home Assistant actions"*.

Without this the panel displays fine but every tap is refused.

## 6. Watch it run

```
tools/panel logs
```

USB serial logging is deliberately off: ESPHome's USB-Serial-JTAG driver goes
into a crash loop that survives resets if the cable is replugged while running.
Logs go over Wi-Fi instead.

## Going back to the reader firmware

```
scripts/select-slot.sh slot1
```

Swaps the boot slot without reflashing, provided the stock firmware is still in
`ota_1`. If you have already overwritten it, restore from your backup.

## Day-to-day

- **Home key** returns to Home, **Right key** toggles the frontlight, **Power key**
  wakes it.
- The panel deep-sleeps after the *Sleep timeout* on the Settings page (60 s by
  default) and restores the page you were on. E-ink holds the image with no power.
- **Keep awake** on the Settings page stops it sleeping. Useful while working on
  it; it roughly doubles idle drain, so turn it off afterwards.
