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
the vision/LOS resolver, KillOutcomes/Actions -> `outcomes` (the clean VERB-PER-PAYLOAD vocabulary, emit_outcomes),
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
from curate_common import (VISION_PLOT, collapse_hide_and_seek, merge_vision, put_art, emit_art, FAMILY_ORDER, de_i, fold_text_to_identity, gate_entity,
                           add_tags, specialunit_tag,
                           emit_sizematters, SM_COMBATMOD_UNIT)
from store import Store, REPO

# ---- identity.base: the create-unit FOUNDATION (§0.6) ----
BASE = {
    # iCombat -> strength family, iMoves -> movement family (owner 2026-07-20: identity.base grab-bag dissolves;
    # base combat is the unit's base STRENGTH, base moves belong to the movement subsystem). Handled in curate().
    "iWorkRate": "workRate", "iAirCombat": "airCombat",
    "iCombatLimit": "combatLimit", "iAirCombatLimit": "airCombatLimit",
    "iAirUnitCap": "airUnitCap",
}
# ---- §5 unit-scope combat-trait families (tag -> (family, member|None, unit)). REUSES the Promotion §5 vocab.
# `strength` holds ONLY the unit's BASE value (strength.unit.flat, ruling 5 info-rebuild.md); everything that
# MODIFIES that base lives in the `combat` family (the semantic modifier kinds + the type-keyed vs-entries). ----
UNIT_FAMILIES = {
    "iCityAttack": ("combat", "cityAttack", "percent"),
    "iCityDefense": ("combat", "cityDefense", "percent"),
    "iHillsAttack": ("combat", "hillsAttack", "percent"),
    "iHillsDefense": ("combat", "hillsDefense", "percent"),
    "iVSBarbs": ("combat", "vsBarbs", "percent"),
    "iAttackCombatModifier": ("combat", "attack", "percent"),
    "iDefenseCombatModifier": ("combat", "defense", "percent"),
    "iLunge": ("combat", "lunge", "percent"),
    "iEnclose": ("combat", "enclose", "percent"),
    "iUnnerve": ("combat", "unnerve", "percent"),
    "iDynamicDefense": ("combat", "dynamicDefense", "percent"),
    "iStealthStrikes": ("combat", "stealthStrikes", "flat"),
    "iStealthCombatModifier": ("combat", "stealth", "percent"),
    "iBreakdownChance": ("combat", "breakdownChance", "flat"),
    "iBreakdownDamage": ("combat", "breakdownDamage", "flat"),
    "iWithdrawalProb": ("withdrawal", None, "percent"),
    "iFirstStrikes": ("firstStrike", "strikes", "flat"),
    "iChanceFirstStrikes": ("firstStrike", "chance", "flat"),
    "iBombardRate": ("bombard", "rate", "percent"),
    # iRBombardDamage/iRBombardDamageLimit/iDCMBombRange/iDCMBombAccuracy: DCM RANGE BOMBARD is ruled FULLY
    # REMOVED (structural-cleanup.md Tier 2) -- DROPped below; vanilla city bombard (iBombardRate) and air
    # bombing (iBombRate, its own slated removal) are separate systems and stay.
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
    # UNDERWORLD, not espionage (owner): the in-city criminal contest -- a criminal's stealth against an
    # investigator's catch. The carriers say it plainly (UNITCOMBAT_CRIMINAL / UNITCOMBAT_LAW_ENFORCEMENT).
    # Same filing as curate_building / curate_specialist / curate_promotion.
    "iInsidiousness": ("underworld", "insidiousness", "flat"),
    "iInvestigation": ("underworld", "investigation", "flat"),
    "iNumHealSupport": ("heal", "support", "flat"),
    "iSelfHealModifier": ("heal", "selfModifier", "percent"),   # mirror curate_promotion FAMILIES (unit self-heal %)
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
    "bPassage": "passage", "bRivalTerritory": "rivalTerritory",   # bRBombardForceAbility DROPs with the DCM-range removal
    "bSabotage": "sabotage", "bStateReligion": "stateReligion", "bStealPlans": "stealPlans",
    "bStealthDefense": "stealthDefense", "bSuicide": "suicide", "bUnlimitedException": "unlimitedException",
    "bUpgradeAnywhere": "upgradeAnywhere", "bWorkerTrade": "workerTrade", "bAttackOnlyCities": "attackOnlyCities",
    "bIgnoreNoEntryLevel": "ignoreNoEntryLevel", "bFliesToMove": "fliesToMove", "bFreeDrop": "freeDrop",
    "bDCMFighterEngage": "dcmFighterEngage", "bRenderBelowWater": "renderBelowWater",
    "bMilitaryTrade": "militaryTrade", "bNoSelfHeal": "noSelfHeal",   # mirror curate_promotion CAP_BOOL (unit no-self-heal skill)
    "bNoRevealMap": "noRevealMap",   # goody-hut gate (CvUnit): unit reveals no map on a hut result
}
# count-int capabilities (>0 -> has it)
# iAnimalIgnoresBorders is DROPPED (owner 2026-07-11): animal border-ignoring is PURELY game-option-driven at runtime
# (GAMEOPTION_ANIMAL_STAY_OUT / _DANGEROUS in CvUnit::canAnimalIgnoresBorders), NOT curated unit data -- so it emits nothing.
CAP_COUNT = {}
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

# ---- grants (one-shot, lists) ----
# NB `Buildings` is NOT here: the unit's <Buildings> (MISSION_CONSTRUCT -- buildings it can construct via a mission)
# is NOT a grant. It emits as the `constructs` OUTCOME verb under outcomes.actions[] (owner 2026-07-21, json.md §8),
# in emit_outcomes() -- moved off the off-grammar grants.buildings.
GRANT_LIST = {"FreePromotions": "promotions", "GreatPeoples": "greatPeople"}
# GroupSpawnUnitCombatTypes is NOT a grant (handout) -- it is the unit's own group-SPAWN config (pack combat class +
# chance + title). It emits to its OWN top-level `groupSpawn` block as struct rows (owner principle 2026-07-11: config
# data gets its own self-documenting block, not `grants`); a flat _typelist would drop iChance + Title.
# ReligionSpreads/CorporationSpreads are NOT timed grants -- they are the unit's standing per-religion / per-corp
# SPREAD STRENGTH (magnitude), and burying them under `grants` misleads a modder (owner 2026-07-11). They emit to
# their OWN `spread.religion` / `spread.corporation` keyed-map block (see the spread emit in curate()).
# `builds` (owner ruling): the per-unit-type list of BUILD_* a unit can PERFORM is NOT a one-shot grant/provision
# handed out -- it is the unit's build REPERTOIRE. So it lives in its OWN top-level `builds` block, not under grants.
# ---- cost ----
COST = {"iCost": "production", "iBaseUpkeep": "upkeep", "iHurryCostModifier": "hurryCostModifier",
        "iInstanceCostModifier": None}  # iInstanceCostModifier -> costs.empire.perInstance per:{SELF} (special)
# ---- identity scalars / lists / config ----
ID_SCALAR = {"iAsset": "worth", "iPower": "militaryWorth", "iXPValueAttack": "xpValueAttack",
             "iXPValueDefense": "xpValueDefense", "iConscription": "conscription", "iAggression": "aggression",
             "iAnimalCombat": "animalCombat", "iCommandRange": "commandRange", "iControlPoints": "controlPoints",
             "iLeaderExperience": "leaderExperience", "iMinAreaSize": "minAreaSize",
             "Domain": "domain", "DefaultUnitAI": "defaultUnitAI", "FormationType": "formationType",
             "Advisor": "advisor", "LeaderPromotion": "leaderPromotion",
             "ReligionType": "religion", "iEspionagePoints": "espionagePoints"}
ID_LIST = {"UnitAIs": "unitAIs", "NotUnitAIs": "notUnitAIs",
           "MapCategoryTypes": "mapCategories", "UniqueNames": "uniqueNames", "FeatureImpassableTypes": "featureImpassable",
           "TerrainImpassableTypes": "terrainImpassable", "DefendAgainstUnit": "defendAgainstUnit"}
# SubCombatTypes -> ROOT `combatClasses` (owner 2026-07-20), NOT identity -- read directly in curate() into sub_combats.
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
    "UnitCombatDefenders", "UnitCombatCollateralImmunes", "UnitTargets", "DefendAgainstUnit", "iCargo", "DomainCargo",
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
# "into settling" as plain grants.buildings (see found_buildings() below; ruling 8 / json.md §5 -- the settler's
# considered action IS founding, so no bespoke foundBuildings key), each gated by its NewCityFree BoolExpr via the
# shared converter `boolexpr.py`. That same converter retrofits the parked building ConstructCondition + unit
# TrainCondition into requires.build. The capital (bCapital -> Palace) is an entry gated on the Palace's own
# absence. Map + rationale: migration-renames "BoolExpr converter + settler-grants".


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


def _keyed_int(rec, wrapper, keytag, valtag):
    """{TypeString: int} from a struct-list wrapper -- e.g. ReligionSpreads/ReligionSpread{ReligionType,iReligionSpread}."""
    node = rec.find(wrapper)
    if node is None:
        return {}
    out = {}
    for c in node:
        k = engine.text(c.find(keytag)) if c.find(keytag) is not None else None
        v = c.find(valtag)
        if k and k != "NONE" and v is not None and engine.is_int(engine.text(v)):
            out[k] = int(engine.text(v))
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
    # GAME-OPTION gates -> the ENTITY-LEVEL enabled/disabled gate (owner ruling 2026-07-08, superseding the
    # 2026-06-25 requires.build routing): a whole-entity option gate authors as the top-level `enabled`/`disabled`
    # condition (`enabled: GAMEOPTION_X`), the ONE canonical form across every type -- `requires` holds only genuine
    # needs (resources/civics/counts). Collected in curate() via gate_unit() below.
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
            # The capital building seeds a founded city iff the empire HAS NO PALACE -- the building gates on its
            # OWN absence, not on a city count. Two reasons this is the right atom:
            #   - a CITY-count proxy is wrong on the case that matters: lose your capital with other cities still
            #     standing and founding the next one SHOULD re-seed a palace; "first city" refuses it.
            #   - it has no off-by-one. The grant applies at CvPlayer::found AFTER initCity has registered the new
            #     city, so a `{CITY, empire, max:0}` gate can never hold at founding and the Palace was silently
            #     never seeded -- a game with no capital (verified live: appliedFirstBuild=0, palace left unbuilt).
            #     The palace count is genuinely 0 at that moment.
            out.append(OrderedDict([("building", b), ("enabled", _atom(b, "empire", max=0))]))
    _FOUND_BUILDINGS = out
    return out


def _set_fam(fams, family, member, unit, value):
    node = fams.setdefault(family, OrderedDict()).setdefault("unit", OrderedDict())
    if member:
        node = node.setdefault(member, OrderedDict())
    node[unit] = value


# ---- PASS 2 tables ----
# vs-keyed combat: tag -> (keyword, member|None). All -> combat.unit.<keyword>.{TYPE}[.member].percent
# (the type-keyed vs-entries are strength MODIFIERS -> the combat family, ruling 5).
VS_KEYED = {
    "TerrainAttacks": ("terrain", "attack"), "TerrainDefenses": ("terrain", "defense"),
    "FeatureAttacks": ("feature", "attack"), "FeatureDefenses": ("feature", "defense"),
    "UnitCombatMods": ("unitCombat", None), "DomainMods": ("domain", None),
    "FlankingStrikesbyUnitCombat": ("flanking", None), "UnitAttackMods": ("vsUnit", "attack"),
    "UnitDefenseMods": ("vsUnit", "defense"),
    "FlankingStrikes": ("flankingUnit", None),   # by-UNIT flanking strength -> strength.unit.flankingUnit.{UNIT}.percent
}
# targeting/immunity capability LISTS -> capabilities.<name>: {TYPE: true}
CAP_LIST = {
    "UnitCombatTargets": "targets", "UnitCombatDefenders": "defenders",
    "UnitTargets": "unitTargets",
    "FeatureImpassableTypes": None, "TerrainImpassableTypes": None,  # handled as identity lists already
}
# UnitCombatCollateralImmunes -> the boolean SKILL `collateralImmune` (owner 2026-07-20): immune to the siege-VARIANT
# collateral (keyed only to SIEGE/ASSAULT_MECH/ROBOT sources, never MOUNTED-flanking) -> collapses to one pure
# enabler; flankImmune is not needed (siege units are the flankable ones). Emitted in curate(), NOT this CAP_LIST.
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
            # the FK reference child: usually <...Type>, but FlankingStrikesbyUnitCombat's key is
            # <FlankingStrikeUnitCombat> (ends "Combat") -- accept both so its rows aren't silently dropped.
            if key is None and (c.tag.endswith("Type") or c.tag.endswith("Combat")):
                key = engine.text(c)
            elif engine.is_int(engine.text(c)):
                val = int(engine.text(c))
        if key and key != "NONE" and val not in (None, 0):
            yield key, val


# ============================================================================
# OUTCOME / ACTION emission -- the VERB-PER-PAYLOAD vocabulary (owner-approved).
# Re-models KillOutcomes + Actions/Action from the raw XML mirror into a clean
# {kill:[...], actions:[...]} where each outcome is {requires?, chance?, <verb payloads>}.
# DATA-SHAPE ONLY: the CvOutcome ENGINE stays XML and does NOT read this block yet.
# Field inventory is the LIVE data (probed): every CvOutcome::read() field is mapped;
# anything that cannot map cleanly is emitted under a clearly-FLAGGED key + reported
# (silent loss is the anti-pattern this rework kills).
# ============================================================================
# Aggregated FLAG ledger (category -> [count, {sample unit types}]); printed by main().
_FLAGS = {}


def _flag(cat, typ):
    e = _FLAGS.setdefault(cat, [0, set()])
    e[0] += 1
    if len(e[1]) < 8:
        e[1].add(typ)


def _raw(node):
    """TAG-PRESERVING faithful dump (unlike engine.generic, which de-asses uniform lists and DROPS the
    operator name). Used only for the flagged `expr`/`_unmapped` escape hatches so no structure is lost."""
    kids = list(node)
    if not kids:
        t = engine.text(node)
        return int(t) if engine.is_int(t) else (t or None)
    out = OrderedDict()
    for k in kids:
        v = _raw(k)
        if k.tag in out:
            if not isinstance(out[k.tag], list):
                out[k.tag] = [out[k.tag]]
            out[k.tag].append(v)
        else:
            out[k.tag] = v
    return out


# Adapt* is PURE ENGINE (owner 2026-07-20): a gamespeed scaler the applier calculates from the reward's context
# (a kill-yield gets the unit-yield scale, a hurry-production the hammer-cost scale) -- NEVER data. So the curator
# UNWRAPS it and emits only the plain inner value; no scale marker survives into the JSON.
_ADAPT_TAGS = ("AdaptUnitYield", "Adapt", "AdaptHammerCost")


def _intexpr(elem, typ):
    """A CvOutcome IntExpr element (iChance / an iYield|iCommerce item / iReduceAnarchyLength) ->
    a clean int  |  {base, random}. `Adapt*` wrappers are pure-engine gamespeed scalers (unwrapped, applier
    scales at grant time); `Constant`/`Plus{Constant[,Random]}` map directly. A genuinely richer node (Mult/
    Property/Python etc.) still falls to {expr:<raw>} + flag -- but after the Adapt unwrap none remain in the data."""
    if elem is None:
        return None
    kids = list(elem)
    if not kids:
        t = engine.text(elem)
        return int(t) if engine.is_int(t) else None
    # pure-engine gamespeed scaler -> transparently unwrap and recurse into the inner expression.
    if len(kids) == 1 and kids[0].tag in _ADAPT_TAGS:
        return _intexpr(kids[0], typ)
    # a bare Constant child (e.g. the unwrapped AdaptHammerCost{Constant:216}) -> the plain int.
    if len(kids) == 1 and kids[0].tag == "Constant":
        t = engine.text(kids[0])
        return int(t) if engine.is_int(t) else None
    if len(kids) == 1 and kids[0].tag == "Plus":
        p = kids[0]
        if set(c.tag for c in p) <= {"Constant", "Random"}:
            c = engine.text(p.find("Constant")) if p.find("Constant") is not None else ""
            r = engine.text(p.find("Random")) if p.find("Random") is not None else ""
            c = int(c) if engine.is_int(c) else 0
            r = int(r) if engine.is_int(r) else 0
            return OrderedDict([("base", c), ("random", r)]) if r else c
    _flag("intexpr_richer:%s" % kids[0].tag, typ)
    return OrderedDict([("expr", _raw(elem))])


def _condition(elem, typ, ctx):
    """A CvOutcome BoolExpr wrapper (bUnitToCity / PlotCondition / UnitCondition) -> bool | clean
    condition (via the shared boolexpr converter; e.g. Has GOM_TECH -> a tech requirement) |
    {expr:<raw>} (FLAGGED when the converter cannot map it -- e.g. Is/IntegrateOr/Greater plot preds)."""
    if elem is None:
        return None
    kids = list(elem)
    if not kids:
        return None
    if len(kids) == 1 and kids[0].tag == "Constant":   # a BoolExpr literal -> plain bool
        t = engine.text(kids[0])
        return bool(int(t)) if engine.is_int(t) else None
    try:
        return boolexpr.convert_field(elem)
    except ValueError:
        _flag("boolexpr_richer:%s" % ctx, typ)
        return OrderedDict([("expr", _raw(elem))])


# outcome child tags this emitter consumes -- anything else on an <Outcome> is FLAGGED as _unmapped.
_OC_HANDLED = {
    "OutcomeType", "iChance", "iChancePerPop", "UnitType", "bUnitToCity", "PromotionType", "BonusType",
    "iGPP", "GPUnitType", "iHappinessTimer", "iPopulationBoost", "iReduceAnarchyLength", "EventTrigger",
    "PythonCallback", "PythonName", "Python", "bKill", "Yields", "Commerces", "PlotCondition",
    "UnitCondition", "Properties",
}
# key order for a rendered outcome payload (meta first, then verbs).
_OC_ORDER = ["requires", "chance", "chancePerPop", "consumes", "spawns", "promotes", "places",
             "greatPeople", "triggers", "population", "revolution", "happiness",
             "food", "production", "commerce", "gold", "research", "culture", "espionage",
             "properties", "python"]


def _python_code(elem):
    """The inline <Python> body -> a compilable snippet. The XML nests the code, so lines 2+ carry the element's
    indentation tabs while line 1 (inline right after <Python>) lost its own -- inconsistent indentation that the
    engine's preparePython (which strips only the FIRST line's prefix) cannot fix, so it SyntaxErrors. We dedent
    here: drop blank edge lines, then strip the COMMON leading whitespace of the indented (2nd+) lines from them,
    leaving line 1 at column 0. Result: every top-level statement at 0, bodies relatively indented -- compiles clean."""
    if elem is None:
        return None
    raw = elem.text if elem.text is not None else ""
    lines = raw.split("\n")
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    if not lines:
        return None
    # The XML author indents only line 1 (to sit under <Python>) and writes the rest at column 0, relying on the
    # engine's preparePython to strip line 1's prefix. Reproduce that OFFLINE so the JSON is already-clean Python:
    # strip line 1's leading-whitespace prefix from every line that carries it (line 1 -> col 0; lines already at 0
    # are unchanged). The consumer then compiles it directly (no preparePython needed).
    prefix = lines[0][:len(lines[0]) - len(lines[0].lstrip("\t "))]
    if prefix:
        lines = [(l[len(prefix):] if l.startswith(prefix) else l) for l in lines]
    return "\n".join(lines)


def _outcome_payload(oc, typ):
    """One <Outcome> -> {requires?, chance?, <verb payloads>}. The OUTCOME_* id + plot/unit gates fold
    into `requires` (the gate/tier). Every CvOutcome::read() field is mapped or flag-emitted."""
    p = OrderedDict()
    # requires: the OUTCOME_* id (gate/tier) + plot/unit BoolExpr gates
    req = OrderedDict()
    ot = _txt(oc, "OutcomeType")
    if ot:
        req["outcome"] = ot
    plot = _condition(oc.find("PlotCondition"), typ, "plot")
    if plot is not None:
        req["plot"] = plot
    ucond = _condition(oc.find("UnitCondition"), typ, "unit")
    if ucond is not None:
        req["unit"] = ucond
    if req:
        p["requires"] = req
    # META: chance (+ the rarer per-pop scaler, no approved verb -> flagged clean key)
    ch = _intexpr(oc.find("iChance"), typ)
    if ch is not None:
        p["chance"] = ch
    cpp = _int(oc, "iChancePerPop")
    if cpp:
        p["chancePerPop"] = cpp
        _flag("field_no_verb:iChancePerPop->chancePerPop", typ)
    # consumes (bKill)
    if _bool(oc, "bKill"):
        p["consumes"] = True
    # spawns {unit, toCity?}
    u = _txt(oc, "UnitType")
    if u:
        sp = OrderedDict([("unit", u)])
        tc = _condition(oc.find("bUnitToCity"), typ, "toCity")
        if tc is True:
            sp["toCity"] = True
        elif tc not in (None, False):
            sp["toCity"] = tc          # conditional to-city (e.g. a tech requirement)
        p["spawns"] = sp
    # promotes / places
    pr = _txt(oc, "PromotionType")
    if pr:
        p["promotes"] = pr
    bo = _txt(oc, "BonusType")
    if bo:
        p["places"] = bo
    # greatPeople {points, unit?}
    gpp = _int(oc, "iGPP")
    gpu = _txt(oc, "GPUnitType")
    if gpp or gpu:
        gp = OrderedDict()
        if gpp:
            gp["points"] = gpp
        if gpu:
            gp["unit"] = gpu
        p["greatPeople"] = gp
    # triggers (event) / population / revolution (reduceAnarchyLength)
    ev = _txt(oc, "EventTrigger")
    if ev:
        p["triggers"] = ev
    pb = _int(oc, "iPopulationBoost")
    if pb:
        p["population"] = pb
    ral = _intexpr(oc.find("iReduceAnarchyLength"), typ)
    if ral:
        p["revolution"] = ral
    # iHappinessTimer -> `happiness: {duration: N}` (owner 2026-07-20): a timed happiness pulse lasting N turns.
    # `duration` is the generic timed-effect term (NOT pre-specced -- establishes it; the greenfield unit-`state`
    # timer model, state.md, should reuse it).
    ht = _int(oc, "iHappinessTimer")
    if ht:
        p["happiness"] = OrderedDict([("duration", ht)])
    # Yields (index -> food/production/commerce) + Commerces (index -> gold/research/culture/espionage)
    for tag, names in (("Yields", engine.YIELDS), ("Commerces", engine.COMMERCES)):
        node = oc.find(tag)
        if node is None:
            continue
        for i, item in enumerate(list(node)):
            if i >= len(names):
                _flag("extra_index:%s" % tag, typ)
                continue
            v = _intexpr(item, typ)
            if v not in (None, 0):
                p[names[i]] = v
    # Properties (CvProperties block; absent in unit data -> defensive raw+flag)
    props = oc.find("Properties")
    if props is not None and list(props):
        pp = OrderedDict()
        for c in props:
            k = _txt(c, "PropertyType")
            val = next((int(engine.text(cc)) for cc in c if cc.tag != "PropertyType" and engine.is_int(engine.text(cc))), None)
            if k:
                pp[k] = val
        p["properties"] = pp or OrderedDict([("expr", _raw(props))])
        _flag("properties_block", typ)
    # Python escape hatch (callback / module / code) -> preserved + flagged
    py = OrderedDict()
    for tag, key in (("PythonCallback", "callback"), ("PythonName", "module")):
        t = _txt(oc, tag)
        if t:
            py[key] = t
    code = _python_code(oc.find("Python"))   # inline <Python> body -- dedented so it compiles (see helper)
    if code:
        py["code"] = code
    if py:
        p["python"] = py
        _flag("python_escape", typ)
    # LEFTOVER detection: any unconsumed <Outcome> child -> _unmapped + flag (no silent loss)
    for c in oc:
        if c.tag not in _OC_HANDLED:
            p.setdefault("_unmapped", OrderedDict())[c.tag] = _raw(c)
            _flag("unmapped_outcome_tag:%s" % c.tag, typ)
    ordered = OrderedDict((k, p[k]) for k in _OC_ORDER if k in p)
    for k in p:
        if k not in ordered:
            ordered[k] = p[k]
    return ordered


def emit_outcomes(typ, rec):
    """KillOutcomes -> `kill` (array of outcome payloads); Actions/Action -> `actions` (array of
    {mission, requires?, chance?, <verb payloads>} -- the {MISSION_X:[...]} nesting flattened to a
    `mission` key). A single ActionOutcomes/Outcome merges onto the action; multiple -> `outcomes:[...]`."""
    out = OrderedDict()
    ko = rec.find("KillOutcomes")
    if ko is not None:
        kills = [_outcome_payload(oc, typ) for oc in ko.findall("Outcome")]
        if kills:
            out["kill"] = kills
    acts = rec.find("Actions")
    if acts is not None:
        arr = []
        for a in acts.findall("Action"):
            ao = OrderedDict()
            m = _txt(a, "MissionType")
            if m:
                ao["mission"] = m
            if _bool(a, "bKill"):                 # Action-level: performing the mission kills the unit
                ao["consumes"] = True
            ic = _int(a, "iCost")                 # Action-level cost (no approved verb -> flagged clean key)
            if ic is not None and ic != 0:
                ao["cost"] = ic
                _flag("action_field_no_verb:iCost->cost", typ)
            for c in a:                           # action-level leftover tags -> _unmapped + flag
                if c.tag not in ("MissionType", "ActionOutcomes", "bKill", "iCost"):
                    ao.setdefault("_unmapped", OrderedDict())[c.tag] = _raw(c)
                    _flag("unmapped_action_tag:%s" % c.tag, typ)
            aon = a.find("ActionOutcomes")
            payloads = [_outcome_payload(oc, typ) for oc in aon.findall("Outcome")] if aon is not None else []
            if len(payloads) == 1:
                for k, v in payloads[0].items():
                    ao[k] = v
            elif len(payloads) > 1:
                ao["outcomes"] = payloads
            arr.append(ao)
        if arr:
            out["actions"] = arr
    # MISSION_CONSTRUCT: the unit's <Buildings> = the buildings it can construct via a mission. Emitted as the
    # `constructs` OUTCOME verb (owner 2026-07-21, json.md §8) -- ONE action per building -- NOT grants.buildings
    # (a construct is a mission-action producing an outcome, not a one-shot provision).
    for b in _typelist(rec, "Buildings"):
        out.setdefault("actions", []).append(OrderedDict([("mission", "MISSION_CONSTRUCT"), ("constructs", b)]))
    return out or None


def pass2(typ, rec, store, fams, caps, grants, vision, identity):
    """vs-keyed combat, vision/LOS, outcomes, GP-action grants, properties, BonusProductionModifiers, cargo."""
    su = fams.setdefault  # noqa
    # vs-keyed combat -> combat.unit.<kw>.{TYPE}[.member].percent (strength MODIFIERS -> combat, ruling 5)
    for tag, (kw, member) in VS_KEYED.items():
        node = rec.find(tag)
        if node is None:
            continue
        for k, v in _pairs(node):
            base = fams.setdefault("combat", OrderedDict()).setdefault("unit", OrderedDict()).setdefault(kw, OrderedDict()).setdefault(k, OrderedDict())
            if member:
                base = base.setdefault(member, OrderedDict())
            base["percent"] = v
    # targeting/immunity per-type lists are NOT skills (they carry a value -- the TYPE): owner 2026-07-20, skills are
    # pure boolean ENABLERS. Route to the combat family, keyed by type -- combat data, not a skill.
    # FLAG: this family placement is a reasonable combat home pending owner confirmation of the exact shape.
    for tag, name in CAP_LIST.items():
        if name is None:
            continue
        lst = _typelist(rec, tag)
        if lst:
            node = fams.setdefault("combat", OrderedDict()).setdefault("unit", OrderedDict()).setdefault(name, OrderedDict())
            for x in lst:
                node[x] = True
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
    # outcomes (KillOutcomes / Actions) -> the clean VERB-PER-PAYLOAD vocabulary (owner-approved; emit_outcomes
    # above). DATA-SHAPE ONLY -- the CvOutcome ENGINE stays XML and does NOT read this block yet.
    oc = emit_outcomes(typ, rec)
    if oc:
        identity["_outcomes"] = oc   # placed under a top-level `outcomes` in curate()
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
    # cargo CAPACITY + WHAT it carries -> cargo.unit.space.{unit: <predicate>?, flat} ([modifier.md] par.6).
    # An unrestricted hold is just cargo.space.flat; a restricted one qualifies by a unit PREDICATE. All three
    # legacy restrictions are the SAME qualifier shape, which is the whole reason the special-group one needed no
    # new form (owner: "that is what TAGS are for"):
    #   DomainCargo       -> IS_<DOMAIN>      (you can't transport a plane on a landing craft)
    #   SpecialCargo      -> IS_<TAG>         the group's membership tag
    #   SMNotSpecialCargo -> !IS_<TAG>        the same tag, negated (json par.3.4's ! on a single leaf)
    # Several compose through an `all` node rather than a bespoke multi-field shape.
    icargo = _int(rec, "iCargo")
    preds = []
    dom = _txt(rec, "DomainCargo")
    if dom and dom.startswith("DOMAIN_"):
        preds.append("IS_" + dom[len("DOMAIN_"):])
    spec = _txt(rec, "SpecialCargo")
    if spec and spec.startswith("SPECIALUNIT_"):
        preds.append("IS_" + specialunit_tag(spec).upper())
    notspec = _txt(rec, "SMNotSpecialCargo")
    if notspec and notspec.startswith("SPECIALUNIT_"):
        preds.append("!IS_" + specialunit_tag(notspec).upper())
    if icargo:
        space = OrderedDict()
        if len(preds) == 1:
            space["unit"] = preds[0]
        elif preds:
            space["unit"] = OrderedDict([("all", preds)])
        space["flat"] = icargo
        fams.setdefault("cargo", OrderedDict()).setdefault("unit", OrderedDict())["space"] = space
    elif preds:
        # A restriction with no capacity to qualify carries nothing -- say so rather than drop it silently.
        _flag("cargo_restriction_no_capacity", typ)
    # <Capture> -- the unit you GET for capturing this one. ⚖ The capture family carries BOTH what you get and
    # the odds (owner), so the result lands beside capture.unit.probability / .resistance rather than sitting in
    # identity, which carries no effects (json par.7).
    becomes = _txt(rec, "Capture")
    if becomes and becomes.startswith("UNIT_"):
        fams.setdefault("capture", OrderedDict()).setdefault("unit", OrderedDict())["becomes"] = becomes
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
    # EVERY unit carries a BASE SIGHT (owner): one plot of open ground, the unit's own contribution and
    # nothing else -- elevation belongs to the ground it happens to stand on and is added at read, and
    # promotions expand this. Authored on every unit rather than defaulted in the engine so that making a
    # scout see further is a data edit (200) and not a special case.
    vision["unit"] = OrderedDict([("flat", VISION_PLOT)])

    # --- identity.base (create-unit foundation) ---
    for tag, key in BASE.items():
        v = _int(rec, tag)
        if v is not None and v != 0:
            base[key] = v
    # base STRENGTH + base MOVES are MODIFIER FAMILIES (owner 2026-07-20), not identity.base scalars. A unit that
    # cannot attack/defend has NO strength block at all (absent, never 0 -- membership, like skills/tags).
    # `strength` = ONLY this base value (ruling 5); every strength MODIFIER authors in the `combat` family.
    ic = _int(rec, "iCombat")
    if ic:
        _set_fam(fams, "strength", None, "flat", ic)
    im = _int(rec, "iMoves")
    if im:
        _set_fam(fams, "movement", None, "flat", im)
    # the unit's combat CLASSES -> ROOT, not identity (owner 2026-07-20): primary <Combat> + <SubCombatTypes>.
    combat_class = _txt(rec, "Combat")
    sub_combats = _typelist(rec, "SubCombatTypes")
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
    # collateralImmune -> boolean SKILL (owner 2026-07-20): immune to the siege-variant collateral. The legacy
    # UnitCombatCollateralImmunes keys the source (SIEGE/ASSAULT_MECH/ROBOT -- all the siege variant, never
    # MOUNTED-flanking), so it collapses to one pure enabler; flankImmune is not needed.
    if _typelist_struct(rec, "UnitCombatCollateralImmunes", "UnitCombatType"):
        caps["collateralImmune"] = True
    # dcmAirBomb: DEAD (owner 2026-07-20 -- DCM air bombing is slated for removal); DROPPED, not emitted. It also
    # carried a COUNT, and a skill that carries a value is not a skill (skills are pure boolean enablers).
    # --- tags (derived classification; greenfield first pass — see TAG_BY_UNITAI) ---
    # A specific DefaultUnitAI role (worker/spy/merchant/…) classifies the unit and SUPPRESSES `military` (owner:
    # a spy just needs `spy`, not military — the military flags over-fire on non-combat roles). `military` (the
    # IS_MILITARY signal) is the fallback for combat units that have no specific role.
    # The signal is ANY of the three legacy military flags (support/production/happiness) -- skills.md §3 unifies all
    # three onto the ONE `military` tag / IS_MILITARY predicate (the 1276/1325/1007 sets DIFFER: ~69 units carry
    # production/happiness but NOT support, so deriving from support alone dropped their military membership entirely).
    uai = _txt(rec, "DefaultUnitAI")
    if uai in TAG_BY_UNITAI:
        for t in TAG_BY_UNITAI[uai]:
            tags[t] = True
    elif _bool(rec, "bMilitarySupport") or _bool(rec, "bMilitaryProduction") or _bool(rec, "bMilitaryHappiness"):
        tags["military"] = True
    # domain -> a membership TAG (owner 2026-07-20): landUnit/seaUnit/airUnit. The DOMAIN_* enum stays in
    # identity.domain for the engine (movement/stacking); the tag is the classification view.
    dom_tag = {"DOMAIN_LAND": "landUnit", "DOMAIN_SEA": "seaUnit", "DOMAIN_AIR": "airUnit"}.get(_txt(rec, "Domain"))
    if dom_tag:
        tags[dom_tag] = True
    # the SPECIALUNIT_* group is type-derived MEMBERSHIP -> a TAG (json par.8), never identity: it carries no
    # value and exists to be QUERIED, which is exactly what a carrier's cargo restriction does (IS_<TAG> above).
    spec_grp = _txt(rec, "Special")
    if spec_grp and spec_grp.startswith("SPECIALUNIT_"):
        tags[specialunit_tag(spec_grp)] = True
    # NB the unit does NOT bake its combat classes' identity tags (mounted/gunpowder/naval/outlaw/...). Those are
    # authored ON the UNITCOMBAT and the engine unions them into the unit at load
    # (CvUnitInfo::deriveAtRegistryComplete over primary + combatClasses), so the fact has ONE home. What stays
    # here is what the UNIT itself is: its role, its military membership and its domain.
    # --- grants (lists) ---
    for tag, key in GRANT_LIST.items():
        lst = _typelist(rec, tag)
        if lst:
            grants[key] = lst
    # --- spread: the unit's per-religion / per-corporation SPREAD STRENGTH (a standing capability, NOT a timed grant;
    # owner 2026-07-11). Its OWN self-documenting block so a modder reads spread.religion / spread.corporation. ---
    spread = {}
    rel = _keyed_int(rec, "ReligionSpreads", "ReligionType", "iReligionSpread")
    if rel:
        spread["religion"] = rel
    corp = _keyed_int(rec, "CorporationSpreads", "CorporationType", "iCorporationSpread")
    if corp:
        spread["corporation"] = corp
    if spread:
        out["spread"] = spread
    # --- groupSpawn: the unit's group-SPAWN config (pack combat class + chance + title) -- its own block, struct rows
    # (owner 2026-07-11); a flat _typelist dropped iChance + Title. ---
    gs = []
    gsnode = rec.find("GroupSpawnUnitCombatTypes")
    if gsnode is not None:
        for c in gsnode:
            uc = engine.text(c.find("UnitCombatType")) if c.find("UnitCombatType") is not None else None
            if not uc or uc == "NONE":
                continue
            row = {"unitCombat": uc}
            ch = c.find("iChance")
            if ch is not None and engine.is_int(engine.text(ch)):
                row["chance"] = int(engine.text(ch))
            ti = engine.text(c.find("Title")) if c.find("Title") is not None else None
            if ti and ti != "NONE":
                row["title"] = ti
            gs.append(row)
    if gs:
        out["groupSpawn"] = gs
    # --- builds (the unit's per-type build REPERTOIRE -> top-level `builds`, NOT a grant; owner ruling) ---
    blds = _typelist(rec, "Builds")
    if blds:
        out["builds"] = blds
    if _bool(rec, "bGoldenAge"):
        grants["goldenAge"] = True
    # --- settler-grants-buildings: a FOUNDER (bFound) seeds its new city with the NewCityFree set (+ Palace),
    # each gated by its condition; relocated off the buildings (owner 2026-06-16; GitHub #7). ---
    if _bool(rec, "bFound"):
        # plain grants.buildings (ruling 8 / json.md §5): the settler's considered action IS founding, so no
        # bespoke foundBuildings key -- entry form {building, enabled?} unchanged.
        grants["buildings"] = found_buildings(store)
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
    # --- game options: the ENTITY-LEVEL enabled/disabled gate (owner 2026-07-08) -- emitted beside requires. ---
    # --- art ---
    for tag in ART:
        put_art(art_blocks, tag, engine.text(rec.find(tag)))
    # world.art.define -- the on-map 3D art. A unit's art lives in <UnitMeshGroups> as per-era art-define tags; the
    # ART_DEF_* id (resolved to the NIF model + textures by ARTFILEMGR) is the PRIMARY mesh group's first non-empty
    # era band (Early first, matching the engine getArtInfo fall-through default). Only the tag id goes to JSON
    # (json.md §7); the definition stays in CIV4ArtDefines_Unit.xml. Fixes the createUnitEntity NULL-artinfo crash.
    mg = rec.find("UnitMeshGroups/UnitMeshGroup")
    if mg is not None:
        for band in ("EarlyArtDefineTag", "ClassicalArtDefineTag", "MiddleArtDefineTag", "RennArtDefineTag",
                     "IndustrialArtDefineTag", "LateArtDefineTag", "FutureArtDefineTag"):
            tagv = engine.text(mg.find(band))
            if tagv:
                put_art(art_blocks, "ArtDefineTag", tagv)
                break

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
    # entity-level enabled/disabled gate (game options; owner ruling 2026-07-08 -- the one canonical form):
    # PrereqGameOption (engine canEverTrain -- option must be ON) -> enabled; NotGameOption (must be OFF) -> disabled.
    gate_entity(out,
                _typelist(rec, "PrereqGameOption") or ([_txt(rec, "PrereqGameOption")] if _txt(rec, "PrereqGameOption") else []),
                _typelist(rec, "NotGameOption") or ([_txt(rec, "NotGameOption")] if _txt(rec, "NotGameOption") else []))
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
        out["skills"] = list(caps.keys())    # pure boolean ENABLERS only (the unit mirror of capabilities) -> array
                                             # of strings; a skill carries no value. Valued/keyed abilities are NOT skills.
    out["tags"] = list(tags.keys())          # ALWAYS present (json.md §8): immutable membership, empty array if none
    if combat_class:
        out["combatClass"] = combat_class    # PRIMARY combat class -> ROOT (owner 2026-07-20), not identity
    if sub_combats:
        out["combatClasses"] = sub_combats   # SUB combat classes -> ROOT, not identity
    # Collapse the per-type invisibility tables onto the vision family FIRST (vision.md §4), then merge
    # whatever the mechanic did not claim -- the collapse writes vision.unit, so a bare assign would
    # overwrite it.
    collapse_hide_and_seek(out, vision, True)
    merge_vision(out, vision)
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
    emit_sizematters(out, lambda t: _int(rec, t), combatmod=SM_COMBATMOD_UNIT)   # unit's own SM per-rank combat mods
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
    # combat modifiers a special-unit group confers on its members (e.g. SPECIALUNIT_CAPTIVE -5% combat / -10 withdrawal
    # -- a transport carrying a captive fights worse). Previously dropped (only the bools were read); the combat/
    # withdrawal unit families are the faithful home (matching the unit curator; ruling 5).
    cp = engine.text(rec.find("iCombatPercent"))
    if cp and engine.is_int(cp) and int(cp) != 0:
        # a percent MODIFIER on member strength -> the combat family (ruling 5; strength = base value only)
        out["combat"] = OrderedDict([("unit", OrderedDict([("percent", int(cp))]))])
    wc = engine.text(rec.find("iWithdrawalChange"))
    if wc and engine.is_int(wc) and int(wc) != 0:
        out["withdrawal"] = OrderedDict([("unit", OrderedDict([("percent", int(wc))]))])
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7) -- the special-unit path too
    return out


HANDLED = (set(BASE) | set(UNIT_FAMILIES) | set(CAP_BOOL) | set(CAP_COUNT) | set(DCM_AIRBOMB) | set(GRANT_LIST)
           | set(COST) | set(ID_SCALAR) | set(ID_LIST) | set(TEXT) | set(ART) | REQUIRES_TAGS | STORE_TAGS
           | PASS2_TAGS | {"Buildings",   # -> outcomes.actions[] `constructs` verb (MISSION_CONSTRUCT, owner 2026-07-21)
                           "Type", "Combat", "SubCombatTypes",   # -> root combatClass / combatClasses (owner 2026-07-20)
                           "iCombat", "iMoves",   # -> strength / movement families (owner 2026-07-20)
                           "Flavors", "iAIWeight", "iInstanceCostModifier", "bGoldenAge",
                           "DefaultUnitAI", "UnitMeshGroups", "FreePromotions", "Builds",
                           "ReligionSpreads", "CorporationSpreads",   # -> spread.religion / spread.corporation (own block)
                           "GroupSpawnUnitCombatTypes",   # -> groupSpawn (own block, struct rows)
                           "iAnimalIgnoresBorders",   # DROPPED: pure game-option runtime, not curated (owner 2026-07-11)
                           "bSpy",   # dropped: redundant with the 'spy' tag (every bSpy unit is UNITAI_SPY); spy isn't a skill (owner 2026-06-23)
                           # DCM RANGE BOMBARD ruled FULLY REMOVED (structural-cleanup.md Tier 2) -- all dropped:
                           "iRBombardDamage", "iRBombardDamageLimit", "iDCMBombRange", "iDCMBombAccuracy",
                           "bRBombardForceAbility",
                           # reclassified to IS_MILITARY (json §3.5) -- military-ness is the `military` tag, not a skill
                           # (bMilitarySupport's IS_MILITARY tag-signal read at ~656 stays intact -- only its skill emit is dropped)
                           "bMilitaryHappiness", "bMilitaryProduction", "bMilitarySupport"})


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

    if _FLAGS:
        print("\nOUTCOME/ACTION FLAGGED (emitted under a flagged key -- NOT dropped):")
        for cat in sorted(_FLAGS):
            cnt, samp = _FLAGS[cat]
            print("  %-45s %6d  e.g. %s" % (cat, cnt, ", ".join(sorted(samp)[:3])))

    has = lambda k: sum(1 for o in results.values() if k in o)
    STRUCT = {"type", "description", "civilopedia", "help", "enables", "obsoletes", "requires", "allowed", "skills", "tags",
              "vision", "outcomes", "grants", "succession", "cost", "ai", "enabled", "disabled", "ui", "world", "sound", "identity"}
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
