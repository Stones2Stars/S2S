#!/usr/bin/env python3
"""Curate Corporation (#428) — a per-city institution (corps spread city-to-city; effects apply in each city
where active). BESPOKE. Verified vs CvCity (calculateCorporationMaintenanceTimes100 ~7807,
getCorporationYield/CommerceByCorporation ~12575/12596, applyCorporationModifiers ~15182) + CvGame spread.

FOUR distinct commerce/yield families that must NOT merge (the first-pass mapping lumped them):
- CommerceChanges / YieldChanges -> the SPLIT base families at CITY scope, `flat`: the genuinely-flat per-city
  add while the corp is active.
- CommercesProduced / YieldsProduced -> the SAME split families, CITY scope, member `produced`, unit
  `perBonus`: scaled by getNumBonuses(prereqBonus) x world CorporationMaintenancePercent (a per-owned-resource
  output), distinct from the flat add.
- HeadquarterCommerces -> split commerce families, EMPIRE scope, member `headquarters`, unit
  `perCorporationLevel`: the HQ-revenue lever (feeds corp maintenance, corp tax, and a HQ building's
  globalCorporationCommerce x countCorporationLevels) — empire-wide, not a per-city add.

Other modifiers (all city scope unless noted): iMaintenance -> maintenance.city.corporation.perBonus (per-owned
-bonus maintenance rate); iHealth/iHappiness -> health/happiness flat; iFreeXP -> experience flat;
iMilitaryProductionModifier -> production.city.military.percent.

Spread mechanic: iSpread (influence->spread %), iSpreadFactor (spread-unit cost scaling) -> identity config;
iSpreadCost -> a `cost` section ({spread: N}) — an intrinsic base GOLD cost to spread the corp, NOT the
GameSpeed/Era costs-MULTIPLIER family (overriding the classification, which conflated `cost` with `costs`).

Enabler chain: TechPrereq / PrereqBonuses / PrereqBuildings dropped (store inverts to tech/bonus/building
.enables.corporations). The corp's own `enables.buildings` is derived from BuildingInfo.PrereqCorporation (a
building gated by the active corp). BonusProduced -> grants (free resource in corp cities). Categories /
CompetingCorporations (corp<->corp exclusion) -> identity (none in base XML).

DEFERRED to the heavy-phase Building/Unit curation (these are SOURCE-side edges other entities declare, not the
corp's authored data): BuildingInfo.FoundsCorporation (the HQ-founding building), UnitInfo.CorporationSpreads
(which units spread the corp), BuildingInfo.GlobalCorporationCommerce (a building amplifying HQ commerce).

  python3 curate_corporation.py --sample CORPORATION_1 CORPORATION_2
  python3 curate_corporation.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import de_i
from store import Store, REPO

# tag -> (family, scope, member, unit, valueKeys). valueKeys => SPLIT base family (identifier IS the family).
FAMILIES = {
    "iHealth":                    ("health",      "city",   None,           "flat",               None),
    "iHappiness":                 ("happiness",   "city",   None,           "flat",               None),
    "iFreeXP":                    ("experience",  "city",   None,           "flat",               None),
    "iMilitaryProductionModifier":("production",  "city",   "military",     "percent",            None),
    "iMaintenance":               ("maintenance", "city",   "corporation",  "perBonus",           None),
    "CommerceChanges":            (None,          "city",   None,           "flat",               engine.COMMERCES),
    "YieldChanges":               (None,          "city",   None,           "flat",               engine.YIELDS),
    "CommercesProduced":          (None,          "city",   "produced",     "perBonus",           engine.COMMERCES),
    "YieldsProduced":             (None,          "city",   "produced",     "perBonus",           engine.YIELDS),
    "HeadquarterCommerces":       (None,          "empire", "headquarters", "perCorporationLevel",engine.COMMERCES),
}
TEXT = {"Description": "description", "Civilopedia": "civilopedia"}
ART = {"Button": "icon", "MovieFile": "movieFile", "MovieSound": "movieSound", "Sound": "sound",
       "iTGAIndex": "tgaIndex"}
IDENTITY = {"iSpread": "spread", "iSpreadFactor": "spreadFactor", "Categories": "categories",
            "CompetingCorporations": "competingCorporations"}
GRANTS = {"BonusProduced": "bonusProduced", "FreeUnit": "freeUnit"}
DROP = {"TechPrereq", "PrereqBonuses", "PrereqBuildings"}
FAMILY_ORDER = ["food", "production", "commerce", "gold", "research", "culture", "espionage",
                "health", "happiness", "experience", "maintenance"]


def _put(fam, family, scope, member, unit, val):
    node = fam.setdefault(family, {}).setdefault(scope, {})
    if member:
        node = node.setdefault(member, {})
    node[unit] = val


def _apply_family(fam, spec, c):
    family, scope, member, unit, keys = spec
    if keys:                                   # SPLIT base family: the identifier IS the family name
        for ident, v in engine.named_array(c, keys).items():
            _put(fam, ident, scope, member, unit, v)
    else:
        t = engine.text(c)
        if engine.is_int(t) and int(t) != 0:
            _put(fam, family, scope, member, unit, int(t))


def curate(typ, rec, store):
    text, fam, art, identity, grants, cost, leftover = {}, {}, {}, {}, {}, {}, []
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in FAMILIES:
            _apply_family(fam, FAMILIES[tag], c)
        elif tag == "iSpreadCost":
            if engine.is_int(t) and int(t) != 0:
                cost["spread"] = int(t)
        elif tag in GRANTS:
            v = engine.text(c)
            if v and v != "NONE":
                grants[GRANTS[tag]] = v
        elif tag in ART:
            v = engine.generic(c)
            if v not in (None, "", [], {}, "NONE"):
                art[ART[tag]] = v
        elif tag in IDENTITY:
            if engine.is_int(t):
                if int(t) != 0:
                    identity[IDENTITY[tag]] = int(t)
            elif (list(c) or t) and t != "NONE":
                identity[IDENTITY[tag]] = engine.generic(c)
        else:
            if list(c) or t:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(c)

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "civilopedia"):
        if k in text:
            out[k] = text[k]
    enables = store.enabled_by(typ)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    for family in FAMILY_ORDER:
        if family in fam:
            out[family] = fam[family]
    for family in fam:
        if family not in out:
            out[family] = fam[family]
    if grants:
        out["grants"] = grants
    if cost:
        out["cost"] = cost
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
    table = store.table("CorporationInfo")
    results, all_leftover = OrderedDict(), set()
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec, store)
        results[typ] = obj
        all_leftover.update(leftover)
    print("CorporationInfo curated: %d" % len(results))
    print("  with enables: %d" % sum(1 for o in results.values() if "enables" in o))
    if all_leftover:
        print("  leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "corporations")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d CorporationInfo JSON files under Assets/Data/corporations" % len(results))


if __name__ == "__main__":
    main()
