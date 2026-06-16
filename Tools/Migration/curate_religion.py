#!/usr/bin/env python3
"""Curate Religion (#428) — a foundable faith. BESPOKE. Its gameplay payload is THREE per-commerce tables,
all CITY scope, all flat, distinguished by the CONDITION under which they deposit — modelled as a condition
MEMBER of the SPLIT commerce families (parallel to Heritage's `byEra`), verified vs CvCity (per the
classify-light-batch workflow):

- StateReligionCommerces -> member `stateReligion`: deposits in EVERY city that has this religion present
  (CvCity.cpp:12503, isHasReligion).
- HolyCityCommerces      -> member `holyCity`: deposits only in this religion's holy city (CvCity.cpp:12507).
- GlobalReligionCommerces-> member `shrine`: the religion's SHRINE contribution — consumed in whichever city
  holds a building whose getGlobalReligionCommerce() points at this religion (CvCity.cpp:12185/12288), the
  per-commerce value world-scaled by countReligionLevels. The world-scaling is intrinsic to the `shrine`
  concept (a consumer rule), not encoded in the data. (Mapping's "player" scope was wrong on all three.)

All three are raw per-commerce ints multiplied into the x100 commerce paths downstream; carried faithfully (#432).

Inbound boost: BuildingInfo.ReligionChanges (the ONLY per-specific-religion-keyed table in the codebase,
CvCity.cpp:4695 changeReligionInfluence) -> the `religionInfluence` family, city scope, keyed by the source
building. Family name provisional.

enables: a religion gates buildings/units that require it (BuildingInfo/UnitInfo.PrereqReligion, a GOM_RELIGION
construct/train requirement — isHasReligion, the religion is the CONDITIONER, parallel to PrereqCorporation).
Derived from the store -> religion.enables.{buildings,units}.

Other fields: FreeUnit/iFreeUnits -> grants, but the award is gated ENTIRELY by the count (CvPlayer.cpp:8838
`if getNumFreeUnits() > 0`) — so freeUnit is emitted ONLY when iFreeUnits>0; an FK with count 0 is inert and
dropped (the game grants nothing). iSpreadFactor -> identity (per-religion spread-decay scalar). Adjective ->
text. Flavors -> ai. TechPrereq -> DROP (store inverts to tech.enables.religions). PropertyManipulators handled
defensively (gated empire-scope list, like Heritage) — none in the base XML. Runtime
m_iMissionType/m_iChar/m_shrineBuildings are not XML-backed -> never appear.

  python3 curate_religion.py --sample RELIGION_JUDAISM
  python3 curate_religion.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import curate_common as cc
from curate_common import de_i
from store import Store, REPO

# Conditional commerce (v0.3, owner 2026-06-15): the gate tables become `enabled` deposits using OBJECT-EVALUATED
# PREDICATES (the city/player self-reports the runtime state the static Info can't hold). The data names the
# predicate + the specific religion as the single conditional; the engine owns the compound logic (for
# STATE_RELIGION the C++ relaxes to "present AND (is-state-religion OR no-state-religion OR non-state-commerce)").
COMMERCE_PREDICATE = {"StateReligionCommerces": "STATE_RELIGION", "HolyCityCommerces": "HOLY_CITY"}
# GlobalReligionCommerce is NOT a city gate — it's value x countReligionLevels(religion) (WORLD-scaled) consumed
# through a shrine building. PARKED to the Building pass (the religion<->shrine-building routing + the world
# religion-levels count token live there); the raw per-commerce values are kept faithfully in a `shrine` section.
SHRINE_TABLE = "GlobalReligionCommerces"
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Adjective": "adjective"}
# art tags this entity carries — routed to ui/world/sound via the canonical curate_common.ART_BLOCK.
ART = {"Button", "TechButton", "GenericTechButton", "MovieFile", "MovieSound", "Sound", "iTGAIndex"}
GRANTS = {"FreeUnit": "freeUnit", "iFreeUnits": "numFreeUnits"}
IDENTITY = {"iSpreadFactor": "spreadFactor"}
DROP = {"TechPrereq"}
FAMILY_ORDER = ["gold", "research", "culture", "espionage", "religionInfluence"]

# inbound entity-targeted modifiers: (sourceEntity, field, targetType, family, valueKeys, unit, scope)
RELIGION_BOOSTS = [
    ("BuildingInfo", "ReligionChanges", "buildings", "religionInfluence", None, "flat", "city"),
]


def _properties(node, props):
    """PropertyManipulators -> v3 deposits via the shared converter (engine.property_source_v3 — the standard).
    Religions carry no PropertySources today (defensive); kept uniform with Property/Civic."""
    for src in node.findall("PropertySource"):
        conv = engine.property_source_v3(src)
        if conv is None:
            continue
        prop, scope, unit, value = conv
        props.setdefault(prop, OrderedDict()).setdefault(scope, OrderedDict())[unit] = value


def curate(typ, rec, store, boosts):
    text, fam, props, grants, art_blocks, identity, ai, leftover = {}, {}, {}, {}, {}, {}, {}, []
    shrine = OrderedDict()
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in COMMERCE_PREDICATE:                        # state-religion / holy-city -> `enabled` deposits
            predicate = COMMERCE_PREDICATE[tag]                # using an OBJECT-EVALUATED predicate {PRED: religion}
            for commerce, v in engine.named_array(c, engine.COMMERCES).items():   # commerce IS the family (split)
                entry = OrderedDict([("value", v), ("enabled", OrderedDict([(predicate, typ)]))])
                fam.setdefault(commerce, {}).setdefault("city", {}).setdefault("flat", []).append(entry)
        elif tag == SHRINE_TABLE:                              # GlobalReligionCommerce -> parked `shrine` values
            for commerce, v in engine.named_array(c, engine.COMMERCES).items():   # (Building wires world-scaling)
                shrine[commerce] = v
        elif tag == "PropertyManipulators":
            _properties(c, props)
        elif tag == "Flavors":
            v = engine.generic(c)
            if v:
                ai["flavours"] = v
        elif tag in GRANTS:
            v = int(t) if engine.is_int(t) else (t or None)
            if v not in (None, "", "NONE", 0):
                grants[GRANTS[tag]] = v
        elif tag in ART:
            cc.put_art(art_blocks, tag, engine.generic(c))   # -> ui/world/sound via ART_BLOCK (+ drop empty/NONE)
        elif tag in IDENTITY:
            if engine.is_int(t):
                if int(t) != 0:
                    identity[IDENTITY[tag]] = int(t)
            elif t or list(c):
                identity[IDENTITY[tag]] = engine.generic(c)
        else:
            if list(c) or t:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(c)

    for family, fdata in boosts.items():                   # fold the religionInfluence inbound boost
        fam[family] = cc._merge_val(fam[family], fdata) if family in fam else fdata

    # The free-unit award is gated ENTIRELY by the count (CvPlayer.cpp:8838 `if getNumFreeUnits() > 0`): when
    # iFreeUnits is 0 the FreeUnit FK is inert and the game grants nothing. Drop the FK when the count didn't
    # survive, so we don't assert a grant the game never makes (~13 religions have FreeUnit set but iFreeUnits=0).
    if "numFreeUnits" not in grants:
        grants.pop("freeUnit", None)

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "civilopedia", "adjective"):
        if k in text:
            out[k] = text[k]
    enables = store.enabled_by(typ)                        # religion -> building/unit (PrereqReligion gate)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    for family in FAMILY_ORDER:
        if family in fam:
            out[family] = fam[family]
    for family in fam:
        if family not in out:
            out[family] = fam[family]
    for prop in sorted(props):
        out[prop] = props[prop]
    if shrine:                                                 # parked GlobalReligionCommerce values (Building pass
        out["shrine"] = shrine                                 # wires the world-scaling + shrine-building routing)
    if grants:
        out["grants"] = grants
    if ai:
        out["ai"] = ai
    cc.emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    return out, leftover


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    boosts = cc.accumulate_boosts(store, RELIGION_BOOSTS)
    table = store.table("ReligionInfo")
    results, all_leftover = OrderedDict(), set()
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec, store, boosts.get(typ, {}))
        results[typ] = obj
        all_leftover.update(leftover)
    print("ReligionInfo curated: %d" % len(results))
    print("  with religionInfluence: %d" % sum(1 for o in results.values() if "religionInfluence" in o))
    if all_leftover:
        print("  leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "religions")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d ReligionInfo JSON files under Assets/Data/religions" % len(results))


if __name__ == "__main__":
    main()
