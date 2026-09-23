"""Material Design Icons: name -> codepoint, and the glyph sets each font needs.

Every `mdi:` name in panel.yaml is resolved here and emitted into the minimal
glyph list for the font that draws it. ESPHome compiles one bitmap per glyph per
size, so an unresolved name is a build error rather than an empty box on a panel
you have already flashed.
"""
import json
from pathlib import Path

_TABLE = json.loads((Path(__file__).parent / "mdi.json").read_text())["icons"]

# Battery glyphs for the status line, always compiled in.
BATTERY = [
    ("battery", 95), ("battery-90", 85), ("battery-80", 75), ("battery-70", 65),
    ("battery-60", 55), ("battery-50", 45), ("battery-40", 35), ("battery-30", 25),
    ("battery-20", 15), ("battery-10", 5),
]
BATTERY_EMPTY, BATTERY_UNKNOWN = "battery-outline", "battery-unknown"

MEDIA_TRANSPORT = ["skip-previous", "play-pause", "skip-next",
                   "volume-minus", "volume-off", "volume-plus"]
SETTINGS_ICONS = ["brightness-6", "timer-outline", "eye", "refresh"]

DEFAULT_LIGHT_ON, DEFAULT_LIGHT_OFF = "lightbulb", "lightbulb-outline"


class IconError(ValueError):
    pass


def codepoint(name):
    """'mdi:lightbulb' or 'lightbulb' -> 'F0335'."""
    key = name[4:] if name.startswith("mdi:") else name
    cp = _TABLE.get(key)
    if cp is None:
        near = [k for k in _TABLE if key in k][:6]
        hint = f" Did you mean: {', '.join(near)}?" if near else ""
        raise IconError(f"unknown Material Design Icon 'mdi:{key}'.{hint}")
    return cp


def escape(name):
    """-> the \\U000F0335 escape ESPHome's YAML reader understands."""
    return "\\U000" + codepoint(name)


class GlyphSets:
    """Collects the glyphs each icon font must carry, deduplicated and ordered."""

    def __init__(self):
        self.sets = {}

    def add(self, font, *names):
        s = self.sets.setdefault(font, {})
        for n in names:
            key = n[4:] if n.startswith("mdi:") else n
            s[key] = codepoint(key)          # resolves, and raises early if wrong

    def items(self, font):
        return sorted(self.sets.get(font, {}).items())
