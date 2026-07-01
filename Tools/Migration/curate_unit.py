#!/usr/bin/env python3
"""Curate Unit (#428, Tier E #34, THE LAST MONSTER) + SpecialUnit (#33, rides this pass).

The TARGET of everything (tech/building-prereq/bonus/religion/civic/promotion/unitcombat). But its gnarliness is
largely RESOLVED: the combat/capability/vision/heal/cargo surface is the DEFINED shared §5 vocabulary (Promotion #28 /
UnitCombat #29) — this curator REUSES the family NAMES/members (the unit deposits at `unit` scope, the self-accumulator
§5). CvUnitInfo has NO getDataMembers (legacy read()), so the field inventory is the live XML (2073 records, 219 tags).

THE BASE/DEPOSIT SPLIT (owner §0.6 + ranking #34: "base -> identity, deltas are modifiers"):
- **identity.base** = the create-unit FOUNDATION the subroutine sets: `iCombat` (base strength), `iMoves`, `iWorkRate`,
  `iAirCombat` (base air strength), `iCargo` (base cargo capacity), `iCombatLimit`. ⚑ The base/deposit boundary is a
  judgement — flagged for owner inspection.
- **§5 unit-scope FAMILIES** = the combat TRAITS the unit contributes (summed with promotions): strength(cityAttack/
  hillsAttack/vsBarbs/attack/defense/perSize/perVolume/lunge/enclose/unnerve/…) + withdrawal/firstStrike/bombard/
  collateral/air/capture/espionage/heal. Same vocab as Promotion (the *Change suffix dropped on the unit).

PASS 1 (this file): identity.base + the §5 scalar families + requires.build + store enables/obsoletes + cost
(+iInstanceCostModifier -> costs.empire.perInstance per:{SELF}) + grants + succession (upgradesTo) + capabilities + ai +
COVERAGE CHECK. PASS 2 (deferred, shows as UNHANDLED): vs-keyed combat (Terrain/Feature/UnitCombat/Domain/Unit mods),
the vision/LOS resolver, KillOutcomes/Actions -> `outcomes` (faithful, like UnitCombat; defs = CvOutcome Tier-G),
GP-action magnitudes (discover/hurry/trade/greatWork -> grants/outcomes), PropertyManipulators, BonusProductionModifiers.

SpecialUnit #33 (curate_special_unit): cargo-load rules (bValid/bCityLoad/bSMLoadSame) + any combat deposits onto the
loaded unit. Newly registered in store.ENTITIES.

  python3 curate_unit.py --sample UNIT_WARRIOR UNIT_AXEMAN UNIT_GREAT_ENGINEER
  python3 curate_unit.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import boolexpr
from curate_common import put_art, emit_art, FAMILY_ORDER, de_i, fold_text_to_identity
from store import Store, REPO

# ---- identity.base: the create-unit FOUNDATION (§0.6) ----
BASE = {
    "iCombat": "combat", "iMoves": "moves", "iWorkRate": "workRate", "iAirCombat": "airCombat",
    "iCombatLimit": "combatLimit", "iAirCombatLimit": "airCombatLimit",
    "iAirUnitCap": "airUnitCap",
}
# ---- §5 unit-scope combat-trait families (tag -> (family, member|None, unit)). REUSES the Promotion §5 vocab. ----
UNIT_FAMILIES = {
    "iCityAttack": ("strength", "cityAttack", "percent"),
    "iCityDefense": ("strength", "cityDefense", "percent"),
    "iHillsAttack": ("strength", "hillsAttack", "percent"),
    "iHillsDefense": ("strength", "hillsDefense", "percent"),
    "iVSBarbs": ("strength", "vsBarbs", "percent"),
    "iAttackCombatModifier": ("strength", "attack", "percent"),
    "iDefenseCombatModifier": ("strength", "defense", "percent"),
    "iCombatModifierPerSizeMore": ("strength", "perSizeMore", "percent"),
    "iCombatModifierPerSizeLess": ("strength", "perSizeLess", "percent"),
    "iCombatModifierPerVolumeMore": ("strength", "perVolumeMore", "percent"),
    "iCombatModifierPerVolumeLess": ("strength", "perVolumeLess", "percent"),
    "iLunge": ("strength", "lunge", "percent"),
    "iEnclose": ("strength", "enclose", "percent"),
    "iUnnerve": ("strength", "unnerve", "percent"),
    "iDynamicDefense": ("strength", "dynamicDefense", "percent"),
    "iStealthStrikes": ("strength", "stealthStrikes", "flat"),
    "iStealthCombatModifier": ("strength", "stealth", "percent"),
    "iBreakdownChance": ("strength", "breakdownChance", "flat"),
    "iBreakdownDamage": ("strength", "breakdownDamage", "flat"),
    "iWithdrawalProb": ("withdrawal", None, "percent"),
    "iFirstStrikes": ("firstStrike", "strikes", "flat"),
    "iChanceFirstStrikes": ("firstStrike", "chance", "flat"),
    "iBombardRate": ("bombard", "rate", "percent"),
    "iRBombardDamage": ("bombard", "rangedDamage", "flat"),
    "iRBombardDamageLimit": ("bombard", "rangedDamageLimit", "flat"),
    "iDCMBombRange": ("bombard", "dcmRange", "flat"),
    "iDCMBombAccuracy": ("bombard", "dcmAccuracy", "flat"),
    "iBombRate": ("bombard", "airBombRate", "flat"),
    "iCollateralDamage": ("collateral", "damage", "percent"),
    "iCollateralDamageLimit": ("collateral", "limit", "flat"),
    "iCollateralDamageMaxUnits": ("collateral", "maxUnits", "flat"),
    "iAirRange": ("range", None, "flat"),   # the top-level `range` family (modifier.md §6.6) -- air-only today (the
                                            # universal siege=1 is deferred: AI runs ground ranged combat poorly).
    "iInterceptionProbability": ("air", "intercept", "percent"),
    "iEvasionProbability": ("air", "evasion", "percent"),
    "iNukeRange": ("air", "nukeRange", "flat"),
    "iCaptureProbabilityModifier": ("capture", "probability", "flat"),
    "iCaptureResistanceModifier": ("capture", "resistance", "flat"),
    "iInsidiousness": ("espionage", "insidiousness", "flat"),
    "iInvestigation": ("espionage", "investigation", "flat"),
    "iNumHealSupport": ("heal", "support", "flat"),
    "iDropRange": ("movement", "dropRange", "flat"),
    "iCultureGarrison": ("culture", "garrison", "flat"),
    # iCargo is NOT a plain family entry — it combines with DomainCargo into cargo.space.{unit, flat} (see pass2).
}
# ---- capabilities (separate boolean group) — Unit's bools (some shared with Promotion's CAP vocab). ----
CAP_BOOL = {
    "bAlwaysHostile": "alwaysHostile", "bAssassin": "assassin", "bHiddenNationality": "hiddenNationality",
    "bNoCapture": "noCapture", "bNoDefensiveBonus": "noDefensiveBonus", "bPillage": "pillage",
    "bStampede": "stampede", "bBlendIntoCity": "blendIntoCity", "bBarbCoExist": "barbCoExist",
    "bCanMoveAllTerrain": "canMoveAllTerrain", "bCanMoveImpassable": "canMoveImpassable",
    "bCounterSpy": "counterSpy", "bDestroy": "destroy", "bFirstStrikeImmune": "firstStrikeImmune",
    "bFlatMovementCost": "flatMovementCost", "bFood": "food", "bFound": "found", "bGoldenAge": "goldenAge",
    "bGreatGeneral": "greatGeneral", "bIgnoreBuildingDefense": "ignoreBuildingDefense",
    "bIgnoreTerrainCost": "ignoreTerrainCost", "bIgnoreZoneofControl": "ignoreZoneOfControl",
    "bInquisitor": "inquisitor", "bInvestigate": "investigate", "bInvisible": "alwaysInvisible",
    "bMechanized": "mechanized", "bNoBadGoodies": "noBadGoodies", "bNoNonOwnedCityEntry": "noNonOwnedCityEntry",
    "bNoNonTypeProdMods": "noNonTypeProdMods", "bNukeImmune": "nukeImmune", "bOnlyDefensive": "onlyDefensive",
    "bPassage": "passage", "bRBombardForceAbility": "rBombardForceAbility", "bRivalTerritory": "rivalTerritory",
    "bSabotage": "sabotage", "bStateReligion": "stateReligion", "bStealPlans": "stealPlans",
    "bStealthDefense": "stealthDefense", "bSuicide": "suicide", "bUnlimitedException": "unlimitedException",
    "bUpgradeAnywhere": "upgradeAnywhere", "bWorkerTrade": "workerTrade", "bAttackOnlyCities": "attackOnlyCities",
    "bIgnoreNoEntryLevel": "ignoreNoEntryLevel", "bFliesToMove": "fliesToMove", "bFreeDrop": "freeDrop",
    "bDCMFighterEngage": "dcmFighterEngage", "bRenderBelowWater": "renderBelowWater",
    "bMilitaryHappiness": "militaryHappiness", "bMilitaryProduction": "militaryProduction",
    "bMilitarySupport": "militarySupport", "bMilitaryTrade": "militaryTrade",
}
# count-int capabilities (>0 -> has it)
CAP_COUNT = {"iAnimalIgnoresBorders": "animalIgnoresBorders"}
# DCM air-bomb tier bools -> capabilities.dcmAirBomb (the highest set tier)
DCM_AIRBOMB = ["bDCMAirBomb1", "bDCMAirBomb2", "bDCMAirBomb3", "bDCMAirBomb4", "bDCMAirBomb5"]
# ---- tags: a unit's IMMUTABLE, accounting-only classification (json.md §8). DERIVED, greenfield — there is NO
# legacy tag boolean (it is a NEW concept; IS_MILITARY had to be *detected*). FIRST PASS: `military` from the
# IS_MILITARY signal (bMilitarySupport, owner-verified); civilian roles from DefaultUnitAI; a role tag rides with
# its category (worker -> worker+civilian). Incomplete is FINE (owner: low-risk, fixed during validation). The
# accounting foundation for the future unitcombat -> activeOn.unit:<tag> work (post-migration); mounted/gunpowder/
# mechanized etc. will be derived from unitcombats THEN, not here. ----
TAG_BY_UNITAI = {
    "UNITAI_WORKER":   ["worker", "civilian"], "UNITAI_WORKER_SEA": ["worker", "civilian"],
    "UNITAI_SETTLE":   ["settler", "civilian"], "UNITAI_MISSIONARY": ["missionary", "civilian"],
    "UNITAI_MERCHANT": ["merchant", "civilian"], "UNITAI_SPY": ["spy"],
    # `civilian` is opt-in for genuinely-civilian units (workers, merchants — owner); `UNITAI_SPY` = the actual spy
    # (only spies run espionage missions) but is NOT civilian. spy is a TAG ONLY (owner 2026-06-23: espionage isn't a
    # skill -- only the spy unit class gets it), so the legacy bSpy CAP no longer emits a `spy` skill (dropped above).
    # settler/missionary carry `civilian` provisionally (peaceful non-combatants) — confirm in validation.
    # `entertainer` (no clean signal) untagged is fine.
}
# The `outlaw` tag is NOT a DefaultUnitAI role — it is the criminal COMBAT CLASS (owner 2026-06-23): a unit is
# criminal-type iff its primary <Combat> is UNITCOMBAT_CRIMINAL **or** UNITCOMBAT_CRIMINAL is among its
# <SubCombatTypes>. This is BROADER than the old UNITAI_INFILTRATOR gate (13 units): it also catches OUTLAW (primary
# RUFFIAN + subcombat CRIMINAL), ASSASSIN/HASHISHIN (primary STRIKE_TEAM + subcombat CRIMINAL), CUTTHROAT, etc.
# (~21-24 total). Emitted from the combat signal beside the role tags (see curate()).
CRIMINAL_COMBAT = "UNITCOMBAT_CRIMINAL"

# ---- grants (one-shot, lists) ----
GRANT_LIST = {"FreePromotions": "promotions", "GreatPeoples": "greatPeople", "Builds": "builds",
              "GroupSpawnUnitCombatTypes": "groupSpawn", "ReligionSpreads": "religionSpreads",
              "CorporationSpreads": "corporationSpreads", "Buildings": "buildings"}
# ---- cost ----
COST = {"iCost": "production", "iBaseUpkeep": "upkeep", "iHurryCostModifier": "hurryCostModifier",
        "iInstanceCostModifier": None}  # iInstanceCostModifier -> costs.empire.perInstance per:{SELF} (special)
# ---- identity scalars / lists / config ----
ID_SCALAR = {"iAsset": "worth", "iPower": "militaryWorth", "iXPValueAttack": "xpValueAttack",
             "iXPValueDefense": "xpValueDefense", "iConscription": "conscription", "iAggression": "aggression",
             "iAnimalCombat": "animalCombat", "iCommandRange": "commandRange", "iControlPoints": "controlPoints",
             "iLeaderExperience": "leaderExperience", "iMinAreaSize": "minAreaSize",
             "Domain": "domain", "DefaultUnitAI": "defaultUnitAI", "FormationType": "formationType",
             "Special": "special", "Advisor": "advisor", "LeaderPromotion": "leaderPromotion",
             "ReligionType": "religion", "iEspionagePoints": "espionagePoints", "Capture": "captures"}
ID_LIST = {"UnitAIs": "unitAIs", "NotUnitAIs": "notUnitAIs", "SubCombatTypes": "combatClasses",
           "MapCategoryTypes": "mapCategories", "UniqueNames": "uniqueNames", "FeatureImpassableTypes": "featureImpassable",
           "TerrainImpassableTypes": "terrainImpassable", "DefendAgainstUnit": "defendAgainstUnit"}
ID_BOOL_GP = {}  # placeholder

# requires-feeding (read by requires_unit; listed for coverage)
REQUIRES_TAGS = {
    "PrereqTech", "TechTypes", "BonusType", "PrereqBonuses", "VicinityBonusType", "PrereqVicinityBonuses",
    "PrereqReligion", "PrereqCorporation", "PrereqOrCivics", "PrereqAndBuildings", "PrereqOrBuildings",
    "PrereqAndHeritage", "PrereqOrHeritage", "iMinAreaSize", "StateReligion", "bRequiresStateReligionInCity",
    "HolyCity", "Domain", "EnabledCivilizationTypes",
    "TrainCondition", "PrereqGameOption", "NotGameOption", "iMaxGlobalInstances", "iMaxPlayerInstances",
    "iMaxTeamInstances",
}
STORE_TAGS = {"ObsoleteTech"}
# DEFERRED to pass 2 (keyed/outcome/GP/property) — shown as deferred in coverage.
PASS2_TAGS = {
    "TerrainAttacks", "TerrainDefenses", "FeatureAttacks", "FeatureDefenses", "UnitCombatMods", "DomainMods",
    "FlankingStrikes", "FlankingStrikesbyUnitCombat", "UnitAttackMods", "UnitDefenseMods", "UnitCombatTargets",
    "UnitCombatDefenders", "UnitCombatCollateralImmunes", "UnitTargets", "DefendAgainstUnit", "DomainCargo",
    "SpecialCargo", "SMNotSpecialCargo", "HealUnitCombatTypes", "PropertyManipulators", "BonusProductionModifiers",
    "KillOutcomes", "Actions", "Action", "Invisible", "SeeInvisible", "InvisibilityIntensityTypes",
    "VisibilityIntensityTypes", "InvisibleFeatureChanges", "InvisibleTerrainChanges", "FeaturePassableTechs",
    "TerrainPassableTechs", "UnitUpgrades", "SupersedingUnits", "UnitCombatMod",
    "iBaseDiscover", "iDiscoverMultiplier", "iBaseHurry", "iHurryMultiplier", "iBaseTrade", "iTradeMultiplier",
    "iGreatWorkCulture", "iBaseFoodChange", "GreatPeoples", "Heritage", "iAdvancedStartCost", "FlankingStrikesbyUnitCombat",
}
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Help": "help"}
ART = ("Button",)


def _txt(rec, tag):
    t = engine.text(rec.find(tag))
    return t if (t and t != "NONE") else None


def _int(rec, tag):
    t = engine.text(rec.find(tag))
    return int(t) if engine.is_int(t) else None


def _bool(rec, tag):
    return engine.text(rec.find(tag)) in ("1", "true", "True")

# ✅ DONE (owner 2026-06-16; GitHub #7): the SETTLER-grants-buildings edge — bFound units carry the NewCityFree set
# "into settling" as grants.foundBuildings (see found_buildings() below), each gated by its NewCityFree BoolExpr via the
# shared converter `boolexpr.py`. That same converter retrofits the parked building ConstructCondition + unit
# TrainCondition into requires.build. The capital (bCapital -> Palace) is a foundBuildings entry gated {type:CITY,
# scope:empire, max:0} (first city only). Map + rationale: migration-renames "BoolExpr converter + settler-grants".


def _typelist(rec, wrapper):
    node = rec.find(wrapper)
    if node is None:
        return []
    return [t for t in (engine.text(c).strip() for c in node) if t and t != "NONE"]


def _typelist_struct(rec, wrapper, keytag):
    node = rec.find(wrapper)
    if node is None:
        return []
    out = []
    for c in node:
        k = engine.text(c.find(keytag)) if c.find(keytag) is not None else engine.text(c)
        if k and k != "NONE":
            out.append(k)
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
    # Collapse a plain presence to a BARE STRING -- the parser implies scope from the ID's domain, so a redundant
    # {type, scope} only invites authoring bugs (owner 2026-06-23). Object form ONLY for a special case: any kw
    # (connection/role/min/max), a non-default scope, or a plot-substrate predicate type
    # (TERRAIN_/FEATURE_/IMPROVEMENT_/MAPCATEGORY_) the parser routes to a plot predicate by the `type` key.
    is_plot_pred = isinstance(typ, str) and typ.startswith(("TERRAIN_", "FEATURE_", "IMPROVEMENT_", "MAPCATEGORY_"))
    if not kw and not is_plot_pred and scope == _default_scope(typ):
        return typ
    a = OrderedDict([("type", typ), ("scope", scope)])
    a.update(kw)
    return a


def requires_unit(rec, store):
    """Unit `requires.build` ONLY (units are leaf actions — no operate/dormancy yet; future fuel = operate). Most
    SOURCE->unit edges are store-wired onto the source -> dropped here; this authors the unit's OWN means."""
    allc, anyc, none = [], [], []
    b = _txt(rec, "BonusType")
    if b:
        allc.append(_atom(b, "city", connection="trade|vicinity"))
    orb = _typelist_struct(rec, "PrereqBonuses", "BonusType")
    if orb:
        anyc.append([_atom(x, "city", connection="trade|vicinity") for x in orb])
    # Vicinity bonus = the engine's hasVicinityBonus (CvCity::canTrainInternal:2241 single, :2250 OR) -- the OBTAINED
    # level (owned+valid+connected), so it carries the `vicinity:"connected"` discriminator (json.md §3.4), IDENTICAL
    # to the building curator's VicinityBonus. (A bare `connection:"vicinity"` would loosely accept any radius tile.)
    vb = _txt(rec, "VicinityBonusType")
    if vb:
        allc.append(_atom(vb, "city", connection="vicinity", vicinity="connected"))
    ovb = _typelist_struct(rec, "PrereqVicinityBonuses", "BonusType")
    if ovb:
        anyc.append([_atom(x, "city", connection="vicinity", vicinity="connected") for x in ovb])
    for x in _typelist_struct(rec, "PrereqAndBuildings", "BuildingType"):
        allc.append(_atom(x, "city"))
    orbld = _typelist_struct(rec, "PrereqOrBuildings", "BuildingType")
    if orbld:
        anyc.append([_atom(x, "city") for x in orbld])
    for x in _typelist_struct(rec, "PrereqAndHeritage", "HeritageType") or _typelist(rec, "PrereqAndHeritage"):
        allc.append(_atom(x, "empire"))
    orher = _typelist_struct(rec, "PrereqOrHeritage", "HeritageType") or _typelist(rec, "PrereqOrHeritage")
    if orher:
        anyc.append([_atom(x, "empire") for x in orher])
    rel = _txt(rec, "PrereqReligion")
    if rel:
        allc.append(_atom(rel, "city"))
    corp = _txt(rec, "PrereqCorporation")
    if corp:
        # A UNIT's corp prereq needs the corp ACTIVE (engine CvCity::canTrainInternal:2186 isActiveCorporation) --
        # NOT merely present. {HAS_CORPORATION: X} evaluates ACTIVE; a bare CORPORATION_X would (wrongly, for a unit)
        # mean isHasCorporation/present, which is the BUILDING semantic (canConstruct:2631). So an executive in a city
        # that has the corp present-but-inactive is correctly NOT trainable.
        allc.append(OrderedDict([("HAS_CORPORATION", corp)]))
    orciv = _typelist_struct(rec, "PrereqOrCivics", "PrereqCivic") or _typelist(rec, "PrereqOrCivics")
    if orciv:
        anyc.append([_atom(x, "empire") for x in orciv])
    # State religion -- THREE distinct legacy fields, all BUILD gates (units have no operate; a trained unit never
    # goes dormant on a religion switch), mirroring the building curator's STATE_RELIGION handling:
    #   * bStateReligion (CvPlayer::canTrain:6498 player HAS a state religion + CvCity::isPlotTrainable:1935 the city
    #     HAS it) and bRequiresStateReligionInCity (CvCity::canTrainInternal:2232 the city HAS the state religion) both
    #     collapse to STATE_RELIGION_IN_CITY (the evaluator's predicate already means "player has one AND city has it").
    #   * StateReligion (m_iStateReligion, a SPECIFIC religion; CvPlayer::canTrain:6417 player's state religion == it)
    #     -> {STATE_RELIGION: RELIGION_X}.
    if _bool(rec, "bStateReligion") or _bool(rec, "bRequiresStateReligionInCity"):
        allc.append("STATE_RELIGION_IN_CITY")
    sr = _txt(rec, "StateReligion")
    if sr:
        allc.append(OrderedDict([("STATE_RELIGION", sr)]))
    # HolyCity (m_iHolyCity; CvCity::canTrainInternal:2190 isHolyCity) -- the city must be the holy city of that religion.
    hc = _txt(rec, "HolyCity")
    if hc:
        allc.append(OrderedDict([("IS_HOLY_CITY", hc)]))
    # iMinAreaSize (CvPlot::canTrain, city case): a DOMAIN_SEA unit needs the city adjacent to a sea-body of >= N tiles
    # (isCoastalLand(N)) -> {HAS_COAST:{minArea:N}}, same shape as a bWater building; any other domain needs the city's
    # LANDMASS to be >= N tiles (area()->getNumTiles()) -> AREA_SIZE. (The old blanket AREA_SIZE wrongly gated sea units
    # on land area and dropped their coastal requirement.)
    ms = _int(rec, "iMinAreaSize")
    if ms and ms > 0:
        if _txt(rec, "Domain") == "DOMAIN_SEA":
            allc.append(OrderedDict([("HAS_COAST", OrderedDict([("minArea", ms)]))]))
        else:
            allc.append(_atom("AREA_SIZE", "city", min=ms))
    elif _txt(rec, "Domain") == "DOMAIN_SEA":
        allc.append("HAS_COAST")   # sea unit, no min sea-body size -> coastal at the default threshold
    # --- tech prereqs -> build.all (AND only: the single PrereqAndTech + every TechTypes entry; units have NO
    # OR-tech — only techs themselves model alternate tech-tree paths, owner 2026-06-17). The SOURCE->unit enable
    # edge is ALSO store-wired (generation/frontier proposal), but that edge cannot encode the multi-tech AND;
    # the per-candidate CONFIRM lives here and the condition engine evaluates it via isHasTech (team-scope).
    # Legacy gate: CvPlayer::canTrain 6392 (PrereqAndTech) + 6407 (PrereqAndTechs). enabler-spec §12 retrofit. ---
    t = _txt(rec, "PrereqTech")
    if t:
        allc.append(_atom(t, "team"))
    for x in _typelist_struct(rec, "TechTypes", "PrereqTech"):
        allc.append(_atom(x, "team"))
    # GAME-OPTION gates as DECLARATIVE requires.build conditions (owner ruling 2026-06-25), NOT loadPrune: a bare
    # GAMEOPTION_X reference the cascade evaluates against the active options. This states the dependency clearly IN
    # THE UNIT JSON (e.g. an inquisitor's GAMEOPTION_RELIGION_INQUISITIONS) so a modder's new option-gated unit needs
    # NO engine special-case. PrereqGameOption (engine canEverTrain:6449 -- option must be ON) -> build.all;
    # NotGameOption (:6454 -- option must be OFF) -> build.noneOf.
    for x in (_typelist(rec, "PrereqGameOption") or ([_txt(rec, "PrereqGameOption")] if _txt(rec, "PrereqGameOption") else [])):
        if x and x != "NONE":
            allc.append(x)
    for x in (_typelist(rec, "NotGameOption") or ([_txt(rec, "NotGameOption")] if _txt(rec, "NotGameOption") else [])):
        if x and x != "NONE":
            none.append(x)
    # EnabledCivilizationTypes is NOT a train gate: CvCity::canTrain applies it ONLY to an isStronglyRestricted()
    # NPC civ (the Neanderthal whitelist, same mechanism as buildings, CvCity.cpp:2218). Real civs SKIP it. So it is
    # an identity whitelist (identity.enabledCivilizations, emitted in curate()), IGNORED by the dry-calc; remodel
    # post-rework (owner 2026-06-24). Was wrongly AND-ed into requires -> under-offered every real civ.
    # --- instance caps are NOT a requires SELF-atom (owner 2026-06-17): they move to the declarative `allowed`
    # cap (authored by allowed_unit() below). SELF leaves requires entirely; uniform with Building/Tech/CultureLevel.
    # enabler-spec §5a/§13.7. ---
    # --- TrainCondition BoolExpr -> build (checked at canTrain, CvCity.cpp:1961-1963). Folded via the shared
    # boolexpr converter (And/Or of Has over bonus/building + the one ATTRIBUTE_POPULATION>=N case). owner 2026-06-16. ---
    boolexpr.merge_into(boolexpr.convert_field(rec.find("TrainCondition")), allc, anyc, none)
    boolexpr.fold_or_groups(allc, anyc)   # OR-groups -> nested {any} under all (any = ||)
    build = OrderedDict()
    if allc:
        build["all"] = allc
    if none:
        build["noneOf"] = none
    # iNukeRange != -1 marks a nuke unit; the engine bars it while the no-nukes verdict holds (CvPlayer::canTrain:6488
    # !isNukesValid() && getNukeRange()!=-1) -- the UN ban, lifted once anyone builds Manhattan. Mirror the bAllowsNukes
    # building: a world-scope NO_NUKES disable (the iNukeRange magnitude itself stays the air.nukeRange modifier).
    nr = _int(rec, "iNukeRange")
    if nr is not None and nr != -1:
        build["disabled"] = "NO_NUKES"
    return {"build": build} if build else None


def allowed_unit(rec):
    """The declarative INSTANCE CAP (owner 2026-06-17): `allowed:{<scope>:N}` — the real cap number, scope-keyed
    (world/empire), NOT a `requires` SELF-atom. Engine enforces (build while tally.count(SELF,scope) < N) and owns
    ignoring it (NO_NATIONAL_UNIT_LIMIT, honoring the per-unit `identity.unlimitedException` exception) + era-scaling
    the base + `+extra` — none touch the parser. A unique unit -> `allowed:{empire:1}`. enabler-spec §5a/§13.7."""
    allowed = OrderedDict()
    g = _int(rec, "iMaxGlobalInstances")
    if g is not None and g >= 0:
        allowed["world"] = g
    # A TEAM instance cap makes no sense for a UNIT -- units belong to PLAYERS, not teams (owner ruling
    # 2026-06-17). So iMaxTeamInstances folds into EMPIRE alongside iMaxPlayerInstances (the tighter of the two
    # wins if a unit ever carries both); it is NOT dropped (the old code silently discarded team caps -> uncapped).
    emp = None
    for tag in ("iMaxPlayerInstances", "iMaxTeamInstances"):
        v = _int(rec, tag)
        if v is not None and v >= 0:
            emp = v if emp is None else min(emp, v)
    if emp is not None:
        allowed["empire"] = emp
    return allowed or None


_FOUND_BUILDINGS = None


def found_buildings(store):
    """The settler-grants-buildings list (owner 2026-06-16): every building's NewCityFree BoolExpr becomes a
    found-time grant {building, enabled:<condition>}, + the Palace gated on no-cities-yet ({type:CITY,scope:empire,
    max:0}). IDENTICAL for every bFound unit (no settler-type difference yet — owner-verified). Computed ONCE from
    the merged, module-included BuildingInfo table (file order = deterministic). Realizes the invariant: a building
    not available at settle time (its tech gate unmet) is simply not granted -> not pre-built. (GitHub #7; renames §Unit.)"""
    global _FOUND_BUILDINGS
    if _FOUND_BUILDINGS is not None:
        return _FOUND_BUILDINGS
    out = []
    for b, brec in store.table("BuildingInfo").items():
        ncf = brec.find("NewCityFree")
        if ncf is None:
            continue
        entry = OrderedDict([("building", b)])
        cond = boolexpr.convert_field(ncf)
        if cond is not None:
            entry["enabled"] = cond
        out.append(entry)
    for b, brec in store.table("BuildingInfo").items():
        if engine.text(brec.find("bCapital")) == "1":
            out.append(OrderedDict([("building", b), ("enabled", _atom("CITY", "empire", max=0))]))
    _FOUND_BUILDINGS = out
    return out


def _set_fam(fams, family, member, unit, value):
    node = fams.setdefault(family, OrderedDict()).setdefault("unit", OrderedDict())
    if member:
        node = node.setdefault(member, OrderedDict())
    node[unit] = value


# ---- PASS 2 tables ----
# vs-keyed combat: tag -> (keyword, member|None). All -> strength.unit.<keyword>.{TYPE}[.member].percent.
VS_KEYED = {
    "TerrainAttacks": ("terrain", "attack"), "TerrainDefenses": ("terrain", "defense"),
    "FeatureAttacks": ("feature", "attack"), "FeatureDefenses": ("feature", "defense"),
    "UnitCombatMods": ("unitCombat", None), "DomainMods": ("domain", None),
    "FlankingStrikesbyUnitCombat": ("flanking", None), "UnitAttackMods": ("vsUnit", "attack"),
    "UnitDefenseMods": ("vsUnit", "defense"),
}
# targeting/immunity capability LISTS -> capabilities.<name>: {TYPE: true}
CAP_LIST = {
    "UnitCombatTargets": "targets", "UnitCombatDefenders": "defenders",
    "UnitCombatCollateralImmunes": "collateralImmune", "UnitTargets": "unitTargets",
    "FeatureImpassableTypes": None, "TerrainImpassableTypes": None,  # handled as identity lists already
}
VISION_STRUCTS = {
    "InvisibleTerrainChanges": ("invisibleTerrain", ["InvisibleType", "TerrainType", "iIntensity"]),
    "InvisibleFeatureChanges": ("invisibleFeature", ["InvisibleType", "FeatureType", "iIntensity"]),
}
# GP-action magnitudes -> grants (one-time great-person actions; base + multiplier).
# NB the "hurry" here (iBaseHurry/iHurryMultiplier = great-engineer "Extra Construction", CvUnit::getHurryProduction
# / CvUnit::canHurry) is a DIFFERENT mechanic from CvHurryInfo / HurryTypes (the gold/pop city-rush curated in
# curate_hurry.py) — two mechanics share the verb "hurry"; do NOT conflate. See curate_hurry.py.
GP_ACTIONS = {
    "discover": ("iBaseDiscover", "iDiscoverMultiplier"), "hurry": ("iBaseHurry", "iHurryMultiplier"),
    "trade": ("iBaseTrade", "iTradeMultiplier"), "greatWork": ("iGreatWorkCulture", None),
    "food": ("iBaseFoodChange", None),
}


def _pairs(node):
    for item in list(node):
        key, val = None, None
        for c in item:
            if key is None and c.tag.endswith("Type"):
                key = engine.text(c)
            elif engine.is_int(engine.text(c)):
                val = int(engine.text(c))
        if key and key != "NONE" and val not in (None, 0):
            yield key, val


def pass2(typ, rec, store, fams, caps, grants, vision, identity):
    """vs-keyed combat, vision/LOS, outcomes, GP-action grants, properties, BonusProductionModifiers, cargo."""
    su = fams.setdefault  # noqa
    # vs-keyed combat -> strength.unit.<kw>.{TYPE}[.member].percent
    for tag, (kw, member) in VS_KEYED.items():
        node = rec.find(tag)
        if node is None:
            continue
        for k, v in _pairs(node):
            base = fams.setdefault("strength", OrderedDict()).setdefault("unit", OrderedDict()).setdefault(kw, OrderedDict()).setdefault(k, OrderedDict())
            if member:
                base = base.setdefault(member, OrderedDict())
            base["percent"] = v
    # targeting/immunity capability lists
    for tag, name in CAP_LIST.items():
        if name is None:
            continue
        lst = _typelist(rec, tag)
        if lst:
            caps.setdefault(name, OrderedDict()).update((x, True) for x in lst)
    # vision: the unit's own invisibility + see-invisible + intensity pairs + struct tables
    inv = _txt(rec, "Invisible")
    if inv:
        vision["invisible"] = inv
    see = _typelist(rec, "SeeInvisible")
    if see:
        vision["seeInvisible"] = see
    for tag, key in (("VisibilityIntensityTypes", "visibilityIntensity"),
                     ("InvisibilityIntensityTypes", "invisibilityIntensity")):
        node = rec.find(tag)
        if node is not None:
            pr = OrderedDict((k, v) for k, v in _pairs(node))
            if pr:
                vision[key] = pr
    for tag, (name, child_tags) in VISION_STRUCTS.items():
        node = rec.find(tag)
        if node is None:
            continue
        rows = []
        for item in list(node):
            row = OrderedDict()
            for ct in child_tags:
                t = engine.text(item.find(ct))
                if ct[:1] == "i":
                    if engine.is_int(t):
                        row[de_i(ct)] = int(t)
                elif t and t != "NONE":
                    row[ct[:-4].lower() if ct.endswith("Type") else ct] = t
            if row:
                rows.append(row)
        if rows:
            vision[name] = rows
    # outcomes (KillOutcomes / Actions) -> faithful (CvOutcome system; defs = CvOutcome Tier-G)
    outcomes = OrderedDict()
    for tag, key in (("KillOutcomes", "kill"), ("Actions", "actions")):
        node = rec.find(tag)
        if node is not None and list(node):
            outcomes[key] = engine.generic(node)
    if outcomes:
        identity["_outcomes"] = outcomes   # placed under a top-level `outcomes` in curate()
    # GP-action grants (one-time great-person action magnitudes)
    gp = OrderedDict()
    for act, (base_tag, mult_tag) in GP_ACTIONS.items():
        b = _int(rec, base_tag)
        m = _int(rec, mult_tag) if mult_tag else None
        if b or m:
            e = OrderedDict()
            if b:
                e["base"] = b
            if m:
                e["multiplier"] = m
            gp[act] = e
    if gp:
        grants["greatPersonAction"] = gp
    # properties (v3) -> per-PROPERTY family deposit
    pm = rec.find("PropertyManipulators")
    if pm is not None:
        for s in pm.findall("PropertySource"):
            res = engine.property_source_v3(s)
            if res:
                prop, scope, unit, value = res
                node = fams.setdefault(prop, OrderedDict()).setdefault(scope, OrderedDict())
                if unit in node and isinstance(node[unit], int) and isinstance(value, int):
                    node[unit] += value
                else:
                    node[unit] = value
    # BonusProductionModifiers -> buildRate.self (build THIS unit faster WHILE a bonus is present; owner 2026-06-16).
    # SELF build-rate gated by bonus presence, NOT city output and NOT keyed-by-bonus-as-target; unified with the
    # building curator into the buildRate family (same shape as curate_building COND_KEYED bonus -> buildRate.self).
    bpm = rec.find("BonusProductionModifiers")
    if bpm is not None:
        lst = fams.setdefault("buildRate", OrderedDict()).setdefault("self", OrderedDict()).setdefault("percent", [])
        for k, v in _pairs(bpm):
            lst.append(OrderedDict([("value", v),
                                    ("enabled", OrderedDict([("type", k), ("scope", "city"), ("min", 1)]))]))
    # cargo CAPACITY + what-it-carries -> cargo.unit.space.{unit: IS_<DOMAIN>?, flat}. A domain-restricted hold
    # carries only that domain: a carrier is cargo.space.{unit: IS_AIR, flat} (you can't transport a plane on a
    # landing craft); an unrestricted hold is just cargo.space.flat. Finer special-unit restrictions
    # (Special/SMNotSpecial) stay in identity.cargo for now (folded later).
    icargo = _int(rec, "iCargo")
    if icargo:
        space = OrderedDict()
        dom = _txt(rec, "DomainCargo")
        if dom and dom.startswith("DOMAIN_"):
            space["unit"] = "IS_" + dom[len("DOMAIN_"):]
        space["flat"] = icargo
        fams.setdefault("cargo", OrderedDict()).setdefault("unit", OrderedDict())["space"] = space
    cargo = OrderedDict()
    for tag, key in (("SpecialCargo", "special"), ("SMNotSpecialCargo", "smNotSpecial")):
        v = _txt(rec, tag)
        if v:
            cargo[key] = v
    if cargo:
        identity["cargo"] = cargo
    # tech-gated passability + heritage + advanced-start -> identity (parked, faithful)
    for tag, key in (("FeaturePassableTechs", "featurePassableTechs"), ("TerrainPassableTechs", "terrainPassableTechs")):
        node = rec.find(tag)
        if node is not None and list(node):
            identity[key] = engine.generic(node)
    her = _typelist(rec, "Heritage")
    if her:
        identity["heritage"] = her
    asc = _int(rec, "iAdvancedStartCost")
    if asc is not None and asc != 0:
        identity.setdefault("advancedStart", OrderedDict())["cost"] = asc
    # HealUnitCombatTypes -> heal.unit.unitCombat.{UC} (struct: UnitCombatType + iHeal + iAdjacentHeal)
    hn = rec.find("HealUnitCombatTypes")
    if hn is not None:
        for item in list(hn):
            uc = _txt(item, "UnitCombatType")
            heal, adj = _int(item, "iHeal"), _int(item, "iAdjacentHeal")
            if uc and (heal or adj):
                e = fams.setdefault("heal", OrderedDict()).setdefault("unit", OrderedDict()).setdefault("unitCombat", OrderedDict()).setdefault(uc, OrderedDict())
                if heal:
                    e["heal"] = heal
                if adj:
                    e["adjacentHeal"] = adj


def curate(typ, rec, store):
    out = OrderedDict([("type", typ)])
    for tag, key in TEXT.items():
        t = _txt(rec, tag)
        if t:
            out[key] = t

    fams = OrderedDict()
    caps = OrderedDict()
    tags = OrderedDict()
    grants = OrderedDict()
    succession = OrderedDict()
    identity = OrderedDict()
    base = OrderedDict()
    cost = OrderedDict()
    ai = OrderedDict()
    art_blocks = OrderedDict()
    vision = OrderedDict()

    # --- identity.base (create-unit foundation) ---
    for tag, key in BASE.items():
        v = _int(rec, tag)
        if v is not None and v != 0:
            base[key] = v
    # the unit's combat CLASS (Combat tag) + domain
    combat_class = _txt(rec, "Combat")
    if combat_class:
        base["combatClass"] = combat_class
    # --- §5 combat-trait families (unit scope) ---
    for tag, (family, member, unit) in UNIT_FAMILIES.items():
        v = _int(rec, tag)
        if v:
            _set_fam(fams, family, member, unit, v)
    # --- capabilities ---
    for tag, name in CAP_BOOL.items():
        if _bool(rec, tag):
            caps[name] = True
    for tag, name in CAP_COUNT.items():
        v = _int(rec, tag)
        if v:
            caps[name] = True
    dcm = [t for t in DCM_AIRBOMB if _bool(rec, t)]
    if dcm:
        caps["dcmAirBomb"] = len(dcm)   # the tier = count of set levels
    # --- tags (derived classification; greenfield first pass — see TAG_BY_UNITAI) ---
    # A specific DefaultUnitAI role (worker/spy/merchant/…) classifies the unit and SUPPRESSES `military` (owner:
    # a spy just needs `spy`, not military — bMilitarySupport over-fires on non-combat roles). `military` (the
    # IS_MILITARY signal) is the fallback for combat units that have no specific role.
    uai = _txt(rec, "DefaultUnitAI")
    if uai in TAG_BY_UNITAI:
        for t in TAG_BY_UNITAI[uai]:
            tags[t] = True
    elif _bool(rec, "bMilitarySupport"):
        tags["military"] = True
    # `outlaw` = the criminal COMBAT CLASS (owner 2026-06-23): primary <Combat> == UNITCOMBAT_CRIMINAL OR it appears
    # in <SubCombatTypes>. combat_class is the primary read above; SubCombatTypes is the same list curated to
    # identity.combatClasses. Independent of (and additive to) the role/military tags above.
    if combat_class == CRIMINAL_COMBAT or CRIMINAL_COMBAT in _typelist(rec, "SubCombatTypes"):
        tags["outlaw"] = True
    # --- grants (lists) ---
    for tag, key in GRANT_LIST.items():
        lst = _typelist(rec, tag)
        if lst:
            grants[key] = lst
    if _bool(rec, "bGoldenAge"):
        grants["goldenAge"] = True
    # --- settler-grants-buildings: a FOUNDER (bFound) seeds its new city with the NewCityFree set (+ Palace),
    # each gated by its condition; relocated off the buildings (owner 2026-06-16; GitHub #7). ---
    if _bool(rec, "bFound"):
        grants["foundBuildings"] = found_buildings(store)
    # --- succession (upgrade chain; manual, NOT replaces) ---
    ups = _typelist_struct(rec, "UnitUpgrades", "UnitType") or _typelist(rec, "UnitUpgrades")
    if ups:
        succession["upgradesTo"] = ups
    # SupersedingUnits = the genuine REPLACE edge (owner ruling 2026-06-25): a successor that REMOVES the predecessor
    # from the buildable set once itself buildable (engine isSupersedingUnitAvailable). Modeled with the EXISTING
    # `replacedBy` enabler edge (target-side, like obsoletedBy) -- NOT a bespoke supersededBy mechanism. Distinct from
    # upgradesTo (the all-reachable dormancy). Emitted as out["replacedBy"] in the assembly below.
    sup = _typelist(rec, "SupersedingUnits")
    # --- cost ---
    for tag, key in COST.items():
        if key is None:
            continue
        v = _int(rec, tag)
        if v is not None and v != 0 and v != -1:
            cost[key] = v
    # iInstanceCostModifier -> costs.empire.perInstance per:{type:SELF} (the priority count-scaled cost case)
    icm = _int(rec, "iInstanceCostModifier")
    if icm:
        fams.setdefault("costs", OrderedDict()).setdefault("empire", OrderedDict())["perInstance"] = \
            OrderedDict([("percent", icm), ("per", _atom("SELF", "empire"))])
    # spawn-only nature: legacy marks NON-player-buildable units (wildlife/spawned) with the iCost == -1 SENTINEL
    # (the CvPlayer::canTrain getProductionCost()==-1 gate). Translate that dumb sentinel into an explicit clean
    # flag — the cascade gates buildability on this, never on a -1 cost. NB settlers carry NO iCost tag (their
    # real cost is population-based, "not the full cost") so they are NOT spawn-only and stay buildable.
    if _int(rec, "iCost") == -1:
        identity["spawnOnly"] = True
    # --- identity scalars / lists ---
    for tag, key in ID_SCALAR.items():
        iv = _int(rec, tag)
        sv = _txt(rec, tag)
        if iv is not None and iv != 0:
            identity[key] = iv
        elif sv and not engine.is_int(sv):
            identity[key] = sv
    for tag, key in ID_LIST.items():
        lst = _typelist(rec, tag)
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
    # --- game options: DECLARATIVE requires.build conditions (GAMEOPTION_X in build.all / build.noneOf), authored in
    # requires_unit() above -- NOT loadPrune. States the option dependency clearly on the unit and lets the cascade
    # evaluate it (modder-extensible, no engine special-case). So units emit NO loadPrune block at all. ---
    # --- art ---
    for tag in ART:
        put_art(art_blocks, tag, engine.text(rec.find(tag)))

    # --- PASS 2: vs-keyed combat, vision/LOS, outcomes, GP-action grants, properties, BonusProd, cargo ---
    pass2(typ, rec, store, fams, caps, grants, vision, identity)
    outcomes = identity.pop("_outcomes", None)

    # --- assemble (reserved order) ---
    enables = store.enabled_by(typ)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    obsoletes = store.obsoletes_of(typ)
    if obsoletes:
        out["obsoletes"] = OrderedDict((k, obsoletes[k]) for k in sorted(obsoletes))
    # ObsoleteTech -> TARGET-side obsoletedBy.techs, mirroring the building curator (owner 2026-06-22): the unit
    # carries its own obsoleting tech so the cascade prunes it from the buildable set via the SAME obsoletedBy edge
    # buildings use (engine CvPlayer::canTrain:6404 -- obsolete-tech held => not trainable). On-map persistence is out
    # of scope (the cascade answers canTrain only); an obsolete unit simply leaves the buildable set.
    ot = _txt(rec, "ObsoleteTech")
    if ot:
        out["obsoletedBy"] = OrderedDict([("techs", [ot])])
    # SupersedingUnits -> the `replacedBy` replace edge (see succession note above): the unit is removed from
    # buildable when a superseder is buildable. The existing replace mechanic, not a bespoke supersededBy.
    if sup:
        out["replacedBy"] = OrderedDict([("units", sup)])
    requires = requires_unit(rec, store)
    # UPGRADE DORMANCY (owner ruling 2026-06-25): a unit is dormant OUT of the buildable set when ALL of the units it
    # DIRECTLY upgrades to are active -> requires.build.dormant.all = the IMMEDIATE upgradesTo (UnitUpgrades), NOT a
    # transitive closure. The cascade recurses the chain ENGINE-SIDE, mirroring CvCity::allUpgradesAvailable (a unit
    # hides only when EVERY direct upgrade resolves to a reachable-trainable unit), so authoring just the direct edges
    # keeps it short (a swordsman's chain is huge) and matches the engine. `dormant` rides `build` (units have no
    # operate; build/operate share conditionals) and is fail-safe (default not-dormant). Distinct from identity.spawnOnly.
    # The dormant set is the immediate upgrades EXCLUDING any that are also SupersedingUnits: the engine's
    # allUpgradesAvailable SKIPS a superseding upgrade (`if (isSupersedingUnit(eTempUnit)) continue`, CvCity.cpp:2055)
    # -- those are the separate isSupersedingUnitAvailable gate, NOT the "all upgrades reachable" hide. Without this,
    # a NEANDERTHAL_* variant (an unreachable, civ-restricted superseding upgrade) wrongly kept the base unit offered.
    superseding = set(_typelist_struct(rec, "SupersedingUnits", "UnitType") or _typelist(rec, "SupersedingUnits"))
    ups_imm = [u for u in (_typelist_struct(rec, "UnitUpgrades", "UnitType") or _typelist(rec, "UnitUpgrades"))
               if u and u != "NONE" and u not in superseding]
    if ups_imm:
        requires = requires or OrderedDict()
        requires.setdefault("build", OrderedDict())["dormant"] = OrderedDict([("all", [_atom(u, "city") for u in ups_imm])])
    if requires:
        out["requires"] = requires
    # EnabledCivilizationTypes -> identity whitelist (NPC-only train gate; dry-calc ignores it; remodel post-rework).
    _civs = _typelist_struct(rec, "EnabledCivilizationTypes", "CivilizationType")
    if _civs:
        identity["enabledCivilizations"] = _civs
    allowed = allowed_unit(rec)
    if allowed:
        out["allowed"] = allowed
    ordered = [f for f in FAMILY_ORDER if f in fams] + [f for f in fams if f not in FAMILY_ORDER]
    for f in ordered:
        out[f] = fams[f]
    if caps:
        out["skills"] = caps
    if tags:
        out["tags"] = tags
    if vision:
        out["vision"] = vision
    if outcomes:
        out["outcomes"] = outcomes
    if grants:
        out["grants"] = grants
    if succession:
        out["succession"] = succession
    if base:
        identity["base"] = base
    if cost:
        out["cost"] = cost
    if ai:
        out["ai"] = ai
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out


def curate_special_unit(typ, rec, store):
    out = OrderedDict([("type", typ)])
    d = _txt(rec, "Description")
    if d:
        out["description"] = d
    identity = OrderedDict()
    if engine.text(rec.find("bValid")) in ("0", "false", "False"):
        identity["valid"] = False
    for tag, key in (("bCityLoad", "cityLoad"), ("bSMLoadSame", "smLoadSame")):
        if _bool(rec, tag):
            identity[key] = True
    if identity:
        out["identity"] = identity
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7) -- the special-unit path too
    return out


HANDLED = (set(BASE) | set(UNIT_FAMILIES) | set(CAP_BOOL) | set(CAP_COUNT) | set(DCM_AIRBOMB) | set(GRANT_LIST)
           | set(COST) | set(ID_SCALAR) | set(ID_LIST) | set(TEXT) | set(ART) | REQUIRES_TAGS | STORE_TAGS
           | PASS2_TAGS | {"Type", "Combat", "Flavors", "iAIWeight", "iInstanceCostModifier", "bGoldenAge",
                           "DefaultUnitAI", "UnitMeshGroups", "FreePromotions",
                           "bSpy"})   # dropped: redundant with the 'spy' tag (every bSpy unit is UNITAI_SPY); spy isn't a skill (owner 2026-06-23)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("UnitInfo")
    results = OrderedDict((typ, curate(typ, rec, store)) for typ, rec in table.items())
    su_results = OrderedDict((typ, curate_special_unit(typ, rec, store))
                             for typ, rec in store.table("SpecialUnitInfo").items())
    n = len(results)

    from collections import Counter
    leftover = Counter()
    for _typ, rec in table.items():
        for c in rec:
            if c.tag not in HANDLED:
                leftover[c.tag] += 1
    if leftover:
        print("UNHANDLED tags (count): %s" % ", ".join("%s=%d" % (t, c) for t, c in leftover.most_common()))
    else:
        print("COVERAGE: all XML tags handled or deferred (pass 2).")

    has = lambda k: sum(1 for o in results.values() if k in o)
    STRUCT = {"type", "description", "civilopedia", "help", "enables", "obsoletes", "requires", "allowed", "skills", "tags",
              "vision", "outcomes", "grants", "succession", "cost", "ai", "loadPrune", "ui", "world", "sound", "identity"}
    seen = sorted({f for o in results.values() for f in o if f not in STRUCT})
    print("UnitInfo curated: %d  | SpecialUnitInfo: %d" % (n, len(su_results)))
    for k in ("enables", "obsoletes", "requires", "allowed", "skills", "tags", "grants", "succession", "cost", "identity"):
        print("  with %-11s: %d" % (k, has(k)))
    print("  families seen: %s" % ", ".join(seen))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            src = results if nm in results else (su_results if nm in su_results else None)
            if src is not None:
                print("\n=== %s ===" % nm)
                print(json.dumps(src[nm], indent=1, ensure_ascii=False))
            else:
                print("\n(%s not found)" % nm)
    if args.write:
        base = os.path.join(REPO, "Assets", "Data", "units")
        os.makedirs(base, exist_ok=True)
        for typ, obj in results.items():
            with open(os.path.join(base, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        sud = os.path.join(REPO, "Assets", "Data", "specialunits")
        os.makedirs(sud, exist_ok=True)
        for typ, obj in su_results.items():
            with open(os.path.join(sud, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        print("\nwrote %d UnitInfo + %d SpecialUnitInfo JSON under Assets/Data" % (n, len(su_results)))


if __name__ == "__main__":
    main()
