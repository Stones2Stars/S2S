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
- HeadquarterCommerces -> split commerce families, EMPIRE scope, member `headquarters`, flat with
  `per:"CORPORATION_LEVEL"` (ruling 4, §3.7 per-scaler): the HQ-revenue lever (feeds corp maintenance, corp
  tax, and a HQ building's globalCorporationCommerce x countCorporationLevels) — empire-wide, not a per-city add.

Other modifiers (all city scope unless noted): iMaintenance -> maintenance.city.corporation.perBonus (per-owned
-bonus maintenance rate); iHealth/iHappiness -> health/happiness flat; iFreeXP -> experience flat;
iMilitaryProductionModifier -> buildRate.city.military.percent (the L10 census fix 2026-07-05: an item-COST
discount for military units — the buildRate category, per modifier.md's production-vs-buildRate split; the
earlier production.city.military address was the named "Versailles bug" class and no reader consumed it).

Spread mechanic (owner ruling 2026-07-01 — propensity-name ALIGNMENT with religion + a misnomer FIX):
- iSpread -> identity.spreadFactor: the "how readily I spread" scalar, the concept-parallel of religion's
  identity.spreadFactor (owner "reuse religion"). Output key renamed spread -> spreadFactor.
- iSpreadFactor -> identity.competingSpreadCostPercent: the LEGACY name is a MISNOMER — its real meaning is a
  cost-inflation % on a COMPETING corp's spread (CvUnit.cpp:8687), not a spread factor. Renamed to say so.
  (No key collision: iSpread now owns `spreadFactor`, iSpreadFactor moves OFF it to `competingSpreadCostPercent`.)
- iSpreadCost -> a `cost` section ({spread: N}) — an intrinsic base GOLD cost to spread the corp, NOT the
  GameSpeed/Era costs-MULTIPLIER family (overriding the classification, which conflated `cost` with `costs`).

Enabler chain: TechPrereq / PrereqBonuses / PrereqBuildings dropped (store inverts to tech/bonus/building
.enables.corporations). The corp's own `enables.buildings` is derived from BuildingInfo.PrereqCorporation (a
building gated by the active corp). BonusProduced -> provides.bonuses (owner 2026-07-01): while the corp is
present+active it CONTINUOUSLY supplies that BONUS_ to corp cities — a §5a continuous in-vicinity supply, NOT a
one-shot `grants` handout. Categories / CompetingCorporations (corp<->corp exclusion) -> identity (none in base XML).

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
# pass, NOT here. (FIRST PASS — corps clearly need a dedicated rework pass: the HQ-city modeling + the spread
# mechanics land there; HeadquarterCommerces already authors as the CORPORATION_LEVEL per-scaler, ruling 4.)
# tag -> (family, scope, member, unit, valueKeys, perBonus). valueKeys => SPLIT base family; perBonus => add the
# prereq-bonus `per` scaling.
FAMILIES = {
    "iHealth":                    ("health",      "city",   None,           "flat",    None,             False),
    "iHappiness":                 ("happiness",   "city",   None,           "flat",    None,             False),
    "iFreeXP":                    ("experience",  "city",   None,           "flat",    None,             False),
    "iMilitaryProductionModifier":("buildRate",   "city",   "military",     "percent", None,             False),
    "iMaintenance":               ("maintenance", "city",   "corporation",  "flat",    None,             True),
    "CommerceChanges":            (None,          "city",   None,           "flat",    engine.COMMERCES, False),
    "YieldChanges":               (None,          "city",   None,           "flat",    engine.YIELDS,    False),
    "CommercesProduced":          (None,          "city",   None,           "flat",    engine.COMMERCES, True),
    "YieldsProduced":             (None,          "city",   None,           "flat",    engine.YIELDS,    True),
}
# HQ revenue, scaled by countCorporationLevels: a §3.7 per-scaler (ruling 4, info-rebuild.md) --
# <c>.empire.headquarters.flat = {value, per:"CORPORATION_LEVEL"} (the minted §3.1 count token, bare-string
# sugar). NB the engine count is the WORLD-wide corporation-level tally (CvCity.cpp countCorporationLevels);
# the `headquarters` member itself is still on the ruling-4 triage (suspected corp-HQ FK value plane).
HQ_COMMERCE = "HeadquarterCommerces"
TEXT = {"Description": "description", "Civilopedia": "civilopedia"}
# art tags -> ui/world/sound via the canonical curate_common.ART_BLOCK.
ART = {"Button", "MovieFile", "MovieSound", "Sound", "iTGAIndex"}
# owner 2026-07-01: iSpread -> spreadFactor (reuse religion's propensity name); iSpreadFactor ->
# competingSpreadCostPercent (its real meaning per CvUnit.cpp:8687 — a % cost-inflation on a COMPETING corp's
# spread, so `spreadFactor` was a misnomer). Both still land in `identity`; no key collision (see docstring).
IDENTITY = {"iSpread": "spreadFactor", "iSpreadFactor": "competingSpreadCostPercent", "Categories": "categories"}
# CompetingCorporations -> top-level `excludes` list (owner ruling 2026-07-01, REVERSED from the earlier
# identity-parking): corp<->corp mutual exclusion IS the json §9 same-tier `excludes` model
# ("excludes": ["CORPORATION_X", ...]). Empty in the shipped base XML (no corp carries it), so no shipped corp
# emits `excludes` today — this migrates the MAPPING so future data lands in `excludes`, not identity.
EXCLUDES = "CompetingCorporations"
GRANTS = {"FreeUnit": "freeUnit"}
DROP = {"TechPrereq", "PrereqBonuses"}
# PrereqBuildings -> requires.spread count atoms ({type: BUILDING_X, scope: empire, min: N}, json §4.3): the corp's
# per-building EMPIRE-count need for SPREADING into a city (the executive-spread gate, CvUnit.cpp; evaluated against
# the target city's owner at spread time -- never the enabler). Empty in ALL shipped base+module XML (verified
# 2026-07-17), so no corp emits it today -- the mapping is served so future data lands live (owner ruling: need fix).
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


def _excludes(node):
    """CompetingCorporations -> a list of CORPORATION_* this corp mutually excludes (json §9 same-tier
    `excludes`). Shape: CompetingCorporation{CorporationType, bCompeting}; include the type where bCompeting is
    true (engine CvCorporationInfo::isCompetingCorporation; the CvGame check is symmetric)."""
    out = []
    for e in node.findall("CompetingCorporation"):
        ct = engine.text(e.find("CorporationType"))
        comp = engine.text(e.find("bCompeting"))
        if ct and ct != "NONE" and comp in ("1", "true", "True"):
            out.append(ct)
    return out


def curate(typ, rec, store):
    text, fam, art_blocks, identity, grants, cost, leftover = {}, {}, {}, {}, {}, {}, []
    excludes = []
    provides_bonuses = []   # BonusProduced -> provides.bonuses (§5a continuous in-vicinity supply while active)
    spread_buildings = []   # PrereqBuildings pairs -> requires.spread count atoms (see the mapping note above)
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
        elif tag == HQ_COMMERCE:                               # HQ revenue -> per-scaler (ruling 4; see HQ_COMMERCE note)
            for ident, v in engine.named_array(c, engine.COMMERCES).items():
                _put(fam, ident, "empire", "headquarters", "flat",
                     OrderedDict([("value", v), ("per", "CORPORATION_LEVEL")]))
        elif tag == "iSpreadCost":
            if engine.is_int(t) and int(t) != 0:
                cost["spread"] = int(t)
        elif tag == EXCLUDES:                                  # -> top-level `excludes` (json §9, owner 2026-07-01)
            excludes.extend(_excludes(c))
        elif tag == "BonusProduced":                           # -> provides.bonuses (§5a continuous supply, owner 2026-07-01)
            if t and t != "NONE":
                provides_bonuses.append(t)
        elif tag == "PrereqBuildings":                         # -> requires.spread count atoms (mapping note above)
            for pair in c:
                kids = list(pair)
                if len(kids) < 2:
                    continue
                b, n = engine.text(kids[0]), engine.text(kids[1])
                if b and b != "NONE" and engine.is_int(n) and int(n) > 0:
                    spread_buildings.append(OrderedDict(
                        [("type", b), ("scope", "empire"), ("min", int(n))]))
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
    if spread_buildings:
        out["requires"] = OrderedDict([("spread", OrderedDict([("all", spread_buildings)]))])
    if excludes:
        out["excludes"] = excludes
    for family in FAMILY_ORDER:
        if family in fam:
            out[family] = fam[family]
    for family in fam:
        if family not in out:
            out[family] = fam[family]
    if grants:
        out["grants"] = grants
    if provides_bonuses:
        out["provides"] = OrderedDict([("bonuses", provides_bonuses)])
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
