#!/usr/bin/env python3
"""Curate Promotion (#428, Tier D #28) — a UNIT-PLANE stat SOURCE. Every effect is a `unit`-scope
SELF-ACCUMULATOR deposit (source==target via CvUnit::processPromotion's changeExtra*/change*Count stack,
modifier-spec §5). Shares the unit-stat vocabulary with UnitCombat #29 + SpecialUnit (Unit pass).

VOCABULARY (owner-blessed 2026-06-16; names PROVISIONAL — a reader-pass refines them, owner):
- MODIFIER families (changeExtra*-verified, additive): `strength` is THE combat family — "the strength of
  something, or weakness ON / INTO / AGAINST something" (owner) — so it holds the general combat %, flat
  strength, the SM modifiers, the situational (city/hills) AND the vs-keyed (terrain/feature/unitCombat/domain/
  flanking) modifiers. Plus `withdrawal` `firstStrike` `bombard` `collateral` `air` `heal` `movement`
  `experience` `workRate` `cargo` `upkeep` `vision` `capture` `poison` `espionage` `trap` + small singletons
  (`revoltProtection`/`pillage`/`survivor`). per-PROPERTY via property_source_v3.
- CAPABILITIES = a SEPARATE group of BOOLEANS (owner): grant=true / revoke=false (the Add/Subtract & Remove
  pairs). change*Count-verified. Includes the count-int abilities (excile/passage/… >0 => the unit HAS it) +
  the per-type ability lists (terrain/featureDoubleMove, trap immunity/target).
- VISION/LOS RESOLVER (non-cascade, §7/§0.6): the 2D Invisible/Visible{Terrain,Feature,Improvement}[Range]
  tables + the Visibility/InvisibilityIntensity[Range] pair-lists + NegatesInvisibility -> a structured `vision`
  block read by the visibility resolver, NOT additive families.

AVAILABILITY (owner #4 2026-06-16): modeled on the PROMOTIONLINE, not the promotion. Tech/Bonus/PlotBonus
prereqs -> store (tech/bonus `enables.promotions`), DROPPED here. A promotion-on-promotion prereq
(PromotionPrereq/Or1/Or2) -> the enabling lives on the TECH it requires; the chain ORDER is carried by
PromotionLine+iLinePriority -> so PromotionPrereq is DROPPED (line+priority+tech carry it). The promotion's own
applicability/plot/state gates (UnitCombats/NotOnDomain/NotOnUnitCombat, Prereq{Terrain,Feature,Improvement,
PlotBonus,LocalBuilding}, StateReligion/Era/Level, cargo/rBombard/normInvisible prereqs) are PARKED in identity,
deferred to the unit-plane enabling pass (as PromotionLine #27 parked its gates — the broader enabling picture
is still being shaped, owner). OnGameOptions/NotOnGameOptions -> the ENTITY-LEVEL `enabled`/`disabled` gate
(owner ruling 2026-07-08 -- `enabled: GAMEOPTION_X`; the retired `loadPrune` invention's replacement).

grants: SubCombatChangeTypes (confers a unitcombat), RemovesUnitCombatTypes, FreetoUnitCombats, AddsBuildTypes,
setSpecialUnit -> things the promotion confers on the unit.
identity: PromotionLine+linePriority (the chain), command (controlPoints/commandRange/commandType/leader),
RenamesUnitTo, ReplacesUnitCombat, layerAnimationPath, the status-promotion flags, zeroesXP (the XP-reset cost
of a quality upgrade), + the parked availability gates above.
celebrity: the numeric per-unit celebrity-happiness AMOUNT (iCelebrityHappy) is DROPPED (owner 2026-07-01: "not a
random field on a unit"); celebrity becomes a boolean SKILL (skills.celebrity=true when iCelebrityHappy != 0),
and CvCity is fixed POST-MIGRATION to scan for celebrity-skilled units and award the happiness itself.
art: Button -> ui.art.icon ; Sound -> sound.sound. ObsoleteTech -> store tech.obsoletes.promotions (new edge).

DROPS: BATTLEWORN trio iDamageperTurn/iStrAdjperTurn/iWeakenperTurn (nuked, owner — not even applied in
processPromotion, pedia-only) ; Categories (dead) ; Qualified/DisqualifiedUnitCombatTypes (pedia-derived,
doPostLoadCaching). EXE-link: 0 DllExport on CvPromotionInfo -> unconstrained.

  python3 curate_promotion.py --sample PROMOTION_COMBAT1 PROMOTION_WARMTH3 PROMOTION_CRUCIBLE_STEEL
  python3 curate_promotion.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import (put_art, emit_art, FAMILY_ORDER, de_i, descale100, fold_text_to_identity, gate_entity,
                           emit_sizematters, SM_FLAT_CHANGE, SM_COMBATMOD_CHANGE, SM_CARGO_CHANGE)
from store import Store, REPO

# ---- scalar deposits: tag -> (family, member|None, unit). All at `unit` scope (self-accumulator, §5). ----
# strength = the combat family (general % + flat + SM mods + situational + the TB/S&D sub-stats).
STRENGTH = {
    "iCombatPercent":               (None, "percent"),        # general combat strength %
    "iStrengthChange":              (None, "flat"),           # flat strength points
    "iAttackCombatModifierChange":  ("attack", "percent"),
    "iDefenseCombatModifierChange": ("defense", "percent"),
    "iVSBarbsChange":               ("vsBarbs", "percent"),
    "iReligiousCombatModifierChange": ("religious", "percent"),
    "iStealthCombatModifierChange": ("stealth", "percent"),
    "iDamageModifierChange":        ("damageModifier", "percent"),
    "iEnduranceChange":             ("endurance", "flat"),
    "iTauntChange":                 ("taunt", "flat"),
    "iBreakdownChanceChange":       ("breakdownChance", "flat"),
    "iBreakdownDamageChange":       ("breakdownDamage", "flat"),
    "iUnnerveChange":               ("unnerve", "percent"),    # S&D (GAMEOPTION_COMBAT_SURROUND_DESTROY, live/deferred)
    "iEncloseChange":               ("enclose", "percent"),    # S&D
    "iLungeChange":                 ("lunge", "percent"),      # S&D
    "iDynamicDefenseChange":        ("dynamicDefense", "percent"),  # S&D
    "iCityAttack":                  ("cityAttack", "percent"),
    "iCityDefense":                 ("cityDefense", "percent"),
    "iHillsAttack":                 ("hillsAttack", "percent"),
    "iHillsDefense":                ("hillsDefense", "percent"),
    "iKamikazePercent":             ("kamikaze", "percent"),
    "iCombatLimitChange":           ("combatLimit", "flat"),
    "iStealthStrikesChange":        ("stealthStrikes", "flat"),
}
# other families: tag -> (family, member|None, unit)
FAMILIES = {
    "iWithdrawalChange":            ("withdrawal", None, "percent"),
    "iFirstStrikesChange":          ("firstStrike", "strikes", "flat"),
    "iChanceFirstStrikesChange":    ("firstStrike", "chance", "flat"),
    "iBombardRateChange":           ("bombard", "rate", "percent"),
    # iRBombard*/iDCMBomb*Change: DCM RANGE BOMBARD ruled FULLY REMOVED (structural-cleanup.md Tier 2) -- DROPped.
    "iCollateralDamageChange":      ("collateral", "damage", "percent"),
    "iCollateralDamageLimitChange": ("collateral", "limit", "flat"),
    "iCollateralDamageMaxUnitsChange": ("collateral", "maxUnits", "flat"),
    "iCollateralDamageProtection":  ("collateral", "protection", "percent"),
    "iAirRangeChange":              ("air", "range", "flat"),
    "iInterceptChange":             ("air", "intercept", "percent"),
    "iEvasionChange":               ("air", "evasion", "percent"),
    "iAirCombatLimitChange":        ("air", "combatLimit", "flat"),
    "iEnemyHealChange":             ("heal", "enemy", "flat"),
    "iNeutralHealChange":           ("heal", "neutral", "flat"),
    "iFriendlyHealChange":          ("heal", "friendly", "flat"),
    "iSameTileHealChange":          ("heal", "sameTile", "flat"),
    "iAdjacentTileHealChange":      ("heal", "adjacentTile", "flat"),
    "iSelfHealModifier":            ("heal", "selfModifier", "percent"),
    "iNumHealSupport":              ("heal", "support", "flat"),
    "iVictoryHeal":                 ("heal", "victory", "flat"),
    "iVictoryAdjacentHeal":         ("heal", "victoryAdjacent", "flat"),
    "iVictoryStackHeal":            ("heal", "victoryStack", "flat"),
    "iMovesChange":                 ("movement", "moves", "flat"),
    "iMoveDiscountChange":          ("movement", "moveDiscount", "flat"),
    "iExtraDropRange":              ("movement", "dropRange", "flat"),
    "iExperiencePercent":           ("experience", None, "percent"),
    "iWorkRateModifier":            ("workRate", "rate", "percent"),
    "iHillsWorkModifier":           ("workRate", "hills", "percent"),
    "iPeaksWorkModifier":           ("workRate", "peaks", "percent"),
    "iCargoChange":                 ("cargo", "space", "flat"),
    "iUpkeepModifier":              ("upkeep", "modifier", "percent"),
    "iExtraUpkeep100":              ("upkeep", "extra", "flat"),       # x100 legacy (calcUpkeep100) -> de-scaled to human in the applier (tag endswith 100); member renamed off the 100 (cold-modder)
    "iUpgradeDiscount":             ("upkeep", "upgradeDiscount", "percent"),
    "iVisibilityChange":            ("vision", "range", "flat"),
    "iCaptureProbabilityModifierChange":   ("capture", "probability", "flat"),
    "iCaptureResistanceModifierChange":    ("capture", "resistance", "flat"),
    "iPoisonProbabilityModifierChange":    ("poison", "probability", "flat"),  # afflictions (inert), kept-for-now
    "iInsidiousnessChange":         ("espionage", "insidiousness", "flat"),    # revolutions (owner)
    "iInvestigationChange":         ("espionage", "investigation", "flat"),    # crime (owner)
    "iRevoltProtection":            ("revoltProtection", None, "percent"),
    "iPillageChange":               ("pillage", None, "flat"),
    "iSurvivorChance":              ("survivor", None, "percent"),
}
# vs-keyed pair-lists: tag -> (family, "<keyword>.{TYPE}.<member>", unit). member None => family.unit.<kw>.{TYPE}.<unit>.
VS_KEYED = {
    "TerrainAttacks":   ("strength", "terrain", "attack", "percent"),
    "TerrainDefenses":  ("strength", "terrain", "defense", "percent"),
    "FeatureAttacks":   ("strength", "feature", "attack", "percent"),
    "FeatureDefenses":  ("strength", "feature", "defense", "percent"),
    "UnitCombatMods":   ("strength", "unitCombat", None, "percent"),
    "DomainMods":       ("strength", "domain", None, "percent"),
    "FlankingStrikesbyUnitCombatChange": ("strength", "flanking", None, "percent"),
    "TerrainWorks":     ("workRate", "terrain", None, "percent"),
    "FeatureWorks":     ("workRate", "feature", None, "percent"),
    "BuildWorkRateModifierChangeTypes": ("workRate", "build", None, "percent"),
}
# CAPABILITIES (separate boolean group). plain bool -> name:true.
CAP_BOOL = {
    "bBlitz": "blitz", "bAmphib": "amphib", "bRiver": "river", "bEnemyRoute": "enemyRoute",
    "bAlwaysHeal": "alwaysHeal", "bHillsDoubleMove": "hillsDoubleMove",
    "bImmuneToFirstStrikes": "immuneToFirstStrikes", "bDefensiveVictoryMove": "defensiveVictoryMove",
    "bFreeDrop": "freeDrop", "bOffensiveVictoryMove": "offensiveVictoryMove", "bOneUp": "oneUp",
    "bPillageEspionage": "pillageEspionage", "bPillageMarauder": "pillageMarauder",
    "bPillageOnMove": "pillageOnMove", "bPillageOnVictory": "pillageOnVictory",
    # canPassPeaks (was canMovePeaks): dual-plane same-name ruling (owner 2026-07-02, capabilities.md) -- the
    # promotion grants the unit SKILL; TECH_MOUNTAINEERING grants the empire CAPABILITY under the SAME name.
    "bPillageResearch": "pillageResearch", "bCanMovePeaks": "canPassPeaks",
    "bCanLeadThroughPeaks": "canLeadThroughPeaks", "bZoneOfControl": "zoneOfControl",
    "bOnslaughtChange": "onslaught", "bParalyze": "paralyze", "bNoSelfHeal": "noSelfHeal",
}
# grant/revoke pairs -> name: true (Add/grant) or false (Subtract/Remove).
CAP_PAIR = {
    "bStampedeChange": ("stampede", True), "bRemoveStampede": ("stampede", False),
    "bAttackOnlyCitiesAdd": ("attackOnlyCities", True), "bAttackOnlyCitiesSubtract": ("attackOnlyCities", False),
    "bIgnoreNoEntryLevelAdd": ("ignoreNoEntryLevel", True), "bIgnoreNoEntryLevelSubtract": ("ignoreNoEntryLevel", False),
    "bIgnoreZoneofControlAdd": ("ignoreZoneofControl", True), "bIgnoreZoneofControlSubtract": ("ignoreZoneofControl", False),
    "bFliesToMoveAdd": ("fliesToMove", True), "bFliesToMoveSubtract": ("fliesToMove", False),
}
# count-int abilities (>0 grants the capability; treated as a boolean grant). Some carry the meaning the owner
# explained: passage = non-combat units enter foreign land without granting military passage; excile = an
# investigation/criminal state; hiddenNationality / assassin / barbCoExist / blendIntoCity / gatherHerd / ...
CAP_COUNT = {
    "iNoDefensiveBonusChange": "noDefensiveBonus", "iGatherHerdChange": "gatherHerd",
    "iAnimalIgnoresBordersChange": "animalIgnoresBorders", "iExcileChange": "excile",
    "iPassageChange": "passage", "iNoNonOwnedCityEntryChange": "noNonOwnedCityEntry",
    "iBarbCoExistChange": "barbCoExist", "iBlendIntoCityChange": "blendIntoCity",
    "iUpgradeAnywhereChange": "upgradeAnywhere", "iHiddenNationalityChange": "hiddenNationality",
    "iAssassinChange": "assassin", "iStealthDefenseChange": "stealthDefense",
    "iDefenseOnlyChange": "defenseOnly", "iNoInvisibilityChange": "noInvisibility",
}
# per-type capability LISTS (simple list -> {TYPE: true}) and pair-lists handled in VS_KEYED above.
CAP_LIST = {
    "TerrainDoubleMoves": "terrainDoubleMove", "FeatureDoubleMoves": "featureDoubleMove",
}
# VISION / LOS RESOLVER (non-cascade, §7): pair-lists (InvisibleType -> intensity) + struct-vectors.
VISION_PAIRS = {
    "VisibilityIntensityChangeTypes": "visibilityIntensity",
    "InvisibilityIntensityChangeTypes": "invisibilityIntensity",
    "VisibilityIntensityRangeChangeTypes": "visibilityIntensityRange",
}
VISION_STRUCTS = {  # tag -> (key, [child tags in order]) ; eInvisible + eTerrain/Feature/Improvement + iIntensity
    "InvisibleTerrainChanges": ("invisibleTerrain", ["InvisibleType", "TerrainType", "iIntensity"]),
    "InvisibleFeatureChanges": ("invisibleFeature", ["InvisibleType", "FeatureType", "iIntensity"]),
    "InvisibleImprovementChanges": ("invisibleImprovement", ["InvisibleType", "ImprovementType", "iIntensity"]),
    "VisibleTerrainChanges": ("visibleTerrain", ["InvisibleType", "TerrainType", "iIntensity"]),
    "VisibleFeatureChanges": ("visibleFeature", ["InvisibleType", "FeatureType", "iIntensity"]),
    "VisibleImprovementChanges": ("visibleImprovement", ["InvisibleType", "ImprovementType", "iIntensity"]),
    "VisibleTerrainRangeChanges": ("visibleTerrainRange", ["InvisibleType", "TerrainType", "iIntensity"]),
    "VisibleFeatureRangeChanges": ("visibleFeatureRange", ["InvisibleType", "FeatureType", "iIntensity"]),
    "VisibleImprovementRangeChanges": ("visibleImprovementRange", ["InvisibleType", "ImprovementType", "iIntensity"]),
}
# grants: things the promotion confers on the unit (simple lists / scalar).
GRANT_LIST = {"FreetoUnitCombats": "freeToUnitCombats", "AddsBuildTypes": "builds"}
# skills: unitcombat membership the promotion mutates on the unit (setHasUnitCombat on acquire/lose) — a SKILL
# (mutable ability), not a grant (owner 2026-07-01). Emitted as UNITCOMBAT_* lists under the skills block,
# alongside the per-type-keyed skills (targets/collateralImmune) which are also UNITCOMBAT_* lists (skills.md §1).
SKILL_UNITCOMBAT_LIST = {"SubCombatChangeTypes": "unitCombats", "RemovesUnitCombatTypes": "removesUnitCombats"}
# identity (parked): availability/plot/state gates (deferred to the unit-plane enabling pass) + config flags.
ID_SCALAR = {"iControlPoints": "controlPoints", "iCommandRange": "commandRange",
             "LayerAnimationPath": "layerAnimationPath", "RenamesUnitTo": "renamesUnitTo",
             "ReplacesUnitCombat": "replacesUnitCombat",
             "StateReligionPrereq": "stateReligionPrereq", "MinEraType": "minEra", "MaxEraType": "maxEra",
             "iLevelPrereq": "levelPrereq", "DomainCargoChange": "domainCargoChange",
             "SpecialCargoChange": "specialCargoChange", "SpecialCargoPrereq": "specialCargoPrereq",
             "SMNotSpecialCargoChange": "smNotSpecialCargoChange", "SMNotSpecialCargoPrereq": "smNotSpecialCargoPrereq",
             "SetSpecialUnit": "setSpecialUnit", "iSMCargoVolumeChange2": None}
ID_LIST = {"UnitCombats": "unitCombats", "NotOnDomainTypes": "notOnDomains",
           "NotOnUnitCombatTypes": "notOnUnitCombats", "NegatesInvisibilityTypes": "negatesInvisibility",
           "PrereqTerrainTypes": "prereqTerrains", "PrereqFeatureTypes": "prereqFeatures",
           "PrereqImprovementTypes": "prereqImprovements", "PrereqPlotBonusTypes": "prereqPlotBonuses",
           "PrereqLocalBuildingTypes": "prereqLocalBuildings", "PrereqBonusTypes": "prereqBonuses"}
ID_BOOL = {"bLeader": "leader", "bStatus": "status", "bQuick": "quick", "bStarsign": "starsign",
           "bZeroesXP": "zeroesXP", "bRemoveAfterSet": "removeAfterSet", "bForOffset": "forOffset",
           "bSetOnHNCapture": "setOnHNCapture", "bSetOnInvestigated": "setOnInvestigated",
           "bPlotPrereqsKeepAfter": "plotPrereqsKeepAfter", "bCargoPrereq": "cargoPrereq",
           "bPrereqNormInvisible": "prereqNormInvisible"}   # bRBombardPrereq DROPs with the DCM-range removal
# DROP entirely (store-handled, dead, or pedia-derived).
DROP = {"Type", "Description", "Help", "Sound", "Button",
        "TechPrereq", "ObsoleteTech",                       # store (tech enables/obsoletes promotions)
        "PromotionPrereq", "PromotionPrereqOr1", "PromotionPrereqOr2",  # store-inverted -> the prereq promo's enables.promotions (owner 2026-07-17; the dependent drops the forward view, trait-prereq pattern)
        "iDamageperTurn", "iStrAdjperTurn", "iWeakenperTurn",  # BATTLEWORN (nuked, owner)
        "Categories",                                       # dead
        "iStealthCombatModifier",                           # XML typo (engine reads ...Change); ignored in-game (2 recs)
        # DCM RANGE BOMBARD ruled FULLY REMOVED (structural-cleanup.md Tier 2):
        "iRBombardDamageChange", "iRBombardDamageLimitChange", "iRBombardDamageMaxUnitsChange",
        "iDCMBombRangeChange", "iDCMBombAccuracyChange", "bRBombardPrereq",
        "QualifiedUnitCombatTypes", "DisqualifiedUnitCombatTypes",  # pedia-derived (doPostLoadCaching)
        # trap sub-system -- DEAD (traps removed from the game)
        "iTrapDamageMax", "iTrapDamageMin", "iTrapComplexity", "iNumTriggers",
        "TrapDisableUnitCombatTypes", "TrapAvoidanceUnitCombatTypes", "TrapTriggerUnitCombatTypes",
        "TrapImmunityUnitCombatTypes", "TargetUnitCombatTypes", "TrapSetWithPromotionTypes",
        "iTriggerBeforeAttackChange"}


def _txt(rec, tag):
    t = engine.text(rec.find(tag))
    return t if (t and t != "NONE") else None


def _int(rec, tag):
    t = engine.text(rec.find(tag))
    return int(t) if engine.is_int(t) else None


def _simple_list(node):
    out = [engine.text(c) for c in list(node)]
    return [v for v in out if v and v != "NONE"]


def _pairs(node):
    """keyed pair-list: each item has a *Type child (key) + a value child -> (TYPE, int)."""
    for item in list(node):
        key, val = None, None
        for c in item:
            if key is None and c.tag.endswith("Type"):
                key = engine.text(c)
            elif engine.is_int(engine.text(c)):
                val = int(engine.text(c))
        if key and key != "NONE" and val not in (None, 0):
            yield key, val


def _set(d, path, val):
    for k in path[:-1]:
        d = d.setdefault(k, OrderedDict())
    d[path[-1]] = val


def curate(typ, rec, store):
    out = OrderedDict([("type", typ)])
    for tag, key in (("Description", "description"), ("Help", "help")):
        t = _txt(rec, tag)
        if t:
            out[key] = t

    fams = OrderedDict()          # family -> nested dict (everything under a "unit" scope key)
    caps = OrderedDict()          # capabilities group
    vision = OrderedDict()        # vision block (range + LOS resolver)
    grants = OrderedDict()
    gate_on, gate_off = [], []   # entity-level enabled/disabled (owner 2026-07-08)
    identity = OrderedDict()
    art_blocks = OrderedDict()

    def fam_unit(family):
        return fams.setdefault(family, OrderedDict()).setdefault("unit", OrderedDict())

    # --- scalar strength members ---
    for tag, (member, unit) in STRENGTH.items():
        v = _int(rec, tag)
        if v:
            node = fam_unit("strength")
            if member:
                node = node.setdefault(member, OrderedDict())
            node[unit] = v
    # --- other scalar families ---
    for tag, (family, member, unit) in FAMILIES.items():
        v = _int(rec, tag)
        if tag.endswith("100"):                # one-time x100 -> human de-scale (cascade-fixed-point.md §2; iExtraUpkeep100)
            v = descale100(v) if v is not None else v
        if v:
            node = fam_unit(family)
            if family == "vision":
                node = vision           # route vision.range into the vision block instead
            if member:
                node = node.setdefault(member, OrderedDict())
            node[unit] = v
    # vision.range got mis-routed above only for the scalar; clean the empty stub
    if "vision" in fams and not fams["vision"]["unit"]:
        fams.pop("vision")

    # --- vs-keyed pair-lists -> family.unit.<kw>.{TYPE}[.member].unit ---
    for tag, (family, kw, member, unit) in VS_KEYED.items():
        node = rec.find(tag)
        if node is None:
            continue
        for k, v in _pairs(node):
            base = fam_unit(family).setdefault(kw, OrderedDict()).setdefault(k, OrderedDict())
            if member:
                base = base.setdefault(member, OrderedDict())
            base[unit] = v

    # --- HealUnitCombatChangeTypes (struct: UnitCombatType + iHeal + iAdjacentHeal) -> heal.unit.unitCombat.{UC} ---
    hnode = rec.find("HealUnitCombatChangeTypes")
    if hnode is not None:
        for item in list(hnode):
            uc = _txt(item, "UnitCombatType")
            heal, adj = _int(item, "iHeal"), _int(item, "iAdjacentHeal")
            if uc and (heal or adj):
                e = fam_unit("heal").setdefault("unitCombat", OrderedDict()).setdefault(uc, OrderedDict())
                if heal:
                    e["heal"] = heal
                if adj:
                    e["adjacentHeal"] = adj

    # --- capabilities ---
    for tag, name in CAP_BOOL.items():
        if engine.text(rec.find(tag)) in ("1", "true", "True"):
            caps[name] = True
    for tag, (name, grant) in CAP_PAIR.items():
        if engine.text(rec.find(tag)) in ("1", "true", "True"):
            caps[name] = grant
    for tag, name in CAP_COUNT.items():
        v = _int(rec, tag)
        if v:
            # SIGN-AWARE (2026-07-02, the PROMOTION_WANTED find): a NEGATIVE count-ability is a REVOKE
            # (iAssassinChange=-1 takes assassin AWAY) -- emit false, the CAP_PAIR revoke shape. Collapsing
            # every nonzero to true silently inverted the revokes (THUG read as an assassin).
            caps[name] = v > 0
    for tag, name in CAP_LIST.items():
        node = rec.find(tag)
        if node is not None:
            for k in _simple_list(node):
                caps.setdefault(name, OrderedDict())[k] = True
    # unitcombat membership the promotion adds/strips -> a SKILL (mutable ability), emitted as UNITCOMBAT_* lists.
    for tag, name in SKILL_UNITCOMBAT_LIST.items():
        node = rec.find(tag)
        if node is not None:
            lst = _simple_list(node)
            if lst:
                caps[name] = lst
    # celebrity: iCelebrityHappy is NUMERIC (a per-unit city-happiness amount), so it can't ride CAP_BOOL; the
    # AMOUNT is DROPPED (owner 2026-07-01: "not a random field on a unit") -> a boolean skill when non-zero.
    # CvCity is fixed POST-MIGRATION to scan for celebrity-skilled units and award the happiness itself.
    if _int(rec, "iCelebrityHappy"):
        caps["celebrity"] = True

    # --- vision / LOS resolver ---
    for tag, name in VISION_PAIRS.items():
        node = rec.find(tag)
        if node is not None:
            for k, v in _pairs(node):
                vision.setdefault(name, OrderedDict())[k] = v
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
    neg = rec.find("NegatesInvisibilityTypes")
    if neg is not None:
        lst = _simple_list(neg)
        if lst:
            vision["negates"] = lst

    # --- properties: the live crime/disease/education unit->city emission, translated to the SCOPED MODIFIER
    # system (owner 2026-06-16: "scoped like other property yields, like a modifier"). Each PropertySource ->
    # a per-PROPERTY family deposit at its GameObjectType scope (GAMEOBJECT_PLOT->plot, GAMEOBJECT_CITY->city);
    # SAME_PLOT is the containment default (dropped). A promotion emitting to both = TWO modifiers (plot + city).
    # Feeds the PromotionLine buildUp.property baseline. Via the shared engine.property_source_v3 (the standard). ---
    pm = rec.find("PropertyManipulators")
    if pm is not None:
        for src in pm.findall("PropertySource"):
            res = engine.property_source_v3(src)
            if res:
                prop, scope, unit, value = res
                node = fams.setdefault(prop, OrderedDict()).setdefault(scope, OrderedDict())
                if unit in node and isinstance(node[unit], int) and isinstance(value, int):
                    node[unit] += value           # same property+scope+unit on one promotion -> sum (cascade rule)
                else:
                    node[unit] = value

    # --- grants ---
    for tag, key in GRANT_LIST.items():
        node = rec.find(tag)
        if node is not None:
            lst = _simple_list(node)
            if lst:
                grants[key] = lst
    su = _txt(rec, "SetSpecialUnit")
    if su:
        grants["specialUnit"] = su

    # --- ai: AIWeightbyUnitCombatTypes (how the AI weights this promotion per unitcombat) -> ai group ---
    ai = OrderedDict()
    aiw = rec.find("AIWeightbyUnitCombatTypes")
    if aiw is not None:
        weights = OrderedDict()
        for item in list(aiw):
            uc = _txt(item, "UnitCombatType")
            w = _int(item, "iAIWeight")
            if uc and w is not None:
                weights[uc] = w
        if weights:
            ai["unitCombatWeights"] = weights

    # --- entity-level enabled/disabled gate (game options; owner ruling 2026-07-08) ---
    for tag, dst in (("OnGameOptions", gate_on), ("NotOnGameOptions", gate_off)):
        node = rec.find(tag)
        if node is not None:
            dst.extend(_simple_list(node) or [])

    # --- promotionLine: line membership as a {LINE: rank} OBJECT (owner 2026-06-16). iLinePriority is a RANK
    # within the line (COMBAT1=1, COMBAT2=2, ...), not a priority. The OBJECT (keyed by line) is accumulator-
    # friendly: a unit's promotions CONCATENATE by MERGING their maps into one {LINE: rank, ...} object on the one
    # unit (the §5 runtime accumulator, #430 engine) — static and accumulated share the shape. A static promotion
    # is in exactly one line -> a single-key object; ~261 of 1229 promotions have NO line -> omitted. ---
    promo_lines = OrderedDict()
    line = _txt(rec, "PromotionLine")
    if line:
        rank = _int(rec, "iLinePriority")
        promo_lines[line] = rank if rank is not None else 0

    # --- identity (parked gates + config) ---
    for tag, key in ID_SCALAR.items():
        if key is None:
            continue
        if tag == "SetSpecialUnit":
            continue   # handled as a grant
        v = _txt(rec, tag)
        iv = _int(rec, tag)
        if iv is not None and iv != 0:
            identity[key] = iv
        elif v and not engine.is_int(v):
            identity[key] = v
    for tag, key in ID_LIST.items():
        node = rec.find(tag)
        if node is not None:
            lst = _simple_list(node)
            if lst:
                identity[key] = lst
    for tag, key in ID_BOOL.items():
        if engine.text(rec.find(tag)) in ("1", "true", "True"):
            identity[key] = True
    cmd = _int(rec, "iCommandType")
    if cmd:
        identity["commandType"] = cmd

    # --- art ---
    put_art(art_blocks, "Button", engine.text(rec.find("Button")))
    put_art(art_blocks, "Sound", engine.text(rec.find("Sound")))

    # --- assemble (reserved order: type/text, requires-less, families, capabilities, vision, grants, enabled/disabled, art, identity) ---
    enables = store.enabled_by(typ)   # the promotion->promotion chain inversion (store.py ENABLE rows, owner 2026-07-17)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    obsoletes = store.obsoletes_of(typ)
    if obsoletes:
        out["obsoletes"] = OrderedDict((k, obsoletes[k]) for k in sorted(obsoletes))
    ordered = [f for f in FAMILY_ORDER if f in fams] + [f for f in fams if f not in FAMILY_ORDER]
    for f in ordered:
        out[f] = fams[f]
    if caps:
        out["skills"] = caps
    if vision:
        out["vision"] = vision
    if promo_lines:
        out["promotionLine"] = promo_lines    # line membership as [{LINE: rank}] (owner 2026-06-16); accumulator-shaped
    if grants:
        out["grants"] = grants
    if ai:
        out["ai"] = ai
    gate_entity(out, gate_on, gate_off)
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    emit_sizematters(out, lambda t: _int(rec, t), flat=SM_FLAT_CHANGE, combatmod=SM_COMBATMOD_CHANGE, cargo=SM_CARGO_CHANGE)
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("PromotionInfo")
    results = OrderedDict((typ, curate(typ, rec, store)) for typ, rec in table.items())
    n = len(results)
    # COVERAGE CHECK (verify = nothing silently dropped): report XML tags handled by NO table/special-case.
    handled = (set(STRENGTH) | set(FAMILIES) | set(VS_KEYED) | set(CAP_BOOL) | set(CAP_PAIR)
               | set(CAP_COUNT) | set(CAP_LIST) | set(SKILL_UNITCOMBAT_LIST) | set(VISION_PAIRS) | set(VISION_STRUCTS)
               | set(GRANT_LIST) | set(ID_SCALAR) | set(ID_LIST) | set(ID_BOOL) | DROP
               | set(SM_FLAT_CHANGE) | set(SM_COMBATMOD_CHANGE) | set(SM_CARGO_CHANGE)   # consumed by emit_sizematters (json.md §9) -- were mis-reported UNHANDLED
               | {"PropertyManipulators", "HealUnitCombatChangeTypes", "NegatesInvisibilityTypes",
                  "SetSpecialUnit", "OnGameOptions", "NotOnGameOptions", "iCommandType",
                  "AIWeightbyUnitCombatTypes", "PromotionLine", "iLinePriority",
                  "iCelebrityHappy"})   # -> skills.celebrity (boolean), amount dropped (owner 2026-07-01)
    from collections import Counter
    leftover = Counter()
    for _typ, rec in table.items():
        for c in rec:
            if c.tag not in handled:
                leftover[c.tag] += 1
    if leftover:
        print("UNHANDLED tags (count): %s" % ", ".join("%s=%d" % (t, c) for t, c in leftover.most_common()))
    else:
        print("COVERAGE: all XML tags handled.")
    has = lambda k: sum(1 for o in results.values() if k in o)
    STRUCT = {"type", "description", "help", "obsoletes", "skills", "vision", "promotionLine",
              "grants", "ai", "enabled", "disabled", "ui", "world", "sound", "identity"}
    seen_fams = sorted({f for o in results.values() for f in o if f not in STRUCT})
    print("PromotionInfo curated: %d" % n)
    for k in ("obsoletes", "skills", "vision", "grants", "identity"):
        print("  with %-12s: %d" % (k, has(k)))
    print("  families seen: %s" % ", ".join(seen_fams))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            if nm in results:
                print("\n=== %s ===" % nm)
                print(json.dumps(results[nm], indent=1, ensure_ascii=False))
            else:
                print("\n(%s not found)" % nm)
    if args.write:
        base = os.path.join(REPO, "Assets", "Data", "promotions")
        os.makedirs(base, exist_ok=True)
        for typ, obj in results.items():
            with open(os.path.join(base, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        print("\nwrote %d PromotionInfo JSON files under Assets/Data/promotions" % n)


if __name__ == "__main__":
    main()
