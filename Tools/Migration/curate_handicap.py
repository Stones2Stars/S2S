#!/usr/bin/env python3
"""Curate Handicap to the top-down model (#428) — a CONFIG entity (enables nothing): modifiers only.

FLAT FAMILY structure (owner ruling 2026-06-14): there is NO `modifiers` wrapper. Each modifier FAMILY —
the *kind* of thing modified (maintenance / upkeep / diplomacy / property / happiness / health / growth /
combat / …) — is its own top-level section, and they all share ONE uniform structure:

    <family>.<scope>.<member>.<unit> = value        (member omitted for single-concept families)

The human/AI duality is an `ai:` audience sub-object beside the unit. Families are distinct kinds that
interact but never merge (you never add a 5th commerce into `yield`); the flat surface keeps each one focused.
`grants` (one-shot setup) and `identity` (the goody list) are NOT modifier families and stay their own sections.

Scope/unit/audience + the AI-only-vs-dual split are verified against Sources/Infos/CvHandicapInfo.h and
Sources/docs/reference/handicaps.md (game-vs-own sourcing, dead fields, the maintenance computation).
Each PROPERTY_* is its own top-level family (split, no `property` wrapper — owner ruling 2026-06-14). NB: the
AI-economy family names (growth/techCost/workRate/buildCost/perEra) are PROVISIONAL; `research`->`techCost`
so it can't read as the research commerce. advanced-start is NOT a modifier (a pre-game points budget; poorly
supported) -> parked in identity.advancedStart for review.

  python3 curate_handicap.py --sample HANDICAP_CHIEFTAIN
  python3 curate_handicap.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import de_i
from store import Store, REPO

# tag -> (family, scope, member, unit, audience). member=None => single-concept family. audience "ai" => AI override.
FAMILIES = {
    # --- maintenance: gold city-maintenance cost (post-income). Components mirror CvCity::calculateBaseMaintenance ---
    "iDistanceMaintenancePercent":    ("maintenance", "empire", "distance",    "percent", None),
    "iNumCitiesMaintenancePercent":   ("maintenance", "empire", "numCities",   "percent", None),
    "iColonyMaintenancePercent":      ("maintenance", "empire", "colony",      "percent", None),
    "iCorporationMaintenancePercent": ("maintenance", "empire", "corporation", "percent", None),
    "iMaxColonyMaintenance":          ("maintenance", "empire", "colony",      "cap",     None),  # caps the colony component
    # --- upkeep: recurring gold upkeep costs ---
    "iUnitUpkeepPercent":             ("upkeep", "empire", "unit",      "percent", None),
    "iAIUnitUpkeepPercent":           ("upkeep", "empire", "unit",      "percent", "ai"),
    "iCivicUpkeepPercent":            ("upkeep", "empire", "civic",     "percent", None),
    "iAICivicUpkeepPercent":          ("upkeep", "empire", "civic",     "percent", "ai"),
    "iInflationPercent":              ("upkeep", "empire", "inflation", "percent", None),
    "iAIInflationPercent":            ("upkeep", "empire", "inflation", "percent", "ai"),
    "iAIUnitSupplyPercent":           ("upkeep", "empire", "supply",    "percent", "ai"),
    "iAIUnitUpgradePercent":          ("upkeep", "empire", "upgrade",   "percent", "ai"),
    # --- wellbeing: single-concept families ---
    "iHealthBonus":                   ("health",    "empire", None, "flat", None),
    "iHappyBonus":                    ("happiness", "empire", None, "flat", None),
    # --- AI economy rates (PROVISIONAL family names) ---
    "iAIGrowthPercent":               ("growth",        "empire", None, "percent", "ai"),
    "iAIResearchPercent":             ("techCost",      "empire", None, "percent", "ai"),  # AI tech-cost % (NOT the research commerce)
    "iAIWorkRateModifier":            ("workRate",      "empire", None, "percent", "ai"),
    "iAITrainPercent":                ("buildCost", "empire", "train",          "percent", "ai"),
    "iAIWorldTrainPercent":           ("buildCost", "empire", "worldTrain",     "percent", "ai"),
    "iAIConstructPercent":            ("buildCost", "empire", "construct",      "percent", "ai"),
    "iAIWorldConstructPercent":       ("buildCost", "empire", "worldConstruct", "percent", "ai"),
    "iAICreatePercent":               ("buildCost", "empire", "create",         "percent", "ai"),
    "iAIWorldCreatePercent":          ("buildCost", "empire", "worldCreate",    "percent", "ai"),
    "iAIPerEraModifier":              ("perEra",        "empire", None, "percent", "ai"),  # meta: ramps the AI family per era
    "iRevolutionIndexPercent":        ("revolution",    "empire", None, "percent", None),  # INCOMPLETE (WIP mechanic, tracked issue) — keep, NOT dead
    # --- diplomacy ---
    "iAttitudeChange":                ("diplomacy", "empire", "attitude",       "flat",    None),  # via TARGET player's handicap
    "iAIDeclareWarProb":              ("diplomacy", "empire", "declareWar",     "percent", "ai"),
    "iAIWarWearinessPercent":         ("diplomacy", "empire", "warWeariness",   "percent", "ai"),
    "iNoTechTradeModifier":           ("diplomacy", "team",   "noTechTrade",    "percent", None),
    "iTechTradeKnownModifier":        ("diplomacy", "team",   "techTradeKnown", "percent", None),
    # --- combat: wildlife/barbarian odds (game-global; freeWinsVsBarbs is per-player own) ---
    "iAnimalBonus":                   ("combat", "world",  "animal",          "percent", None),
    "iAIAnimalBonus":                 ("combat", "world",  "animal",          "percent", "ai"),
    "iBarbarianBonus":                ("combat", "world",  "barbarian",       "percent", None),
    "iAIBarbarianBonus":              ("combat", "world",  "barbarian",       "percent", "ai"),
    "iSubdueAnimalBonusAI":           ("combat", "world",  "subdueAnimal",    "percent", "ai"),
    "iFreeWinsVsBarbs":               ("combat", "empire", "freeWinsVsBarbs", "flat",    None),
    # --- barbarians: game-global spawn rules ---
    "iAnimalAttackProb":                  ("barbarians", "world", "animalAttackProb",  "percent", None),
    "iUnownedWaterTilesPerBarbarianUnit": ("barbarians", "world", "waterTilesPerUnit", "flat", None),
    "iUnownedTilesPerBarbarianCity":      ("barbarians", "world", "tilesPerCity",      "flat", None),
    "iBarbarianCityCreationTurnsElapsed": ("barbarians", "world", "cityCreationTurns", "flat", None),
    "iBarbarianCityCreationProb":         ("barbarians", "world", "cityCreationProb",  "percent", None),
    "iBarbarianDefenders":                ("barbarians", "world", "defenders",         "flat", None),
}

# one-shot game-start grants: tag -> (grantKey, audience). AI overrides under grants.ai.
GRANTS = {
    "iGold":                   ("startingGold",         None),
    "iStartingDefenseUnits":   ("startingDefenseUnits", None),
    "iStartingWorkerUnits":    ("startingWorkerUnits",  None),
    "iStartingExploreUnits":   ("startingExploreUnits", None),
    "iAIStartingDefenseUnits": ("startingDefenseUnits", "ai"),
    "iAIStartingWorkerUnits":  ("startingWorkerUnits",  "ai"),
    "iAIStartingExploreUnits": ("startingExploreUnits", "ai"),
}

# advanced-start: a pre-game POINTS BUDGET (spent to "buy" techs/cities/units/settlers before turn 1) — NOT a
# modifier, and poorly supported in the DLL. Parked in identity pending a review of the whole advanced-start
# mechanic (it also governs starting units/settlers under that mode). tag -> identity.advancedStart key.
ADVANCED_START = {"iAdvancedStartPointsMod": "pointsMod", "iAIAdvancedStartPercent": "aiPercent"}

HOIST_TEXT = {"Description": "description", "Help": "help"}
GAMEOBJECT_SCOPE = {"GAMEOBJECT_CITY": "city", "GAMEOBJECT_PLOT": "plot", "GAMEOBJECT_UNIT": "unit"}
SOURCE_UNIT = {"CONSTANT": "perTurn", "DECAY": "decay"}
# output order of the family sections (those present)
FAMILY_ORDER = ["maintenance", "upkeep", "happiness", "health", "growth", "techCost", "workRate", "buildCost",
                "perEra", "revolution", "diplomacy", "combat", "barbarians"]   # PROPERTY_* families fall in after


def _put(fam, family, scope, member, unit, audience, val):
    """Deposit into a family section: <family>.<scope>[.<member>][.ai].<unit> = val."""
    node = fam.setdefault(family, {}).setdefault(scope, {})
    if member:
        node = node.setdefault(member, {})
    if audience == "ai":
        node = node.setdefault("ai", {})
    node[unit] = val


def curate(typ, rec):
    text_fields, fam, grants, identity, leftover = {}, {}, {}, {}, []
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type":
            continue
        elif tag in HOIST_TEXT:
            if t:
                text_fields[HOIST_TEXT[tag]] = t
        elif tag in FAMILIES:
            if engine.is_int(t) and int(t) != 0:          # 0 is the additive identity -> drop (lossless)
                family, scope, member, unit, aud = FAMILIES[tag]
                _put(fam, family, scope, member, unit, aud, int(t))
        elif tag in GRANTS:
            if engine.is_int(t) and int(t) != 0:
                key, aud = GRANTS[tag]
                (grants.setdefault("ai", {}) if aud == "ai" else grants)[key] = int(t)
        elif tag in ADVANCED_START:                       # not a modifier -> parked in identity, needs review
            if engine.is_int(t) and int(t) != 0:
                identity.setdefault("advancedStart", {})[ADVANCED_START[tag]] = int(t)
        elif tag == "PropertyManipulators":               # each PROPERTY_* is its OWN family: PROPERTY_X.<scope>.<unit>
            for src in c:
                if src.tag != "PropertySource":
                    continue
                cp = engine.clean_property_source(src)    # {source, property, on, relation, amountPerTurn}
                on, prop = cp.get("on", ""), cp.get("property")
                scope = GAMEOBJECT_SCOPE.get(on, on.replace("GAMEOBJECT_", "").lower())
                unit = SOURCE_UNIT.get(cp.get("source", ""), cp.get("source", "").lower())
                val = cp.get("amountPerTurn")
                # relation is containment (ASSOCIATED/SAME_PLOT) -> folded into scope; NEAR would be #429 leakage
                if prop and val not in (None, "", {}):
                    _put(fam, prop, scope, None, unit, None, val)   # family = the property type (split, no wrapper)
        elif tag == "Goodies":
            g = engine.generic(c)
            if g:
                identity["goodies"] = g
        else:
            if list(c) or t:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(c)

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "help"):
        if k in text_fields:
            out[k] = text_fields[k]
    for family in FAMILY_ORDER:                            # families at TOP LEVEL (no `modifiers` wrapper)
        if family in fam:
            out[family] = fam[family]
    for family in fam:                                     # any family not in the ordering (safety)
        if family not in out:
            out[family] = fam[family]
    if grants:
        out["grants"] = grants
    if identity:
        out["identity"] = identity
    return out, leftover


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    s = Store()
    table = s.table("HandicapInfo")
    results, all_leftover, families_seen = OrderedDict(), set(), set()
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec)
        results[typ] = obj
        all_leftover.update(leftover)
        families_seen.update(k for k in obj if k not in ("type", "description", "help", "grants", "identity"))
    print("HandicapInfo curated: %d" % len(results))
    print("  families: %s" % ", ".join(sorted(families_seen)))
    if all_leftover:
        print("  !! UNHANDLED tags routed to identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "handicaps")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d HandicapInfo JSON files under Assets/Data/handicaps" % len(results))


if __name__ == "__main__":
    main()
