#!/usr/bin/env python3
"""Curate Handicap to the top-down model (#428) — a CONFIG entity (enables nothing): modifiers only.

⚠ FUTURE REWORK FLAG (owner, 2026-06-15): the HANDICAP SYSTEM'S STRUCTURE NEEDS HELP we cannot give it at this
juncture (out of scope for #428). Because of that, the documentation below is deliberately VERBOSE about WHAT
EACH FIELD CURRENTLY MEANS, so a later structural pass has the full picture. This migration only faithfully
carries the present meaning into the locked shape; it does NOT fix the awkward parts (the human/AI duality, the
own-vs-game-handicap sourcing, the meta `perEra` ramp, the provisional AI-economy family names). Treat the
current structure as a faithful snapshot to improve later, not as a settled design.

FLAT FAMILY structure: there is NO `modifiers` wrapper. Each modifier FAMILY — the *kind* of thing modified —
is its own top-level section, all sharing ONE uniform shape:  <family>.<scope>.<member>.<unit> = value
(member omitted for single-concept families). Values are PERCENTAGES (`percent`) or additive amounts (`flat`)
authored as what they ARE — never reshaped to the engine's combination math (owner rulings 2026-06-15).

THE HUMAN/AI DUALITY (the core awkwardness — read this before touching the data):
- A leaf's BARE unit (`percent`/`flat`) is the BASE value, applying to ALL players. An optional sibling `ai:`
  block (v3 audience qualifier) is the AI-ONLY value. Two cases occur:
    * DUAL field (e.g. unit upkeep): bare = base for everyone; `ai` = an EXTRA modifier stacked for AI players.
      At Deity an AI's unit upkeep is base 200% × the AI 50% modifier — AIs pay less, the human pays full.
    * AI-ONLY field (e.g. buildCost/techCost/growth/supply/upgrade): NO bare value, only `ai` — these knobs
      exist solely to discount the AI; the human has no equivalent handicap lever for them.
- SOURCING IS ENGINE FETCHING, NOT DATA (so it is NOT encoded here): a human reads the BASE off their OWN
  handicap; an AI reads the BASE off its own handicap AND the `ai` modifier off the derived GAME handicap
  (the integer average of human handicaps). The JSON just states each handicap's base + ai values per field;
  which record a given player reads is the reworked engine's job. Full read-site map: handicaps.md.

WHAT EACH FAMILY MEANS (current behaviour — the meanings that must survive the later rework):
- `maintenance.empire.{distance,numCities,colony,corporation}.percent` — % scale on each gold city-maintenance
  COMPONENT (mirrors CvCity::calculateBaseMaintenance). `colony.cap` = a hard CAP on the colony component
  (iMaxColonyMaintenance), NOT a percent — a clamp carried in the family structure (modifier-spec §7).
- `upkeep.empire.{unit,civic,inflation,supply,upgrade}.percent` — % scale on recurring gold upkeep costs.
  unit/civic/inflation are DUAL (base + ai); supply/upgrade are AI-ONLY.
- `happiness.empire.flat` / `health.empire.flat` — flat happy/health bonus in every city of the owner.
- `growth.empire.ai.percent` — AI city food-to-grow % (AI-only; lower = AI grows faster).
- `techCost.empire.ai.percent` — AI tech-research COST % (renamed off `research` so it can't read as the
  research commerce). AI-only.
- `workRate.empire.ai.percent` — AI worker build-rate %. AI-only.
- `buildCost.empire.{train,worldTrain,construct,worldConstruct,create,worldCreate}.ai.percent` — AI build-cost
  % per produced kind (unit / world-unit / building / world-building / wonder|project / world-wonder). AI-only.
- `perEra.empire.ai.percent` — META: a per-era ramp applied to the WHOLE AI-economy family (× current era). A
  modifier-of-modifiers — the least-natural field; flagged for the rework. AI-only.
- `revolution.empire.percent` — % into the Revolution index. INCOMPLETE mechanic (WIP, tracked), NOT dead — kept.
- `diplomacy.empire.attitude.flat` (AI attitude shift, via the TARGET's handicap); `diplomacy.empire.declareWar`
  / `warWeariness` (AI behaviour); `diplomacy.team.{noTechTrade,techTradeKnown}.percent` (tech-trade thresholds).
- `combat.world.{animal,barbarian,subdueAnimal}.percent` — wildlife/barbarian combat-odds modifiers (game-global,
  so `world` scope; vs-human is the base, vs-AI is the `ai` audience). `combat.empire.freeWinsVsBarbs.flat`
  (per-player free wins).
- `barbarians.world.{...}` — game-global barbarian/animal SPAWN rules (densities, timings, garrison, probs).
- `PROPERTY_*.<scope>.<unit>` — per-handicap property sources (crime/education), one family per property type.
  NB scalar shape here differs from the gated-LIST property shape other entities use → reconcile in the
  property/#429 pass (a flagged, deferred mismatch — not fixed now).
- `grants[.ai]` — one-shot GAME-START provisioning (starting gold + defense/worker/explore units); humans and
  AIs split entirely (own vs game handicap). NOT a per-turn modifier → its own section.
- `identity.advancedStart` — the pre-game POINTS BUDGET mod (buys techs/cities/units before turn 1). NOT a
  modifier, poorly supported in the DLL → parked in identity pending an advanced-start review.

Verified against Sources/Infos/CvHandicapInfo.h + docs/dev/reference/handicaps.md (read-sites, sourcing,
the maintenance computation, the no-dead-fields verdict). PROVISIONAL family names: growth/techCost/workRate/
buildCost/perEra (kept until the future rework). Manual renames logged in migration-renames.md.

  python3 curate_handicap.py --sample HANDICAP_CHIEFTAIN
  python3 curate_handicap.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import de_i, fold_text_to_identity
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
        elif tag == "PropertyManipulators":               # each PROPERTY_* is its OWN family (v3 — like any yield)
            for src in c:
                if src.tag != "PropertySource":
                    continue
                conv = engine.property_source_v3(src)     # CONSTANT->flat, DECAY->percent, attr-scaled->per, active->enabled
                if conv is None:
                    continue
                prop, scope, unit, value = conv
                _put(fam, prop, scope, None, unit, None, value)   # family = the property type (split, no wrapper)
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
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
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
