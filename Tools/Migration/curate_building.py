#!/usr/bin/env python3
"""Curate Building (#428, Tier E #32) + SpecialBuilding (#31, rides this pass) — THE deepest modifier surface.

Curated from `classifications/building-classification.json` (the adversarial classify-building workflow, 288 fields)
+ the owner rulings (handover-2026-06-16-6). Building is the most-targeted entity; the SOURCE->building enabler edges
(tech/bonus/civic/religion/corp/cultureLevel `enables`, ObsoleteTech, ReplacementBuildings) are ALREADY store-wired,
so on the building side they DROP — the building authors only its OWN: ~70 modifier families, a `requires` MEANS gate,
`grants` (pure payload) + `triggers` (trigger -> chance -> action, json.md §5), cost, properties, identity.
EXE-link: 0 DllExport -> UNCONSTRAINED.

OWNER RULINGS folded in (handover #6):
- §6.1 DELIVERYGUY: the 22 "inversions" KEEP-ON-BUILDING keyed by target (NOT inverted). Tech/Bonus/Building gated via
  `enabled`; Improvement/Terrain/Plot yields target-keyed (food.city.improvements.{IMP}.flat). Tech ones PROVISIONAL (Phase F).
- top-level `triggers[]` (json.md §5, ruling 8): PropertySpawn (onTurn, chance via `per`) + iNumUnitFullHeal/
  HealUnitCombat (onTurn heal actions) + FreePromoTypes (onTurnEnd promote-present).
- shrine (GlobalReligionCommerce, a RELIGION FK) -> the TOP-LEVEL `shrine` bespoke section (values live on the religion);
  headquarters (GlobalCorporationCommerce, a CORPORATION FK) -> the TOP-LEVEL `headquarters` bespoke section (json §9,
  owner 2026-07-01, un-nested from identity).
- CommerceChangeDoubleTimes -> 2nd age-gated deposit `enabled:{existedFor:{min:N}}`.
- cityCapture = its OWN family (capturing CITIES, distinct from the §5 unit `capture` gradient).
- capability bools split (owner 2026-07-01, json §8): the 16 HELD city-scope intrinsics -> the `attributes` block
  (CAP_ATTRIBUTES); buildability/placement markers stay in identity (CAP_IDENTITY). CommerceFlexibles -> the
  `capabilities` block (PROVIDED to the empire: setCultureRate/setScienceRate/setEspionageRate). noHolyCity ->
  requires.build.disabled:IS_HOLY_CITY; applyFreePromotionOnMove -> grants.freePromotionsOnPresence; counter-damage
  (bDamageAllAttackers + MayDamageAttackingUnitCombatTypes) -> defense.city.counterDamage. EXCEPTIONS to enables-family:
  bForceTeamVoteEligible->enables.votes, Hurrys->enables.hurries, FoundsCorporation->enables.corporations.
- DROP (dead §8-i): iMaxPopulationAllowed, iMaxPopulationChange, iDCMNukesOkay/bDCMNukesOkay.
- power split (owner 2026-07-15): `bPower` (PROVIDES power, the power-plant flag) -> attributes.providesPower;
  `bPrereqPower` (NEEDS power) -> requires.operate HAS_POWER. The engine DORMS on the operate legs (checkBuildings):
  bPrereqPower (CvCity.cpp:21559), bFreshWater (:21567), iPrereqPopulation (:21582) -> all three author OPERATE
  (operate implies build, json §4.3). iCitiesPrereq/iTeamsPrereq stay build (canConstruct-only, no disable leg).
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
import xml.etree.ElementTree as ET
from collections import OrderedDict

import engine
import boolexpr
from curate_common import put_art, emit_art, FAMILY_ORDER, de_i, fold_text_to_identity, gate_entity, wipe_entity_json, skip_inert
from curate_unit import TAG_BY_UNITCOMBAT   # the ONE unitcombat->tag map (a building's free-promotion condition keys on it)
from store import Store, REPO

# ---- scalar/percent modifier families: tag -> (family, scope, member|None, unit). Corrected scopes from the
# classification (the mapping's were often wrong). Names PROVISIONAL (reader-pass refines). ----
SCALAR_FAMILIES = {
    # health / happiness / healing
    "iHealth": ("health", "city", None, "flat"),
    # AREA scope is abolished (owner): the legacy iArea* rows are modders reading "area" as "player". The ONE
    # legitimate area concept is a PHYSICAL CONTIGUITY constraint -- you cannot run power lines across an ocean
    # (the clean-power flag, engine-side, no cascade channel) -- and health/happiness/specialists carry no such
    # constraint, so nothing justifies them stopping at a coastline. They author at EMPIRE.
    "iAreaHealth": ("health", "empire", None, "flat"),
    "iGlobalHealth": ("health", "empire", None, "flat"),
    # per-pop rows author as §3.7 per-scalers (ruling 4, info-rebuild.md): flat + per{POPULATION, each:100}.
    # Engine math verified ×pop÷100 (CvCity.cpp:22017/22031 `m_i… * getPopulation() / 100`), so the RAW XML
    # value is human per-100-pop. The unit token below routes the curate() loop to _inject_per.
    "iHealthPercentPerPopulation": ("health", "city", None, "perPopulation"),
    "iHappiness": ("happiness", "city", None, "flat"),
    "iAreaHappiness": ("happiness", "empire", None, "flat"),   # area abolished -> empire (see iAreaHealth)
    "iGlobalHappiness": ("happiness", "empire", None, "flat"),
    "iHappinessPercentPerPopulation": ("happiness", "city", None, "perPopulation"),   # per-scaler, see health above
    "iHealRateChange": ("heal", "city", None, "flat"),   # ruling 13: folds into the heal family (the engine's cityContribution term of the one heal calc) -- `healing` was a fresh-key rollerskate
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
    # iAreaMaintenanceModifier and iOtherAreaMaintenanceModifier are NOT here: the home/other-area maintenance
    # pair re-authors as a conditioned deposit on the IS_HOME_AREA predicate (ruling 2, info-rebuild.md;
    # json.md §3.5) -- handled explicitly in curate(). NOTHING authors either tag today, so both surface in the
    # UNHANDLED coverage report if data ever appears; model it then, never as an area-scope deposit.
    "iDistanceMaintenanceModifier": ("maintenance", "empire", "distance", "percent"),
    "iNumCitiesMaintenanceModifier": ("maintenance", "empire", "numCities", "percent"),
    "iCoastalDistanceMaintenanceModifier": ("maintenance", "empire", "coastalDistance", "percent"),
    "iConnectedCityMaintenanceModifier": ("maintenance", "empire", "connectedCity", "percent"),
    "iInflationModifier": ("inflation", "empire", None, "percent"),
    # war weariness
    "iWarWearinessModifier": ("warWeariness", "city", None, "percent"),
    "iGlobalWarWearinessModifier": ("warWeariness", "empire", None, "percent"),
    "iEnemyWarWearinessModifier": ("warWeariness", "city", "enemy", "percent"),
    # hurry (ruling 18): iHurryCostModifier is the "hurrying ME" per-entity base modifier (CvCity.cpp:6047
    # kBuilding.getHurryCostModifier() as iBaseModifier) -> the entity's own `cost` section, handled in curate();
    # iGlobalHurryModifier is an empire hurry-cost % (CvPlayer.cpp:7389 changeHurryModifier -> CvCity.cpp:6058)
    # -> the ONE costs family, kind hurry.
    "iGlobalHurryModifier": ("costs", "empire", "hurry", "percent"),
    "iHurryAngerModifier": ("hurryAnger", "city", None, "percent"),
    # production specials
    "iMilitaryProductionModifier": ("buildRate", "city", "military", "percent"),
    "iSpaceProductionModifier": ("buildRate", "city", "space", "percent"),
    "iGlobalSpaceProductionModifier": ("buildRate", "empire", "space", "percent"),
    "iWorkerSpeedModifier": ("workRate", "empire", None, "percent"),
    # trade routes -- ONE family with conditions (ruling 11): the route COUNT is the MEMBERLESS scope-wide
    # amount (kind 0 IS the count -- the reconciliation micro-fix; the transient `routes` member collided with
    # the ROUTE_*-keyed target-container token); kinds modifier (route-yield %) / max (cap). The coastal/
    # foreign variants are CONDITIONS -> handled in curate() as conditioned deposits.
    "iTradeRoutes": ("tradeRoutes", "city", None, "flat"),
    "iGlobalTradeRoutes": ("tradeRoutes", "empire", None, "flat"),
    "iWorldTradeRoutes": ("tradeRoutes", "world", None, "flat"),
    "iTradeRouteModifier": ("tradeRoutes", "city", "modifier", "percent"),
    # experience / free specialists
    "iExperience": ("experience", "city", None, "flat"),
    "iGlobalExperience": ("experience", "empire", None, "flat"),
    "iFreeSpecialist": ("freeSpecialists", "city", None, "any"),
    "iAreaFreeSpecialist": ("freeSpecialists", "empire", None, "any"),   # area abolished -> empire
    "iGlobalFreeSpecialist": ("freeSpecialists", "empire", None, "any"),
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
    # underworld (renamed from copsAndRobbers, ruling 3 info-rebuild.md): the in-city criminal game -- criminal
    # stealth (insidiousness) vs city catch (investigation) -> the makeWanted/arrest mechanic (verified
    # CvUnit::doInsidiousnessVSInvestigationCheck). Dedicated block, NOT espionage (one stat, one name -- espionage
    # never "becomes" insidiousness); `detection` stays reserved for map-level hide-and-seek. iPillageGoldModifier
    # dropped (dead).
    "iInsidiousness": ("underworld", "city", "insidiousness", "flat"),
    "iInvestigation": ("underworld", "city", "investigation", "flat"),
    "iEspionageDefense": ("espionageDefense", "city", None, "flat"),
    # cityCapture (NEW family — capturing CITIES, distinct from §5 unit capture)
    "iNationalCaptureProbabilityModifier": ("cityCapture", "empire", "probability", "percent"),
    "iNationalCaptureResistanceModifier": ("cityCapture", "empire", "resistance", "percent"),
    "iLocalCaptureProbabilityModifier": ("cityCapture", "city", "probability", "percent"),
    "iLocalCaptureResistanceModifier": ("cityCapture", "city", "resistance", "percent"),
    # ruling 18: unit-upgrade price % (CvPlayer changeUnitUpgradePriceModifier -> CvUnit::upgradePrice
    # CvUnit.cpp:10356, a percent modifier on the price) -> the ONE costs family, kind upgrade.
    "iUnitUpgradePriceModifier": ("costs", "empire", "upgrade", "percent"),
    # What a building raises is the city's ELEVATION -- tree platforms put the lookout a storey higher (owner) --
    # and elevation grants sight to whoever looks from it (vision.md). The scope is CITY and that is load-bearing:
    # a building cannot elevate a unit that moves through, only the fixed observer it belongs to, which is exactly
    # what separates it from a watchtower improvement on the same plot (`elevation.plot`).
    "iLineOfSight": ("vision", "city", "elevation", "flat"),
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
}
# scope-wide yield/commerce families: tag -> (scope, keys, kind). SPLIT into per-identifier families (food/gold/…).
YIELD_FAMILIES = {
    "YieldChanges": ("city", engine.YIELDS, "flat"),
    "YieldModifiers": ("city", engine.YIELDS, "percent"),
    "YieldPerPopChanges": ("city", engine.YIELDS, "perPopulation"),
    # AreaYieldModifiers is NOT here: area scope is abolished, and nothing authors the tag (it surfaces in the
    # UNHANDLED coverage report if data ever appears). A yield percent confined to one landmass has no physical
    # constraint justifying it -- unlike clean power, which is the ONE real area concept and is engine-side.
    "GlobalYieldModifiers": ("empire", engine.YIELDS, "percent"),
    # GlobalSeaPlotYieldChanges + RiverPlotYieldChanges are PLOTS-TARGET folds (owner 2026-06-22): a scope-wide
    # source depositing onto every matching plot in scope. Handled in pass2 via _inject_plots (the "PLOTS-TARGET
    # folds" block), NOT the scope-wide flat path -- so `plotTypes`/`empire.flat`+post_process/`.river` are retired.
    "CommerceChanges": ("city", engine.COMMERCES, "flat"),
    "CommerceModifiers": ("city", engine.COMMERCES, "percent"),
    "CommercePerPopChanges": ("city", engine.COMMERCES, "perPopulation"),
    "GlobalCommerceModifiers": ("empire", engine.COMMERCES, "percent"),
    # SpecialistExtraCommerces is NOT here: it re-authors as a §3.7 per-scaler (ruling 4) -- handled in curate().
}

# building `attributes` block (json §8, owner ruling 2026-07-01): the building's OWN HELD, immutable, city-scope
# intrinsic capability bools. HELD (the building is/does this itself) — the opposite of `capabilities` (PROVIDED to
# the empire). Plain b-flag -> clean name: true (false omitted). Emitted under the `attributes` section.
CAP_ATTRIBUTES = {
    "bNukeImmune": "nukeImmune", "bZoneOfControl": "zoneOfControl",
    "bProtectedCulture": "protectedCulture", "bBorderObstacle": "borderObstacle", "bNoUnhappiness": "noUnhappiness",
    "bNoUnhealthyPopulation": "noUnhealthyPopulation", "bBuildingOnlyHealthy": "buildingOnlyHealthy",
    "bForceAllTradeRoutes": "forceAllTradeRoutes",
    "bQuarantine": "quarantine", "bMapCentering": "mapCentering",
    "bTeamShare": "teamShare", "bOrbital": "orbital", "bOrbitalInfrastructure": "orbitalInfrastructure",
    "bGovernmentCenter": "governmentCenter", "bCapital": "capital",
    "bProvidesFreshWater": "providesFreshWater",  # fresh water is NOT a BONUS_, so an attribute, NOT `provides`
    # bPower = the building PROVIDES power (a power plant -- legacy CvCity::processBuilding changePowerCount leg).
    # HELD intrinsic like providesFreshWater; power is NOT a BONUS_, so an attribute, NOT `provides`. Distinct from
    # bPrereqPower (NEEDS power -> requires.operate HAS_POWER); the two were once collapsed into one HAS_POWER
    # requires atom, making ~800 buildings read as power plants (the circular-power defect).
    "bPower": "providesPower",
    # bNeverCapture (the 17th attribute, owner ruling 2026-07-01): the building is destroyed/not-transferred when its
    # city is captured (CvPlayer.cpp:2565) -- a real HELD building attribute, RENAMED for clarity to `destroyedOnCapture`.
    "bNeverCapture": "destroyedOnCapture",
}
# capability/marker bools that STAY in identity (owner ruling 2026-07-01):
#  - autoBuild/noInstanceLimit/forceNoPrereqScaling/centerInCity: buildability/placement markers (json §7), NOT
#    held capabilities.
#  - allowsNukes: also drives `requires.build.disabled: NO_NUKES` (authored in requires_building); kept as an
#    identity marker here (not named in the attribute list).
# NOTE: bNoHolyCity -> requires.build.disabled:IS_HOLY_CITY, bApplyFreePromotionOnMove -> DROPPED (redundant; all
#       freePromotions are end-turn-stay by definition, owner 2026-07-01), and bDamageAllAttackers -> defense family
#       are handled specially (NOT in a bool table).
CAP_IDENTITY = {
    "bCenterInCity": "centerInCity", "bAllowsNukes": "allowsNukes",
    "bAutoBuild": "autoBuild", "bNoLimit": "noInstanceLimit", "bForceNoPrereqScaling": "forceNoPrereqScaling",
}
# identity scalars: tag -> key (non-zero int OR non-empty string).
ID_SCALAR = {
    "iAsset": "worth", "iPower": "militaryWorth", "iConquestProb": "conquestProbability",
    "iAirlift": "airlift", "iAirUnitCapacity": "airUnitCapacity",
    "iNumPopulationEmployed": "populationEmployed",
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
DROP_DEAD = {"iMaxPopulationAllowed", "iMaxPopulationChange", "iDCMNukesOkay", "bDCMNukesOkay", "iNukeExplosionRand",
    # dead, re-verified 2026-06-20 (zero consumers): building field unwired -- live pillage-gold is the promotion PillageChange
    "iPillageGoldModifier",
    # dead, re-verified 2026-06-20: the repel combat mechanic was REMOVED from the engine (CvCombatModel.cpp:294); field is a vestige (only debug TestCode.py reads getLocalRepel)
    "iLocalRepel",
    # dead, re-verified 2026-06-20: old wonder item never wired (only the loader/getter decl, zero consumers)
    "bNoEnemyPillagingIncome"}
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
    "PlotYieldChanges", "GlobalSeaPlotYieldChanges", "RiverPlotYieldChanges", "BonusDefenseChanges", "PowerYieldModifiers",
    # building-on-building:
    "BuildingHappinessChanges", "BuildingProductionModifiers", "GlobalBuildingProductionModifiers",
    "GlobalBuildingCostModifiers", "GlobalBuildingExtraCommerces",
    # specialist-keyed:
    "SpecialistYieldChanges", "SpecialistCommerceChanges", "LocalSpecialistCommerceChanges",
    # properties:
    "Properties", "PropertiesAllCities", "PropertyManipulators",
    # triggers entries (json.md §5):
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
    "TechCommerceChanges":         (None, "city", engine.COMMERCES, "flat", "tech"),     # FLAT change (changeBuildingCommerceTechChange -> getBaseCommerceRate100, CvCity.cpp:12136) -- NOT a percent (XML sub-tag "CommercePercents" is a misnomer; verify-then-fix, cascade-fixed-point.md §2)
    "TechCommerceModifiers":       (None, "city", engine.COMMERCES, "percent", "tech"),
    "TechHappinessChanges":        ("happiness", "city", None, "flat", "tech"),
    "TechHealthChanges":           ("health", "city", None, "flat", "tech"),
    # PRESENCE-gated (verified 2026-07-03: processBonus fires ONLY on the has<->hasn't TRANSITION --
    # processNumBonusChange gates on bOldHasBonus != bNewHasBonus -- so the enabled:{min:1} threshold IS the
    # faithful model; a x-count reading of processBonus' iChange was tried and falsified, 5x overshoot).
    "BonusHealthChanges":          ("health", "city", None, "flat", "bonus"),
    "BonusHappinessChanges":       ("happiness", "city", None, "flat", "bonus"),
    "BonusYieldChanges":           (None, "city", engine.YIELDS, "flat", "bonus"),
    "BonusYieldModifiers":         (None, "city", engine.YIELDS, "percent", "bonus"),
    "BonusCommercePercentChanges": (None, "city", engine.COMMERCES, "flat", "bonus"),   # FLAT x100 (getBonusCommercePercentChanges -> getBaseCommerceRate100, CvCity.cpp:12135) -- NOT a percent (the "Percent" XML name is a misnomer, like TechCommerceChanges); de-scaled via PER100_TAGS
    "BonusProductionModifiers":    ("buildRate", "self", None, "percent", "bonus"),
    "VicinityBonusYieldChanges":   (None, "city", engine.YIELDS, "flat", "vicinityBonus"),
    "GlobalBuildingCostModifiers": ("costs", "empire", None, "percent", "building"),
}
# TARGET-keyed deposits (the effect lands ON the keyed entity): tag -> (family|None, scope, targetType|None, valuekeys|None, unit).
# targetType None => key DIRECTLY under scope (religion influence). family None + valuekeys => split member is family.
TARGET_KEYED = {
    "ImprovementYieldChanges":       (None, "city", "improvements", engine.YIELDS, "flat"),
    "GlobalImprovementYieldChanges": (None, "empire", "improvements", engine.YIELDS, "flat"),
    "TerrainYieldChanges":           (None, "city", "terrains", engine.YIELDS, "flat"),
    # PlotYieldChanges -> a PLOTS-TARGET fold (owner 2026-06-22): per-plot-TYPE map folds into the `plots` target
    # filtered by a plot predicate (IS_WATER/IS_LAND/HAS_HILLS/HAS_PEAK). Handled in pass2 (_inject_plots), NOT plotTypes.
    "GlobalBuildingExtraCommerces":  (None, "empire", "buildings", engine.COMMERCES, "flat"),
    # BuildingHappinessChanges: +N happiness to EVERY city holding the keyed building (legacy PLAYER-scope
    # extraBuildingHappiness, CvPlayer::processBuilding:7490 -> per-city realization over its own buildings).
    # The Royal Tomb class: +1 keyed to Palace/Forbidden Palace/Versailles SPECIFICALLY (owner 2026-07-04 --
    # the named targets, never the generic gov-center predicate). Was mis-homed in COND_KEYED as a city-scope
    # co-location gate (fired only when target+source shared a city -- silently dead almost everywhere; the
    # precipice-review L3 find). The exact happiness twin of GlobalBuildingExtraCommerces above.
    "BuildingHappinessChanges":      ("happiness", "empire", "buildings", None, "flat"),
    # Specialist{Yield,Commerce}Changes + Local* -> NOT here: a building boosting a specialist is the SPECIALIST's
    # OWN output conditioned by the building's presence (own-output home, modifier.md §6.5, owner 2026-06-20), so it
    # lives ON THE SPECIALIST -- emitted by curate_specialist's SPECIALIST_BOOSTS, dropped at the building.
    "BonusDefenseChanges":           ("defense", "city", "bonuses", None, "flat"),
    "ReligionChanges":               ("religion", "city", None, None, "flat"),
    "UnitCombatFreeExperiences":     ("experience", "city", "unitCombats", None, "flat"),
    "DomainFreeExperiences":         ("experience", "city", "domains", None, "flat"),
    "UnitCombatExtraStrengths":      ("combat", "city", "unitCombats", None, "flat"),   # strength-MODIFYING -> combat (ruling 5)
    "UnitProductionModifiers":       ("buildRate", "city", "units", None, "percent"),
    "UnitCombatProdModifiers":       ("buildRate", "city", "unitCombats", None, "percent"),
    "DomainProductionModifiers":     ("buildRate", "city", "domains", None, "percent"),
    "BuildingProductionModifiers":   ("buildRate", "city", "buildings", None, "percent"),
    "GlobalBuildingProductionModifiers": ("buildRate", "empire", "buildings", None, "percent"),
    "UnitCombatDefenseAgainstModifiers": ("defense", "city", "unitCombats", None, "flat"),
    # ImprovementFreeSpecialists -> NOT keyed here: it's a conditioner (improvement presence), handled as a
    # conditioned `freeSpecialists.city.any` deposit in the body (modifier.md §6.7).
}
_KEY_TAGS = ("PrereqTech", "TechType", "BuildingType", "BonusType", "ImprovementType", "TerrainType",
             "SpecialistType", "ReligionType", "UnitType", "UnitCombatType", "DomainType", "PlotType")

# PER-100 legacy fields -- the curator's ONE-TIME de-scale to HUMAN-READABLE numbers (docs/dev/reference/
# cascade-fixed-point.md §0.1/§2). These XML fields are stored x100 (their C++ accessor is `get...100()`; the value
# flows straight into the x100 `m_buildingExtraYield100` bucket, CvCity.cpp:4951 -- verified from the math, not the
# field name alone). The JSON layer must be human ("+7", not 700); the human->x100 conversion is readJson's sole job.
# After this single XML->JSON conversion no per-100/normal mix survives anywhere downstream (owner 2026-06-19).
PER100_TAGS = frozenset(("TechYieldChanges", "TechCommerceChanges", "BonusCommercePercentChanges"))


def _descale100(v):
    """x100 legacy value -> human-readable: int when divisible by 100 (700 -> 7), else a 2-decimal float (150 -> 1.5).
    Recurses into the split/member dict ({food: 700, production: 100} -> {food: 7, production: 1})."""
    if isinstance(v, dict):
        return OrderedDict((k, _descale100(x)) for k, x in v.items())
    if isinstance(v, int):
        return v // 100 if v % 100 == 0 else round(v / 100.0, 2)
    return v


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
        # VicinityBonusYieldChanges keys off hasVicinityBonus (obtained) -> the "connected" discriminator (json.md S3.4).
        return _atom(ref, "city", connection="vicinity", min=1, vicinity="connected")
    if ref_kind == "building":
        return _atom(ref, "empire" if scope == "empire" else "city")
    if ref_kind == "power":
        return "HAS_POWER"
    return _atom(ref, scope)


def _inject_cond(fams, family, scope, unit, value, enabled, member=None):
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    if member:
        node = node.setdefault(member, OrderedDict())
    entry = OrderedDict([("value", value), ("enabled", enabled)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


def _inject_per(fams, family, scope, unit, value, per):
    """Like _inject_cond but the deposit carries a `per` count-scaler (json.md S3.7) instead of an `enabled` gate:
    effect = value * (count(per.type)/per.each). List-aware so it coexists with a plain count on the same leaf."""
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    entry = OrderedDict([("value", value), ("per", per)])
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


# PlotType key -> the plots-target predicate (the plotTypes fold, owner 2026-06-22). PLOT_LAND is *flat land* (the plot
# type is exclusive of hills/peak/ocean), so it is NOT bare IS_LAND (which the evaluator reads as !water -> matches
# hills/peak); it is domain+relief {all:[IS_LAND, IS_FLATLANDS]} (owner 2026-06-26, json §3.5).
PLOT_PRED = {"PLOT_OCEAN": "IS_WATER", "PLOT_LAND": OrderedDict([("all", ["IS_LAND", "IS_FLATLANDS"])]),
             "PLOT_HILLS": "HAS_HILLS", "PLOT_PEAK": "HAS_PEAK"}


def _inject_plots(fams, family, scope, unit, value, enabled):
    """A `plots`-TARGET deposit (owner 2026-06-22): <family>.<scope>.plots.<unit> = [{value, enabled:<predicate>}, ...]
    (data-model.md §4.1/§6). A scope-wide source deposits onto EVERY plot in scope matching the predicate -- ONE
    uniform mechanism retiring getYieldChangeAt / getSeaPlotYield / getRiverPlotYield + the per-plot-type accumulators.
    Always a LIST of entries (matches _inject_cond), so multiple predicates accumulate under one (family, scope)."""
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict()).setdefault("plots", OrderedDict())
    entry = OrderedDict([("value", value), ("enabled", enabled)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


def pass2(typ, rec, store, fams, grants, triggers, identity, enables, capabilities, bespoke):
    """The custom-shape layer: keyed inversions (§6.1), properties, `triggers` entries (ruling 8, json.md §5:
    trigger -> chance -> action), one-shot grants/pulses, enables-from-XML, the conditional/temporal deposits.
    Mutates the passed-in collections. `capabilities` collects empire-PROVIDED bools (commerce sliders);
    `bespoke` collects top-level sections (shrine/headquarters)."""
    # --- CONDITION-gated keyed deposits (Tech/Bonus/Building/Power conditioners) ---
    for tag, (family, scope, vkeys, unit, kind) in COND_KEYED.items():
        node = rec.find(tag)
        if node is None:
            continue
        for ref, val in _keyed(node, vkeys):
            if tag in PER100_TAGS:                       # one-time de-scale x100 -> human (cascade-fixed-point.md §2)
                val = _descale100(val)
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
    # --- PLOTS-TARGET folds (owner 2026-06-22): plotTypes/seaPlot/.river -> the explicit `plots` target filtered by a
    # plot predicate. A scope-wide source deposits onto EVERY matching plot in scope (data-model §4.1/§6); ONE uniform
    # mechanism retiring getYieldChangeAt / getSeaPlotYield / getRiverPlotYield + the per-plot-type accumulators. ---
    pyc = rec.find("PlotYieldChanges")           # per-plot-TYPE (OCEAN/LAND/HILLS/PEAK) -> <yield>.city.plots.flat {pred}
    if pyc is not None:
        for ref, val in _keyed(pyc, engine.YIELDS):
            pred = PLOT_PRED.get(ref)
            if pred is None or not isinstance(val, dict):
                continue                          # unknown PlotType -> skip (never invent a predicate)
            for member, v in val.items():
                _inject_plots(fams, member, "city", "flat", v, pred)
    gsp = rec.find("GlobalSeaPlotYieldChanges")  # empire-wide flat to water tiles -> <yield>.empire.plots.flat {IS_WATER}
    if gsp is not None:
        for member, v in engine.named_array(gsp, engine.YIELDS).items():
            if v:
                _inject_plots(fams, member, "empire", "flat", v, "IS_WATER")
    rpy = rec.find("RiverPlotYieldChanges")      # yield to the city's worked river plots -> <yield>.city.plots.flat {HAS_RIVER}
    if rpy is not None:
        for member, v in engine.named_array(rpy, engine.YIELDS).items():
            if v:
                _inject_plots(fams, member, "city", "flat", v, "HAS_RIVER")
    spy = rec.find("SeaPlotYieldChanges")        # LOCAL sea-plot yield (THIS city's water plots) -> <yield>.city.plots.flat {IS_WATER}
    if spy is not None:                          # (distinct from GlobalSeaPlotYieldChanges = empire-wide; uniformity, no data today)
        for member, v in engine.named_array(spy, engine.YIELDS).items():
            if v:
                _inject_plots(fams, member, "city", "flat", v, "IS_WATER")
    # --- PowerYieldModifiers: a DIRECT per-yield array (<iYield>..</iYield>), NOT entity-keyed -> emit each yield as a
    # city-scope `percent` deposit gated `enabled: HAS_POWER` (legacy getPowerYieldRateModifier, summed only when
    # isPower(), CvCity.cpp:11228). Was mis-classified in COND_KEYED (which expects a key-tag) -> silently dropped. ---
    pym = rec.find("PowerYieldModifiers")
    if pym is not None:
        for member, v in engine.named_array(pym, engine.YIELDS).items():
            if v:
                _inject_cond(fams, member, "city", "percent", v, "HAS_POWER")
    # --- StateReligionCommerces: the engine accumulates each building's value into a PLAYER POOL
    # (kPlayer.getStateReligionBuildingCommerce = Σ ALL the player's buildings' StateReligionCommerces, CvPlayer:7472),
    # then adds the WHOLE pool ONCE PER city building whose religion == the state religion (getBuildingCommerceByBuilding
    # :12266 -- a pool × matching-count cross-multiplication). A per-building gated flat models neither, so emit a clean
    # per-channel MARKER (the raw value); the cascade pools it over the player's buildings and applies it per matching
    # building (its religion = identity.religion). ---
    src = rec.find("StateReligionCommerces")
    if src is not None:
        srb = OrderedDict((member, v) for member, v in engine.named_array(src, engine.COMMERCES).items() if v)
        if srb:
            identity["stateReligionCommerce"] = srb
    # --- iStateReligionHappiness: happiness while THIS BUILDING'S religion IS the player's state religion. The
    # engine keys the apply by the building's ReligionType (changeStateReligionHappiness(kBuilding.getReligionType(),
    # ...), CvCity.cpp:4692) and getCurrentStateReligionHappiness reads the STATE religion's slot -- so the gate is
    # the parameterized {STATE_RELIGION: <building religion>} (json S3.5), NOT the bare HAS_STATE_RELIGION ("has ANY
    # state religion"), which wrongly paid a Jewish Masada's bonus to a Confucian empire (wellbeing parity find
    # 2026-07-03). A carrier without a ReligionType keeps the bare form (no key to gate on). ---
    srh = _int(rec, "iStateReligionHappiness")
    if srh:
        srh_rel = rec.findtext("ReligionType")
        _inject_cond(fams, "happiness", "city", "flat", srh,
                     OrderedDict([("STATE_RELIGION", srh_rel)]) if srh_rel and srh_rel != "NONE" else "HAS_STATE_RELIGION")
    # --- CommerceChangeDoubleTimes: the engine DOUBLES the building's WHOLE per-building commerce (base + shrine +
    # corpHQ + event + stateRel) once it has existed >= N game-years (getBuildingCommerceByBuilding iCommerce*=2,
    # CvCity.cpp:12284-12290). A +base-only second flat UNDER-doubles a building that is also a shrine/corp-HQ, so emit
    # a clean per-channel MARKER (the age threshold) the cascade uses to double the building's FULL computed commerce.
    # `identity.commerceDoubleTime: {commerce: years}` (mirrors identity.shrine -- a per-building relationship/marker). ---
    cdt = rec.find("CommerceChangeDoubleTimes")
    if cdt is not None:
        dbl = OrderedDict((member, turns) for member, turns in engine.named_array(cdt, engine.COMMERCES).items() if turns)
        if dbl:
            identity["commerceDoubleTime"] = dbl
    # --- CommerceHappinesses: legacy "+V happiness at 100% on the <commerce> slider" -> ordinary happiness
    # deposits per-scaled on the json.md §3.1 slider-rate tokens (ruling 20, info-rebuild.md: the whole
    # commerceHappiness family DISSOLVES -- wellbeing mints zero kinds). Engine site (shared by all channels):
    # CvCity.cpp:12803 getCommerceHappinessByType = per * getCommercePercent(commerce) / 100 (what-ifs
    # CvCity.cpp:8462 + :8938 transcribe the same math) -> value V, per {<CHANNEL>_RATE, each: 100}. ---
    ch = rec.find("CommerceHappinesses")
    if ch is not None:
        for member, v in engine.named_array(ch, engine.COMMERCES).items():
            if v:
                _inject_per(fams, "happiness", "city", "flat", v,
                            OrderedDict([("type", member.upper() + "_RATE"), ("each", 100)]))
    # --- shrine (GlobalReligionCommerce = a single RELIGION FK, addEnumAsInt): the building is the SHRINE for that
    # religion. The per-commerce VALUES live on the Religion (ReligionInfo::getGlobalReligionCommerce, parked
    # religion.shrine #15); the full modifier = religion.shrine.{commerce} x countReligionLevels(religion) is
    # assembled at #430 (CvCity.cpp:12378-12384). The building declares only the shrine RELATIONSHIP (the FK) ->
    # the TOP-LEVEL `shrine` bespoke section (json §9), un-nested from identity (owner 2026-07-01: the shrine
    # relationship IS the data). ⚑ NB the FK is the building's ONLY shrine data — the commerce {culture:...} lives
    # on the religion, NOT the building (verified addEnumAsInt + CvCity.cpp:12275-12284). ---
    shrine = _txt(rec, "GlobalReligionCommerce")
    if shrine:
        bespoke["shrine"] = shrine

    # --- CvProperties: Properties (city) / PropertiesAllCities (empire) -> per-PROPERTY family deposits ---
    for tag, scope in (("Properties", "city"), ("PropertiesAllCities", "empire")):
        node = rec.find(tag)
        if node is not None:
            for c in node:
                p = engine.text(c.find("PropertyType")) if c.find("PropertyType") is not None else None
                amt = _intval(c)
                # PROPERTY_FLAMMABILITY rebalance (owner 2026-07-21): positive flammability values are divided by 5
                # (rounded to nearest); negative values pass through unchanged. Only this property, only the positive
                # side. A value that rounds to 0 (was 1-2) drops out via the `and amt` guard below.
                if p == "PROPERTY_FLAMMABILITY" and amt and amt > 0:
                    amt = int(round(amt / 5.0))
                if p and p != "NONE" and amt:
                    fams.setdefault(p, OrderedDict()).setdefault(scope, OrderedDict())["flat"] = amt
    pm = rec.find("PropertyManipulators")
    if pm is not None:
        for s in pm.findall("PropertySource"):
            res = engine.property_source_v3(s)
            if res:
                prop, pscope, unit, value = res
                node = fams.setdefault(prop, OrderedDict()).setdefault(pscope, OrderedDict())
                if unit not in node:
                    node[unit] = value
                elif isinstance(node[unit], int) and isinstance(value, int):
                    node[unit] += value
                else:
                    # MULTI-SOURCE same property/unit with mixed shapes (a plain value + a conditioned/per
                    # entry, or several entries): a LIST of entries (json §3.9) -- the old overwrite LOST the
                    # prior value (ANCIENT_CUSTOMS' +1 vanished under its -1@TECH_LITERATURE; found by the
                    # property-channel parity probe 2026-07-03).
                    if not isinstance(node[unit], list):
                        node[unit] = [node[unit]]
                    if isinstance(value, list):
                        node[unit].extend(value)
                    else:
                        node[unit].append(value)
    # --- `triggers` entries (ruling 8, json.md §5: trigger -> chance -> action; the old repeatable+interval
    # wrapper dissolves into the trigger): PropertySpawn + the per-turn heal generalization. ---
    sp_prop = _txt(rec, "PropertySpawnProperty")
    sp_unit = _txt(rec, "PropertySpawnUnit")
    if sp_prop and sp_unit:
        # the §5 exemplar: per-turn roll, odds scaled by the city's property value, granting the unit on success.
        triggers.append(OrderedDict([
            ("trigger", "onTurn"),
            ("chance", OrderedDict([("per", _atom(sp_prop, "city"))])),
            ("action", OrderedDict([("grant", OrderedDict([("units", [sp_unit])]))])),
        ]))
    fh = _int(rec, "iNumUnitFullHeal")
    if fh:
        # heal-verb payload keys carried over from the legacy entry (verb vocabulary is OPEN, json.md §5).
        triggers.append(OrderedDict([
            ("trigger", "onTurn"),
            ("action", OrderedDict([("heal", "full"), ("count", fh)])),
        ]))
    hnode = rec.find("HealUnitCombatTypes")
    if hnode is not None:
        for item in list(hnode):
            uc = _txt(item, "UnitCombatType")
            heal, adj = _int(item, "iHeal"), _int(item, "iAdjacentHeal")
            if uc and (heal or adj):
                action = OrderedDict()
                if heal:
                    action["heal"] = heal
                if adj:
                    action["adjacentHeal"] = adj
                action["unitCombat"] = uc
                triggers.append(OrderedDict([("trigger", "onTurn"), ("action", action)]))
    # --- one-shot grants / pulses --- (ExtraFreeBonuses is NOT a one-shot grant: it's a continuous while-active
    # bonus supply -> provides.bonuses, handled in curate().)
    # FreeTraitTypes -> enables.traits (owner ruling 2026-07-01, json §5/§8): a whole civ-trait conferred on the
    # OWNER empire *while the building is active*, reverting on loss (owner.setHasTrait, CvCity.cpp:4614) -- the same
    # grantor-PROVIDES / empire-HOLDS pattern as capabilities, but the held thing is a full trait (effect-bundle), so
    # it is held-while-active, NOT a one-shot handout. Merged into the enables dict as the `traits` bucket, uniform
    # with the FoundsCorporation/Hurrys enables-from-XML below (rj_walkEnableEdge resolves it generically ->
    # edges["enables.traits"] = [TRAIT_ ids]). Was previously mis-homed as grants["traits"].
    ftraits = _typelist(rec, "FreeTraitTypes")
    if ftraits:
        enables.setdefault("traits", []).extend(ftraits)
    st = _txt(rec, "FreeSpecialTech")
    if st:
        grants.setdefault("techs", []).append(st)
    # HolyCity -> requires.build (owner ruling 2026-07-01, json §5): a read-only "only in RELIGION_X's holy city"
    # BUILD gate -- canConstruct returns false unless the city is already that religion's holy city (CvCity.cpp:2728);
    # it hands out NOTHING (the holy city is set by religion FOUNDING, never by a building). So it is a build GATE, not
    # a grant. Authored in requires_building() as an {IS_HOLY_CITY: RELIGION_X} parameterized predicate (json §3.5,
    # carried by cp_parseObject IS_HOLY_CITY, CvCascadeConditionParse.cpp:199). Was previously mis-homed as grants["holyCity"].
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
    # FreeSpecialistCounts -> freeSpecialists COUNT family, keyed by specialist type (modifier.md §6.7 (A)).
    fsc = rec.find("FreeSpecialistCounts")
    if fsc is not None:
        node = fams.setdefault("freeSpecialists", OrderedDict()).setdefault("city", OrderedDict())
        for k, v in _pairs_generic(fsc):
            if v:
                node[k] = v
    # FreePromoTypes -> a `triggers` end-turn-presence entry (ruling 8, json.md §5: the old grants.freePromotions
    # dissolves -- promoting is not the source's considered action). ONE mechanism (owner ruling 2026-07-01): the
    # promotions are granted at END-TURN to units PRESENT in the city -- a unit trained there is present at end-turn;
    # a unit that walks in and stays is covered the same way. The legacy bApplyFreePromotionOnMove flag (a funky/racy
    # mid-turn/on-move re-apply) is DROPPED as redundant -- all freePromotions are end-turn-stay by definition.
    # NB FreePromoTypes is a STRUCT-list (<FreePromoType><PromotionType>...); read the PromotionType child (a bare
    # _typelist yielded '' and silently dropped every promo -- a pre-existing latent bug, fixed here 2026-07-01).
    # Each <FreePromoType> may carry a sibling <FreePromotionCondition> -- the building declaring WHICH UNITS it can
    # deal with (a Riding School deals with `mounted`). Every shipped one is a single
    # <Has><GOMType>GOM_UNITCOMBAT</GOMType><ID>UNITCOMBAT_X</ID></Has>, which maps through TAG_BY_UNITCOMBAT onto the
    # unit's TAG -- so the four MOUNT_HORSE/CAMEL/DEER/BISON variants all collapse to one `mounted` predicate.
    # Emitted as the ordinary conditioned-entry shape (json §3.9: every grant entry takes `enabled`), read live by
    # cascadeEvalCondition's IS_<TAG> predicate. Dropping these made a targeted promotion apply to EVERY unit.
    fpn = rec.find("FreePromoTypes")
    promos = []
    if fpn is not None:
        for fp in fpn.findall("FreePromoType"):
            pt = fp.find("PromotionType")
            if pt is None or not (pt.text or "").strip():
                continue
            promo = pt.text.strip()
            tags = []
            for has in fp.iter("Has"):
                gom = has.find("GOMType")
                idn = has.find("ID")
                if gom is not None and (gom.text or "").strip() == "GOM_UNITCOMBAT" and idn is not None:
                    for t in TAG_BY_UNITCOMBAT.get((idn.text or "").strip(), ()):
                        if t not in tags:
                            tags.append(t)
            if not tags:
                entry = promo
            elif len(tags) == 1:
                entry = {"promotion": promo, "enabled": "IS_" + tags[0].upper()}
            else:
                entry = {"promotion": promo, "enabled": {"any": ["IS_" + t.upper() for t in tags]}}
            # DEDUPE: the legacy authored one entry per unitcombat VARIANT (Riding School carries four --
            # MOUNT_HORSE/CAMEL/DEER/BISON -- all granting PROMOTION_SPEED). They map to the SAME tag predicate,
            # so the tag-level model collapses them to a single entry; keeping four identical copies would just
            # make the apply do the same work four times.
            if entry not in promos:
                promos.append(entry)
    if promos:
        # `promote` verb shape (curator-chosen minimal form; json.md §5 names the verb but does not pin the
        # payload): promotions = the entry list (a bare promotion string, or {promotion, enabled:<unit predicate>}
        # -- the §3.9 entry form), units:"present" = every unit present in the city at end-turn.
        triggers.append(OrderedDict([
            ("trigger", "onTurnEnd"),
            ("action", OrderedDict([("promote", OrderedDict([("promotions", promos), ("units", "present")]))])),
        ]))
    # SpecialistCounts (capacity/slots) -> allowedSpecialists COUNT family, keyed by specialist type (the cap on
    # manual assignment, modifier.md §6.7 (A)).
    scn = rec.find("SpecialistCounts")
    if scn is not None:
        node = fams.setdefault("allowedSpecialists", OrderedDict()).setdefault("city", OrderedDict())
        for k, v in _pairs_generic(scn):
            if v:
                node[k] = v
    # TechSpecialistChanges = the SAME capacity, tech-conditioned (inner XML is <SpecialistCounts>, verified 2026-06-20)
    # -> allowedSpecialists.city.SPECIALIST_X enabled by the team-tech. NOT freeSpecialists (the old label was wrong).
    tsc = rec.find("TechSpecialistChanges")
    if tsc is not None:
        for entry in list(tsc):
            tech = _txt(entry, "PrereqTech")
            counts = entry.find("SpecialistCounts")
            if tech and counts is not None:
                pred = OrderedDict([("type", tech), ("scope", "team")])
                for k, v in _pairs_generic(counts):
                    if v:
                        _inject_cond(fams, "allowedSpecialists", "city", k, v, pred)
    # ImprovementFreeSpecialists -> free ANY specialist, SCALED BY the count of improved plots in the city.
    # Engine (CvCity.cpp:5758): totalFreeSpecialists += getImprovementFreeSpecialists(imp) * countNumImprovedPlots(imp)
    # -- a PER count-scaler (n free specialists per city plot carrying that improvement), NOT a presence gate.
    # VERIFIED 2026-06-27, correcting the earlier unverified `enabled`-presence model (json.md §3.7 `per:{type,scope}`).
    ifs = rec.find("ImprovementFreeSpecialists")
    if ifs is not None:
        for entry in list(ifs):
            imp = _txt(entry, "ImprovementType")
            n = _int(entry, "iFreeSpecialistCount")
            if imp and n:
                _inject_per(fams, "freeSpecialists", "city", "any", n, OrderedDict([("type", imp), ("scope", "city")]))
    # --- enables-family authored on the building (FoundsCorporation / Hurrys / vote eligibility) ---
    fc = _txt(rec, "FoundsCorporation")
    if fc:
        enables.setdefault("corporations", []).append(fc)
    hurries = _typelist(rec, "Hurrys")
    if hurries:
        enables.setdefault("hurries", []).extend(hurries)
    if _bool(rec, "bForceTeamVoteEligible"):
        enables.setdefault("votes", []).append("FORCE_TEAM_ELIGIBLE")
    # ObsoletesToBuilding is authored TARGET-side as `obsoletedBy` in the main emit (not here) — owner 2026-06-22.
    # --- NewCityFree: RELOCATED off the building onto the FOUNDER units as plain grants.buildings (owner 2026-06-16:
    # the settler "carries buildings into settling"; gated by each building's NewCityFree BoolExpr -> a tech-gated
    # building unavailable at settle time is not pre-built). curate_unit.found_buildings() reads NewCityFree off the
    # store's BuildingInfo table + the boolexpr converter; nothing is emitted building-side now. (renames §Unit.) ---
    # --- CommerceFlexibles -> the building's `capabilities` block, PROVIDED to the empire (owner ruling 2026-07-01,
    # json §8): which commerce SLIDERS this building unlocks (CvPlayer::changeCommerceFlexibleCount on build ->
    # isCommerceFlexible gates slider-setting). Positional per-commerce array (engine.COMMERCES order). Emitted as
    # discrete `canSet<X>Rate` bools (canonical names, owner 2026-07-02 -- the name says what it does), uniform with
    # tech/civic capabilities. COMMERCE_GOLD has no slider -> flagged/skipped
    # (never present in data; ⚑ FLAG if it ever appears). ---
    COMMERCE_SLIDER_CAP = {"research": "canSetScienceRate", "culture": "canSetCultureRate", "espionage": "canSetEspionageRate"}
    cfn = rec.find("CommerceFlexibles")
    if cfn is not None:
        for i, c in enumerate(list(cfn)):
            if i >= len(engine.COMMERCES) or engine.text(c) not in ("1", "true", "True"):
                continue
            commerce = engine.COMMERCES[i]
            cap = COMMERCE_SLIDER_CAP.get(commerce)
            if cap:
                capabilities[cap] = True
            # else: gold has no slider -> intentionally skipped (see comment).
    # --- headquarters (GlobalCorporationCommerce = a single CORPORATION FK, addEnumAsInt): the building is that
    # corporation's HEADQUARTERS -> the TOP-LEVEL `headquarters` bespoke section (json §9), the corp-HQ ANALOG of
    # `shrine`. The per-commerce HeadquarterCommerce VALUES live on the corporation (CorporationInfo, #16), x world
    # countCorporationLevels assembled at #430 (CvCity.cpp:12386-12391); the building declares only the FK
    # relationship. (owner 2026-07-01: un-nested from identity, mirror of shrine.) ---
    corphq = _txt(rec, "GlobalCorporationCommerce")
    if corphq:
        bespoke["headquarters"] = corphq
    # --- COUNTER-DAMAGE is DROPPED, not emitted (owner ruling): a building damaging a unit that attacks its city is
    # a TRIGGER by shape (a happening, a roll, an effect on the attacker), and if the functionality is wanted it is
    # modelled properly on the trigger plane rather than carried over as a defense MEMBER. So the legacy fields are
    # consumed here and emitted nowhere; the rework is tracked as its own issue.
    #
    # The legacy mechanic it drops was half-dead anyway, which is why porting the shape verbatim was never the right
    # move. `CvUnit::checkCityAttackDefensesDamage` gates on `isDamageAttackerCapable()`, a DERIVED flag that
    # `CvBuildingInfo` only ever sets from `bDamageAllAttackers` (read) or from an INHERITED may-damage list (the
    # merge, guarded on the building's own list being empty) -- there is NO path setting it from a building's OWN
    # authored list. So across the 13 trap buildings the 7 carrying `bDamageAllAttackers` fired, and the 6 carrying
    # a hand-authored 7-to-13-entry `MayDamageAttackingUnitCombatTypes` list never fired at all.
    # Both halves go; the reworked mechanic is authored fresh, not migrated. ---
    dc = fams.get("defense", {}).get("city") if isinstance(fams.get("defense"), dict) else None
    if isinstance(dc, dict):
        dc.pop("damageToAttacker", None)
        dc.pop("damageAttackerChance", None)
        if not dc:                                   # cleanup: defense.city emptied
            fams["defense"].pop("city", None)
            if not fams["defense"]:
                fams.pop("defense", None)
    # AidRateChanges / BonusAidModifiers: DROPPED — DEAD: city arrays saved but ZERO write-from-building + ZERO
    # read-for-effect (only AI-valuation/pedia read the raw Info). (owner-confirmed 2026-06-16, rationale corrected
    # 2026-07-01: m_paiAidRate/m_ppaaiExtraBonusAidModifier are allocated+saved but never written from a building nor
    # read for any gameplay effect; the sole readers are CvCityAI building-valuation and the civilopedia.) Not emitted.


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


def _extra_free_bonuses(rec):
    # ExtraFreeBonuses is NESTED: <ExtraFreeBonuses><ExtraFreeBonus><FreeBonus>BONUS_X</FreeBonus>
    # <iNumFreeBonuses>N</iNumFreeBonuses></ExtraFreeBonus>...</ExtraFreeBonuses>. The engine's getFreeBonuses()
    # makes the building supply these bonuses to its city while ACTIVE (bonusAvailableFromBuildings -> hasVicinityBonus),
    # so they become a vicinity-bonus SOURCE -> provides.bonuses (uniform with a map bonus that provides itself).
    node = rec.find("ExtraFreeBonuses")
    if node is None:
        return []
    out = []
    for efb in node.findall("ExtraFreeBonus"):
        b = _txt(efb, "FreeBonus")
        if b:
            n = _int(efb, "iNumFreeBonuses")   # the supply COUNT (HOLLYWOOD = 6, wine = 3, ...)
            # a bare string infers count 1; carry an explicit {BONUS_X: N} only when N > 1 (json §5a) so the
            # count-1 common case stays the clean list form and the tradeable-supply count is not lost.
            out.append({b: n} if (n and n > 1) else b)
    return out


def _default_scope(typ):
    # Mirrors the parser's rjDefaultScope (CvCascadeReadJson.cpp): TECH->team, civic/heritage->empire,
    # everything else (building/bonus/religion/corporation/population/...)->city.
    if typ.startswith("TECH_"):
        return "team"
    if typ.startswith(("CIVIC_", "HERITAGE_")):
        return "empire"
    return "city"


def _atom(typ, scope, **kw):
    # Collapse a plain presence to a BARE STRING -- the parser implies scope from the ID's domain, so emitting a
    # redundant {type, scope} only invites authoring bugs (owner 2026-06-23: forcing type+scope guarantees bugs).
    # Object form ONLY for a special case: any kw (a `connection`, a `role`, a `min`/`max` count), a non-default
    # scope, or a plot-substrate predicate type (TERRAIN_/FEATURE_/IMPROVEMENT_/MAPCATEGORY_) -- the parser routes
    # those to a plot predicate by the object's `type` key, so they must stay objects, never a bare string.
    is_plot_pred = isinstance(typ, str) and typ.startswith(("TERRAIN_", "FEATURE_", "IMPROVEMENT_", "MAPCATEGORY_"))
    if not kw and not is_plot_pred and scope == _default_scope(typ):
        return typ
    a = OrderedDict([("type", typ), ("scope", scope)])
    a.update(kw)
    return a


def requires_building(rec, store):
    """The TARGET-side reversible MEANS gate (enabler-spec §3/§5): build (greying) vs operate (dormancy).
    Most SOURCE->building enabler edges are store-wired onto the source; here we author the building's OWN means."""
    build_all, build_any, build_none, op_all, op_any, op_dormant = [], [], [], [], [], []
    # --- resources: a bonus prereq is REVERSIBLE, but WHERE it lands depends on whether the building NEEDS the
    # resource to FUNCTION or merely to APPEAR (owner ruling 2026-06-22; live-verified):
    #   * MANUALLY-built buildings that need the resource to operate (the gatherer line -- RICE_GATHERER &c.) go
    #     DORMANT when the resource is lost -> requires.OPERATE. (Corrects the 2026-06-21 build-side classification,
    #     which leaned on an INCOMPLETE "isDisabledBuilding == replacement/religion/corp only" grounding; the live
    #     enabler verify proved the engine DOES disable these on bonus loss -- the ~1,193 missed-dormant gatherers.)
    # ⚑ PESTS/CRIME disease pseudobuildings (bAutoBuild) are a KNOWN GAP: the engine dormants them by their
    #   DISEASE/CRIME PROPERTY band (owner 2026-06-22), NOT the bonus -- but they are not in CIV4PropertyInfos, so
    #   the curator doesn't author that band. Routing their bonus to BUILD (no operate) over-permits (the engine
    #   dormants most PESTS -> +misses); routing to OPERATE over-dormants the persisters (e.g. TERMITES -> false-
    #   dormants) but matches MORE cities (28 misses / 61% vs 101 / 51%). Until the PESTS property band is migrated,
    #   operate is the closer approximation, so bonus -> operate uniformly (the gatherer line is the real win here).
    bonus_all, bonus_any = op_all, op_any
    b = _txt(rec, "Bonus")
    if b:
        bonus_all.append(_atom(b, "city", connection="trade|vicinity"))
    orb = _typelist(rec, "PrereqBonuses")
    if orb:
        bonus_any.append([_atom(x, "city", connection="trade|vicinity") for x in orb])
    # Vicinity bonus REFINEMENT (owner ruling 2026-06-24, supersedes the 2026-06-16 "fold raw into vicinity"): bare
    # `connection:"vicinity"` = the bonus on ANY radius tile; a `vicinity` DISCRIMINATOR tightens which tiles count.
    # The engine has TWO flavors with OPPOSITE strictness, so they CANNOT fold to one (json.md S3.4):
    #   VicinityBonus    -> hasVicinityBonus    (owned+valid+connected)  -> vicinity:"connected"  (the obtained semantic)
    #   RawVicinityBonus -> hasRawVicinityBonus (centre OR owned radius tile, no connection) -> vicinity:"owned"
    # (e.g. MINE_GOLD VicinityBonus needs the connected gate; NET_SHRIMP/MUREX RawVicinityBonus need only owned presence.)
    for tag, disc in (("VicinityBonus", "connected"), ("RawVicinityBonus", "owned")):
        v = _txt(rec, tag)
        if v:
            bonus_all.append(_atom(v, "city", connection="vicinity", vicinity=disc))
    for tag, disc in (("PrereqVicinityBonuses", "connected"), ("PrereqRawVicinityBonuses", "owned")):
        lst = _typelist(rec, tag)
        if lst:
            bonus_any.append([_atom(x, "city", connection="vicinity", vicinity=disc) for x in lst])
    # --- plot-state predicates (bare). bWater = the city is COASTAL, not the plot being water: legacy
    # isValidBuildingLocation (CvCity.cpp:18500-18506) gates bWater on isCoastal() (a city sits on LAND, so
    # IS_WATER/pl->isWater() is always false -> every coastal building wrongly hidden). With bRiver also set it is
    # coastal OR river (line 18502). bRiver-alone / bFreshWater stay plot predicates (lines 18507/18516). ---
    bw, br = _bool(rec, "bWater"), _bool(rec, "bRiver")
    # A WATER building's bWater gate is legacy isCoastal(iMinAreaSize): the city must sit adjacent to a SEA-BODY of
    # >= iMinAreaSize tiles (isValidBuildingLocation -> isCoastal -> isCoastalLand(N), CvCity.cpp). A BARE HAS_COAST is
    # isCoastal at the DEFAULT coast threshold, which UNDER-constrains when the building demands a larger body (e.g.
    # MONTREAL_BIODOME iMinAreaSize=10 over-offered on small seas) -> carry the size into {HAS_COAST:{minArea:N}}.
    # (bRiver-alone / bFreshWater stay bare plot predicates.)
    mas = _int(rec, "iMinAreaSize")
    coast = OrderedDict([("HAS_COAST", OrderedDict([("minArea", mas)]))]) if (bw and mas and mas > 0) else "HAS_COAST"
    if bw and br:
        build_any.append([coast, "HAS_RIVER"])   # city is coastal OR on a river
    elif bw:
        build_all.append(coast)
    elif br:
        build_all.append("HAS_RIVER")
    # bFreshWater = NEEDS fresh water; the engine DORMS the built building when access is lost (checkBuildings,
    # CvCity.cpp:21567) -> requires.OPERATE (operate implies build, json §4.3, so construct gating is preserved).
    if _bool(rec, "bFreshWater"):
        op_all.append("HAS_FRESHWATER")
    # --- power: bPrereqPower = NEEDS power; the engine DORMS on power loss (checkBuildings, CvCity.cpp:21559)
    # -> requires.OPERATE. bPower = PROVIDES power -> attributes.providesPower (CAP_ATTRIBUTES), never a
    # requirement (folding it here made power plants require the power they produce -- the circular-power defect).
    if _bool(rec, "bPrereqPower"):
        op_all.append("HAS_POWER")
    pb = _txt(rec, "PowerBonus")
    if pb:
        bonus_all.append(_atom(pb, "city", connection="trade|vicinity", role="power"))   # operate/build by bAutoBuild
    # --- city / world counts + size (tally) ---
    # iLevelPrereq (the "empire has a unit of level >= N" gate, CvPlayer::canConstruct:6766 getHighestUnitLevel) is
    # INTENTIONALLY DROPPED (owner ruling 2026-06-24): the gate is removed from the game (XML iLevelPrereq zeroed on
    # the 6 MA_* academies, its only users) and from the model -- so parity holds with the requirement simply gone.
    # iPrereqPopulation = the engine DORMS the built building when population drops below it (checkBuildings,
    # CvCity.cpp:21582) -> requires.OPERATE. iCitiesPrereq/iTeamsPrereq have NO disable leg (canConstruct-only)
    # -> stay build.
    v = _int(rec, "iPrereqPopulation")
    if v and v > 0:
        op_all.append(_atom("POPULATION", "city", min=v))
    for tag, scope in (("iCitiesPrereq", "empire"), ("iTeamsPrereq", "world")):
        v = _int(rec, tag)
        if v and v > 0:
            kind = {"iCitiesPrereq": "CITY", "iTeamsPrereq": "TEAM"}[tag]
            build_all.append(_atom(kind, scope, min=v))
    # iMinAreaSize for a LAND building -> AREA_SIZE atom = the landmass tile count (area()->getNumTiles()). For a
    # WATER building it is the SEA-BODY size, folded into {HAS_COAST:{minArea:N}} above (legacy isCoastal(N)).
    if mas and mas > 0 and not bw:
        build_all.append(_atom("AREA_SIZE", "city", min=mas))
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
        # PrereqAmountBuildings: the required count SCALES (CvPlayer::getBuildingPrereqBuilding) --
        # getModifiedIntValue(base, worldBuildingPrereqModifier) * (1 + count(SELF)), unless SELF/prereq is a limited
        # wonder or SELF is forceNoPrereqScaling. `min` is the BASE; the enforcer (cascade) owns the scaling -> mark it.
        build_all.append(_atom(k, "empire", min=v, scaling="amountBuildings"))
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
    # MapCategoryTypes: the placement gate (legacy isMapCategory, CENTER-plot) -- the city plot must be in ONE of the
    # building's map categories (uncategorized plot = valid, handled engine-side in PRED_HAS_MAP_CATEGORY). Also kept
    # in identity.mapCategories (classification). Needed so space scenarios don't break (owner 2026-06-18); returns as
    # a viewport/zone gate when "all maps in one map" lands (multimap-zone-rework).
    mapcats = _typelist(rec, "MapCategoryTypes")
    if mapcats:
        build_any.append([_atom(x, "plot") for x in mapcats])
    orher = _typelist_struct(rec, "PrereqOrHeritage", "HeritageType") or _typelist(rec, "PrereqOrHeritage")
    if orher:
        build_any.append([_atom(x, "empire") for x in orher])
    # --- victory prereq (world) ---
    vp = _txt(rec, "VictoryPrereq")
    if vp:
        build_all.append(_atom(vp, "world"))
    # --- tech prereqs -> build.all (AND only: single PrereqTech + every TechTypes entry; buildings have NO OR-tech
    # -- only techs themselves model alternate tech-tree paths). The SOURCE->building enable edge is store-wired
    # (generation/frontier); this is the per-candidate CONFIRM the condition engine evaluates via isHasTech.
    # Without it the cascade can't gate future-tech wonders (the modern-wonder over-offer). enabler-spec §13.8;
    # legacy gate CvPlayer::canConstruct 6584 (PrereqAndTech) + 6589 (PrereqAndTechs). Mirrors curate_unit. ---
    t = _txt(rec, "PrereqTech")
    if t:
        build_all.append(_atom(t, "team"))
    for x in _typelist_struct(rec, "TechTypes", "PrereqTech"):
        build_all.append(_atom(x, "team"))
    # --- SpecialBuilding GROUP-gate inheritance (data-model §7, the building-group wrangle): a member building
    # inherits its GROUP's shared gates. TechPrereq -> the same build.all TECH confirm (the group carries the tech
    # the member lacks -- monasteries/temples/cathedrals/punk lines; legacy CvPlayer::canConstruct 6599). NB
    # TechPrereqAnyone is UNUSED across all 36 groups -> skipped; the group cap + waiver are handled in curate()/the
    # cascade; ObsoleteTech inheritance is store-wired (store.py). ---
    sb = _txt(rec, "SpecialBuildingType")
    if sb and store is not None:
        sbrec = store.get("SpecialBuildingInfo", sb)
        if sbrec is not None:
            sbt = sbrec.findtext("TechPrereq")
            if sbt:
                build_all.append(_atom(sbt, "team"))
    # --- instance caps are NOT a requires atom (owner 2026-06-17) — they move to the declarative `allowed` cap.
    # A requires SELF-atom forced an off-by-one (cap 1 -> max:0) and conflated "needed" (requires) with "allowed"
    # (the ceiling). `allowed:{scope:N}` names the ceiling with the REAL number; SELF leaves requires entirely.
    # Authored in curate() via allowed_building(). enabler-spec §5/§13.7. ---
    # --- civics / state-religion -> OPERATE (dormancy) ---
    for x in _typelist_struct(rec, "PrereqAndCivics", "PrereqCivic") or _typelist(rec, "PrereqAndCivics"):
        op_all.append(_atom(x, "empire"))
    orciv = _typelist_struct(rec, "PrereqOrCivics", "PrereqCivic") or _typelist(rec, "PrereqOrCivics")
    if orciv:
        # OR-group -> operate.any (a TOP-LEVEL any), NOT a nested {any:} inside operate.all: the cascade parser
        # (CvCascadeReadJson.cpp:218-224) treats each operate.all element as a LEAF, so a nested {any:} was SKIPPED
        # -> the civic gate was dropped -> over-offer. Mirror build_any. (A2 "civics" cluster fix, 2026-06-18.)
        op_any.append([_atom(x, "empire") for x in orciv])
    pc = _txt(rec, "PrereqCivic")
    if pc:
        op_all.append(_atom(pc, "empire"))
    # bNeedStateReligionInCity (STATE_RELIGION_IN_CITY) and StateReligion (getPrereqStateReligion,
    # m_iStateReligion, "this religion must be your STATE religion") are BUILD gates (greying), NOT operate
    # (dormancy): both are checked ONLY in canConstruct (needStateReligionInCity CvCity.cpp:2604;
    # getPrereqStateReligion CvPlayer.cpp:6676) and appear in NO disable/isActiveBuilding path -> once built, the
    # building does NOT go dormant if the player later switches state religion away. So -> build. (verified 2026-06-21)
    if _bool(rec, "bNeedStateReligionInCity"):
        build_all.append("STATE_RELIGION_IN_CITY")
    sr = _txt(rec, "StateReligion")
    if sr:
        build_all.append(OrderedDict([("STATE_RELIGION", sr)]))
    # HolyCity -> requires.build (owner ruling 2026-07-01, json §5): the building may be constructed ONLY in
    # RELIGION_X's holy city -- canConstruct returns false unless isHolyCity(religion) (CvCity.cpp:2728). A pure
    # buildability GATE (hands out nothing; the holy city is set by religion FOUNDING), so it belongs on build, not
    # grants. Authored as the {IS_HOLY_CITY: RELIGION_X} parameterized predicate (json §3.5), AND-combined into
    # build_all with every other build condition (parity with the STATE_RELIGION build gate above). The parser carries
    # it cleanly: cp_parseObject IS_HOLY_CITY -> CASC_PRED_IS_HOLY_CITY with the religion FK-resolved
    # (CvCascadeConditionParse.cpp:199); enabler evaluation of the predicate is a KNOWN follow-up.
    hc = _txt(rec, "HolyCity")
    if hc:
        build_all.append(OrderedDict([("IS_HOLY_CITY", hc)]))
    # PrereqReligion / PrereqCorporation are the building's reversible MEANS (a religion can leave via inquisition,
    # a corp can be lost) and the engine DOES disable a built building when they go (CvCity.cpp applyReligionModifiers
    # ~14999 / applyCorporationModifiers ~15198 set isDisabledBuilding) -> author on operate (dormancy, forward check).
    for tag, scope in (("PrereqReligion", "city"), ("PrereqCorporation", "city")):
        v = _txt(rec, tag)
        if v:
            op_all.append(_atom(v, scope))
    # ReplacementBuildings is reversible DORMANCY in the engine -- CvCity.cpp:14413 setDisabledBuilding(predecessor,
    # true) while the successor is present (NOT removal: the predecessor STAYS, hasBuilding() true), re-enabled when
    # it's gone (unless another replacer holds). Mirror it EXACTLY with requires.operate.dormant (parity -- a
    # behavioural redesign of whether e.g. blackened-skies should dorm an observatory is POST-migration). The
    # `replaces` enabler edge stays a defined concept but is now UNUSED -- the whole mechanic is operate.dormant.
    # (owner 2026-06-23; engine-verified.)
    for x in _typelist_struct(rec, "ReplacementBuildings", "BuildingType"):
        op_dormant.append(_atom(x, "city"))
    # PrereqCultureLevel is a BUILD gate (greying), NOT operate (dormancy): getPrereqCultureLevel is checked ONLY in
    # canConstruct (CvCity.cpp:2788, sets probabilityEverConstructable) and appears in NO disable/isActiveBuilding
    # path -> a built building does NOT go dormant if its city's culture level later drops. (verified 2026-06-21)
    v = _txt(rec, "PrereqCultureLevel")
    if v:
        build_all.append(_atom(v, "city"))
    # --- ConstructCondition BoolExpr -> build (greying). It is checked ONLY at canConstruct (CvCity.cpp:2976-2999),
    # never isActiveBuilding, so losing a ConstructCondition bonus after build does nothing -> build (greying), NOT
    # operate (dormancy). Folded via the shared boolexpr converter (And/Or of Has over bonus/feature/tech/terrain/
    # building). owner 2026-06-16; renames §Building. ---
    boolexpr.merge_into(boolexpr.convert_field(rec.find("ConstructCondition")), build_all, build_any, build_none)
    # EnabledCivilizationTypes is NOT a requires gate: CvCity::canConstruct:2557 applies it ONLY to an
    # isStronglyRestricted() NPC civ (Neanderthal) -- a WHITELIST so the era-locked NPC may build a thing past its
    # block (e.g. the buffalo trainer, which sits past SEDENTARY_LIFESTYLE). For a real civ the check is SKIPPED.
    # So it is an IDENTITY field (identity.enabledCivilizations, emitted in curate()), IGNORED by the dry-calc (NPCs
    # excluded); remodel post-rework (owner 2026-06-24). Was wrongly AND-ed into requires.build -> under-offered every
    # real civ (the whole animal-trainer cluster).

    boolexpr.fold_or_groups(build_all, build_any)   # OR-groups -> nested {any} under all (any = ||, never list-of-groups)
    build = OrderedDict()
    if build_all:
        build["all"] = build_all
    if build_none:
        build["noneOf"] = build_none
    # GOVERNMENT-CENTER buildings (Palace + the bGovernmentCenter pseudo-palaces) can't be player-BUILT where a
    # government center already exists. The engine gate is EXACTLY `kBuilding.isGovernmentCenter() && isGovernmentCenter()`
    # (CvCity.cpp:2664), where city isGovernmentCenter() == m_iGovernmentCenterCount>0 == "holds a gov-center building".
    # So the disable predicate is IS_GOVERNMENT_CENTER, NOT IS_CAPITAL (verified 2026-06-24: canConstruct has NO
    # isCapital gate; the two predicates are distinct and IS_CAPITAL was a mis-naming of this gov-center rule).
    # NB this is the PLAYER build gate only — the engine's FORCED relocation (capital falls) is an ungated actor
    # that bypasses requires (the #437 placement-gate invariant: gate the checked path, engine outcomes bypass).
    disabled = []
    if _bool(rec, "bGovernmentCenter"):
        disabled.append("IS_GOVERNMENT_CENTER")
    # bNoHolyCity: a BUILD gate (greying), NOT an attribute -- the engine bars building it IN a holy city
    # (CvCity::canConstruct:2591, verified 2026-07-01: `!bExposed && kBuilding.isNoHolyCity() && isHolyCity()` ->
    # can't construct; it does NOT prevent the city BECOMING a holy city). So -> requires.build.disabled: IS_HOLY_CITY.
    if _bool(rec, "bNoHolyCity"):
        disabled.append("IS_HOLY_CITY")
    # bAllowsNukes: the engine BARS an allowsNukes building (MANHATTAN_PROJECT) while isNoNukes() holds -- the UN
    # no-nukes verdict (CvPlayer::canConstruct:6746). Model as a world-scope NO_NUKES disable (owner ruling 2026-06-24:
    # "disabled.world.NO_NUKES"). isNoNukes() is false once nukes are enabled (anyone has built Manhattan), so the
    # building re-enables then. (MANHATTAN also needs uranium|heavy_water -- already on operate.)
    if _bool(rec, "bAllowsNukes"):
        disabled.append("NO_NUKES")
    if len(disabled) == 1:
        build["disabled"] = disabled[0]
    elif disabled:
        build["disabled"] = OrderedDict([("any", disabled)])   # suppressed while ANY disable predicate holds
    out = OrderedDict()
    if build:
        out["build"] = build
    boolexpr.fold_or_groups(op_all, op_any)   # OR-groups -> nested {any} under all
    if op_all or op_dormant:
        operate = OrderedDict()
        if op_all:
            operate["all"] = op_all
        if op_dormant:
            operate["dormant"] = op_dormant   # dormant while ANY listed is present (the reversible-disable mirror)
        out["operate"] = operate
    return out or None


def fold_build_into_operate(req):
    """A QUEUE-EXCLUDED entity has no build gate -- fold `requires.build` into `requires.operate` (owner ruling).

    A `notConstructible` building is never offered through the production queue: it is PLACED by another system
    (the property bands, autobuild, grants, spawns) and the enabler holds it statically excluded, so it is HIDDEN
    by construction. Placement is UNCONDITIONAL -- every such building is placed in every city, and DORMANCY then
    decides whether it does anything (the uniform band model, enabler.md par.3, generalized to the whole class).

    That leaves `requires.build` with NO consumer at all. `build` only ever GREYS a queue candidate and is checked
    ONCE at build (enabler.md par.3); the ongoing dormancy gate reads `operate` alone. So a condition left in
    `build` would simply never be evaluated again -- a cliff dwelling placed in a flat city would come up ACTIVE
    because its TERRAIN_PEAK clause sat in the half nothing reads. The conditions must therefore live in `operate`,
    which is re-checked every recompute.

    This is strictly MORE correct than the position it leaves: `operate` reacts to the plot substrate changing
    (terrain levelled to sea level -- the WMD case), so the building correctly dorms if the ground it needed stops
    existing, which a checked-once `build` clause could never do.

    The merge is plain boolean algebra -- an AND of two ANDs is one AND; two `disabled` clauses suppress if EITHER
    holds (`any`); two `enabled` clauses both bind (`all`)."""
    if not isinstance(req, dict) or not isinstance(req.get("build"), dict):
        return req
    build = req["build"]
    operate = OrderedDict(req["operate"]) if isinstance(req.get("operate"), dict) else OrderedDict()
    conj = list(build.get("all") or [])
    for comb in ("any", "noneOf"):                       # a combinator node joins the AND as one child (json par.3.4)
        if comb in build:
            conj.append(OrderedDict([(comb, build[comb])]))
    if conj:
        operate["all"] = list(operate.get("all") or []) + conj
    for twin, joiner in (("enabled", "all"), ("disabled", "any")):
        if twin not in build:
            continue
        operate[twin] = OrderedDict([(joiner, [operate[twin], build[twin]])]) if twin in operate else build[twin]
    out = OrderedDict()
    placed = False
    for key, val in req.items():
        if key in ("build", "operate"):
            if not placed and operate:                   # operate takes the slot the pair occupied
                out["operate"] = operate
                placed = True
            continue
        out[key] = val
    if not placed and operate:
        out["operate"] = operate
    return out or None


def allowed_building(rec):
    """The declarative INSTANCE CAP (owner 2026-06-17): `allowed:{<scope>:N}` = "at most N of THIS may exist at
    scope" — the REAL cap number (NOT a requires SELF-atom, which forced an off-by-one and conflated needed/allowed).
    Scope-keyed (world/team/empire); for a building the cap scope ALSO derives its wonder category
    (world->worldWonder, team->teamWonder, empire->nationalWonder; CvGameCoreUtils.cpp:340-369 isWorldWonder =
    getMaxGlobalInstances()!=-1). Absent => uncapped. The new canDoStuff gate enforces it (build while
    tally.count(SELF,scope) < N) and owns ignoring it (NO_WONDER_LIMIT/NO_NATIONAL_UNIT_LIMIT/CHALLENGE_ONE_CITY),
    era-scaling, and +extra — all engine, never the parser (enabler-spec §5/§13.7)."""
    # ALL caps emit regardless of bNoLimit -- the cap IS the wonder CATEGORY (world->worldWonder,
    # team->teamWonder, empire->nationalWonder; isWorldWonder = getMaxGlobalInstances()!=-1), and legacy carries
    # the category and the enforcement WAIVER as two independent axes: PALACE authors iMaxPlayerInstances=1 (a
    # national wonder) AND bNoLimit (relocatable). Folding bNoLimit into a cap-absence stripped the CATEGORY from
    # every consumer -- isLimitedWonder's prereq-SCALING exemption above all (getBuildingPrereqBuilding: a
    # limited-wonder prereq never scales by self-count), which set the palace-prereq'd autobuild markers
    # oscillating (need = K+1 palaces). The waiver keeps its own home (bNoLimit -> identity.noInstanceLimit) and
    # the ENGINE composes it at the enforcement gate (CvPlayer::isBuildingMaxedOut early-outs on isNoLimit).
    # The empire cap is maxPlayer + extraPlayer.
    allowed = OrderedDict()
    gw = _int(rec, "iMaxGlobalInstances")
    if gw is not None and gw >= 0:
        allowed["world"] = gw
    tw = _int(rec, "iMaxTeamInstances")
    if tw is not None and tw >= 0:
        allowed["team"] = tw
    pw = _int(rec, "iMaxPlayerInstances")
    if pw is not None and pw >= 0:
        allowed["empire"] = pw + (_int(rec, "iExtraPlayerInstances") or 0)
    return allowed or None


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


def gate_building(rec):
    """The entity-level enabled/disabled option gate (owner 2026-07-08; zero buildings author the legacy tags
    today -- kept so a future authoring lands in the canonical form, never a bespoke section)."""
    on = _typelist(rec, "PrereqGameOption") or ([_txt(rec, "PrereqGameOption")] if _txt(rec, "PrereqGameOption") else [])
    noton = _typelist(rec, "NotGameOption") or ([_txt(rec, "NotGameOption")] if _txt(rec, "NotGameOption") else [])
    return [x for x in on if x], [x for x in noton if x]


def _set_fam(fams, family, scope, member, unit, value):
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    if member:
        node = node.setdefault(member, OrderedDict())
    node[unit] = value


def _gold_cost_to_maintenance(fams):
    """gold UPKEEP -> maintenance (owner ruling 2026-06-20; DEC-maintenance-bookkeeping). A building's UNCONDITIONAL
    negative gold-commerce is its gold COST: legacy charges it to MAINTENANCE (TREAT_NEGATIVE_GOLD_AS_MAINTENANCE /
    calculateBuildingMaintenanceTimes100), NEVER to gold commerce. So move that negative out of the `gold` family into
    the `maintenance` family as a flat cost (a POSITIVE amount) — maintenance is a separate bookkeeping channel,
    OUTSIDE the commerce chain. Positive gold stays as commerce; CONDITIONAL gold (dict deposits, e.g. age-gated
    CommerceChangeDoubleTimes) is left in place — legacy's maintenance gate reads the static base getBuildingCommerce,
    not conditionals."""
    gold = fams.get("gold")
    if not isinstance(gold, dict):
        return
    city = gold.get("city")
    if not isinstance(city, dict) or "flat" not in city:
        return
    flat = city["flat"]
    cost = 0
    if isinstance(flat, (int, float)):
        if flat < 0:
            cost = -flat
            del city["flat"]
    elif isinstance(flat, list):
        cost = sum(-it for it in flat if isinstance(it, (int, float)) and it < 0)
        if cost:
            keep = [it for it in flat if not (isinstance(it, (int, float)) and it < 0)]
            if keep:
                city["flat"] = keep[0] if len(keep) == 1 else keep
            else:
                del city["flat"]
    if not cost:
        return
    if not city:
        del gold["city"]
    if not gold:
        del fams["gold"]
    _set_fam(fams, "maintenance", "city", None, "flat", cost)


# Reserved (non-family) top-level sections; everything else a curated building carries is a modifier family
# (json.md §1). Used to lift a relic shell's modifier tree into a source building's `whenObsolete`.
_RESERVED_TOPLEVEL = frozenset((
    "type", "identity", "cost", "ui", "world", "sound", "ai",
    "enables", "obsoletes", "obsoletedBy", "replaces", "disables",
    "requires", "allowed", "grants", "triggers", "provides",
    "skills", "tags", "state", "attributes", "capabilities",
    "shrine", "headquarters", "enabled", "disabled", "whenObsolete",
    "description", "help", "civilopedia", "strategy", "adjective",
    "shortDescription", "quote", "message",
))


def _families_of(building_json):
    # the modifier-family sections of a curated building (reserved sections dropped) -- json.md §1.
    return OrderedDict((k, v) for k, v in building_json.items() if k not in _RESERVED_TOPLEVEL)


def curate(typ, rec, store):
    out = OrderedDict([("type", typ)])
    for tag, key in TEXT.items():
        t = _txt(rec, tag)
        if t:
            out[key] = t

    fams = OrderedDict()
    identity = OrderedDict()
    capabilities = OrderedDict()   # PROVIDED to the empire (commerce sliders, json §8) — populated in pass2
    cost = OrderedDict()
    art_blocks = OrderedDict()
    ai = OrderedDict()

    # per-pop per-scaler (ruling 4, info-rebuild.md): the verified ×pop÷100 fields author as flat + this per
    # (fixed-point-and-scales.md §4c: the raw legacy value is human per-100-pop; each:100 carries the quantum).
    per_100_pop = OrderedDict([("type", "POPULATION"), ("each", 100)])
    # --- scalar/percent families ---
    for tag, (family, scope, member, unit) in SCALAR_FAMILIES.items():
        v = _int(rec, tag)
        if v:
            if unit == "perPopulation":
                _inject_per(fams, family, scope, "flat", v, per_100_pop)
            else:
                _set_fam(fams, family, scope, member, unit, v)
    # --- yield/commerce split families ---
    for tag, spec in YIELD_FAMILIES.items():
        node = rec.find(tag)
        if node is None:
            continue
        scope, keys, unit = spec[0], spec[1], spec[2]
        for member, v in engine.named_array(node, keys).items():
            # per-pop (Yield/CommercePerPopChanges): engine adds the RAW value ×pop into the ×100-space rate
            # (CvCity.cpp:12346 / getExtraYield100) -> human effect = value × pop / 100 -> a §3.7 per-scaler
            # {flat: value, per:{POPULATION, each:100}} (ruling 4; replaces the old descale100+perPopulation unit).
            if unit == "perPopulation":
                _inject_per(fams, member, scope, "flat", v, per_100_pop)
            else:
                _set_fam(fams, member, scope, None, unit, v)   # member IS the family (split)
    # +commerce per specialist of ALL types (legacy getSpecialistExtraCommerce; each city multiplies by ITS total
    # specialist count, CvCity.cpp:11810) -> <c>.empire.flat {value, per:"SPECIALIST"} (ruling 4 + json.md §3.7
    # bare-string sugar). UNIFORM with civic/trait. NB the count the engine takes is CITY-local.
    sec_node = rec.find("SpecialistExtraCommerces")
    if sec_node is not None:
        for member, v in engine.named_array(sec_node, engine.COMMERCES).items():
            _inject_per(fams, member, "empire", "flat", v, "SPECIALIST")
    # iOtherAreaMaintenanceModifier -> ordinary conditioned percent deposit on the IS_HOME_AREA predicate
    # (ruling 2; json.md §3.5: "other areas" = the plain negation). NB the legacy engine apply is "every area
    # other than the BUILDING's own" (CvPlayer.cpp:7440-7448); zero buildings author the field today, and the
    # owner-ruled shape is the capital-relative predicate.
    oam = _int(rec, "iOtherAreaMaintenanceModifier")
    if oam:
        _inject_cond(fams, "maintenance", "empire", "percent", oam, "!IS_HOME_AREA")
    # tradeRoutes conditioned variants (ruling 11): coastal routes -> the MEMBERLESS route count gated
    # HAS_COAST; the foreign route-yield % -> modifier kind gated IS_FOREIGN (engine: getTeam() != other team,
    # CvCity.cpp:11539-11541).
    ctr = _int(rec, "iCoastalTradeRoutes")
    if ctr:
        _inject_cond(fams, "tradeRoutes", "empire", "flat", ctr, "HAS_COAST")
    ftr = _int(rec, "iForeignTradeRouteModifier")
    if ftr:
        _inject_cond(fams, "tradeRoutes", "city", "percent", ftr, "IS_FOREIGN", "modifier")

    # --- enables / obsoletes / replaces (store-derived; COPIED so pass2 can extend FoundsCorporation/ObsoletesToBuilding) ---
    enables = OrderedDict((k, list(v)) for k, v in (store.enabled_by(typ) or {}).items())
    # supersession, TARGET-side (owner ruling 2026-06-22): authored on THIS (the superseded) building, read off
    # its OWN fields, no store inversion. obsoletedBy = obsoleting tech (ObsoleteTech) + superseding building
    # (ObsoletesToBuilding). NB `ReplacementBuildings` is NOT authored as `replacedBy` here -- it is reversible
    # DORMANCY in the engine (setDisabledBuilding, not removal), so it lands on `requires.operate.dormant` (above);
    # the `replaces` enabler edge stays a defined concept but is now UNUSED. The cascade builds the obsoletion
    # reverse map (superseder -> [superseded]) at load — never stored in the JSON.
    obsoleted_by = OrderedDict()
    _ot = _txt(rec, "ObsoleteTech")
    if _ot:
        obsoleted_by["techs"] = [_ot]
    _otb = _txt(rec, "ObsoletesToBuilding")
    when_obsolete = None
    if _otb:
        _otb_rec = store.table("BuildingInfo").get(_otb)
        # A NON-CONSTRUCTIBLE ObsoletesToBuilding target is a RELIC SHELL (the 6 wonder relics; owner 2026-07-07):
        # the source keeps a REDUCED output once obsolete, so the relic's OWN modifier tree becomes the source's
        # `whenObsolete` (a separate full modifier tree, json §4.2 / enabler §2) -- NOT `obsoletedBy.buildings`
        # (a relic is not a buildable superseder). A CONSTRUCTIBLE target is a real upgrade tier and stays the
        # obsoletedBy supersession edge. (Non-constructible == the curator's iCost -1/absent gate, line ~1318.)
        if _otb_rec is not None and _int(_otb_rec, "iCost") in (None, -1):
            when_obsolete = _families_of(curate(_otb, _otb_rec, store)) or None
        else:
            obsoleted_by["buildings"] = [_otb]
    # SpecialBuilding GROUP obsoletion inherited onto the MEMBER, target-side (owner 2026-06-22: put the group's
    # ObsoleteTech on the monastery itself; double-representation with the store's source-side _inherit_group_obsoletes
    # is fine). The target-side cascade reads the member's obsoletedBy, not the tech's obsoletes, so author it here —
    # this is exactly the edge my target-side migration orphaned when it stopped reading store.obsoletes_of(typ).
    _sbt = _txt(rec, "SpecialBuildingType")
    if _sbt:
        _grp = store.table("SpecialBuildingInfo").get(_sbt)
        _grp_ot = _txt(_grp, "ObsoleteTech") if _grp is not None else None
        if _grp_ot and _grp_ot != "NONE":
            obsoleted_by.setdefault("techs", []).append(_grp_ot)

    grants = OrderedDict()
    triggers = []   # top-level `triggers` array (ruling 8, json.md §5): trigger -> chance -> action entries
    # provides: bonuses this building SUPPLIES while active (XML ExtraFreeBonuses) -- a vicinity-bonus source,
    # uniform with a map bonus that provides itself. The cascade's vicinity check unions plot bonuses + active
    # buildings' provides.bonuses.
    provides = OrderedDict()
    _pb = _extra_free_bonuses(rec)
    if _pb:
        provides["bonuses"] = _pb
    requires = requires_building(rec, store)
    allowed = allowed_building(rec)
    gate_on, gate_off = gate_building(rec)
    # EnabledCivilizationTypes -> identity whitelist (NPC-only gate; dry-calc ignores it; remodel post-rework).
    _civs = _typelist_struct(rec, "EnabledCivilizationTypes", "CivilizationType")
    if _civs:
        identity["enabledCivilizations"] = _civs

    # --- PASS 2: keyed inversions (§6.1), properties, triggers entries, one-shot grants, enables-from-XML ---
    bespoke = OrderedDict()   # top-level bespoke sections (shrine/headquarters, json §9)
    pass2(typ, rec, store, fams, grants, triggers, identity, enables, capabilities, bespoke)

    # gold UPKEEP -> maintenance (DEC-maintenance-bookkeeping): a building's unconditional negative gold-commerce is
    # its gold COST, which legacy charges to MAINTENANCE (TREAT_NEGATIVE_GOLD_AS_MAINTENANCE), not gold commerce.
    _gold_cost_to_maintenance(fams)

    # --- cost ---
    for tag, key in COST.items():
        v = _int(rec, tag)
        if v is not None and v != -1 and v != 0:
            cost[key] = v
    # iHurryCostModifier = "hurrying ME costs X% more/less" (CvCity.cpp:6047, the per-entity base modifier of
    # the hurry-cost calc) -> the entity's OWN cost data (ruling 18 plane 1), key `hurryModifier` (curator-chosen;
    # a percent modifier beside the flat production cost).
    hcm = _int(rec, "iHurryCostModifier")
    if hcm:
        cost["hurryModifier"] = hcm

    # legacy iCost == -1 (or absent -> the XML read default is -1, CvBuildingInfo.cpp:1764) = NOT player-constructible
    # (the CvPlayer::canConstruct getProductionCost()==-1 gate, CvPlayer.cpp:6667). Such buildings are instantiated by
    # OTHER systems -- autobuild-on-condition, property spawn, outcome missions, GP/event relics, doctrine toggles --
    # never built via the city production queue. Translate the dumb sentinel into an explicit clean flag (the building
    # twin of the unit identity.spawnOnly); the cascade gates buildability on this, never on a raw -1 cost. NB this is
    # the buildability GATE only -- it is distinct from identity.autoBuild (a PLACEMENT behavior: "auto-place me where
    # my requires holds"), which overlaps but is not the same.
    if _int(rec, "iCost") in (None, -1):
        identity["notConstructible"] = True
        # Queue-excluded => placement is unconditional and DORMANCY decides everything, so `requires.build` has no
        # consumer left and its clauses must move where they are still read (see fold_build_into_operate).
        requires = fold_build_into_operate(requires)

    # --- attributes (HELD city-scope intrinsics, json §8) + the remaining identity markers ---
    attributes = OrderedDict()
    for tag, name in CAP_ATTRIBUTES.items():
        if _bool(rec, tag):
            attributes[name] = True
    # iWorkableRadius -> the `adds3rdRing` ATTRIBUTE (owner). The legacy field is a numeric OVERRIDE of the
    # city radius, but it carries no information: every one of the 12 authorings is exactly 3, and the city
    # radius is PURE STATE driven by culture expansion (culture grants 2 at the low tiers, 3 from ILLUSTRIOUS),
    # so what a building actually says is the boolean "this city gets the third ring early". Held, immutable,
    # city-scope -> `attributes` (json.md par.8). ⚑ Only METROPOLITAN_ADMINISTRATION (renaissance) does real
    # work with it; the other 11 are transhuman-and-later, by which point culture already grants 3.
    if (_int(rec, "iWorkableRadius") or 0) >= 3:
        attributes["adds3rdRing"] = True
    for tag, name in CAP_IDENTITY.items():
        if _bool(rec, tag):
            identity[name] = True
    # bNoHolyCity is a BUILD gate, NOT an attribute (verified CvCity.cpp:2591: canConstruct returns false when
    # kBuilding.isNoHolyCity() && isHolyCity() -- "can't be built IN a holy city"), authored in requires_building
    # as requires.build.disabled: IS_HOLY_CITY.
    # bApplyFreePromotionOnMove -> a grants pulse (folded with FreePromoTypes), authored in pass2.
    # bDamageAllAttackers + MayDamageAttackingUnitCombatTypes -> the defense counter-damage target, authored in pass2.
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
    if obsoleted_by:
        out["obsoletedBy"] = OrderedDict((k, obsoleted_by[k]) for k in sorted(obsoleted_by))
    if when_obsolete:
        out["whenObsolete"] = when_obsolete   # the relic shell's modifier tree (json §4.2) -- fires for the 6 wonder relics
    if requires:
        out["requires"] = requires
    if allowed:
        out["allowed"] = allowed
    if provides:
        out["provides"] = provides
    ordered = [f for f in FAMILY_ORDER if f in fams] + [f for f in fams if f not in FAMILY_ORDER]
    for f in ordered:
        out[f] = fams[f]
    if grants:
        out["grants"] = grants
    if triggers:
        out["triggers"] = triggers                # trigger -> chance -> action entries (json §5)
    for k in ("shrine", "headquarters"):          # top-level bespoke FK sections (json §9)
        if k in bespoke:
            out[k] = bespoke[k]
    if cost:
        out["cost"] = cost
    if ai:
        out["ai"] = ai
    gate_entity(out, gate_on, gate_off)
    if attributes:
        out["attributes"] = attributes           # BUILDING held city-scope intrinsics (json §8)
    if capabilities:
        out["capabilities"] = capabilities        # PROVIDED to the empire (commerce sliders, json §8)
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out


def curate_special(typ, rec, store):
    """SpecialBuilding (#31) — a per-player-capped building GROUP (getMaxPlayerInstances, enforced by
    isBuildingGroupMaxedOut/getBuildingGroupCount). Buildings join it via their SpecialBuildingType FK
    (identity.specialBuildingType). TechPrereq/TechPrereqAnyone -> store (tech.enables.specialBuildings), dropped here.
    ObsoleteTech -> `obsoletedBy.techs` (the target-side edge, json.md §4.2 — the same shape a BUILDING authors, read
    off the generic edge dispatch). It is ALSO inherited onto the member buildings by store._inherit_group_obsoletes,
    which is what drives the gameplay retire (CvTeam::setHasTech); this group-level edge is what the GROUP's own
    getObsoleteTech reads, so the pedia's "Obsolete with <tech>" line resolves instead of blanking."""
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
    obs_tech = engine.text(rec.find("ObsoleteTech"))
    if obs_tech and obs_tech != "NONE":
        out["obsoletedBy"] = OrderedDict([("techs", [obs_tech])])
    cap = _int(rec, "iMaxPlayerInstances")
    if cap is not None and cap >= 0:
        out["allowed"] = OrderedDict([("empire", cap)])   # the per-player GROUP cap -> unified `allowed` idiom
    identity = OrderedDict()
    if engine.text(rec.find("bValid")) in ("0", "false", "False"):
        identity["valid"] = False                      # bValid=0 -> the group is disabled (rare)
    art_blocks = OrderedDict()
    put_art(art_blocks, "Button", engine.text(rec.find("Button")))
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7) -- the special-building path too
    return out


def _is_struct(node):
    return node is not None and len(node) > 0 and len(list(node)[0]) > 0


def _generic_list(rec, tag):
    node = rec.find(tag)
    if node is None:
        return []
    g = engine.generic(node)
    return g if isinstance(g, list) else ([g] if g else [])


HANDLED = (set(SCALAR_FAMILIES) | set(YIELD_FAMILIES) | set(CAP_ATTRIBUTES) | set(CAP_IDENTITY) | set(ID_SCALAR)
           | set(ID_LIST) | set(COST) | set(TEXT) | set(ART) | REQUIRES_TAGS | STORE_TAGS | DROP_DEAD | DROP_MODULE
           | PASS2_TAGS | {"Type", "Flavors", "iAIWeight"}
           # consciously routed/dropped, not in a bool table: noHolyCity -> requires.build.disabled;
           # damageAllAttackers -> defense.counterDamage; applyFreePromotionOnMove -> DROPPED (redundant, all
           # freePromotions are end-turn-stay; owner 2026-07-01). SpecialistExtraCommerces + the otherArea
           # maintenance modifier are handled explicitly in curate() (rulings 4 + 2).
           | {"bNoHolyCity", "bApplyFreePromotionOnMove", "bDamageAllAttackers",
              "SpecialistExtraCommerces", "iOtherAreaMaintenanceModifier",
              # ruling 11/18 explicit routings in curate(): conditioned tradeRoutes variants + the per-entity
              # hurry-cost modifier (cost.hurryModifier).
              "iCoastalTradeRoutes", "iForeignTradeRouteModifier", "iHurryCostModifier"})


# ============================ PROPERTY-BAND REALIGNMENT (owner 2026-06-23) ============================
# Property-effect "pseudobuildings" (the BuildingType under each PROPERTY's <PropertyBuildings>) are placed/removed
# by the property-band system (checkPropertyBuildings) on an [iMinValue, iMaxValue] band (data-model §2.1, enabler-
# spec §3). apply_property_bands authors a `requires.operate` PROPERTY-in-band atom {type:PROPERTY_X, scope:city,
# min, max} on each pseudobuilding -- the band is ACTIVE iff the city's property value is in [min, max]. Crime/
# disease/tourism/pollution bands carry no ReplacementBuildings, so every in-band band stays active (they COMPOUND).
#
# EDUCATION's 4 ladders (positive/negative era, argumentative-awareness, blissful-ignorance) ARE succession chains
# (legacy ReplacementBuildings) -- and that is now handled UNIFORMLY, with NO special case. ReplacementBuildings is
# reversible DORMANCY in the engine (setDisabledBuilding, CvCity.cpp:14413 -- NOT removal), so requires_building
# mirrors it as `requires.operate.dormant: successor`: a lower band dorms while a higher band is present => only the
# highest reached band is active, FULL per-band value, == the LEGACY engine (parity). The same mirror covers the
# pseudo->REAL case (BLACKENED_SKIES dorms the telescope/observatory; it is not nuked from orbit).
#
# ⛔ The earlier (2026-06-19/22) "pseudobuildings COMPOUND, NEVER replace -- strip replaces + increment-convert
# education + a banded only-highest disable" model is OBSOLETE (owner 2026-06-23): it rollerskated away, and its
# strip/ladder code was reading a `replaces` key the curator never emitted (dead). The uniform ReplacementBuildings
# -> requires.operate.dormant mirror replaces it and RESTORES education parity (the old ~1,100-building BY-DESIGN
# divergence is gone). Behavioural redesign (should education be only-highest at all?) is POST-migration, never here.

PROPERTY_INFOS_XML = os.path.join(REPO, "Assets", "XML", "GameInfo", "CIV4PropertyInfos.xml")
# top-level reserved (non-family) keys -- everything else on a band object is a modifier family to increment.
RESERVED_NONFAMILY = {"type", "description", "civilopedia", "help", "enables", "obsoletedBy", "replacedBy", "requires",
                      "allowed", "provides", "grants", "triggers", "cost", "ai", "enabled", "disabled", "ui", "world",
                      "sound", "identity", "attributes", "capabilities", "shrine", "headquarters"}


def property_band_buildings():
    """The set of BuildingType in any PROPERTY's <PropertyBuildings> -- the property-effect pseudobuildings."""
    pseudo = set()
    root = ET.parse(PROPERTY_INFOS_XML).getroot()
    for node in root.iter():
        if node.tag.split("}")[-1] != "PropertyBuilding":
            continue
        for ch in node:
            if ch.tag.split("}")[-1] == "BuildingType" and ch.text:
                pseudo.add(ch.text.strip())
    return pseudo


def property_band_atoms():
    """Map each property-effect pseudobuilding -> its band atom (PROPERTY_X, iMinValue, iMaxValue) from the XML
    <PropertyBuilding>. The atom makes the band CUMULATIVELY active when the city's property value is in the band
    (owner 2026-06-19: pseudobuildings compound, set up by `requires`, never `replace`)."""
    atoms = {}
    root = ET.parse(PROPERTY_INFOS_XML).getroot()
    bare = lambda n: n.tag.split("}")[-1]
    for pi in root.iter():
        if bare(pi) != "PropertyInfo":
            continue
        ptype = next((ch.text.strip() for ch in pi if bare(ch) == "Type" and ch.text), None)
        if not ptype:
            continue
        for pbs in pi:
            if bare(pbs) != "PropertyBuildings":
                continue
            for pb in pbs:
                if bare(pb) != "PropertyBuilding":
                    continue
                d = {bare(x): (x.text.strip() if x.text else "") for x in pb}
                bt = d.get("BuildingType")
                if not bt:
                    continue
                lo = int(d["iMinValue"]) if d.get("iMinValue") not in (None, "") else None
                hi = int(d["iMaxValue"]) if d.get("iMaxValue") not in (None, "") else None
                atoms[bt] = (ptype, lo, hi)
    return atoms


def _set_requires(obj, req):
    """Place/replace `requires` in its canonical slot (after enables/obsoletes/replaces/disables, before families)."""
    LEADING = ("type", "description", "civilopedia", "help", "enables", "obsoletedBy", "replacedBy", "disables")
    out = OrderedDict()
    inserted = False
    for k, v in obj.items():
        if k == "requires":
            continue
        if not inserted and k not in LEADING:
            out["requires"] = req
            inserted = True
        out[k] = v
    if not inserted:
        out["requires"] = req
    return out


def apply_property_bands(results, pseudo):
    """Author the requires.operate PROPERTY-in-band atom on every property-effect pseudobuilding -- the band is
    ACTIVE iff the city's property value is in [iMinValue, iMaxValue] (data-model §2.1, enabler-spec §3). The
    only-highest education ladders + the pseudo->REAL disable (blackened-skies dorms the observatory) are NOT
    special-cased here: their legacy `ReplacementBuildings` is reversible DORMANCY (engine `setDisabledBuilding`,
    CvCity.cpp:14413 -- never removal), already mirrored as `requires.operate.dormant` by requires_building
    (owner 2026-06-23, engine-verified; the prior 2026-06-19/22 'pseudobuildings compound, never replace' ruling
    is OBSOLETE). Returns n_banded; mutates results."""
    band_atoms = property_band_atoms()   # building -> (ptype, iMinValue, iMaxValue)
    # Author the requires.operate PROPERTY-in-band atom on every pseudobuilding -- ACTIVE iff the city's property
    # value is in [iMinValue, iMaxValue]. Merged into requires.operate.all, preserving any requires.build tech gate
    # + the requires.operate.dormant emitted by requires_building (the only-highest / pseudo->real dormancy mirror).
    banded = 0
    for b, (ptype, lo, hi) in band_atoms.items():
        obj = results.get(b)
        if not obj:
            continue
        atom = OrderedDict([("type", ptype), ("scope", "city")])
        if lo is not None:
            atom["min"] = lo
        if hi is not None:
            atom["max"] = hi
        req = obj.get("requires")
        req = OrderedDict(req) if isinstance(req, dict) else OrderedDict()
        op = OrderedDict(req.get("operate")) if isinstance(req.get("operate"), dict) else OrderedDict()
        allc = list(op.get("all")) if isinstance(op.get("all"), list) else []
        allc.append(atom)
        op["all"] = allc
        req["operate"] = op
        results[b] = _set_requires(obj, req)
        banded += 1
    return banded


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
    # Drop the shells: an entity that produces no effect, unlocks nothing and is named by nothing is dead
    # weight -- loaded resident, listed in the manifest, offered in the build list and scored by the AI, all to
    # do nothing. Fail-closed + reference-guarded + announced (curate_common.skip_inert).
    skip_inert(results, store, "buildings")
    n = len(results)
    # SpecialBuilding #31 rides this pass (the per-player-capped GROUP).
    sb_results = OrderedDict((typ, curate_special(typ, rec, store))
                             for typ, rec in store.table("SpecialBuildingInfo").items())

    # PROPERTY-BAND realignment (owner 2026-06-23): author the requires.operate PROPERTY-in-band atom on every
    # pseudobuilding. The only-highest education ladders + the pseudo->REAL dormancy (blackened-skies) are NOT
    # special-cased here -- they ride the uniform `ReplacementBuildings -> requires.operate.dormant` mirror.
    pseudo = property_band_buildings()
    n_band = apply_property_bands(results, pseudo)
    print("PROPERTY BANDS: %d pseudobuildings | %d got requires.operate band atom" % (len(pseudo), n_band))

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
              "allowed", "provides", "cost", "ai", "enabled", "disabled", "ui", "world", "sound", "identity",
              "attributes", "capabilities", "shrine", "headquarters", "grants", "triggers", "obsoletedBy"}
    seen = sorted({f for o in results.values() for f in o if f not in STRUCT})
    print("BuildingInfo curated: %d" % n)
    for k in ("enables", "obsoletes", "replaces", "requires", "allowed", "cost", "ai", "identity"):
        print("  with %-9s: %d" % (k, has(k)))
    print("  families seen: %s" % ", ".join(seen))
    print("SpecialBuildingInfo curated: %d  (with cap: %d)"
          % (len(sb_results), sum(1 for o in sb_results.values() if o.get("allowed") is not None)))
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
        wipe_entity_json(base, recurse=True)   # drop-before-rewrite: stale/dropped types don't linger (keeps _order.json)
        for typ, obj in results.items():
            folder = os.path.join(base, era_of.get(typ, "none"))
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        sbdir = os.path.join(REPO, "Assets", "Data", "specialbuildings")
        os.makedirs(sbdir, exist_ok=True)
        wipe_entity_json(sbdir, recurse=False)
        for typ, obj in sb_results.items():
            with open(os.path.join(sbdir, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        print("\nwrote %d BuildingInfo + %d SpecialBuildingInfo JSON files under Assets/Data" % (n, len(sb_results)))


if __name__ == "__main__":
    main()
