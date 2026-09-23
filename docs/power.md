# Power

Idle drain is **~1.07 %/h**, so a full charge lasts about **4 days** with the
frontlight off.

## How it got there

| Setup | Drain | From full |
|---|---|---|
| Awake, no Wi-Fi power save | 9.7 %/h | ~10 h |
| Awake, Wi-Fi light power save | 4.5 %/h | ~22 h |
| Processor light sleep when idle | 1.5 %/h | ~2.7 days |
| Plus Wi-Fi max modem sleep when idle | ~1.07 %/h | ~3.9 days |

Each row is a configuration this repo ships, not a theoretical figure.

### Wi-Fi light power save (`esphome/base/network.yaml`)

`power_save_mode: light` lets the radio nap between router beacons while staying
associated. Sends are never delayed, so taps are unaffected; incoming updates can
wait up to one beacon interval (~100-300 ms). This alone halves the drain.

### Processor light sleep (`esphome/base/light_sleep.yaml`, `esphome/light_sleep.h`)

The CPU is held active for 10 s after a touch or key press, then allowed to
light-sleep between scheduled work. The GT911 interrupt pin wakes the chip *and*
the main loop, so the tap that wakes it is acted on immediately rather than
dropped.

Two ESP-IDF details matter. `CONFIG_PM_SLP_DISABLE_GPIO` floats GPIO2 (GT911
power, active LOW) and powers the touch chip off, so those pads are held with
`gpio_sleep_sel_dis`. And USB-Serial-JTAG dies in light sleep, which also blocks
USB flashing, so `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION` blocks light sleep while a
USB host is connected.

### Wi-Fi max modem sleep when idle

While idle the panel listens every 10th beacon (~1.0 s) instead of every one, and
returns to every beacon on a touch. Raising the interval from 3 to 10 took about
10 % off idle drain with no effect on how quickly taps or HA updates land.

### Deep sleep

After the configured timeout the panel deep-sleeps entirely, and e-ink holds the
last image at zero cost. The page is restored on wake. This is what makes the
4-day figure a floor rather than a ceiling in normal use.

## How to measure your own

Do not trust a figure taken over an hour: the first few percent from full always
go quickly, so only compare runs over the same part of the curve.

Pull the recorder history for the battery entity, split it into monotonic runs,
and keep only the long undisturbed discharges:

```python
import requests, datetime as dt
r = requests.get(
    f"{HA}/api/history/period/2026-09-17T00:00:00+00:00",
    headers={"Authorization": f"Bearer {TOKEN}"},
    params={"filter_entity_id": "sensor.your_panel_battery",
            "end_time": dt.datetime.now(dt.timezone.utc).isoformat(),
            "minimal_response": ""})
```

Note `end_time`: without it the API returns **one day** and quietly looks like it
worked.

Then segment on the sign of the change, drop anything under ~10 hours or under
2 %, and take the weighted mean of what is left. The figures above came from
three runs totalling 90 hours that agreed to within 0.03 %/h.

Two things will skew a measurement: **Keep awake** being on (the panel never
deep-sleeps, so you are measuring the worst case), and USB being connected (it
charges, which shows up as a charge segment rather than a flat line).

## What costs power in a design

Every screen refresh is ~670 ms of panel activity regardless of how little
changed. So the expensive thing is not what is on screen, it is how often it
changes. The generated summary card polls but only redraws when the value **as
displayed** changes; a readouts page refreshes only while it is the active page.
A live clock would be 60 refreshes an hour plus ghost cleans, which is why there
isn't one.
