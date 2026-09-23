#!/usr/bin/env python3
"""Regenerate tools/panelgen/mdi.json from the Material Design Icons webfont CSS.

Only the name -> codepoint mapping is stored. The font is not redistributed -
ESPHome downloads it at build time - so this file carries no glyph artwork.

    python3 tools/update-mdi.py [ref]        # ref defaults to master
"""
import json, re, sys, urllib.request
from datetime import date
from pathlib import Path

REPO = "Templarian/MaterialDesign-Webfont"
ref = sys.argv[1] if len(sys.argv) > 1 else "master"


def get(url):
    with urllib.request.urlopen(url, timeout=60) as r:
        return r.read().decode()


css = get(f"https://raw.githubusercontent.com/{REPO}/{ref}/css/materialdesignicons.css")
pkg = json.loads(get(f"https://raw.githubusercontent.com/{REPO}/{ref}/package.json"))
sha = json.loads(get(f"https://api.github.com/repos/{REPO}/commits/{ref}"))["sha"][:12]

icons = {n: c.upper().zfill(5) for n, c in re.findall(
    r'\.mdi-([a-z0-9-]+)::before\s*\{\s*content:\s*"\\([0-9A-Fa-f]{4,6})"', css)}
if len(icons) < 1000:
    sys.exit(f"only {len(icons)} icons parsed - the CSS format has probably changed")

out = {"version": pkg.get("version", "unknown"),
       "source": f"https://github.com/{REPO} css/materialdesignicons.css",
       "source_commit": sha,
       "license": "Apache-2.0 (Pictogrammers Free License)",
       "generated": date.today().isoformat(),
       "note": "Name -> codepoint only. The font itself is not redistributed; "
               "ESPHome downloads it at build time. Regenerate with tools/update-mdi.py",
       "icons": icons}
dst = Path(__file__).parent / "panelgen/mdi.json"
dst.write_text(json.dumps(out, separators=(",", ":"), sort_keys=True), encoding="utf-8")
print(f"{dst}: {len(icons)} icons, @mdi/font {out['version']} @ {sha}")
