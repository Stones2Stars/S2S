#!/usr/bin/env python3
"""Curate Bonus to the top-down model (#428) — thin config over curate_common (the shared core).

Wave 1: Bonus's own Health/Happiness/YieldChanges validate the City-scope channel path, and the
Building/Unit/Project -> Bonus conditioner effects fold in as entity-targeted modifiers. `enables` =
the buildings/units that require the bonus (PrereqAndBonus / PrereqOrBonuses, indexed by the store);
the Culture chain (a Culture national wonder grants a BONUS_* that gates the punk buildings) shows up
here as those buildings appearing in the bonus's `enables`.

  python3 curate_bonus.py --sample BONUS_ATOMPUNK
  python3 curate_bonus.py --write
"""
import os

import engine
import curate_common as cc
from store import REPO

# Building/Unit/Project conditioner effects that invert ONTO the bonus (fold into the bonus's modifiers):
#   (sourceEntity, field, targetType, channel, valueKeys, unit, scope)
# Base yield/commerce SPLIT into per-identifier families (food/gold/…) by curate_common; vicinityYield stays a
# distinct grouped concept. BonusProductionModifiers -> `buildRate` family (renamed off "production" so it can't
# be read as the production YIELD). Scopes are first-pass (city) — to be confirmed by the verify pass.
BONUS_BOOSTS = [
    ("BuildingInfo", "BonusYieldChanges",           "buildings", "yield",         engine.YIELDS,    "flat",    "city"),
    ("BuildingInfo", "BonusYieldModifiers",         "buildings", "yield",         engine.YIELDS,    "percent", "city"),
    ("BuildingInfo", "BonusCommercePercentChanges", "buildings", "commerce",      engine.COMMERCES, "flat",    "city"),  # misnamed "Percent": a FLAT x100 commerce add (#432), not a modifier
    ("BuildingInfo", "BonusCommerceModifiers",      "buildings", "commerce",      engine.COMMERCES, "percent", "city"),  # the genuine percent commerce modifier (verify found it missed)
    ("BuildingInfo", "BonusHealthChanges",          "buildings", "health",        None,             "flat",    "city"),
    ("BuildingInfo", "BonusHappinessChanges",       "buildings", "happiness",     None,             "flat",    "city"),
    ("BuildingInfo", "VicinityBonusYieldChanges",   "buildings", "vicinityYield", engine.YIELDS,    "flat",    "city"),
    ("BuildingInfo", "BonusProductionModifiers",    "buildings", "buildRate",     None,             "percent", "city"),
    ("UnitInfo",     "BonusProductionModifiers",    "units",     "buildRate",     None,             "percent", "city"),
    ("ProjectInfo",  "BonusProductionModifiers",    "projects",  "buildRate",     None,             "percent", "city"),
    # Civic BonusCommerceModifiers: the bonus is the conditioner (have-bonus -> +commerce while the civic is
    # active), so it inverts ONTO the bonus keyed by the civic (dropped from CivicInfo). Same convention as the
    # building/unit/project bonus rows above; verified by the classify-civic double-author audit.
    ("CivicInfo",    "BonusCommerceModifiers",      "civics",    "commerce",      engine.COMMERCES, "percent", "city"),
    # Trait BonusHappinessChanges: the bonus is the conditioner (have-bonus -> +happiness while you hold the
    # trait), so it inverts ONTO the bonus keyed by the trait (dropped from TraitInfo). The only fresh trait
    # CREST; city scope matches the consumer (per-city via hasBonus, CvCity.cpp:4342-4357) and the
    # building/civic fold convention. Owner ruling 2026-06-14; classify-trait wf_cc8659b5.
    ("TraitInfo",    "BonusHappinessChanges",       "traits",    "happiness",     None,             "flat",    "city"),
]

def bonus_folder(rec, store):
    """Sub-folder by category. `cultures` isolates the removal-candidate culture bonuses; `map` = spawns
    on the map (has placement); `manufactured` = produced/refined, not placed."""
    typ = engine.text(rec.find("Type"))
    if typ in store.culture_bonuses():
        return "cultures"
    tiles, appear = engine.text(rec.find("iTilesPer")), engine.text(rec.find("iConstAppearance"))
    placed = (engine.is_int(tiles) and int(tiles) > 0) or (engine.is_int(appear) and int(appear) > 0)
    return "map" if placed else "manufactured"


# Map-generation / placement fields -> a `mapGeneration` group (out of the identity catch-all). Names are
# de-Hungarianized for now; some (placementOrder/constAppearance/rands/...) may get clearer names later.
BONUS_MAP_GEN = {
    "iPlacementOrder", "iConstAppearance", "Rands", "iTilesPer", "iMinAreaSize", "bFlatlands",
    "TerrainBooleans", "bHills", "iUniqueRange", "FeatureBooleans", "FeatureTerrainBooleans", "bArea",
    "iMaxLatitude", "bNormalize", "iGroupRange", "iGroupRand", "bPeaks", "iMinLatitude", "bNoRiverSide",
    "iMinLandPercent", "bBonusCoastalOnly",
}

# TechObsolete is the obsoleting tech's edge (store OBSOLETE_FIELDS -> tech.obsoletes.bonuses), dropped here.
CFG = cc.EntityConfig("BonusInfo", extra_drop=["TechObsolete"], era_fn=bonus_folder, map_gen=BONUS_MAP_GEN)

if __name__ == "__main__":
    cc.main(CFG, BONUS_BOOSTS, os.path.join(REPO, "Assets", "Data", "bonuses"))
