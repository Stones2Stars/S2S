#!/usr/bin/env python3
"""Curate Terrain (#428, Tier C #20) — a plot-leaf TARGET (enables nothing; store.enabled_by(TERRAIN_*) empty).

A terrain CARRIES ITS OWN modifiers onto the plot it forms (owner 2026-06-16: a hill delivers hammers; a
feature then modifies the plot on top). EVERY effect is a TERRAIN-OWN deposit at PLOT scope; NO inbound boost
folds onto the terrain. Ownership rule (owner 2026-06-16, the "deliveryguy"): the entity that BRINGS a modifier
to the table owns it — a Building/Civic TerrainYieldChanges stays on the building/civic (it delivers the buff,
conditioned), an Improvement/Feature/Route on itself. The terrain only owns what it intrinsically delivers.
Per-field dispositions: classifications/terrain-classification.json (adversarially verified).

Modeling calls (verified vs CvTerrainInfo + CvPlot::calculateYield/movementCost/getDefenseModifier + CvCity):
- Yields            -> SPLIT base-yield families food/production/commerce, PLOT scope (CvPlot.cpp:8077, summed
                       into the tile's base yield). The grassland-food / hill-hammers base.
- iMovement         -> `movement` family, PLOT scope (owner: "movement is a family"; terrain seeds the per-plot
                       move cost, features/hills add on top — CvPlot.cpp:4555).
- iDefense          -> `defense.plot.amount.percent` (owner: a family, "improvable by a fort"; terrain seeds the
                       per-plot defense, feature/hills/peak add — CvPlot.cpp:4400; same `amount` member as
                       CultureLevel's defense.city.amount).
- iCultureDistance  -> NEW `cultureDistance` family, PLOT/flat (owner: a summable family for the REALISTIC_CULTURE
                       option; the base for the later culture-equilibrium structure; CvCity.cpp:6302+ accumulates
                       getCultureDistance() across worked plots into a city total).
- iBuildModifier    -> NEW `buildTime` family, PLOT/percent (owner: ties to workRate/work-capacity — the
                       buildTime<->workRate unification is later structural work; CvPlot.cpp:3607 multiplies the
                       build time of work done on this terrain).
- iHealthPercent    -> DROP (dead on terrain: every getHealthPercent reader is Feature/Specialist/Improvement/
                       Building, none against a terrain; pre-existing note modifier-cascade-mapping.json:2667). cat i.
- ClimateZoneType / iDistanceToLand / bFound / bFoundCoast / bFoundFreshWater / bFreshWaterTerrain /
  MapCategoryTypes -> identity (world-gen classification, city-found gates, fresh-water, placement categories;
                       read directly, never summed).
- ArtDefineTag -> art.artDefineTag (the ON-MAP terrain graphics FK -> CIV4ArtDefines_Terrain.xml, a separate art
  tier); Button -> art.icon (the UI icon); FootstepSounds / WorldSoundscapeAudioScript -> art (audio).
- Categories / PropertyManipulators / bImpassable -> NOT authored on ANY terrain (0/42) -> never emitted (the
  consumers exist but no terrain populates them; Categories is also dead, no live reader).

EXE-link: 0 DllExport that constrains data on CvTerrainInfo (getArtDefineTag is the lone DllExport, an art FK).

  python3 curate_terrain.py --sample TERRAIN_GRASSLAND
  python3 curate_terrain.py --write
"""
import os
from collections import OrderedDict

import engine
import curate_common as cc
from store import REPO

# --- SYNTHETIC plot-yield deposits not sourced from a terrain's OWN XML (owner 2026-06-16) -------------------
# Two plot-yield concepts have no terrain XML field but, per the deliveryguy/semantic-sense rule (modifier-spec
# §6.1), belong ON the terrains: (1) the RIVER bonus — a HAS_RIVER-conditional add-on every river-capable land
# terrain carries (the river is a plot edge-attribute; the Info data is just the conditioner on CvPlot's live
# river boolean); (2) the HILLS/PEAK plot-yield deltas, which today live on YieldInfo's plot-type changes but are
# owned by their own TERRAIN_HILL/TERRAIN_PEAK terrains (retiring YieldInfo.getHillsChange/getPeakChange). Both
# are read from YieldInfo so there are no magic numbers. The cascade engine (#430) reads these terrain yields for
# hills/peak/river plots instead of the YieldInfo plot-type path.
_Y = {"YIELD_FOOD": "food", "YIELD_PRODUCTION": "production", "YIELD_COMMERCE": "commerce"}
# `enabled` shorthand (owner 2026-06-16): an UNPARAMETERIZED object-predicate is a BARE STRING (the name is
# self-describing), not `{HAS_RIVER: true}`. Parameterized predicates ({HAS_RELIGION: RELIGION_X}) + tally atoms
# ({type,scope,min}) stay objects. enabler-spec §3.
HAS_RIVER = "HAS_RIVER"
# space map-categories never carry a river (their terrains lack MAPCATEGORY_EARTH, so the EARTH test excludes them).


def _yield_deltas(store):
    """Read YieldInfo -> {hills:{yield:delta}, peak:{...}, river:{...}} (non-zero only), keyed by short yield name."""
    out = {"hills": OrderedDict(), "peak": OrderedDict(), "river": OrderedDict()}
    for typ, rec in store.table("YieldInfo").items():
        y = _Y.get(typ)
        if not y:
            continue
        for tag, key in (("iHillsChange", "hills"), ("iPeakChange", "peak"), ("iRiverChange", "river")):
            t = engine.text(rec.find(tag))
            if engine.is_int(t) and int(t) != 0:
                out[key][y] = int(t)
    return out


def _river_capable(typ, mc):
    """A terrain can carry a river iff it is LAND (not water, not space). EARTH-and-not-AQUATIC = the 17 earth
    land terrains; the category-less special entries TERRAIN_HILL/PEAK are land too (owner: include them);
    TERRAIN_NONE and every space terrain (no MAPCATEGORY_EARTH) are excluded."""
    if "MAPCATEGORY_AQUATIC" in mc:
        return False
    if not mc:
        return typ in ("TERRAIN_HILL", "TERRAIN_PEAK")
    return "MAPCATEGORY_EARTH" in mc


def _inject(obj, family, scope, unit, value, enabled=None):
    """Add one deposit to obj[family][scope][unit], handling scalar / cumulative-list / conditional-entry merge
    (modifier-spec §1.3): two unconditional scalars SUM; a scalar + a conditional entry become a mixed LIST."""
    leaf = obj.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    entry = value if enabled is None else OrderedDict([("value", value), ("enabled", enabled)])
    cur = leaf.get(unit)
    if cur is None:
        leaf[unit] = entry if enabled is None else [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    elif enabled is None:
        leaf[unit] = cur + value
    else:
        leaf[unit] = [cur, entry]


_PREFIX = ["type", "description", "civilopedia", "help", "quote", "strategy",
           "enables", "obsoletes", "replaces", "disables", "requires"]
_SUFFIX = ["grants", "cost", "ai", "art", "mapGeneration", "identity"]


def _reorder(obj):
    """Re-place injected family sections into FAMILY_ORDER position (between requires and grants)."""
    fams = [k for k in obj if k not in _PREFIX and k not in _SUFFIX]
    ordered = [f for f in cc.FAMILY_ORDER if f in fams] + [f for f in fams if f not in cc.FAMILY_ORDER]
    new = OrderedDict()
    for k in _PREFIX:
        if k in obj:
            new[k] = obj[k]
    for f in ordered:
        new[f] = obj[f]
    for k in _SUFFIX:
        if k in obj:
            new[k] = obj[k]
    obj.clear()
    obj.update(new)


_DELTAS = {}  # filled lazily on first post_process call (one Store load)


def post_process(typ, obj, rec, store):
    if not _DELTAS:
        _DELTAS.update(_yield_deltas(store))
    # (1) HILLS/PEAK own plot-yield deltas (unconditional), moved off YieldInfo onto their terrains.
    own = {"TERRAIN_HILL": _DELTAS["hills"], "TERRAIN_PEAK": _DELTAS["peak"]}.get(typ)
    if own:
        for y, v in own.items():
            _inject(obj, y, "plot", "flat", v)
    # (2) RIVER bonus (HAS_RIVER-conditional) on every river-capable land terrain.
    mc = (obj.get("identity") or {}).get("mapCategories") or []
    if _river_capable(typ, mc):
        for y, v in _DELTAS["river"].items():
            _inject(obj, y, "plot", "flat", v, HAS_RIVER)
    _reorder(obj)

# Terrain's OWN modifier families (override the empty mapping channels). Every deposit is PLOT scope — the
# terrain forms the plot's base, which features/improvements/routes then modify on top.
TERRAIN_FAMILIES = {
    "Yields":           {"channel": "yield",           "scope": "plot", "kind": "flat", "valueKeys": engine.YIELDS},
    "iMovement":        {"channel": "movement",        "scope": "plot", "kind": "flat"},
    "iDefense":         {"channel": "defense",         "scope": "plot", "kind": "percent", "member": "amount"},
    "iCultureDistance": {"channel": "cultureDistance", "scope": "plot", "kind": "flat"},
    "iBuildModifier":   {"channel": "buildTime",       "scope": "plot", "kind": "percent"},
}

# Keep ArtDefineTag (the ON-MAP terrain graphics FK) DISTINCT from the UI icon (Button): terrain carries BOTH,
# and the global ArtDefineTag->icon collapse would otherwise overwrite Button->icon (silent data loss).
TERRAIN_ART_RENAME = {"ArtDefineTag": "artDefineTag"}

# Clearer identity key for the placement-category vector (MAPCATEGORY_* membership).
TERRAIN_ID_RENAME = {"MapCategoryTypes": "mapCategories"}

CFG = cc.EntityConfig("TerrainInfo", extra_drop=["iHealthPercent"],
                      families=TERRAIN_FAMILIES, id_rename=TERRAIN_ID_RENAME,
                      art_rename=TERRAIN_ART_RENAME)

# NO inbound boosts: a terrain is never the deliveryguy for another entity's modifier (owner 2026-06-16).
TERRAIN_BOOSTS = []

if __name__ == "__main__":
    cc.main(CFG, TERRAIN_BOOSTS, os.path.join(REPO, "Assets", "Data", "terrains"), post_process=post_process)
