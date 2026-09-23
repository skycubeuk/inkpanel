#!/usr/bin/env python3
"""Tests for the generator. Run: python3 tools/test_panelgen.py

The important one is test_glyphs_present: it asserts that every MDI glyph a page
draws is compiled into the font that draws it. Getting this wrong does not fail
the build - it renders an empty box on a panel you have already flashed.
"""
import os, sys, tempfile, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import yaml
from panelgen import config, emit, icons, layout


class _L(yaml.SafeLoader):
    pass


_L.add_multi_constructor("!", lambda l, s, n: None)

HEAD = "panel:\n  name: t\n  friendly_name: T\npages:\n"


def render(cfg):
    f = tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False)
    f.write(cfg)
    f.close()
    try:
        return yaml.load(emit.generate(config.load(f.name)), Loader=_L)
    finally:
        os.unlink(f.name)


def lights(n, dim=False):
    return HEAD + "  - type: lights\n    title: L\n    entities:\n" + "".join(
        f"      - {{entity: light.l{i}, label: L{i}, dim: {str(dim).lower()}}}\n" for i in range(n))


class TestLayout(unittest.TestCase):
    def test_nothing_runs_off_the_screen(self):
        for n in (1, 3, 8, 9, 20):
            for placed in layout.paginate_lights([type("E", (), {"dim": True})() for _ in range(n)]):
                for p in placed:
                    bottom = p.y + p.h + (layout.STEPPER_GAP + layout.STEPPER_H)
                    self.assertLessEqual(bottom, layout.SCREEN_H, f"{n} lights overflow")

    def test_nav_indices_are_contiguous(self):
        for n in range(1, 20):
            pages = layout.paginate_nav(n, n <= 5)
            self.assertEqual([p.item for pg in pages for p in pg], list(range(n)))

    def test_touch_targets_meet_the_minimum(self):
        self.assertGreaterEqual(layout.STEPPER_H, layout.TOUCH_MIN)
        self.assertGreaterEqual(layout.TILE_H, layout.TOUCH_MIN)
        self.assertGreaterEqual(layout.MEDIA_BTN_H, layout.TOUCH_MIN)


class TestEmit(unittest.TestCase):
    def test_page_ids_unique(self):
        d = render(lights(20, dim=True))
        ids = [p["id"] for p in d["lvgl"]["pages"]]
        self.assertEqual(len(ids), len(set(ids)))

    def test_glyphs_present(self):
        """Every icon a page draws must exist in the font it is drawn with."""
        d = render(lights(4) +
                   "  - type: media\n    title: M\n    entity: media_player.tv\n" +
                   "  - type: climate\n    title: C\n    entities:\n"
                   "      - {entity: climate.h, label: H, icon: mdi:radiator}\n")
        fonts = {f.get("id"): f for f in d["font"]}
        used = set()

        def walk(o):
            if isinstance(o, dict):
                t = o.get("text")
                if isinstance(t, str) and t and "\U000F0000" <= t[0] <= "\U000FFFFF":
                    used.add((o.get("text_font"), t))
                for v in o.values():
                    walk(v)
            elif isinstance(o, list):
                for v in o:
                    walk(v)

        walk(d["lvgl"]["pages"])
        self.assertTrue(used, "no icons found to check")
        for font, glyph in used:
            if font in ("icon_m", "icon_l"):
                self.assertIn(glyph, fonts[font]["glyphs"],
                              f"{glyph!r} drawn in {font} but not compiled into it")

    def test_single_text_sensor_key(self):
        """ESPHome rejects a duplicated top-level key."""
        out = emit.generate(config.load(_write(lights(2))))
        self.assertEqual(sum(1 for l in out.splitlines() if l == "text_sensor:"), 1)


class TestConfigErrors(unittest.TestCase):
    def _bad(self, cfg, fragment):
        with self.assertRaises((config.ConfigError, icons.IconError)) as c:
            config.load(_write(cfg))
        self.assertIn(fragment, str(c.exception))

    def test_unknown_icon(self):
        self._bad(HEAD + "  - type: lights\n    title: L\n    icon: mdi:not-an-icon\n"
                  "    entities:\n      - {entity: light.a, label: A}\n", "unknown Material Design Icon")

    def test_wrong_domain(self):
        self._bad(HEAD + "  - type: lights\n    title: L\n    entities:\n"
                  "      - {entity: switch.a, label: A}\n", "'light' domain")

    def test_columns_only_on_readouts(self):
        self._bad(HEAD + "  - type: lights\n    title: L\n    columns: 2\n    entities:\n"
                  "      - {entity: light.a, label: A}\n", "only the 'readouts' page type")

    def test_summary_cap(self):
        self._bad("panel:\n  name: t\n  friendly_name: T\nhome:\n  summary:\n" +
                  "".join(f"    - {{label: S{i}, entity: sensor.s{i}}}\n" for i in range(5)) +
                  "pages:\n  - type: readouts\n    title: R\n    entities:\n"
                  "      - {entity: sensor.a, label: A}\n", "at most 4 cells")


def _write(cfg):
    f = tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False)
    f.write(cfg)
    f.close()
    return f.name


if __name__ == "__main__":
    unittest.main(verbosity=2)
