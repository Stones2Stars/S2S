#!/usr/bin/env python3
"""Curate Terrain (#428, Tier C #20) — a plot-leaf TARGET (enables nothing; store.enabled_by(TERRAIN_*) empty).

A terrain CARRIES ITS OWN modifiers onto the plot it forms (owner 2026-06-16: a hill delivers hammers; a
feature then modifies the plot on top). EVERY effect is a TERRAIN-OWN deposit at PLOT scope; NO inbound boost
folds onto the terrain. Ownership rule (owner 2026-06-16, the "deliveryguy"): the entity that BRINGS a modifier
to the table owns it — a Building/Civic TerrainYieldChanges stays on the building/civic (it delivers the buff,
conditioned), an Improvement/Feature/Route on itself. The terrain only owns what it intrinsically delivers.
Per-field dispositions: classifications/terrain-classification.json (adversarially verified).

Modeling calls (verified vs CvTerrainInfo + CvPlot::calculateYield/movementCost/getDefenseModifier + CvCity):
- Yields            -> SPLIT base-yield families food/production/commerce, PLOT scope (CvPlot.cpp:8077, summed
                       into the tile's base yield). The grassland-food / hill-hammers base.
- iMovement         -> `movement` family, PLOT scope (owner: "movement is a family"; terrain seeds the per-plot
                       move cost, features/hills add on top — CvPlot.cpp:4555).
- iDefense          -> `defense.plot.amount.percent` (owner: a family, "improvable by a fort"; terrain seeds the
                       per-plot defense, feature/hills/peak add — CvPlot.cpp:4400; same `amount` member as
                       CultureLevel's defense.city.amount).
- iCultureDistance  -> NEW `cultureDistance` family, PLOT/flat (owner: a summable family for the REALISTIC_CULTURE
                       option; the base for the later culture-equilibrium structure; CvCity.cpp:6302+ accumulates
                       getCultureDistance() across worked plots into a city total).
- iBuildModifier    -> NEW `buildTime` family, PLOT/percent (owner: ties to workRate/work-capacity — the
                       buildTime<->workRate unification is later structural work; CvPlot.cpp:3607 multiplies the
                       build time of work done on this terrain).
- iHealthPercent    -> DROP (dead on terrain: every getHealthPercent reader is Feature/Specialist/Improvement/
                       Building, none against a terrain; pre-existing note modifier-cascade-mapping.json:2667). cat i.
- ClimateZoneType / iDistanceToLand / bFound / bFoundCoast / bFoundFreshWater / bFreshWaterTerrain /
  MapCategoryTypes -> identity (world-gen classification, city-found gates, fresh-water, placement categories;
                       read directly, never summed).
- ArtDefineTag -> art.artDefineTag (the ON-MAP terrain graphics FK -> CIV4ArtDefines_Terrain.xml, a separate art
  tier); Button -> art.icon (the UI icon); FootstepSounds / WorldSoundscapeAudioScript -> art (audio).
- Categories / PropertyManipulators / bImpassable -> NOT authored on ANY terrain (0/42) -> never emitted (the
  consumers exist but no terrain populates them; Categories is also dead, no live reader).

EXE-link: 0 DllExport that constrains data on CvTerrainInfo (getArtDefineTag is the lone DllExport, an art FK).

  python3 curate_terrain.py --sample TERRAIN_GRASSLAND
  python3 curate_terrain.py --write
"""
import os

import engine
import curate_common as cc
from store import REPO

# Terrain's OWN modifier families (override the empty mapping channels). Every deposit is PLOT scope — the
# terrain forms the plot's base, which features/improvements/routes then modify on top.
TERRAIN_FAMILIES = {
    "Yields":           {"channel": "yield",           "scope": "plot", "kind": "flat", "valueKeys": engine.YIELDS},
    "iMovement":        {"channel": "movement",        "scope": "plot", "kind": "flat"},
    "iDefense":         {"channel": "defense",         "scope": "plot", "kind": "percent", "member": "amount"},
    "iCultureDistance": {"channel": "cultureDistance", "scope": "plot", "kind": "flat"},
    "iBuildModifier":   {"channel": "buildTime",       "scope": "plot", "kind": "percent"},
}

# Keep ArtDefineTag (the ON-MAP terrain graphics FK) DISTINCT from the UI icon (Button): terrain carries BOTH,
# and the global ArtDefineTag->icon collapse would otherwise overwrite Button->icon (silent data loss).
TERRAIN_ART_RENAME = {"ArtDefineTag": "artDefineTag"}

# Clearer identity key for the placement-category vector (MAPCATEGORY_* membership).
TERRAIN_ID_RENAME = {"MapCategoryTypes": "mapCategories"}

CFG = cc.EntityConfig("TerrainInfo", extra_drop=["iHealthPercent"],
                      families=TERRAIN_FAMILIES, id_rename=TERRAIN_ID_RENAME,
                      art_rename=TERRAIN_ART_RENAME)

# NO inbound boosts: a terrain is never the deliveryguy for another entity's modifier (owner 2026-06-16).
TERRAIN_BOOSTS = []

if __name__ == "__main__":
    cc.main(CFG, TERRAIN_BOOSTS, os.path.join(REPO, "Assets", "Data", "terrains"))
