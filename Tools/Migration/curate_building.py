#!/usr/bin/env python3
"""Curate Building (#428, Tier E #32) + SpecialBuilding (#31, rides this pass) — THE deepest modifier surface.

Curated from `classifications/building-classification.json` (the adversarial classify-building workflow, 288 fields)
+ the owner rulings (handover-2026-06-16-6). Building is the most-targeted entity; the SOURCE->building enabler edges
(tech/bonus/civic/religion/corp/cultureLevel `enables`, ObsoleteTech, ReplacementBuildings) are ALREADY store-wired,
so on the building side they DROP — the building authors only its OWN: ~70 modifier families, a `requires` MEANS gate,
`grants` (incl. the NEW `grants.repeatable`), cost, properties, identity. EXE-link: 0 DllExport -> UNCONSTRAINED.

OWNER RULINGS folded in (handover #6):
- §6.1 DELIVERYGUY: the 22 "inversions" KEEP-ON-BUILDING keyed by target (NOT inverted). Tech/Bonus/Building gated via
  `enabled`; Improvement/Terrain/Plot yields target-keyed (food.city.improvements.{IMP}.flat). Tech ones PROVISIONAL (Phase F).
- `grants.repeatable[]` + `interval` (modifier-spec §4.1): PropertySpawn (chance via `per`) + iNumUnitFullHeal/HealUnitCombat.
- shrine (GlobalReligionCommerce) -> `per`-scaled commerce MODIFIER (per city holding the religion), not a block.
- CommerceChangeDoubleTimes -> 2nd age-gated deposit `enabled:{existedFor:{min:N}}`.
- cityCapture = its OWN family (capturing CITIES, distinct from the §5 unit `capture` gradient).
- capability bools -> identity (revisit Phase F); EXCEPTIONS to enables-family: bForceTeamVoteEligible->enables.votes,
  Hurrys->enables.hurries, FoundsCorporation->enables.corporations.
- DROP (dead §8-i): iMaxPopulationAllowed, iMaxPopulationChange, iDCMNukesOkay/bDCMNukesOkay.
- iNukeExplosionRand: NOT emitted (live code, but only the EXCLUDED Bad_Karma/Building_Meltdown module populates it).

⚠ THIS IS PASS 1 (the bulk): scalar/percent families + yield/commerce + requires + store edges + cost + caps + art/ai +
era foldering + the COVERAGE CHECK. The keyed inversions, CvProperties, repeatable grants, building-on-building, and the
one-shot grants/pulses land in PASS 2 (they show as UNHANDLED in the coverage report meanwhile).

  python3 curate_building.py --sample BUILDING_FORGE BUILDING_PALACE BUILDING_WONDER_PYRAMIDS
  python3 curate_building.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import boolexpr
from curate_common import put_art, emit_art, FAMILY_ORDER, de_i
from store import Store, REPO

# ---- scalar/percent modifier families: tag -> (family, scope, member|None, unit). Corrected scopes from the
# classification (the mapping's were often wrong). Names PROVISIONAL (reader-pass refines). ----
SCALAR_FAMILIES = {
    # health / happiness / healing
    "iHealth": ("health", "city", None, "flat"),
    "iAreaHealth": ("health", "area", None, "flat"),
    "iGlobalHealth": ("health", "empire", None, "flat"),
    "iHealthPercentPerPopulation": ("health", "city", "perPopulation", "percent"),
    "iHappiness": ("happiness", "city", None, "flat"),
    "iAreaHappiness": ("happiness", "area", None, "flat"),
    "iGlobalHappiness": ("happiness", "empire", None, "flat"),
    "iHappinessPercentPerPopulation": ("happiness", "city", "perPopulation", "percent"),
    "iHealRateChange": ("healing", "city", None, "flat"),
    "iFoodKept": ("foodKept", "city", None, "percent"),
    # great people / great general
    "iGreatPeopleRateChange": ("greatPeopleRate", "city", None, "flat"),
    "iGreatPeopleRateModifier": ("greatPeopleRate", "city", None, "percent"),
    "iGlobalGreatPeopleRateModifier": ("greatPeopleRate", "empire", None, "percent"),
    "iGreatGeneralRateModifier": ("greatGeneralRate", "city", None, "percent"),
    "iDomesticGreatGeneralRateModifier": ("greatGeneralRate", "city", "domestic", "percent"),
    # maintenance (grouped, cost-style)
    "iMaintenanceModifier": ("maintenance", "city", None, "percent"),
    "iGlobalMaintenanceModifier": ("maintenance", "empire", None, "percent"),
    "iAreaMaintenanceModifier": ("maintenance", "area", None, "percent"),
    "iOtherAreaMaintenanceModifier": ("maintenance", "area", "otherArea", "percent"),
    "iDistanceMaintenanceModifier": ("maintenance", "empire", "distance", "percent"),
    "iNumCitiesMaintenanceModifier": ("maintenance", "empire", "numCities", "percent"),
    "iCoastalDistanceMaintenanceModifier": ("maintenance", "empire", "coastalDistance", "percent"),
    "iConnectedCityMaintenanceModifier": ("maintenance", "empire", "connectedCity", "percent"),
    "iInflationModifier": ("inflation", "empire", None, "percent"),
    # war weariness
    "iWarWearinessModifier": ("warWeariness", "city", None, "percent"),
    "iGlobalWarWearinessModifier": ("warWeariness", "empire", None, "percent"),
    "iEnemyWarWearinessModifier": ("warWeariness", "city", "enemy", "percent"),
    # hurry
    "iHurryCostModifier": ("hurryCost", "city", None, "percent"),
    "iGlobalHurryModifier": ("hurryCost", "empire", None, "percent"),
    "iHurryAngerModifier": ("hurryAnger", "city", None, "percent"),
    # production specials
    "iMilitaryProductionModifier": ("buildRate", "city", "military", "percent"),
    "iSpaceProductionModifier": ("buildRate", "city", "space", "percent"),
    "iGlobalSpaceProductionModifier": ("buildRate", "empire", "space", "percent"),
    "iWorkerSpeedModifier": ("workRate", "empire", None, "percent"),
    # trade routes
    "iTradeRoutes": ("tradeRoutes", "city", None, "flat"),
    "iCoastalTradeRoutes": ("tradeRoutes", "empire", "coastal", "flat"),
    "iGlobalTradeRoutes": ("tradeRoutes", "empire", None, "flat"),
    "iWorldTradeRoutes": ("tradeRoutes", "world", None, "flat"),
    "iTradeRouteModifier": ("tradeRoutes", "city", "modifier", "percent"),
    "iForeignTradeRouteModifier": ("tradeRoutes", "city", "foreignModifier", "percent"),
    # experience / free specialists
    "iExperience": ("experience", "city", None, "flat"),
    "iGlobalExperience": ("experience", "empire", None, "flat"),
    "iFreeSpecialist": ("freeSpecialists", "city", None, "flat"),
    "iAreaFreeSpecialist": ("freeSpecialists", "area", None, "flat"),
    "iGlobalFreeSpecialist": ("freeSpecialists", "empire", None, "flat"),
    # misc city
    "iAnarchyModifier": ("anarchy", "city", None, "percent"),
    "iGoldenAgeModifier": ("goldenAge", "empire", None, "percent"),
    "iOccupationTimeModifier": ("occupationTime", "city", None, "percent"),
    "iPopulationgrowthratepercentage": ("populationGrowthRate", "city", None, "percent"),
    "iGlobalPopulationgrowthratepercentage": ("populationGrowthRate", "empire", None, "percent"),
    # revolution
    "iRevIdxLocal": ("revolution", "city", None, "flat"),
    "iRevIdxNational": ("revolution", "empire", None, "flat"),
    "iRevIdxDistanceModifier": ("revolution", "city", "distanceModifier", "percent"),
    # pillage gold (REVIVE, §8-ii)
    "iPillageGoldModifier": ("pillageGold", "empire", None, "percent"),
    # espionage stats (insidiousness/investigation)
    "iInsidiousness": ("espionage", "city", "insidiousness", "flat"),
    "iInvestigation": ("espionage", "city", "investigation", "flat"),
    "iEspionageDefense": ("espionageDefense", "city", None, "flat"),
    # cityCapture (NEW family — capturing CITIES, distinct from §5 unit capture)
    "iNationalCaptureProbabilityModifier": ("cityCapture", "empire", "probability", "percent"),
    "iNationalCaptureResistanceModifier": ("cityCapture", "empire", "resistance", "percent"),
    "iLocalCaptureProbabilityModifier": ("cityCapture", "city", "probability", "percent"),
    "iLocalCaptureResistanceModifier": ("cityCapture", "city", "resistance", "percent"),
    "iUnitUpgradePriceModifier": ("unitUpgradePrice", "empire", None, "percent"),
    # defense family (grouped; `min` floor clamp lives here, modifier-spec §7)
    "iDefense": ("defense", "city", "amount", "percent"),
    "iBombardDefense": ("defense", "city", "bombardDefense", "percent"),
    "iAllCityDefense": ("defense", "empire", "amount", "percent"),
    "iNukeModifier": ("defense", "city", "nukeDefense", "percent"),
    "iAirModifier": ("defense", "city", "airDefense", "percent"),
    "iMinDefense": ("defense", "city", "min", "flat"),
    "iNoEntryDefenseLevel": ("defense", "city", "noEntryLevel", "flat"),
    "iLocalDynamicDefense": ("defense", "city", "dynamicDefense", "flat"),
    "iRiverDefensePenalty": ("defense", "city", "riverDefensePenalty", "flat"),
    "iBuildingDefenseRecoverySpeedModifier": ("defense", "city", "buildingDefenseRecovery", "percent"),
    "iCityDefenseRecoverySpeedModifier": ("defense", "city", "cityDefenseRecovery", "percent"),
    "iDamageAttackerChance": ("defense", "city", "damageAttackerChance", "flat"),
    "iDamageToAttacker": ("defense", "city", "damageToAttacker", "flat"),
    "iAdjacentDamagePercent": ("defense", "city", "adjacentDamage", "percent"),
    "iLocalRepel": ("defense", "city", "repel", "flat"),
}
# scope-wide yield/commerce families: tag -> (scope, keys, kind). SPLIT into per-identifier families (food/gold/…).
YIELD_FAMILIES = {
    "YieldChanges": ("city", engine.YIELDS, "flat"),
    "YieldModifiers": ("city", engine.YIELDS, "percent"),
    "YieldPerPopChanges": ("city", engine.YIELDS, "perPopulation"),
    "AreaYieldModifiers": ("area", engine.YIELDS, "percent"),
    "GlobalYieldModifiers": ("empire", engine.YIELDS, "percent"),
    "GlobalSeaPlotYieldChanges": ("empire", engine.YIELDS, "flat"),  # IS_WATER-gated (post_process pass 2)
    "CommerceChanges": ("city", engine.COMMERCES, "flat"),
    "CommerceModifiers": ("city", engine.COMMERCES, "percent"),
    "CommercePerPopChanges": ("city", engine.COMMERCES, "perPopulation"),
    "GlobalCommerceModifiers": ("empire", engine.COMMERCES, "percent"),
    "SpecialistExtraCommerces": ("empire", engine.COMMERCES, "flat"),  # empire bonus on all specialists' commerce
}

# capability bools -> identity (owner: revisit Phase F). Plain b-flag -> clean name: true (false omitted).
CAP_IDENTITY = {
    "bNukeImmune": "nukeImmune", "bNeverCapture": "neverCapture", "bZoneOfControl": "zoneOfControl",
    "bProtectedCulture": "protectedCulture", "bBorderObstacle": "borderObstacle", "bNoUnhappiness": "noUnhappiness",
    "bNoUnhealthyPopulation": "noUnhealthyPopulation", "bBuildingOnlyHealthy": "buildingOnlyHealthy",
    "bForceAllTradeRoutes": "forceAllTradeRoutes", "bNoEnemyPillagingIncome": "noEnemyPillagingIncome",
    "bQuarantine": "quarantine", "bMapCentering": "mapCentering", "bCenterInCity": "centerInCity",
    "bTeamShare": "teamShare", "bOrbital": "orbital", "bOrbitalInfrastructure": "orbitalInfrastructure",
    "bGovernmentCenter": "governmentCenter", "bCapital": "capital", "bAllowsNukes": "allowsNukes",
    "bProvidesFreshWater": "providesFreshWater", "bNoHolyCity": "noHolyCity", "bAutoBuild": "autoBuild",
    "bNoLimit": "noInstanceLimit", "bForceNoPrereqScaling": "forceNoPrereqScaling",
    "bApplyFreePromotionOnMove": "applyFreePromotionOnMove",
    "bDamageAllAttackers": "damageAllAttackers",  # derives m_bDamageAttackerCapable (recompute-on-load)
}
# identity scalars: tag -> key (non-zero int OR non-empty string).
ID_SCALAR = {
    "iAsset": "worth", "iPower": "militaryWorth", "iConquestProb": "conquestProbability",
    "iAirlift": "airlift", "iAirUnitCapacity": "airUnitCapacity", "iLineOfSight": "sightRange",
    "iWorkableRadius": "workableRadius", "iNumPopulationEmployed": "populationEmployed",
    "iExtraPlayerInstances": "maxPlayerInstancesExtra", "iDCMAirbombMission": "dcmAirbombMission",
    "ExtendsBuilding": "extends", "ProductionContinueBuilding": "productionContinue",
    "GreatPeopleUnitType": "greatPeopleUnitType", "DiploVoteType": "diploVoteType",
    "SpecialBuildingType": "specialBuildingType",  # FK to the per-player-capped SpecialBuilding GROUP (#31)
    "ReligionType": "religion", "Advisor": "advisor", "fVisibilityPriority": "visibilityPriority",
    "FreeStartEra": "freeStartEra", "MaxStartEra": "maxStartEra", "PromotionLineType": "promotionLineType",
    "iLinePriority": "linePriority",
}
ID_LIST = {"MapCategoryTypes": "mapCategories", "UnitCombatRetrainTypes": "unitCombatRetrainTypes",
           "Categories": "categories", "VictoryThresholds": "victoryThresholds"}
# cost (intrinsic base + the C2C real-cost scaling members). tag -> key.
COST = {"iCost": "production", "iCostSizeModifier": "sizeModifier", "iCostCountModifier": "countModifier",
        "iCostMaterialsModifier": "materialsModifier", "iCostComplexityModifier": "complexityModifier"}

# ---- DROP / DEFER tables ----
# DEAD (§8-i, confirmed zero consumers) + meltdown (excluded-module-only data, not emitted).
DROP_DEAD = {"iMaxPopulationAllowed", "iMaxPopulationChange", "iDCMNukesOkay", "bDCMNukesOkay", "iNukeExplosionRand"}
# Module-LOADER directives (not gameplay): bForceOverwrite is a modular-merge control, never a building property.
DROP_MODULE = {"bForceOverwrite"}
# Handled by requires_fn (read off rec). Most are in the mapping prereqs; listed here for the coverage check.
REQUIRES_TAGS = {
    "PrereqTech", "TechTypes", "Bonus", "PrereqBonuses", "VicinityBonus", "RawVicinityBonus",
    "PrereqVicinityBonuses", "PrereqRawVicinityBonuses", "PrereqReligion", "PrereqCorporation", "PrereqCultureLevel",
    "PrereqCivic", "PrereqAndCivics", "PrereqOrCivics", "bRequiresActiveCivics", "StateReligion",
    "bNeedStateReligionInCity", "bWater", "bRiver", "bFreshWater", "bPower", "bPrereqPower", "PowerBonus",
    "iMinAreaSize", "iMinLatitude", "iMaxLatitude", "iPrereqPopulation", "bPrereqWar", "ConstructCondition",
    "PrereqInCityBuildings", "PrereqNotInCityBuildings", "PrereqOrBuildings", "PrereqAmountBuildings",
    "PrereqAnyoneBuilding", "PrereqOrTerrain", "PrereqAndTerrain", "PrereqOrFeature", "PrereqOrImprovement",
    "PrereqOrHeritage", "VictoryPrereq", "iCitiesPrereq", "iTeamsPrereq", "iLevelPrereq",
    "iMaxGlobalInstances", "iMaxTeamInstances", "iMaxPlayerInstances", "EnabledCivilizationTypes",
    "PrereqGameOption", "NotGameOption",
}
# store-handled enabler edges (DROP building-side: they invert onto the SOURCE) + obsolete/replace.
STORE_TAGS = {"ObsoleteTech", "ObsoletesToBuilding", "ReplacementBuildings", "FreeBuilding", "FreeAreaBuilding"}
# PASS-2 tags (keyed inversions / properties / repeatable grants / building-on-building / one-shot grants/pulses).
# Listed so the coverage check distinguishes "deferred to pass 2" from "genuinely unhandled".
PASS2_TAGS = {
    # keyed inversions (keep-on-building):
    "TechYieldChanges", "TechYieldModifiers", "TechCommerceChanges", "TechCommerceModifiers", "TechHappinessChanges",
    "TechHealthChanges", "TechSpecialistChanges", "BonusHealthChanges", "BonusHappinessChanges", "BonusYieldChanges",
    "BonusYieldModifiers", "BonusCommercePercentChanges", "VicinityBonusYieldChanges", "BonusProductionModifiers",
    "ImprovementYieldChanges", "GlobalImprovementYieldChanges", "TerrainYieldChanges", "ReligionChanges",
    "PlotYieldChanges", "BonusDefenseChanges", "RiverPlotYieldChanges", "PowerYieldModifiers",
    # building-on-building:
    "BuildingHappinessChanges", "BuildingProductionModifiers", "GlobalBuildingProductionModifiers",
    "GlobalBuildingCostModifiers", "GlobalBuildingExtraCommerces",
    # specialist-keyed:
    "SpecialistYieldChanges", "SpecialistCommerceChanges", "LocalSpecialistCommerceChanges",
    # properties:
    "Properties", "PropertiesAllCities", "PropertyManipulators",
    # repeatable grants:
    "PropertySpawnUnit", "PropertySpawnProperty", "iNumUnitFullHeal", "HealUnitCombatTypes",
    # one-shot grants / pulses:
    "ExtraFreeBonuses", "FreeTraitTypes", "FreeSpecialTech", "iFreeTechs", "NewCityFree", "HolyCity",
    "iGlobalPopulationChange", "iPopulationChange", "bGoldenAge", "FreeSpecialistCounts", "SpecialistCounts",
    # enables-family from XML:
    "FoundsCorporation", "Hurrys", "bForceTeamVoteEligible",
    # conditional / temporal:
    "iStateReligionHappiness", "CommerceChangeDoubleTimes",
    # aid (creative, pass 2):
    "BonusAidModifiers", "AidRateChanges", "DomainFreeExperiences", "UnitCombatFreeExperiences",
    "UnitCombatExtraStrengths", "UnitProductionModifiers", "UnitCombatProdModifiers", "DomainProductionModifiers",
    "UnitCombatDefenseAgainstModifiers", "MayDamageAttackingUnitCombatTypes", "FreePromoTypes",
    "CommerceHappinesses", "CommerceFlexibles", "GlobalReligionCommerce",
    "StateReligionCommerces", "ImprovementFreeSpecialists", "GlobalCorporationCommerce", "PropertySource",
    "ObsoletesToBuilding",  # building->building obsolescence (own obsoletes edge; wire in pass 2)
}
# text + always-drop.
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Help": "help"}
ART = ("ArtDefineTag", "Button", "MovieDefineTag", "ConstructSound")


# ---- PASS 2 tables (keep-on-building, §6.1) ----
# CONDITION-gated deposits (the keyed entity is a CONDITIONER you possess): tag -> (family|None, scope, valuekeys|None, unit, ref_kind).
# family None + valuekeys => SPLIT (the yield/commerce member IS the family). ref_kind builds the `enabled` atom.
COND_KEYED = {
    "TechYieldChanges":            (None, "city", engine.YIELDS, "flat", "tech"),
    "TechYieldModifiers":          (None, "city", engine.YIELDS, "percent", "tech"),
    "TechCommerceChanges":         (None, "city", engine.COMMERCES, "percent", "tech"),
    "TechCommerceModifiers":       (None, "city", engine.COMMERCES, "percent", "tech"),
    "TechHappinessChanges":        ("happiness", "city", None, "flat", "tech"),
    "TechHealthChanges":           ("health", "city", None, "flat", "tech"),
    "BonusHealthChanges":          ("health", "city", None, "flat", "bonus"),
    "BonusHappinessChanges":       ("happiness", "city", None, "flat", "bonus"),
    "BonusYieldChanges":           (None, "city", engine.YIELDS, "flat", "bonus"),
    "BonusYieldModifiers":         (None, "city", engine.YIELDS, "percent", "bonus"),
    "BonusCommercePercentChanges": (None, "city", engine.COMMERCES, "percent", "bonus"),
    "BonusProductionModifiers":    ("buildRate", "self", None, "percent", "bonus"),
    "VicinityBonusYieldChanges":   (None, "city", engine.YIELDS, "flat", "vicinityBonus"),
    "BuildingHappinessChanges":    ("happiness", "city", None, "flat", "building"),
    "GlobalBuildingCostModifiers": ("costs", "empire", None, "percent", "building"),
    "PowerYieldModifiers":         (None, "city", engine.YIELDS, "percent", "power"),
}
# TARGET-keyed deposits (the effect lands ON the keyed entity): tag -> (family|None, scope, targetType|None, valuekeys|None, unit).
# targetType None => key DIRECTLY under scope (religion influence). family None + valuekeys => split member is family.
TARGET_KEYED = {
    "ImprovementYieldChanges":       (None, "city", "improvements", engine.YIELDS, "flat"),
    "GlobalImprovementYieldChanges": (None, "empire", "improvements", engine.YIELDS, "flat"),
    "TerrainYieldChanges":           (None, "city", "terrains", engine.YIELDS, "flat"),
    "PlotYieldChanges":              (None, "plot", "plotTypes", engine.YIELDS, "flat"),
    "GlobalBuildingExtraCommerces":  (None, "empire", "buildings", engine.COMMERCES, "flat"),
    "SpecialistYieldChanges":        (None, "specialist", "specialists", engine.YIELDS, "flat"),
    "SpecialistCommerceChanges":     (None, "specialist", "specialists", engine.COMMERCES, "flat"),
    "LocalSpecialistCommerceChanges": (None, "city", "specialists", engine.COMMERCES, "flat"),
    "BonusDefenseChanges":           ("defense", "city", "bonuses", None, "flat"),
    "ReligionChanges":               ("religion", "city", None, None, "flat"),
    "UnitCombatFreeExperiences":     ("experience", "city", "unitCombats", None, "flat"),
    "DomainFreeExperiences":         ("experience", "city", "domains", None, "flat"),
    "UnitCombatExtraStrengths":      ("strength", "city", "unitCombats", None, "flat"),
    "UnitProductionModifiers":       ("buildRate", "city", "units", None, "percent"),
    "UnitCombatProdModifiers":       ("buildRate", "city", "unitCombats", None, "percent"),
    "DomainProductionModifiers":     ("buildRate", "city", "domains", None, "percent"),
    "BuildingProductionModifiers":   ("buildRate", "city", "buildings", None, "percent"),
    "GlobalBuildingProductionModifiers": ("buildRate", "empire", "buildings", None, "percent"),
    "UnitCombatDefenseAgainstModifiers": ("defense", "city", "unitCombats", None, "flat"),
    "ImprovementFreeSpecialists":    ("freeSpecialists", "city", "improvements", None, "flat"),
}
_KEY_TAGS = ("PrereqTech", "TechType", "BuildingType", "BonusType", "ImprovementType", "TerrainType",
             "SpecialistType", "ReligionType", "UnitType", "UnitCombatType", "DomainType", "PlotType")


def _keyed(node, valuekeys):
    """[(ref, value), ...] for a C2C keyed/paired container. ref = the first known key-tag child; value = a
    named_array(rest, valuekeys) dict if valuekeys else the scalar int. Handles the inconsistent key tags."""
    out = []
    for entry in list(node):
        ref, valnode = None, None
        for c in entry:
            if ref is None and (c.tag in _KEY_TAGS or c.tag.endswith("Type")):
                ref = engine.text(c)
            else:
                valnode = c
        if not ref or ref == "NONE":
            continue
        if valuekeys is not None:
            vn = valnode if (valnode is not None and len(valnode)) else entry
            val = engine.named_array(vn, valuekeys)
        elif valnode is not None and len(valnode):
            val = engine.named_array(valnode, None) if False else None
        else:
            tx = engine.text(valnode) if valnode is not None else None
            val = int(tx) if engine.is_int(tx) else None
        if val not in (None, {}, [], 0):
            out.append((ref, val))
    return out


def _enabled(ref_kind, ref, scope):
    if ref_kind == "tech":
        return _atom(ref, "team")
    if ref_kind == "bonus":
        return _atom(ref, "city", min=1)
    if ref_kind == "vicinityBonus":
        return _atom(ref, "city", connection="vicinity", min=1)
    if ref_kind == "building":
        return _atom(ref, "empire" if scope == "empire" else "city")
    if ref_kind == "power":
        return "HAS_POWER"
    return _atom(ref, scope)


def _inject_cond(fams, family, scope, unit, value, enabled):
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    entry = OrderedDict([("value", value), ("enabled", enabled)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


def _inject_keyed(fams, family, scope, target_type, key, unit, value):
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    if target_type:
        node = node.setdefault(target_type, OrderedDict())
    node = node.setdefault(key, OrderedDict())
    if unit in node and isinstance(node[unit], int) and isinstance(value, int):
        node[unit] += value
    else:
        node[unit] = value


def pass2(typ, rec, store, fams, grants, repeatable, identity, enables, obsoletes):
    """The custom-shape layer: keyed inversions (§6.1), properties, repeatable grants, one-shot grants/pulses,
    enables-from-XML, the conditional/temporal deposits. Mutates the passed-in collections."""
    # --- CONDITION-gated keyed deposits (Tech/Bonus/Building/Power conditioners) ---
    for tag, (family, scope, vkeys, unit, kind) in COND_KEYED.items():
        node = rec.find(tag)
        if node is None:
            continue
        for ref, val in _keyed(node, vkeys):
            enabled = _enabled(kind, ref, scope)
            if isinstance(val, dict):                    # split: member IS the family
                for member, v in val.items():
                    _inject_cond(fams, member, scope, unit, v, enabled)
            else:
                _inject_cond(fams, family, scope, unit, val, enabled)
    # --- TARGET-keyed deposits (effect lands on the improvement/terrain/specialist/unit/...) ---
    for tag, (family, scope, ttype, vkeys, unit) in TARGET_KEYED.items():
        node = rec.find(tag)
        if node is None:
            continue
        for ref, val in _keyed(node, vkeys):
            if isinstance(val, dict):
                for member, v in val.items():
                    _inject_keyed(fams, member, scope, ttype, ref, unit, v)
            else:
                _inject_keyed(fams, family, scope, ttype, ref, unit, val)
    # --- StateReligionCommerces: per-commerce, gated on the building's religion being the state religion ---
    src = rec.find("StateReligionCommerces")
    if src is not None:
        relig = _txt(rec, "ReligionType")
        pred = OrderedDict([("STATE_RELIGION", relig)]) if relig else "HAS_STATE_RELIGION"
        for member, v in engine.named_array(src, engine.COMMERCES).items():
            _inject_cond(fams, member, "city", "flat", v, pred)
    # --- iStateReligionHappiness: happiness while the city follows the state religion ---
    srh = _int(rec, "iStateReligionHappiness")
    if srh:
        _inject_cond(fams, "happiness", "city", "flat", srh, "HAS_STATE_RELIGION")
    # --- CommerceChangeDoubleTimes: a SECOND age-gated commerce deposit (existedFor), pairing the base CommerceChanges ---
    cdt = rec.find("CommerceChangeDoubleTimes")
    if cdt is not None:
        base = engine.named_array(rec.find("CommerceChanges"), engine.COMMERCES) if rec.find("CommerceChanges") is not None else {}
        for member, turns in engine.named_array(cdt, engine.COMMERCES).items():
            if member in base:
                _inject_cond(fams, member, "city", "flat", base[member],
                             OrderedDict([("existedFor", OrderedDict([("min", turns)]))]))
    # --- CommerceHappinesses: happiness GAINED per unit of each commerce produced -> a grouped commerceHappiness family ---
    ch = rec.find("CommerceHappinesses")
    if ch is not None:
        for member, v in engine.named_array(ch, engine.COMMERCES).items():
            fams.setdefault("commerceHappiness", OrderedDict()).setdefault("city", OrderedDict()).setdefault(member, OrderedDict())["flat"] = v
    # --- shrine (GlobalReligionCommerce = a single RELIGION FK, getDataMembers enumAsInt): the building is the
    # SHRINE for that religion. The per-commerce VALUES live on the Religion (parked religion.shrine, #15); the
    # full modifier = religion.shrine.{commerce} x countReligionLevels(religion) at world scope is assembled at #430.
    # The building just declares the shrine relationship -> `shrine: RELIGION` (owner: shrine is a `per`-scaled
    # commerce modifier; the building provides the religion ref, the religion provides the values). ---
    shrine = _txt(rec, "GlobalReligionCommerce")
    if shrine:
        identity["shrine"] = shrine

    # --- CvProperties: Properties (city) / PropertiesAllCities (empire) -> per-PROPERTY family deposits ---
    for tag, scope in (("Properties", "city"), ("PropertiesAllCities", "empire")):
        node = rec.find(tag)
        if node is not None:
            for c in node:
                p = engine.text(c.find("PropertyType")) if c.find("PropertyType") is not None else None
                amt = _intval(c)
                if p and p != "NONE" and amt:
                    fams.setdefault(p, OrderedDict()).setdefault(scope, OrderedDict())["flat"] = amt
    pm = rec.find("PropertyManipulators")
    if pm is not None:
        for s in pm.findall("PropertySource"):
            res = engine.property_source_v3(s)
            if res:
                prop, pscope, unit, value = res
                node = fams.setdefault(prop, OrderedDict()).setdefault(pscope, OrderedDict())
                if unit in node and isinstance(node[unit], int) and isinstance(value, int):
                    node[unit] += value
                else:
                    node[unit] = value
    # --- repeatable grants (modifier-spec §4.1): PropertySpawn + the per-turn heal generalization ---
    sp_prop = _txt(rec, "PropertySpawnProperty")
    sp_unit = _txt(rec, "PropertySpawnUnit")
    if sp_prop and sp_unit:
        repeatable.append(OrderedDict([("unit", sp_unit), ("interval", "perTurn"),
                                       ("chance", OrderedDict([("per", _atom(sp_prop, "city"))]))]))
    fh = _int(rec, "iNumUnitFullHeal")
    if fh:
        repeatable.append(OrderedDict([("heal", "full"), ("count", fh), ("interval", "perTurn")]))
    hnode = rec.find("HealUnitCombatTypes")
    if hnode is not None:
        for item in list(hnode):
            uc = _txt(item, "UnitCombatType")
            heal, adj = _int(item, "iHeal"), _int(item, "iAdjacentHeal")
            if uc and (heal or adj):
                g = OrderedDict([("unitCombat", uc), ("interval", "perTurn")])
                if heal:
                    g["heal"] = heal
                if adj:
                    g["adjacentHeal"] = adj
                repeatable.append(g)
    # --- one-shot grants / pulses ---
    for tag, key in (("ExtraFreeBonuses", "bonuses"), ("FreeTraitTypes", "traits")):
        lst = _typelist(rec, tag)
        if lst:
            grants[key] = lst
    st = _txt(rec, "FreeSpecialTech")
    if st:
        grants.setdefault("techs", []).append(st)
    hc = _txt(rec, "HolyCity")
    if hc:
        grants["holyCity"] = hc
    ft = _int(rec, "iFreeTechs")
    if ft:
        grants["freeTechs"] = ft
    for tag in ("iPopulationChange", "iGlobalPopulationChange"):
        v = _int(rec, tag)
        if v:
            sc = "city" if tag == "iPopulationChange" else "empire"
            grants.setdefault("population", OrderedDict())[sc] = v
    if _bool(rec, "bGoldenAge"):
        grants["goldenAge"] = True
    fsc = rec.find("FreeSpecialistCounts")
    if fsc is not None:
        spec = OrderedDict()
        for k, v in _pairs_generic(fsc):
            spec[k] = v
        if spec:
            grants["specialists"] = spec
    promos = _typelist(rec, "FreePromoTypes")
    if promos:
        grants["promotions"] = promos
    # SpecialistCounts (slots, capacity) -> identity
    scn = rec.find("SpecialistCounts")
    if scn is not None:
        slots = OrderedDict((k, v) for k, v in _pairs_generic(scn))
        if slots:
            identity["specialistSlots"] = slots
    # --- enables-family authored on the building (FoundsCorporation / Hurrys / vote eligibility) ---
    fc = _txt(rec, "FoundsCorporation")
    if fc:
        enables.setdefault("corporations", []).append(fc)
    hurries = _typelist(rec, "Hurrys")
    if hurries:
        enables.setdefault("hurries", []).extend(hurries)
    if _bool(rec, "bForceTeamVoteEligible"):
        enables.setdefault("votes", []).append("FORCE_TEAM_ELIGIBLE")
    # --- ObsoletesToBuilding: the building's OWN obsolescence edge ---
    otb = _txt(rec, "ObsoletesToBuilding")
    if otb:
        obsoletes.setdefault("buildings", []).append(otb)
    # --- NewCityFree: RELOCATED off the building onto the FOUNDER units as grants.foundBuildings (owner 2026-06-16:
    # the settler "carries buildings into settling"; gated by each building's NewCityFree BoolExpr -> a tech-gated
    # building unavailable at settle time is not pre-built). curate_unit.found_buildings() reads NewCityFree off the
    # store's BuildingInfo table + the boolexpr converter; nothing is emitted building-side now. (renames §Unit.) ---
    # --- CommerceFlexibles -> identity.commerceFlexible (capability: which commerce SLIDERS this building unlocks;
    # CvPlayer::changeCommerceFlexibleCount on build -> isCommerceFlexible gates slider-setting — owner 2026-06-16). ---
    cfn = rec.find("CommerceFlexibles")
    if cfn is not None:
        flex = [engine.COMMERCES[i] for i, c in enumerate(list(cfn))
                if i < len(engine.COMMERCES) and engine.text(c) in ("1", "true", "True")]
        if flex:
            identity["commerceFlexible"] = flex
    # --- GlobalCorporationCommerce: park the corp FK (the building is corp X's HQ; the per-commerce
    # HeadquarterCommerce VALUES live on the corporation #16, x world countCorporationLevels assembled at #430 — the
    # corp-HQ ANALOG of the shrine, owner 2026-06-16). ---
    corphq = _txt(rec, "GlobalCorporationCommerce")
    if corphq:
        identity["corporationHQ"] = corphq
    # --- MayDamageAttackingUnitCombatTypes: the selective counter-damage list (pairs with the defense family +
    # bDamageAllAttackers/iDamageToAttacker) -> identity capability list. ---
    md = _typelist(rec, "MayDamageAttackingUnitCombatTypes")
    if md:
        identity["damageAttackingUnitCombats"] = md
    # AidRateChanges / BonusAidModifiers: DROPPED (owner 2026-06-16) — an UNWIRED property "aid" mechanic with NO
    # gameplay effect (city arrays m_paiAidRate/m_ppaaiExtraBonusAidModifier allocated+saved but never written-from-
    # building or read-for-effect); only AI building-valuation (CvCityAI /3) + pedia read the raw values. Not emitted.


def _intval(node):
    for ch in node:
        if engine.is_int(engine.text(ch)) and ch.tag != "PropertyType":
            return int(engine.text(ch))
    return None


def _pairs_generic(node):
    if node is None:
        return
    for item in list(node):
        k, v = None, None
        for c in item:
            if k is None and (c.tag.endswith("Type") or c.tag in _KEY_TAGS):
                k = engine.text(c)
            elif engine.is_int(engine.text(c)):
                v = int(engine.text(c))
        if k and k != "NONE" and v not in (None, 0):
            yield k, v


def _txt(rec, tag):
    t = engine.text(rec.find(tag))
    return t if (t and t != "NONE") else None


def _int(rec, tag):
    t = engine.text(rec.find(tag))
    return int(t) if engine.is_int(t) else None


def _bool(rec, tag):
    return engine.text(rec.find(tag)) in ("1", "true", "True")


def _typelist(rec, wrapper):
    node = rec.find(wrapper)
    if node is None:
        return []
    return [t for t in (engine.text(c).strip() for c in node) if t and t != "NONE"]


def _atom(typ, scope, **kw):
    a = OrderedDict([("type", typ), ("scope", scope)])
    a.update(kw)
    return a


def requires_building(rec, store):
    """The TARGET-side reversible MEANS gate (enabler-spec §3/§5): build (greying) vs operate (dormancy).
    Most SOURCE->building enabler edges are store-wired onto the source; here we author the building's OWN means."""
    build_all, build_any, build_none, op_all = [], [], [], []
    # --- resources (build-time greying; PrereqBonuses are a presence check) ---
    b = _txt(rec, "Bonus")
    if b:
        build_all.append(_atom(b, "city", connection="trade|vicinity"))
    orb = _typelist(rec, "PrereqBonuses")
    if orb:
        build_any.append([_atom(x, "city", connection="trade|vicinity") for x in orb])
    # RawVicinity FOLDS into normal vicinity (owner 2026-06-16: lose the adjacency strictness, simpler vocab).
    for tag in ("VicinityBonus", "RawVicinityBonus"):
        v = _txt(rec, tag)
        if v:
            build_all.append(_atom(v, "city", connection="vicinity"))
    for tag in ("PrereqVicinityBonuses", "PrereqRawVicinityBonuses"):
        lst = _typelist(rec, tag)
        if lst:
            build_any.append([_atom(x, "city", connection="vicinity") for x in lst])
    # --- plot-state predicates (bare) ---
    for tag, pred in (("bWater", "IS_WATER"), ("bRiver", "HAS_RIVER"), ("bFreshWater", "IS_FRESHWATER")):
        if _bool(rec, tag):
            build_all.append(pred)
    # --- power (presence) ---
    if _bool(rec, "bPower") or _bool(rec, "bPrereqPower"):
        build_all.append("HAS_POWER")
    pb = _txt(rec, "PowerBonus")
    if pb:
        build_all.append(_atom(pb, "city", connection="trade|vicinity", role="power"))
    # --- city / world counts + size (tally) ---
    for tag, scope in (("iPrereqPopulation", "city"), ("iCitiesPrereq", "empire"), ("iTeamsPrereq", "world"),
                       ("iLevelPrereq", "empire"), ("iMinAreaSize", "world")):
        v = _int(rec, tag)
        if v and v > 0:
            kind = {"iPrereqPopulation": "POPULATION", "iCitiesPrereq": "CITY", "iTeamsPrereq": "TEAM",
                    "iLevelPrereq": "UNIT_LEVEL", "iMinAreaSize": "AREA_SIZE"}[tag]
            build_all.append(_atom(kind, scope, min=v))
    lo, hi = _int(rec, "iMinLatitude"), _int(rec, "iMaxLatitude")
    if (lo and lo > 0) or (hi is not None and hi != 90):
        lat = OrderedDict([("latitude", OrderedDict())])
        if lo and lo > 0:
            lat["latitude"]["min"] = lo
        if hi is not None and hi != 90:
            lat["latitude"]["max"] = hi
        build_all.append(lat)
    # --- in-city buildings (AND / OR / NOT / count) ---
    for x in _typelist_struct(rec, "PrereqInCityBuildings", "BuildingType"):
        build_all.append(_atom(x, "city"))
    notin = _typelist_struct(rec, "PrereqNotInCityBuildings", "BuildingType")
    for x in notin:
        build_none.append(_atom(x, "city"))
    orbld = _typelist_struct(rec, "PrereqOrBuildings", "BuildingType")
    if orbld:
        build_any.append([_atom(x, "city") for x in orbld])
    for k, v in _amount_buildings(rec):
        build_all.append(_atom(k, "empire", min=v))
    anyone = _txt(rec, "PrereqAnyoneBuilding")
    if anyone:
        build_all.append(_atom(anyone, "world", min=1))
    # --- plot terrain/feature/improvement ---
    for tag, container in (("PrereqOrTerrain", None), ("PrereqOrFeature", None), ("PrereqOrImprovement", None)):
        lst = _typelist(rec, tag)
        if lst:
            build_any.append([_atom(x, "plot") for x in lst])
    for x in _typelist(rec, "PrereqAndTerrain"):
        build_all.append(_atom(x, "plot"))
    orher = _typelist_struct(rec, "PrereqOrHeritage", "HeritageType") or _typelist(rec, "PrereqOrHeritage")
    if orher:
        build_any.append([_atom(x, "empire") for x in orher])
    # --- victory prereq (world) ---
    vp = _txt(rec, "VictoryPrereq")
    if vp:
        build_all.append(_atom(vp, "world"))
    # --- instance caps -> max(SELF,N) (world wonder = noneOf SELF world) ---
    for tag, scope in (("iMaxGlobalInstances", "world"), ("iMaxTeamInstances", "team"), ("iMaxPlayerInstances", "empire")):
        v = _int(rec, tag)
        if v is not None and v >= 0:
            build_all.append(_atom("SELF", scope, max=v))
    # --- civics / state-religion -> OPERATE (dormancy) ---
    for x in _typelist_struct(rec, "PrereqAndCivics", "PrereqCivic") or _typelist(rec, "PrereqAndCivics"):
        op_all.append(_atom(x, "empire"))
    orciv = _typelist_struct(rec, "PrereqOrCivics", "PrereqCivic") or _typelist(rec, "PrereqOrCivics")
    if orciv:
        op_all.append(OrderedDict([("any", [[_atom(x, "empire") for x in orciv]])]))
    pc = _txt(rec, "PrereqCivic")
    if pc:
        op_all.append(_atom(pc, "empire"))
    if _bool(rec, "bNeedStateReligionInCity"):
        op_all.append("STATE_RELIGION_IN_CITY")
    if _bool(rec, "StateReligion"):
        op_all.append("HAS_STATE_RELIGION")
    # PrereqReligion / PrereqCorporation / PrereqCultureLevel are store-wired onto the source, BUT they are also
    # the building's reversible MEANS (a religion can leave via inquisition) -> author on operate too (forward check).
    for tag, scope in (("PrereqReligion", "city"), ("PrereqCorporation", "city"), ("PrereqCultureLevel", "city")):
        v = _txt(rec, tag)
        if v:
            op_all.append(_atom(v, scope))
    # --- ConstructCondition BoolExpr -> build (greying). It is checked ONLY at canConstruct (CvCity.cpp:2976-2999),
    # never isActiveBuilding, so losing a ConstructCondition bonus after build does nothing -> build (greying), NOT
    # operate (dormancy). Folded via the shared boolexpr converter (And/Or of Has over bonus/feature/tech/terrain/
    # building). owner 2026-06-16; renames §Building. ---
    boolexpr.merge_into(boolexpr.convert_field(rec.find("ConstructCondition")), build_all, build_any, build_none)

    build = OrderedDict()
    if build_all:
        build["all"] = build_all
    if build_any:
        build["any"] = build_any
    if build_none:
        build["noneOf"] = build_none
    # PALACE-TYPE (government-center) buildings can't be player-BUILT where a government center already exists
    # (CvCity.cpp:2654 isGovernmentCenter gate; Palace + the bGovernmentCenter pseudo-palaces). The negation twin
    # `disabled: IS_CAPITAL` (owner 2026-06-16); IS_CAPITAL = "the city has a palace/palace-adjacent building".
    # NB this is the PLAYER build gate only — the engine's FORCED relocation (capital falls) is an ungated actor
    # that bypasses requires (the #437 placement-gate invariant: gate the checked path, engine outcomes bypass).
    if _bool(rec, "bCapital") or _bool(rec, "bGovernmentCenter"):
        build["disabled"] = "IS_CAPITAL"
    out = OrderedDict()
    if build:
        out["build"] = build
    if op_all:
        out["operate"] = OrderedDict([("all", op_all)])
    # loadPrune (game options) lives in its own section, but author it here for now under requires for visibility.
    return out or None


def _typelist_struct(rec, wrapper, keytag):
    """Struct-list wrapper: each child carries a <keytag> ref (+ maybe a count). Returns [TYPE,...]."""
    node = rec.find(wrapper)
    if node is None:
        return []
    out = []
    for c in node:
        k = engine.text(c.find(keytag)) if c.find(keytag) is not None else engine.text(c)
        if k and k != "NONE":
            out.append(k)
    return out


def _amount_buildings(rec):
    """PrereqAmountBuildings: per-building count threshold -> [(BUILDING, N), ...]."""
    node = rec.find("PrereqAmountBuildings")
    if node is None:
        return []
    out = []
    for c in node:
        b = engine.text(c.find("BuildingType")) if c.find("BuildingType") is not None else None
        n = None
        for ch in c:
            if engine.is_int(engine.text(ch)) and ch.tag != "BuildingType":
                n = int(engine.text(ch))
        if b and b != "NONE" and n:
            out.append((b, n))
    return out


def loadprune_building(rec):
    out = OrderedDict()
    on = _typelist(rec, "PrereqGameOption") or ([_txt(rec, "PrereqGameOption")] if _txt(rec, "PrereqGameOption") else [])
    noton = _typelist(rec, "NotGameOption") or ([_txt(rec, "NotGameOption")] if _txt(rec, "NotGameOption") else [])
    on = [x for x in on if x]
    noton = [x for x in noton if x]
    if on:
        out["onGameOptions"] = on
    if noton:
        out["notOnGameOptions"] = noton
    return out or None


def _set_fam(fams, family, scope, member, unit, value):
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    if member:
        node = node.setdefault(member, OrderedDict())
    node[unit] = value


def curate(typ, rec, store):
    out = OrderedDict([("type", typ)])
    for tag, key in TEXT.items():
        t = _txt(rec, tag)
        if t:
            out[key] = t

    fams = OrderedDict()
    identity = OrderedDict()
    cost = OrderedDict()
    art_blocks = OrderedDict()
    ai = OrderedDict()

    # --- scalar/percent families ---
    for tag, (family, scope, member, unit) in SCALAR_FAMILIES.items():
        v = _int(rec, tag)
        if v:
            _set_fam(fams, family, scope, member, unit, v)
    # --- yield/commerce split families ---
    for tag, (scope, keys, unit) in YIELD_FAMILIES.items():
        node = rec.find(tag)
        if node is None:
            continue
        for member, v in engine.named_array(node, keys).items():
            _set_fam(fams, member, scope, None, unit, v)   # member IS the family (split)

    # --- enables / obsoletes / replaces (store-derived; COPIED so pass2 can extend FoundsCorporation/ObsoletesToBuilding) ---
    enables = OrderedDict((k, list(v)) for k, v in (store.enabled_by(typ) or {}).items())
    obsoletes = OrderedDict((k, list(v)) for k, v in (store.obsoletes_of(typ) or {}).items())
    replaces = store.replaces_of(typ)

    grants = OrderedDict()
    repeatable = []
    requires = requires_building(rec, store)
    loadprune = loadprune_building(rec)

    # --- PASS 2: keyed inversions (§6.1), properties, repeatable grants, one-shot grants, enables-from-XML ---
    pass2(typ, rec, store, fams, grants, repeatable, identity, enables, obsoletes)
    if repeatable:
        grants["repeatable"] = repeatable

    # --- cost ---
    for tag, key in COST.items():
        v = _int(rec, tag)
        if v is not None and v != -1 and v != 0:
            cost[key] = v

    # --- capabilities -> identity ---
    for tag, name in CAP_IDENTITY.items():
        if _bool(rec, tag):
            identity[name] = True
    # --- identity scalars / lists ---
    for tag, key in ID_SCALAR.items():
        iv = _int(rec, tag)
        sv = _txt(rec, tag)
        if iv is not None and iv != 0 and iv != -1:
            identity[key] = iv
        elif sv and not engine.is_int(sv):
            identity[key] = sv
    for tag, key in ID_LIST.items():
        lst = _typelist(rec, tag) if rec.find(tag) is not None and not _is_struct(rec.find(tag)) else _generic_list(rec, tag)
        if lst:
            identity[key] = lst

    # --- ai (Flavors + iAIWeight) ---
    fl = rec.find("Flavors")
    if fl is not None:
        g = engine.generic(fl)
        if g:
            ai["flavours"] = g
    w = _int(rec, "iAIWeight")
    if w:
        ai.setdefault("behaviour", OrderedDict())["weight"] = w

    # --- art ---
    for tag in ART:
        put_art(art_blocks, tag, engine.text(rec.find(tag)))

    # --- assemble (reserved order, modifier-spec §1.1) ---
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    if obsoletes:
        out["obsoletes"] = OrderedDict((k, obsoletes[k]) for k in sorted(obsoletes))
    if replaces:
        out["replaces"] = OrderedDict((k, replaces[k]) for k in sorted(replaces))
    if requires:
        out["requires"] = requires
    ordered = [f for f in FAMILY_ORDER if f in fams] + [f for f in fams if f not in FAMILY_ORDER]
    for f in ordered:
        out[f] = fams[f]
    if grants:
        out["grants"] = grants
    if cost:
        out["cost"] = cost
    if ai:
        out["ai"] = ai
    if loadprune:
        out["loadPrune"] = loadprune
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    return out


def curate_special(typ, rec, store):
    """SpecialBuilding (#31) — a per-player-capped building GROUP (getMaxPlayerInstances, enforced by
    isBuildingGroupMaxedOut/getBuildingGroupCount). Buildings join it via their SpecialBuildingType FK
    (identity.specialBuildingType). TechPrereq/TechPrereqAnyone -> store (tech.enables.specialBuildings), dropped here."""
    out = OrderedDict([("type", typ)])
    d = _txt(rec, "Description")
    if d:
        out["description"] = d
    enables = store.enabled_by(typ)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    obsoletes = store.obsoletes_of(typ)
    if obsoletes:
        out["obsoletes"] = OrderedDict((k, obsoletes[k]) for k in sorted(obsoletes))
    identity = OrderedDict()
    cap = _int(rec, "iMaxPlayerInstances")
    if cap is not None and cap >= 0:
        identity["maxPlayerInstances"] = cap          # the per-player GROUP cap (the key gameplay)
    if engine.text(rec.find("bValid")) in ("0", "false", "False"):
        identity["valid"] = False                      # bValid=0 -> the group is disabled (rare)
    art_blocks = OrderedDict()
    put_art(art_blocks, "Button", engine.text(rec.find("Button")))
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    return out


def _is_struct(node):
    return node is not None and len(node) > 0 and len(list(node)[0]) > 0


def _generic_list(rec, tag):
    node = rec.find(tag)
    if node is None:
        return []
    g = engine.generic(node)
    return g if isinstance(g, list) else ([g] if g else [])


HANDLED = (set(SCALAR_FAMILIES) | set(YIELD_FAMILIES) | set(CAP_IDENTITY) | set(ID_SCALAR) | set(ID_LIST)
           | set(COST) | set(TEXT) | set(ART) | REQUIRES_TAGS | STORE_TAGS | DROP_DEAD | DROP_MODULE | PASS2_TAGS
           | {"Type", "Flavors", "iAIWeight"})


def build_era(store):
    """tech Type -> era short name (from the tech XML <Era>)."""
    cache = {}
    for ttyp, trec in store.table("TechInfo").items():
        e = engine.text(trec.find("Era"))
        if e:
            cache[ttyp] = e.replace("C2C_ERA_", "").replace("ERA_", "").lower()
    return cache


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("BuildingInfo")
    era_map = build_era(store)
    results = OrderedDict()
    era_of = {}
    for typ, rec in table.items():
        results[typ] = curate(typ, rec, store)
        pt = _txt(rec, "PrereqTech")
        era_of[typ] = era_map.get(pt, "none")
    n = len(results)
    # SpecialBuilding #31 rides this pass (the per-player-capped GROUP).
    sb_results = OrderedDict((typ, curate_special(typ, rec, store))
                             for typ, rec in store.table("SpecialBuildingInfo").items())

    from collections import Counter
    leftover = Counter()
    for _typ, rec in table.items():
        for c in rec:
            if c.tag not in HANDLED:
                leftover[c.tag] += 1
    pass2 = Counter()
    for _typ, rec in table.items():
        for c in rec:
            if c.tag in PASS2_TAGS:
                pass2[c.tag] += 1
    if leftover:
        print("UNHANDLED tags (count): %s" % ", ".join("%s=%d" % (t, c) for t, c in leftover.most_common()))
    else:
        print("COVERAGE: all XML tags handled or deferred (pass 2).")
    print("PASS-2 deferred tags present: %d distinct" % len(pass2))

    has = lambda k: sum(1 for o in results.values() if k in o)
    STRUCT = {"type", "description", "civilopedia", "help", "enables", "obsoletes", "replaces", "requires",
              "cost", "ai", "loadPrune", "ui", "world", "sound", "identity"}
    seen = sorted({f for o in results.values() for f in o if f not in STRUCT})
    print("BuildingInfo curated: %d" % n)
    for k in ("enables", "obsoletes", "replaces", "requires", "cost", "ai", "loadPrune", "identity"):
        print("  with %-9s: %d" % (k, has(k)))
    print("  families seen: %s" % ", ".join(seen))
    print("SpecialBuildingInfo curated: %d  (with cap: %d)"
          % (len(sb_results), sum(1 for o in sb_results.values() if (o.get("identity") or {}).get("maxPlayerInstances") is not None)))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            src = results if nm in results else (sb_results if nm in sb_results else None)
            if src is not None:
                print("\n=== %s (era=%s) ===" % (nm, era_of.get(nm, "-")))
                print(json.dumps(src[nm], indent=1, ensure_ascii=False))
            else:
                print("\n(%s not found)" % nm)
    if args.write:
        base = os.path.join(REPO, "Assets", "Data", "buildings")
        for typ, obj in results.items():
            folder = os.path.join(base, era_of.get(typ, "none"))
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        sbdir = os.path.join(REPO, "Assets", "Data", "specialbuildings")
        os.makedirs(sbdir, exist_ok=True)
        for typ, obj in sb_results.items():
            with open(os.path.join(sbdir, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        print("\nwrote %d BuildingInfo + %d SpecialBuildingInfo JSON files under Assets/Data" % (n, len(sb_results)))


if __name__ == "__main__":
    main()
