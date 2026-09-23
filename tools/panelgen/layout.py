"""Geometry for the 480 x 800 panel, and the pagination that follows from it.

Every constant here is a physical fact about the device or a rule from
docs/design-constraints.md. Nothing in panel.yaml carries coordinates: a page
that does not fit is split, never squeezed.
"""
from dataclasses import dataclass

SCREEN_W, SCREEN_H = 480, 800
MARGIN = 30
CONTENT_W = SCREEN_W - 2 * MARGIN          # 420
COL_X = (MARGIN, MARGIN + 220)             # 30, 250 - two 200 px columns, 20 px gutter
COL_W = 200
BOTTOM = SCREEN_H - 10                     # keep 10 px clear of the edge

TOUCH_MIN = 64                             # nothing tappable may be smaller

HEADING_Y = 20                             # 48 px page title, top-left
CONTENT_TOP = 70                           # pages with no heading (Home, Lights)
CARD_TOP = 90                              # pages with a heading

TILE_H = 96                                # light tile
STEPPER_H, STEPPER_W, STEPPER_GAP = 64, 96, 8
ROW_GAP = 10

NAV_W, NAV_H = COL_W, 130                  # Home destination tile
NAV_ROW_PITCH = 150
SUMMARY_Y, SUMMARY_H = 520, 264

READOUT_H, READOUT_PITCH = 100, 112
CLIMATE_H, CLIMATE_PITCH = 240, 260
MEDIA_CARD_H = 240
MEDIA_BTN_W, MEDIA_BTN_H = 130, 96
MEDIA_BTN_X = (30, 175, 320)
MEDIA_ROW_Y = (350, 466)

assert STEPPER_H >= TOUCH_MIN and MEDIA_BTN_H >= TOUCH_MIN and TILE_H >= TOUCH_MIN


@dataclass
class Placed:
    item: object
    x: int
    y: int
    w: int
    h: int
    extra: dict = None


def _light_row_h(pair):
    return TILE_H + (STEPPER_GAP + STEPPER_H if any(e.dim for e in pair) else 0)


def paginate_lights(entities):
    """Two-up rows; a row is taller when either of its tiles has steppers."""
    pages, cur, y = [], [], CONTENT_TOP
    for i in range(0, len(entities), 2):
        pair = entities[i:i + 2]
        h = _light_row_h(pair)
        if y + h > BOTTOM and cur:
            pages.append(cur)
            cur, y = [], CONTENT_TOP
        for k, e in enumerate(pair):
            cur.append(Placed(e, COL_X[k], y, COL_W, TILE_H,
                              {"stepper_y": y + TILE_H + STEPPER_GAP} if e.dim else {}))
        y += h + ROW_GAP
    if cur:
        pages.append(cur)
    return pages


def _paginate_stack(entities, top, pitch, h, w, x):
    pages, cur, y = [], [], top
    for e in entities:
        if y + h > BOTTOM and cur:
            pages.append(cur)
            cur, y = [], top
        cur.append(Placed(e, x, y, w, h))
        y += pitch
    if cur:
        pages.append(cur)
    return pages


def paginate_readouts(entities, columns=1):
    if columns == 1:
        return _paginate_stack(entities, CARD_TOP, READOUT_PITCH, READOUT_H, CONTENT_W, MARGIN)
    pages, cur, y = [], [], CARD_TOP
    for i in range(0, len(entities), 2):
        if y + READOUT_H > BOTTOM and cur:
            pages.append(cur)
            cur, y = [], CARD_TOP
        for k, e in enumerate(entities[i:i + 2]):
            cur.append(Placed(e, COL_X[k], y, COL_W, READOUT_H))
        y += READOUT_PITCH
    if cur:
        pages.append(cur)
    return pages


def paginate_climate(entities):
    return _paginate_stack(entities, CARD_TOP, CLIMATE_PITCH, CLIMATE_H, CONTENT_W, MARGIN)


def paginate_nav(count, has_summary):
    """Home destination tiles: rows of two, the last one widened if it is alone.

    Stops above the summary card when there is one; otherwise runs to the bottom.
    """
    limit = SUMMARY_Y - ROW_GAP if has_summary else BOTTOM
    pages, cur, y, i = [], [], CONTENT_TOP, 0
    while i < count:
        if y + NAV_H > limit and cur:
            pages.append(cur)
            cur, y = [], CONTENT_TOP
        room = min(2, count - i)
        alone = room == 1
        for k in range(room):
            cur.append(Placed(i + k, COL_X[k], y,
                              CONTENT_W if alone else NAV_W, NAV_H))
        i += room
        y += NAV_ROW_PITCH
    if cur:
        pages.append(cur)
    return pages
