#!/usr/bin/env python3
"""Curate Era to the top-down model (#428) — a CONFIG/GLOBAL entity (enables NOTHING). World-scope modifiers
+ one-shot grants + pacing identity + audio art. Classification verified by the classify-era-fields workflow
(5-agent understand + adversarial verify) against Sources/Infos/CvEraInfo.{h,cpp} and the live consumers.

Era and GameSpeed feed the SAME world-scope families. Notable rulings (owner 2026-06-14):
- `costs` members are FINE-GRAINED per produced-thing/cost-type: train (unit hammer cost, keyed off the
  produced unit's era) / construct (building, owner era) / create (project, prereq era) / research (tech) /
  build (worker build-time) / improvementUpgrade (improvement upgrade-time). Each has its OWN era-resolution
  rule (recorded here for the consumption rewrite); they are NOT interchangeable.
- iCuttingEdgeCutsTechCostModifier -> costs.world.researchCutBelowEra: a SUMMED-across-era-bands tech-cost CUT
  applied at the additive-mod stage (CvTeam:2627/2648), NOT the research base multiplier — kept a distinct
  member so the reader does not fold it into the research base.
- food-to-grow is the `growth` family (NOT costs.food) — consistent with GameSpeed + Handicap.
- iEventChancePerTurn -> eventChance.world.flat: carried FAITHFULLY as-is. The random-events system is
  fully calculated in PYTHON — a "gremlin we don't touch quite yet" (owner 2026-06-15). Event handling NEEDS
  fixing eventually, but the approach is not yet known (owner) — so leave the value/unit alone (do NOT
  re-classify flat->percent) until the events system is tackled on its own.
- pacing inputs (historicalStartYear/EndYear/normalSpeedTurns) are STORED identity; the turn/calendar are
  DERIVED downstream on GameSpeed/CvDate (getTurnsInEra = normalSpeedTurns*speed/100), so not modifiers here.
- advancedStart parked in identity (mirrors Handicap; pre-game points budget, not a modifier — pending review).
- DROP bNoAnimals — DEAD as an era field (owner 2026-06-15): it is being RELOCATED to a game-option / BUG-option
  (the "disable animals" feature), tracked by an existing GitHub issue. LIKELY ORIGINAL INTENT (owner, plausible):
  era-gated — past some advanced/space era, wildlife stops spawning (no animals in the cosmos). But it was
  DESIGNED-NOT-WIRED: the field, schema, and a pedia display (Python PediaEra.py:147 isNoAnimals) exist, yet NO
  era sets bNoAnimals=true, so the era-gating never actually fired — hence dead-as-an-era-field and a clean
  candidate to move to a global option. The pedia line goes away with the relocation. Not a #428 concern beyond
  dropping it.
- The barbarian WORLD-STATE gates (bNoGoodies/bNoBarbUnits/bNoBarbCities) are LIVE C++ rules — goody placement
  (CvMapGenerator.cpp:772), barb-unit spawn (CvGame.cpp:7180), barb-city spawn (CvGame.cpp:6941) — so NOT dead,
  but 0/absent in every current era -> not emitted (zero-drop). These are world-RULE GATES (NOT identity, NOT a
  modifier family). NB they have NO home yet: the Vote pass was expected to resolve this but did NOT — Vote's
  bools turned out to be a DIFFERENT mechanic (on-pass vote OUTCOMES handled by processVote, not world-rule
  gates; 2026-06-15). So a world-state-gate concept would need SEPARATE planning if wanted (owner stance, cf.
  the "no cascading config section" ruling) — moot today since no era sets one. If an era ever sets one it
  surfaces in the curator's leftover report rather than silently mis-routing.
  iInitialCityMaintenancePercent is likewise 0 in every era (revisit if set).

  python3 curate_era.py --sample ERA_ANCIENT
  python3 curate_era.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import curate_common as cc
from curate_common import de_i
from store import Store, REPO

# tag -> (family, scope, member, unit). member None = singleton family.
FAMILIES = {
    "iTrainPercent":                    ("costs", "world", "train", "percent"),
    "iConstructPercent":                ("costs", "world", "construct", "percent"),
    "iCreatePercent":                   ("costs", "world", "create", "percent"),
    "iResearchPercent":                 ("costs", "world", "research", "percent"),
    "iBuildPercent":                    ("costs", "world", "build", "percent"),
    "iImprovementPercent":              ("costs", "world", "improvementUpgrade", "percent"),
    "iCuttingEdgeCutsTechCostModifier": ("costs", "world", "researchCutBelowEra", "percent"),
    "iGrowthPercent":                   ("growth", "world", None, "percent"),
    # iAnarchyPercent scales ANARCHY LENGTH and is read at BOTH getCivicAnarchyLength and
    # getReligionAnarchyLength (CvPlayer), so it emits to BOTH anarchy kinds. It is NOT `anger`:
    # traits author `anger` alongside `civicAnarchy`/`religiousAnarchy`, which is what proves the three
    # are distinct kinds -- routing the era scaler to `anger` left both anarchy kinds with no era
    # contribution at all while depositing into a channel no anarchy read consumes.
    "iAnarchyPercent":                  [("durations", "world", "civicAnarchy", "percent"),
                                         ("durations", "world", "religiousAnarchy", "percent")],
    "iGreatPeoplePercent":              ("greatPeopleRate", "world", None, "percent"),
    "iEventChancePerTurn":              ("eventChance", "world", None, "flat"),
    "iInitialCityMaintenancePercent":   ("maintenance", "city", "initial", "flat"),  # x100 fixed-point; 0 in all eras
}
GRANTS = {
    "iStartingGold": "startingGold", "iStartingUnitMultiplier": "startingUnitMultiplier",
    "iStartingDefenseUnits": "startingDefenseUnits", "iStartingWorkerUnits": "startingWorkerUnits",
    "iStartingExploreUnits": "startingExploreUnits", "iFreePopulation": "freePopulation",
}
TEXT = {"Description": "description", "Strategy": "strategy"}
# art/audio tags -> ui/world/sound via the canonical curate_common.ART_BLOCK (incl. bFirstSoundtrackFirst ->
# sound.introSoundtrack, the b-flag making the era lead with its first soundtrack on entry).
ART = {"Button", "AudioUnitVictoryScript", "AudioUnitDefeatScript", "EraInfoSoundtracks",
       "CitySoundscapes", "iSoundtrackSpace", "bFirstSoundtrackFirst"}
IDENTITY = {"iHistoricalStartYear": "historicalStartYear", "iHistoricalEndYear": "historicalEndYear",
            "iNormalSpeedTurns": "normalSpeedTurns", "iAdvancedStartPoints": "advancedStart"}
# barbarian/goody WORLD-STATE gates -> a bespoke `worldGen` block: LIVE C++ world-RULE gates (goody/barb
# placement), NOT identity/modifiers. 0/false in every era -> not emitted (zero-drop), so the ERA GATE on each
# rule never fired and its readers are gone -- dead-as-an-era-field, the bNoAnimals disposition. The C++ rules
# themselves are untouched. The mapping stays so future data has a landing place, but nothing reads it: a
# world-state-gate CONCEPT needs its own design before that block means anything.
WORLDGEN = {"bNoGoodies": "noGoodies", "bNoBarbUnits": "noBarbUnits", "bNoBarbCities": "noBarbCities"}
DROP = {"bNoAnimals"}
FAMILY_ORDER = ["costs", "growth", "greatPeopleRate", "durations", "eventChance", "maintenance"]


def _put(fam, family, scope, member, unit, val):
    node = fam.setdefault(family, {}).setdefault(scope, {})
    if member:
        node = node.setdefault(member, {})
    node[unit] = val


def curate(typ, rec, order):
    text, fam, grants, art_blocks, identity, world_gen, leftover = {}, {}, {}, {}, {}, {}, []
    identity["order"] = order   # the era's sequence index (engine enum / XML order) — eras are ORDERED data, defined
    #                              in the JSON so consumers resolve era-thresholds (byEra cumulative) from curated data, not /state.
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in FAMILIES:
            if engine.is_int(t) and int(t) != 0:          # 0 = additive identity / unset -> drop
                spec = FAMILIES[tag]
                for family, scope, member, unit in (spec if isinstance(spec, list) else [spec]):
                    _put(fam, family, scope, member, unit, int(t))
        elif tag in GRANTS:
            if engine.is_int(t) and int(t) != 0:
                grants[GRANTS[tag]] = int(t)
        elif tag in ART:
            if tag[:1] == "b":                            # boolean audio flag (-> sound.introSoundtrack)
                if t in ("1", "true", "True"):
                    cc.put_art(art_blocks, tag, True)
            else:
                cc.put_art(art_blocks, tag, engine.generic(c))   # -> ui/world/sound via ART_BLOCK
        elif tag in IDENTITY:
            if t or list(c):
                identity[IDENTITY[tag]] = engine.generic(c)
        elif tag in WORLDGEN:
            if t in ("1", "true", "True"):                 # world-RULE gate ON -> worldGen (0/false today -> zero-drop)
                world_gen[WORLDGEN[tag]] = True
        else:
            if list(c) or t:                              # boolean world-gates land here if ever set (default false -> skip)
                if tag[:1] == "b":
                    if t in ("1", "true", "True"):
                        leftover.append(tag)
                        identity[de_i(tag)] = True
                else:
                    leftover.append(tag)
                    identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(c)

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "strategy"):
        if k in text:
            out[k] = text[k]
    for family in FAMILY_ORDER:
        if family in fam:
            out[family] = fam[family]
    for family in fam:
        if family not in out:
            out[family] = fam[family]
    if grants:
        out["grants"] = grants
    if world_gen:
        out["worldGen"] = world_gen
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
    table = Store().table("EraInfo")
    results, all_leftover = OrderedDict(), set()
    for order, (typ, rec) in enumerate(table.items(), start=1):   # 1-based: era is a counter 1..X (owner ruling 2026-06-28)
        obj, leftover = curate(typ, rec, order)
        results[typ] = obj
        all_leftover.update(leftover)
    print("EraInfo curated: %d" % len(results))
    if all_leftover:
        print("  leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "eras")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d EraInfo JSON files under Assets/Data/eras" % len(results))


if __name__ == "__main__":
    main()
