# Getting started

## What you need

- An **Xteink X4 Pro** e-reader (ESP32-S3, 800x480 e-ink, GT911 touch, frontlight).
- A 4-pin pogo adapter for native USB. The 2-pin charge cable will not flash it.
- Python 3.10+, [ESPHome](https://esphome.io) 2026.9 or newer, and `esptool`.
- Home Assistant on the same network, with mDNS reaching it.

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

Generate the API key with `openssl rand -base64 32`. **Keep this key** - Home
Assistant asks for it in step 6.

## 3. Describe your panel

```
cp panel.yaml.example panel.yaml
$EDITOR panel.yaml
```

Put in your own entity ids. See [configuration.md](configuration.md) for the full
reference. `panel.name` becomes the hostname and the name Home Assistant
discovers, so pick something you will recognise. Check the config before building:

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

## 5. First boot - get it onto Wi-Fi

The panel reboots into the new firmware and draws the Home page immediately. The
page renders before Wi-Fi connects, so a screen full of `--` at this stage is
expected: it has nothing from Home Assistant yet.

Watch it come up:

```
tools/panel logs
```

Look for `WiFi Connected` and an IP address.

If it cannot join, it raises a fallback access point named after your
`friendly_name`, with `ap_password` from `secrets.yaml`, and a captive portal
where you can set the network. Be aware of the timing: the panel still
deep-sleeps after its sleep timeout (60 s by default) whether or not Wi-Fi came
up, so that portal is only reachable in short windows - press the power key to
wake it again. If you simply mistyped the Wi-Fi password, fixing
`esphome/secrets.yaml` and reflashing over USB is quicker and less fiddly than
chasing the portal.

## 6. Add it to Home Assistant

This is the step that makes the panel do anything. Until it is adopted there is
no API connection, so it can neither read your entities nor act on them.

**Automatic.** Home Assistant discovers ESPHome devices over mDNS, usually within
a few minutes of first boot. Go to **Settings > Devices & services**. The panel
appears under **Discovered** as an ESPHome device with the name from
`panel.name`. Click **Configure**.

**Manual**, if it does not show up: **Settings > Devices & services > Add
integration > ESPHome**, then enter:

- **Host**: `your-panel-name.local`, or the IP address from `tools/panel logs`
- **Port**: `6053`

Either way Home Assistant then asks for the **encryption key**. Paste the
`api_key` from `esphome/secrets.yaml`.

Once adopted, the panel appears as a device with its own entities - battery,
battery voltage, frontlight, the four keys, current page, sleep timeout, keep
awake and restart - and the readouts on screen start filling in.

## 7. Let it act on Home Assistant

Adoption alone lets the panel *read* your entities. Acting on them - toggling a
light, setting a temperature - needs one more toggle.

**Settings > Devices & services > ESPHome >** your panel **> Configure >** tick
**"Allow the device to perform Home Assistant actions"**, then **Submit**.

Without this the panel displays correctly and tiles invert when you tap them, but
nothing actually happens and the log shows the action being refused. It is the
single most common reason a freshly built panel looks right and does nothing.

## 8. Check it

Tap a light tile. The tile should invert within about 0.7 s and the light should
change. If the tile inverts and then flips back a second later, the panel sent the
action and Home Assistant refused or the light did not respond - see below.

## Troubleshooting

**Not discovered in Home Assistant.** Confirm it is on the network
(`tools/panel logs` shows the IP). mDNS often does not cross VLANs or subnets - if
Home Assistant is on a different one, add it manually by IP. Check port 6053 is
not blocked.

**"Invalid encryption key".** The key Home Assistant wants is `api_key` from
`esphome/secrets.yaml`, base64 including the trailing `=`. If you have lost it,
change it in `secrets.yaml`, rebuild, reflash, then delete and re-add the
integration.

**Adopted, but every value shows `--`.** The entity ids in `panel.yaml` do not
match your Home Assistant. Check each one in **Developer tools > States**;
`tools/panel validate` checks the *shape* of an entity id, not that it exists.

**Tiles invert but nothing happens.** Step 7 - the actions toggle.

**Taps do nothing at all.** Check the panel is awake; the first tap after deep
sleep wakes it and is not passed through.

**Entities appear then go unavailable.** The panel deep-sleeps, which drops the
API connection - this is normal and expected. Home Assistant marks it unavailable
until it wakes. Turn on **Keep awake** while you are working on it.

## Going back to the reader firmware

```
scripts/select-slot.sh slot1
```

Swaps the boot slot without reflashing, provided the stock firmware is still in
`ota_1`. If you have already overwritten it, restore from your backup.

## Day-to-day

- **Home key** returns to Home, **Right key** toggles the frontlight, **Power key**
  wakes it.
- The panel deep-sleeps after the *Sleep timeout* on the Settings page and
  restores the page you were on. E-ink holds the image with no power.
- **Keep awake** stops it sleeping. Useful while working on it; it roughly doubles
  idle drain, so turn it off afterwards.
- Changing `panel.yaml` means `tools/panel build && tools/panel flash` again. The
  Home Assistant integration does not need re-adding; new entities appear on their
  own, and removed ones go unavailable until you delete them.
