#!/usr/bin/env python3
"""Curate Bonus to the top-down model (#428) — thin config over curate_common (the shared core).

A bonus (resource) is a pure SOURCE / CONDITIONER, positioned ABOVE plot/feature/improvement/building in the
containment spine (enabler.md: `… resource → bonus → plot → feature → improvement → building → unit`). It cascades
DOWN; it is NEVER a target ("the coal test": sources/enablers are never targets, modifier.md). So the bonus carries
ONLY:
  - its OWN amplification deposits:
      * `YieldChanges` -> `yield` (food/production/commerce) at **plot** scope — the on-map resource buffs the
        TILE it sits on, downward (owner 2026-06-15: "the actual map bonus buffs the plot downwards").
      * `iHealth`/`iHappiness` -> health/happiness at **empire** scope — the trade-connected resource benefits
        the player's cities (a connected bonus enables/amplifies empire-wide; modifier.md: trade-connected =
        player/empire scope).
  - `enables` — the buildings/units/routes that REQUIRE the bonus (PrereqAndBonus / PrereqOrBonuses / vicinity,
    store-indexed): the bonus is the CONDITIONER, so those targets surface ON it as the `enables` edge. The
    Culture chain (a Culture national wonder GRANTS a BONUS_* that gates the punk buildings) appears here.
  - `mapGeneration` / `identity` / `art` / text.

**NO inbound modifier boosts.** A building/civic/trait effect CONDITIONED on a resource ("+2 production while
this bonus is connected") is that SOURCE entity's own output, gated by the bonus via `enabled:{type:BONUS_X,
scope, min:1}` / `per:{type:BONUS_X}` — authored ON the building/unit/project/civic/trait at THAT entity's pass,
top-down only, NEVER inverted onto the bonus (modifier-spec §6 keep-on-source — supersedes the old pre-v3
"invert onto the conditioner" rule; conditioned-on-source RESOLVED). The prior `BONUS_BOOSTS` inversion table was that old
approach and is removed.

Tech edges are the tech's, dropped here: TechReveal/TechCityTrade -> tech.enables.bonuses (store PREREQ_FIELDS);
TechObsolete -> tech.obsoletes.bonuses (store OBSOLETE_FIELDS, extra_drop below).

  python3 curate_bonus.py --sample BONUS_COAL
  python3 curate_bonus.py --write
"""
import os

import engine
import curate_common as cc
from store import REPO


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

# Scope of the bonus's OWN deposits (verified families OVERRIDE the mapping's first-pass flat `city`):
#   YieldChanges -> plot (the map bonus buffs its tile); iHealth/iHappiness -> empire (connected-resource benefit).
BONUS_FAMILIES = {
    "YieldChanges": {"scope": "plot",   "channel": "yield",     "kind": "flat",
                     "valueKeys": ["food", "production", "commerce"]},
    "iHealth":      {"scope": "empire", "channel": "health",    "kind": "flat"},
    "iHappiness":   {"scope": "empire", "channel": "happiness", "kind": "flat"},
}

# TechObsolete is the obsoleting tech's edge (store OBSOLETE_FIELDS -> tech.obsoletes.bonuses), dropped here.
CFG = cc.EntityConfig("BonusInfo", extra_drop=["TechObsolete"], era_fn=bonus_folder, map_gen=BONUS_MAP_GEN,
                      families=BONUS_FAMILIES)

if __name__ == "__main__":
    # No inbound boosts — a resource is never a target (only enables/amplifies).
    cc.main(CFG, [], os.path.join(REPO, "Assets", "Data", "bonuses"))
