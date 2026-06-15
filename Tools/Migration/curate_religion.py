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

# the 3 conditional commerce tables -> a condition member of the SPLIT commerce families.
COMMERCE_TABLES = {"StateReligionCommerces": "stateReligion", "HolyCityCommerces": "holyCity",
                   "GlobalReligionCommerces": "shrine"}
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Adjective": "adjective"}
ART = {"Button": "icon", "TechButton": "techButton", "GenericTechButton": "genericTechButton",
       "MovieFile": "movieFile", "MovieSound": "movieSound", "Sound": "sound", "iTGAIndex": "tgaIndex"}
GRANTS = {"FreeUnit": "freeUnit", "iFreeUnits": "numFreeUnits"}
IDENTITY = {"iSpreadFactor": "spreadFactor"}
SOURCE_UNIT = {"CONSTANT": "perTurn", "DECAY": "decay"}
DROP = {"TechPrereq"}
FAMILY_ORDER = ["gold", "research", "culture", "espionage", "religionInfluence"]

# inbound entity-targeted modifiers: (sourceEntity, field, targetType, family, valueKeys, unit, scope)
RELIGION_BOOSTS = [
    ("BuildingInfo", "ReligionChanges", "buildings", "religionInfluence", None, "flat", "city"),
]


def _properties(node, props):
    """PropertyManipulators -> {PROPERTY_X: {empire: [ {unit: amount, active?: <gate>} ]}} (gate preserved)."""
    for src in node:
        if src.tag != "PropertySource":
            continue
        cp = engine.clean_property_source(src)
        prop, amount = cp.get("property"), cp.get("amountPerTurn")
        if not prop or amount in (None, "", {}):
            continue
        unit = SOURCE_UNIT.get(cp.get("source", ""), str(cp.get("source", "")).lower())
        dep = OrderedDict()
        dep[unit] = amount
        gate = cp.get("Active")
        if gate not in (None, "", [], {}):
            dep["active"] = gate
        props.setdefault(prop, {}).setdefault("empire", []).append(dep)


def curate(typ, rec, store, boosts):
    text, fam, props, grants, art, identity, ai, leftover = {}, {}, {}, {}, {}, {}, {}, []
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in COMMERCE_TABLES:
            member = COMMERCE_TABLES[tag]
            for commerce, v in engine.named_array(c, engine.COMMERCES).items():   # commerce IS the family (split)
                (fam.setdefault(commerce, {}).setdefault("city", {}).setdefault(member, {}))["flat"] = v
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
            v = engine.generic(c)
            if v not in (None, "", [], {}, "NONE"):       # drop the "NONE" sentinel (e.g. <MovieSound>NONE)
                art[ART[tag]] = v
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
    if grants:
        out["grants"] = grants
    if ai:
        out["ai"] = ai
    if art:
        out["art"] = art
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
