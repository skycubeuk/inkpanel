"""panel.yaml -> validated objects. Every error names the path that is wrong."""
from dataclasses import dataclass, field
import yaml

from . import icons


class ConfigError(ValueError):
    pass


def _req(d, key, where):
    if key not in d:
        raise ConfigError(f"{where}: missing required key '{key}'")
    return d[key]


def _entity(v, where, domain=None):
    if not isinstance(v, str) or "." not in v:
        raise ConfigError(f"{where}: '{v}' is not an entity id (expected e.g. light.kitchen)")
    if domain and not v.startswith(domain + "."):
        raise ConfigError(f"{where}: '{v}' should be in the '{domain}' domain")
    return v


@dataclass
class Light:
    entity: str
    label: str
    dim: bool = False
    icon_on: str = icons.DEFAULT_LIGHT_ON
    icon_off: str = icons.DEFAULT_LIGHT_OFF
    step: int = 20


@dataclass
class Climate:
    entity: str
    label: str
    icon: str = "thermometer-lines"
    step: float = 0.5


@dataclass
class Readout:
    entity: str
    label: str
    icon: str = "gauge"
    format: str = "%.1f"


@dataclass
class Page:
    type: str
    title: str
    icon: str
    entities: list = field(default_factory=list)
    entity: str = ""
    label: str = ""
    columns: int = 1


@dataclass
class SummaryCell:
    label: str
    entity: str = ""
    source: str = ""
    format: str = "%.1f"


@dataclass
class Panel:
    name: str
    friendly_name: str
    pages: list
    summary: list = field(default_factory=list)


_DEFAULT_PAGE_ICON = {"lights": "lightbulb", "climate": "thermometer-lines",
                      "media": "television", "readouts": "gauge"}


def load(path):
    with open(path, encoding="utf-8") as f:
        raw = yaml.safe_load(f)
    if not isinstance(raw, dict):
        raise ConfigError("panel.yaml: expected a mapping at the top level")

    p = _req(raw, "panel", "panel.yaml")
    name = _req(p, "name", "panel")
    if not isinstance(name, str) or not name.replace("-", "").isalnum():
        raise ConfigError("panel.name: use lowercase letters, digits and hyphens")

    pages = []
    raw_pages = _req(raw, "pages", "panel.yaml")
    if not raw_pages:
        raise ConfigError("pages: at least one page is required")

    for i, rp in enumerate(raw_pages):
        where = f"pages[{i}]"
        t = _req(rp, "type", where)
        if t not in ("lights", "climate", "media", "readouts"):
            raise ConfigError(f"{where}.type: unknown page type '{t}' "
                              "(expected lights, climate, media or readouts)")
        title = _req(rp, "title", where)
        icon = rp.get("icon", _DEFAULT_PAGE_ICON[t])
        icons.codepoint(icon)                       # fail now, not at compile time
        page = Page(type=t, title=title, icon=icon, columns=int(rp.get("columns", 1)))

        if t == "media":
            page.entity = _entity(_req(rp, "entity", where), f"{where}.entity", "media_player")
            page.label = rp.get("label", title)
        else:
            ents = _req(rp, "entities", where)
            if not ents:
                raise ConfigError(f"{where}.entities: must list at least one entity")
            for j, e in enumerate(ents):
                w = f"{where}.entities[{j}]"
                if t == "lights":
                    page.entities.append(Light(
                        entity=_entity(_req(e, "entity", w), w, "light"),
                        label=_req(e, "label", w), dim=bool(e.get("dim", False)),
                        icon_on=e.get("icon", icons.DEFAULT_LIGHT_ON),
                        icon_off=e.get("icon_off", icons.DEFAULT_LIGHT_OFF),
                        step=int(e.get("step", 20))))
                elif t == "climate":
                    page.entities.append(Climate(
                        entity=_entity(_req(e, "entity", w), w, "climate"),
                        label=_req(e, "label", w), icon=e.get("icon", "thermometer-lines"),
                        step=float(e.get("step", 0.5))))
                else:
                    page.entities.append(Readout(
                        entity=_entity(_req(e, "entity", w), w),
                        label=_req(e, "label", w), icon=e.get("icon", "gauge"),
                        format=e.get("format", "%.1f")))
            for e in page.entities:
                icons.codepoint(getattr(e, "icon", None) or e.icon_on)
                if isinstance(e, Light):
                    icons.codepoint(e.icon_off)
        if page.columns not in (1, 2):
            raise ConfigError(f"{where}.columns: must be 1 or 2")
        if page.columns == 2 and t != "readouts":
            raise ConfigError(f"{where}.columns: only the 'readouts' page type supports 2 columns")
        pages.append(page)

    summary = []
    for i, c in enumerate(raw.get("home", {}).get("summary", []) or []):
        w = f"home.summary[{i}]"
        cell = SummaryCell(label=_req(c, "label", w), format=c.get("format", "%.1f"))
        if "source" in c:
            if c["source"] != "lights_on":
                raise ConfigError(f"{w}.source: only 'lights_on' is supported")
            cell.source = c["source"]
        elif "entity" in c:
            cell.entity = _entity(c["entity"], w)
        else:
            raise ConfigError(f"{w}: needs either 'entity' or 'source'")
        summary.append(cell)
    if len(summary) > 4:
        raise ConfigError("home.summary: at most 4 cells fit on the card")

    return Panel(name=name, friendly_name=p.get("friendly_name", name),
                 pages=pages, summary=summary)
