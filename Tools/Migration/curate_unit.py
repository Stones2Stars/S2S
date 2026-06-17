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
from curate_common import put_art, emit_art, FAMILY_ORDER, de_i
from store import Store, REPO

# ---- identity.base: the create-unit FOUNDATION (§0.6) ----
BASE = {
    "iCombat": "combat", "iMoves": "moves", "iWorkRate": "workRate", "iAirCombat": "airCombat",
    "iCargo": "cargo", "iCombatLimit": "combatLimit", "iAirCombatLimit": "airCombatLimit",
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
    "iAirRange": ("air", "range", "flat"),
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
    "bSabotage": "sabotage", "bSpy": "spy", "bStateReligion": "stateReligion", "bStealPlans": "stealPlans",
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
    "PrereqAndHeritage", "PrereqOrHeritage", "iMinAreaSize", "StateReligion", "EnabledCivilizationTypes",
    "TrainCondition", "PrereqGameOption", "NotGameOption", "iMaxGlobalInstances", "iMaxPlayerInstances",
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


def _atom(typ, scope, **kw):
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
    vb = _txt(rec, "VicinityBonusType")
    if vb:
        allc.append(_atom(vb, "city", connection="vicinity"))
    ovb = _typelist_struct(rec, "PrereqVicinityBonuses", "BonusType")
    if ovb:
        anyc.append([_atom(x, "city", connection="vicinity") for x in ovb])
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
        allc.append(_atom(corp, "city"))
    orciv = _typelist_struct(rec, "PrereqOrCivics", "PrereqCivic") or _typelist(rec, "PrereqOrCivics")
    if orciv:
        anyc.append([_atom(x, "empire") for x in orciv])
    if _bool(rec, "StateReligion"):
        allc.append("HAS_STATE_RELIGION")
    ms = _int(rec, "iMinAreaSize")
    if ms and ms > 0:
        allc.append(_atom("AREA_SIZE", "world", min=ms))
    # --- instance caps are NOT a requires SELF-atom (owner 2026-06-17): they move to the declarative `allowed`
    # cap (authored by allowed_unit() below). SELF leaves requires entirely; uniform with Building/Tech/CultureLevel.
    # enabler-spec §5a/§13.7. ---
    # --- TrainCondition BoolExpr -> build (checked at canTrain, CvCity.cpp:1961-1963). Folded via the shared
    # boolexpr converter (And/Or of Has over bonus/building + the one ATTRIBUTE_POPULATION>=N case). owner 2026-06-16. ---
    boolexpr.merge_into(boolexpr.convert_field(rec.find("TrainCondition")), allc, anyc, none)
    build = OrderedDict()
    if allc:
        build["all"] = allc
    if anyc:
        build["any"] = anyc
    if none:
        build["noneOf"] = none
    return {"build": build} if build else None


def allowed_unit(rec):
    """The declarative INSTANCE CAP (owner 2026-06-17): `allowed:{<scope>:N}` — the real cap number, scope-keyed
    (world/empire), NOT a `requires` SELF-atom. Engine enforces (build while tally.count(SELF,scope) < N) and owns
    ignoring it (NO_NATIONAL_UNIT_LIMIT, honoring the per-unit `identity.unlimitedException` exception) + era-scaling
    the base + `+extra` — none touch the parser. A unique unit -> `allowed:{empire:1}`. enabler-spec §5a/§13.7."""
    allowed = OrderedDict()
    for tag, scope in (("iMaxGlobalInstances", "world"), ("iMaxPlayerInstances", "empire")):
        v = _int(rec, tag)
        if v is not None and v >= 0:
            allowed[scope] = v
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
# GP-action magnitudes -> grants (one-time great-person actions; base + multiplier)
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
    # cargo (which the unit carries) -> identity.cargo (capacity is base; the carry-kinds are config)
    cargo = OrderedDict()
    for tag, key in (("DomainCargo", "domain"), ("SpecialCargo", "special"), ("SMNotSpecialCargo", "smNotSpecial")):
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
    grants = OrderedDict()
    succession = OrderedDict()
    identity = OrderedDict()
    base = OrderedDict()
    cost = OrderedDict()
    ai = OrderedDict()
    art_blocks = OrderedDict()
    loadprune = OrderedDict()
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
    sup = _typelist(rec, "SupersedingUnits")
    if sup:
        succession["supersededBy"] = sup
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
    # --- loadPrune (game options) ---
    for tag, key in (("PrereqGameOption", "onGameOptions"), ("NotGameOption", "notOnGameOptions")):
        lst = _typelist(rec, tag) or ([_txt(rec, tag)] if _txt(rec, tag) else [])
        lst = [x for x in lst if x]
        if lst:
            loadprune[key] = lst
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
    requires = requires_unit(rec, store)
    if requires:
        out["requires"] = requires
    allowed = allowed_unit(rec)
    if allowed:
        out["allowed"] = allowed
    ordered = [f for f in FAMILY_ORDER if f in fams] + [f for f in fams if f not in FAMILY_ORDER]
    for f in ordered:
        out[f] = fams[f]
    if caps:
        out["capabilities"] = caps
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
    if loadprune:
        out["loadPrune"] = loadprune
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
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
    return out


HANDLED = (set(BASE) | set(UNIT_FAMILIES) | set(CAP_BOOL) | set(CAP_COUNT) | set(DCM_AIRBOMB) | set(GRANT_LIST)
           | set(COST) | set(ID_SCALAR) | set(ID_LIST) | set(TEXT) | set(ART) | REQUIRES_TAGS | STORE_TAGS
           | PASS2_TAGS | {"Type", "Combat", "Flavors", "iAIWeight", "iInstanceCostModifier", "bGoldenAge",
                           "DefaultUnitAI", "UnitMeshGroups", "FreePromotions"})


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
    STRUCT = {"type", "description", "civilopedia", "help", "enables", "obsoletes", "requires", "allowed", "capabilities",
              "vision", "outcomes", "grants", "succession", "cost", "ai", "loadPrune", "ui", "world", "sound", "identity"}
    seen = sorted({f for o in results.values() for f in o if f not in STRUCT})
    print("UnitInfo curated: %d  | SpecialUnitInfo: %d" % (n, len(su_results)))
    for k in ("enables", "obsoletes", "requires", "allowed", "capabilities", "grants", "succession", "cost", "identity"):
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
