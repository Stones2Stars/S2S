#!/usr/bin/env python3
"""Curate Property (#428) — the 7 records that DEFINE the property CHANNELS (crime/education/disease/flammability/
air-pollution/water-pollution/tourism). A Property is NOT a typical modifier/enabler source; it is the channel
DEFINITION, and it participates in BOTH cascades. Owner decomposition (2026-06-15):

- BASIC MODIFIERS (`PropertyManipulators` `PropertySource`, esp. `PROPERTYSOURCE_DECAY`) — these are MODIFIERS,
  just poorly named: the property's per-turn change. DECAY = move `targetLevel`-ward at iPercent/turn. Authored
  as a self-deposit into this channel's own family at the source scope: `<PROPERTY>.<scope>.percent = iPercent`
  (toward `targetLevel`). The "toward target" direction is engine-side — and THIS is the case where the property
  SYSTEM is reworked to fit the data expressions, not the reverse (owner: data leads, by design).
- THRESHOLDS (`PropertyBuildings` {building, min, max}) — TWO PARTS (owner 2026-06-15):
  (1) GRANTS (not enables): the effect-buildings are GRANTED / gifted by the property — NOT "enabled" (owner's
      term). -> `grants.buildings` = a UNIFORM plain LIST of building types (the standard `grants` shape — NO
      special case).
  (2) AUTO-BUILD trigger: enabling needs a trigger that says the granted building is AUTOMATICALLY BUILT when
      enabled (owner) — these effect-buildings are auto-constructed, never player-built. The flag rides the
      building (the existing FreeBuilding/autobuild idea) and the #430 engine acts on it; not a property field.
  (3) REQUIRES (the active gate): each granted+auto-built building then `requires` the property VALUE-BAND
      `{ type: PROPERTY_X, scope: "city", min: N, max?: M }` to be ACTIVE — reversible dormancy (the §3
      pseudobuilding / `PropertyEffect` case; it dorms as the value crosses the band). Authored on the BUILDING
      at the Building pass (reading `PropertyBuildings` off this property, store-accessible), NOT on the property
      — so the parser reads `building.requires` uniformly and never SPECIAL-CASES the property json.
  So: `property.grants.buildings` = the LIST (authored here); the AUTO-BUILD flag + `building.requires` value-band
  = the Building pass. All UNIFORM. The property json carries the grant list but no min/max thresholds; `scope`
  = `city`. (FIRST PASS — the auto-build trigger + requires-band wiring firm up in the second pass / #430.)
- `targetLevel` (+ `TargetLevelbyEraTypes`) — a GENUINE ISOLATED field, OUTSIDE enabler/modifier (owner): the
  equilibrium the decay pulls toward. Kept top-level as `targetLevel`.
- AI: `iAIWeight`/`AIScaleType`/`iTrainReluctance` + `iOperationalRangeMin/Max` (the AI value-normalization band
  read by `CvCityAI` decision-scoring — owner 2026-07-01: AI-only, not the #429 propagation mechanic) -> `ai`.
  Display texts -> `text`; FontButtonIndex -> identity.
- DROPPED -> #429 (the obsolete LEAKING mechanic, owner): every `PropertyPropagator`
  (DIFFUSE, incl. plot->city SAME_PLOT — the unit->city emission re-homes as a containment deposit on the
  unit/building at their passes), and `ChangePropagators`.
- `bSourceDrain` / `bOAType` -> identity (property-system behaviour flags; do NOT cleanly fit enabler/modifier —
  parked pending the property-system rework; bOAType has only a getter, no consumer = likely near-dead).

⚠ GOAL (owner 2026-06-15): migrate PROPERTIES towards FIRST-CLASS CITIZENS — not the "first-and-halfish class"
half-implemented system they are today. The clean modifier(decay)/enabler(threshold) expressions here are the
start of that elevation; the property SYSTEM (C++ AND its Python tendrils) gets reworked to fit them.

⚠ FIRST PASS (owner 2026-06-15): write it on this shape; a SECOND PASS is expected as the property-system rework
firms up the decay/threshold expressions. The property system has significant PYTHON involvement (owner: "near
zero" chance there isn't more) — confirmed: CvEventManager / CvRandomEventInterface (events mutate properties),
RevolutionWatchAdvisor (crime↔revolution), BuildListScreen, advisors. The Python property logic must be
accounted for in the rework / drop re-check — it is NOT captured by this first pass.

  python3 curate_property.py --sample PROPERTY_CRIME
  python3 curate_property.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import fold_text_to_identity
from store import Store, REPO

TEXT = OrderedDict([
    ("ValueDisplayText", "value"), ("ChangeDisplayText", "change"),
    ("ChangeAllCitiesDisplayText", "changeAllCities"),
    ("PrereqMinDisplayText", "prereqMin"), ("PrereqMaxDisplayText", "prereqMax"),
])
SCOPE = {"GAMEOBJECT_CITY": "city", "GAMEOBJECT_PLOT": "plot", "GAMEOBJECT_PLAYER": "empire",
         "GAMEOBJECT_UNIT": "unit", "GAMEOBJECT_AREA": "area"}
# PropertySource kind -> modifier unit. DECAY = % toward targetLevel; CONSTANT = flat per-turn add.
SOURCE_UNIT = {"PROPERTYSOURCE_DECAY": "percent", "PROPERTYSOURCE_CONSTANT": "flat"}
UNBOUNDED_MAX = 100000   # PropertyBuilding iMaxValue sentinel = "no upper bound" (used at the Building pass)


def _modifiers(rec, prop):
    """The property's BASIC modifiers (PropertySource) deposited into its OWN channel family, by scope, via the
    shared v3 converter (engine.property_source_v3 — the standard, so Property/Civic/… are uniform). DECAY->percent,
    CONSTANT/ATTRIBUTE_CONSTANT->flat (+ `per` when attribute-scaled). PropertyPropagator (DIFFUSE) -> DROPPED (#429)."""
    fam = OrderedDict()
    pm = rec.find("PropertyManipulators")
    if pm is None:
        return fam
    for src in pm.findall("PropertySource"):
        conv = engine.property_source_v3(src)
        if conv is None:
            continue
        p, scope, unit, value = conv
        if p == prop:                                          # the property's OWN sources (self-deposit)
            fam.setdefault(scope, OrderedDict())[unit] = value
    return {prop: fam} if fam else {}


def _granted_buildings(rec):
    """PropertyBuildings -> the effect-buildings the property GRANTS — a PURE LIST of building types.
    Per the object spec (owner 2026-06-15): `grants` and `requires` are SEPARATE reserved sections. `grants`
    LISTS the granted buildings (here). The `requires` — WHEN the building is active, the value-band atom
    `{ type: PROPERTY_X, scope: "city", min?, max? }` — belongs to the BUILDING's OWN `requires` section, NOT
    mixed into grants and NOT on the property. It is authored at the Building pass (reading PropertyBuildings off
    this property; the min/max is captured in the XML + store-accessible). Pattern: TECH enables the building →
    on enable the engine checks for a grant → the property GRANT gives it → it is AUTO-BUILT → the building's
    `requires` value-band decides active/dormant (enabler-spec §6.1)."""
    node = rec.find("PropertyBuildings")
    out = []
    if node is not None:
        for pb in node.findall("PropertyBuilding"):
            b = engine.text(pb.find("BuildingType"))
            if b and b != "NONE" and b not in out:
                out.append(b)
    return out


def _target_level(rec):
    base = engine.text(rec.find("iTargetLevel"))
    by_era = OrderedDict()
    node = rec.find("TargetLevelbyEraTypes")
    if node is not None:
        for e in node.findall("TargetLevelbyEraType"):
            era, lvl = engine.text(e.find("EraType")), engine.text(e.find("iTargetLevel"))
            if era and engine.is_int(lvl):
                by_era[era] = int(lvl)
    if by_era:
        out = OrderedDict()
        if engine.is_int(base):
            out["base"] = int(base)
        out["byEra"] = by_era
        return out
    return int(base) if engine.is_int(base) else None


# --- #429/diffusion (owner 2026-07-11: KEEP property spread -- "fairly ingrained in how properties work"; overrides
# the earlier drop). PropertyPropagator(DIFFUSE) + ChangePropagators -> the approved `properties` block. ---
PROP_OBJ_FROM = {"GAMEOBJECT_CITY": "city", "GAMEOBJECT_PLOT": "plot", "GAMEOBJECT_PLAYER": "empire"}
PROP_OBJ_TO = {"GAMEOBJECT_CITY": "city", "GAMEOBJECT_PLOT": "plots", "GAMEOBJECT_PLAYER": "empire"}
PROP_RELATION = {"RELATION_NEAR": "near", "RELATION_SAME_PLOT": "samePlot", "RELATION_TRADE": "trade"}
# the legacy Active-gate plot TAGs -> the EXISTING json.md predicates (§3.5) -- no new predicates invented.
PROP_TAG_PRED = {"TAG_OWNED": "IS_OWNED", "TAG_PEAK": "HAS_PEAK", "TAG_WATER": "IS_WATER", "TAG_CITY": "IS_CITY"}


def _propagators(rec):
    """Diffuse propagators + ChangePropagators -> `properties.diffuse[]` / `properties.changePropagation[]` (the
    approved shape). Each diffuse = {from, to, relation, distance?, percent, enabled?}; the optional Active/Is TAG_*
    gate maps to an existing predicate. changePropagation = {from, to, percent} (City->Player rollup; FLAMMABILITY only)."""
    props = OrderedDict()
    pm = rec.find("PropertyManipulators")
    diffuse = []
    if pm is not None:
        for pp in pm.findall("PropertyPropagator"):
            if engine.text(pp.find("PropertyPropagatorType")) != "PROPERTYPROPAGATOR_DIFFUSE":
                continue
            frm = PROP_OBJ_FROM.get(engine.text(pp.find("GameObjectType")))
            to = PROP_OBJ_TO.get(engine.text(pp.find("TargetObjectType")))
            rel = PROP_RELATION.get(engine.text(pp.find("TargetRelationType")))
            if not frm or not to or not rel:
                continue
            entry = OrderedDict([("from", frm), ("to", to), ("relation", rel)])
            dist = engine.text(pp.find("iTargetDistance"))
            if engine.is_int(dist) and int(dist) != 0:
                entry["distance"] = int(dist)
            pct = engine.text(pp.find("iPercent"))
            if engine.is_int(pct):
                entry["percent"] = int(pct)
            active = pp.find("Active")
            if active is not None:
                istag = active.find("Is")
                pred = PROP_TAG_PRED.get(engine.text(istag)) if istag is not None else None
                if pred:
                    entry["enabled"] = pred
            diffuse.append(entry)
    if diffuse:
        props["diffuse"] = diffuse
    cpn = rec.find("ChangePropagators")
    change = []
    if cpn is not None:
        for cp in cpn.findall("ChangePropagator"):
            frm = PROP_OBJ_FROM.get(engine.text(cp.find("GameObjectTypeFrom")))
            to = PROP_OBJ_TO.get(engine.text(cp.find("GameObjectTypeTo")))
            pct = engine.text(cp.find("iChangePercent"))
            if frm and to and engine.is_int(pct):
                change.append(OrderedDict([("from", frm), ("to", to), ("percent", int(pct))]))
    if change:
        props["changePropagation"] = change
    return {"properties": props} if props else {}


def curate(typ, rec):
    out = OrderedDict([("type", typ)])
    desc = engine.text(rec.find("Description"))
    if desc:
        out["description"] = desc
    text = OrderedDict((k, engine.text(rec.find(tag))) for tag, k in TEXT.items() if engine.text(rec.find(tag)))
    if text:
        out["text"] = text
    out.update(_modifiers(rec, typ))                         # <PROPERTY>.<scope>.<unit> basic modifiers (decay)
    out.update(_propagators(rec))                            # `properties` block: diffuse + changePropagation (owner: keep spread)
    gb = _granted_buildings(rec)
    if gb:
        out["grants"] = OrderedDict([("buildings", gb)])    # threshold bands = the enablers
    tl = _target_level(rec)
    if tl is not None:
        out["targetLevel"] = tl                              # ISOLATED field, outside enabler/modifier
    ai = OrderedDict()
    w = engine.text(rec.find("iAIWeight"))
    if engine.is_int(w) and int(w) != 0:
        ai["weight"] = int(w)
    scale = engine.text(rec.find("AIScaleType"))
    if scale and scale != "NONE":
        ai["scale"] = scale.replace("AISCALE_", "").lower()
    tr = engine.text(rec.find("iTrainReluctance"))
    if engine.is_int(tr) and int(tr) != 0:
        ai["trainReluctance"] = int(tr)
    omin = engine.text(rec.find("iOperationalRangeMin"))   # AI value-normalization band (CvCityAI decision-scoring);
    omax = engine.text(rec.find("iOperationalRangeMax"))   # AI-only -> ai bucket (owner 2026-07-01), NOT #429/dropped.
    orange = OrderedDict()
    if engine.is_int(omin):
        orange["min"] = int(omin)
    if engine.is_int(omax):
        orange["max"] = int(omax)
    if orange:
        ai["operationalRange"] = orange
    if ai:
        out["ai"] = ai
    identity = OrderedDict()                                 # property-system flags (pending rework) + display
    if engine.text(rec.find("bSourceDrain")) in ("1", "true", "True"):
        identity["sourceDrain"] = True
    if engine.text(rec.find("bOAType")) in ("1", "true", "True"):
        identity["oaType"] = True                            # likely near-dead (getter only); kept faithfully
    fb = engine.text(rec.find("FontButtonIndex"))
    if engine.is_int(fb):
        identity["fontButtonIndex"] = int(fb)
    if identity:
        out["identity"] = identity
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: all)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("PropertyInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items())
    print("PropertyInfo curated: %d" % len(results))
    if args.sample is not None:
        for nm in (args.sample or list(results)):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "properties")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d PropertyInfo JSON files under Assets/Data/properties" % len(results))


if __name__ == "__main__":
    main()
