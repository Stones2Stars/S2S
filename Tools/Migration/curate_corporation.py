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
import curate_common as cc
from curate_common import de_i, descale100
from store import Store, REPO

# Corporation follows the RELIGION model (owner 2026-06-15): founding creates the HQ building, then the corp
# SPREADS like a religion (an isHasCorporation city flag) and its per-city effects apply only where active. So every
# per-city deposit is gated by `enabled:{HAS_CORPORATION: SELF}` (the spread-presence predicate, parallel to
# HAS_RELIGION). The `*Produced` output scales by the corp's PrereqBonuses set (C++: YieldProduced x SUM
# getNumBonuses over the prereq bonuses) -> `per:{anyOf:[prereqBonuses], scope:city}`. The FOUND requirement
# (PrereqBonuses needed to establish the corp) is authored on the HQ `FoundsCorporation` building at the Building
# pass, NOT here. (FIRST PASS — corps clearly need a dedicated rework pass: the HQ-revenue HeadquarterCommerces /
# perCorporationLevel modeling + the spread mechanics are deferred to it.)
# tag -> (family, scope, member, unit, valueKeys, perBonus). valueKeys => SPLIT base family; perBonus => add the
# prereq-bonus `per` scaling.
FAMILIES = {
    "iHealth":                    ("health",      "city",   None,           "flat",    None,             False),
    "iHappiness":                 ("happiness",   "city",   None,           "flat",    None,             False),
    "iFreeXP":                    ("experience",  "city",   None,           "flat",    None,             False),
    "iMilitaryProductionModifier":("production",  "city",   "military",     "percent", None,             False),
    "iMaintenance":               ("maintenance", "city",   "corporation",  "flat",    None,             True),
    "CommerceChanges":            (None,          "city",   None,           "flat",    engine.COMMERCES, False),
    "YieldChanges":               (None,          "city",   None,           "flat",    engine.YIELDS,    False),
    "CommercesProduced":          (None,          "city",   None,           "flat",    engine.COMMERCES, True),
    "YieldsProduced":             (None,          "city",   None,           "flat",    engine.YIELDS,    True),
}
# DEFERRED to the corp rework pass (HQ revenue, scaled by countCorporationLevels): kept in its current
# headquarters/perCorporationLevel form, ungated, pending the per-corp-level token + HQ-city modeling.
HQ_COMMERCE = "HeadquarterCommerces"
TEXT = {"Description": "description", "Civilopedia": "civilopedia"}
# art tags -> ui/world/sound via the canonical curate_common.ART_BLOCK.
ART = {"Button", "MovieFile", "MovieSound", "Sound", "iTGAIndex"}
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


def _entry(value, typ, per):
    """A per-city corp deposit: gated by the spread-presence predicate, optionally per-bonus scaled."""
    e = OrderedDict([("value", value), ("enabled", OrderedDict([("HAS_CORPORATION", typ)]))])
    if per:
        e["per"] = per
    return e


def _put_entry(fam, family, scope, member, unit, entry):
    node = fam.setdefault(family, {}).setdefault(scope, {})
    if member:
        node = node.setdefault(member, {})
    node.setdefault(unit, []).append(entry)


def _apply_family(fam, spec, c, typ, per_bonus):
    family, scope, member, unit, keys, perbonus = spec
    per = per_bonus if perbonus else None
    if keys:                                   # SPLIT base family: the identifier IS the family name
        for ident, v in engine.named_array(c, keys).items():
            # `*Produced` (perbonus) is x100 in the legacy XML -- getYieldProduced/getCommerceProduced. The C++
            # accessor is NOT named get...100() so the accessor-name heuristic misses it, but the MATH is decisive:
            # getCorporationYieldByCorporation (CvCity.cpp:12594-12602) makes produced=75 -> 0.75/bonus, so it is x100.
            # De-scale to human (readJson re-applies x100). `*Changes` (perbonus=False) is genuinely x1
            # (getYieldChange is x100'd in-formula) -> stays as-is.
            vv = descale100(v) if perbonus else v
            _put_entry(fam, ident, scope, member, unit, _entry(vv, typ, per))
    else:
        t = engine.text(c)
        if engine.is_int(t) and int(t) != 0:
            _put_entry(fam, family, scope, member, unit, _entry(int(t), typ, per))


def curate(typ, rec, store):
    text, fam, art_blocks, identity, grants, cost, leftover = {}, {}, {}, {}, {}, {}, []
    prereq_bonuses = [b for b in (engine.text(x) for x in rec.findall("PrereqBonuses/BonusType"))
                      if b and b != "NONE"]
    per_bonus = OrderedDict([("anyOf", prereq_bonuses), ("scope", "city")]) if prereq_bonuses else None
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in FAMILIES:
            _apply_family(fam, FAMILIES[tag], c, typ, per_bonus)
        elif tag == HQ_COMMERCE:                               # DEFERRED HQ revenue (corp rework pass)
            for ident, v in engine.named_array(c, engine.COMMERCES).items():
                _put(fam, ident, "empire", "headquarters", "perCorporationLevel", v)
        elif tag == "iSpreadCost":
            if engine.is_int(t) and int(t) != 0:
                cost["spread"] = int(t)
        elif tag in GRANTS:
            v = engine.text(c)
            if v and v != "NONE":
                grants[GRANTS[tag]] = v
        elif tag in ART:
            cc.put_art(art_blocks, tag, engine.generic(c))   # -> ui/world/sound via ART_BLOCK (+ drop empty/NONE)
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
    cc.emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    cc.fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
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
