#!/usr/bin/env python3
"""Curate Improvement (#428, Tier C #22) — a plot-leaf TARGET (Terrain/Feature peer); curated from the
adversarially-verified `classifications/improvement-classification.json` (classify-improvement workflow) + the
owner rulings 2026-06-16. An improvement carries its OWN plot-scope modifiers and a placement `requires.build`;
it is NEVER a source (store.enabled_by empty -> no `enables`).

OWN per-plot modifiers (mapping had the buried-in-identity + wrong city-scope trap, like Terrain/Feature -> plot):
- YieldChanges            -> SPLIT food/production/commerce .plot.flat (the improvement's own tile yield).
- RiverSideYieldChange    -> SPLIT yield .plot.flat, HAS_RIVER-gated (post_process; apply_channel can't condition).
- IrrigatedYieldChange    -> SPLIT yield .plot.flat, HAS_IRRIGATION-gated (post_process).
- BonusTypeStructs.YieldChanges -> SPLIT yield .plot.flat, {HAS_BONUS:bonus}-gated (mine-on-coal; post_process).
- iDefenseModifier        -> defense.plot.amount.percent (fort tile-defense; CvPlot:4428 += into the % accumulator).
- iCulture                -> culture.plot.flat (Super Forts plot culture; iCultureRange -> identity).
- iVisibilityChange/iSeeFrom -> `vision` block: vision.plot.{visibilityRange,seeFrom}.flat (dedicated-block §0.8).

PLACEMENT -> `requires.build` (requires_fn), owner 2026-06-16 "double mapping": improvement/feature/bonus first
ENABLE allowed builds, then `requires` reigns in; the IMPROVEMENT self-gates because EVENTS place improvements
directly (no hard build link), so an event can't drop a disallowed improvement; BuildInfo also gates its action
path (#23). Consumed SOLELY by CvPlot::canHaveImprovement (3010-3202), reading only the ImprovementInfo. Shape:
  build.all   = mandatory AND : {type:TECH,scope:team} (PrereqTech) + bare plot predicates IS_WATER/HAS_PEAK
                (bWaterImprovement/bPeakImprovement domain), HAS_IRRIGATION (bRequiresIrrigation), IS_FLATLANDS
                (bRequiresFlatlands), HAS_FEATURE (bRequiresFeature), HAS_RIVER (bRequiresRiverSide; the
                one-improvement-per-river-side spacing nuance -> #430 FLAG), {IS_LAND,HAS_COAST} (bCanMoveSeaUnits, coastal land) +
                {natureYield:{...}} (PrereqNatureYields min).
  build.any   = the make-valid OR-set (>=1 holds) as ONE OR-group: {terrain:[...]} (TerrainMakesValids),
                {feature:[...]} (FeatureMakesValids), {bonus:[...]} (per-bonus bBonusMakesValid), HAS_HILLS,
                HAS_FRESHWATER, HAS_RIVER (bRiverSideMakesValid), HAS_PEAK (bPeakMakesValid).
  build.noneOf= HAS_FRESHWATER (bNoFreshWater).
  New tokens (owner-approved 2026-06-16; predicate names updated 2026-06-22 to the IS_*/HAS_* split): bare IS_WATER/HAS_PEAK/IS_FLATLANDS/HAS_HILLS/HAS_FRESHWATER/HAS_IRRIGATION/
  HAS_FEATURE/IS_LAND+HAS_COAST (+ existing HAS_RIVER); object {terrain|feature|bonus:[...]}, HAS_BONUS:{BONUS}, and the
  {natureYield:{...}} min-threshold atom. IS_RIVERSIDE rejected as redundant with HAS_RIVER (owner). Domain both-ways
  (a land improvement can't go on water) is a STRUCTURAL engine rule (#430), not an IS_LAND atom on every land record.
  FLAG: an empty `any` (graphical/event-only improvements with no make-valid source) — never-auto-valid vs anywhere
  semantics pins at #430.

IRRIGATION (owner 2026-06-16, three-part): bRequiresIrrigation -> requires.build.all HAS_IRRIGATION (the check =
isIrrigationAvailable). bCarriesIrrigation -> identity.carriesIrrigation:true — KEPT; the improvement must retain its
ability to carry irrigation (propagation is live code, updateIrrigated). Carrying is TEAM-tech-gated
(CvTeam::isIrrigation <- TECH_CANAL_SYSTEMS, already curate_tech bIrrigation->irrigation); engine ANDs at runtime,
the improvement authors NO tech clause for carrying.

CAPABILITY flags -> identity (NOT grants; Lens-B fix + Terrain bFound precedent): bActsAsCity, bCarriesIrrigation,
bIsZOCSource, bIsUniversalTradeBonusProvider (LIVE lynchpin: "trades ALL bonuses on its plot", via
isImprovementBonusTrade(-1) OR-fold CvImprovementInfo.cpp:313/315 — owner-traced; the workflow's zero-reader claim
grepped only the unused direct getter), bMilitaryStructure, bGraphicalOnly, bExtraterresial (spacemap), bOutsideBorders.
Per-bonus bBonusTrade/iDiscoverRand/iDepletionRand -> identity.bonuses.{BONUS}.{trade,discoverRand,depletionRand}
(RNG kept; the per-bonus DepletionRand is the live gated MODDERGAMEOPTION_RESOURCE_DEPLETION mechanic).

LIFECYCLE (deferred OUTCOME system, NOT modifiers) -> identity: ImprovementUpgrade/iUpgradeTime/
AlternativeImprovementUpgradeTypes/bUpgradeRequiresFortify (upgrade), ImprovementPillage/iPillageGold/bBombardable
(pillage/damage), FeatureChangeTypes/bChangeRemove/bPlaces{Bonus,Feature,Terrain} (placement transform — a "random
spawn" mechanic, e.g. apple bonus under a lumberjack; owner: KEEP on identity, needs its OWN pass, OUT OF SCOPE #428).

TechYieldChanges -> KEEP-ON-SOURCE (owner 2026-06-20, Phase-F modifier-ownership): the improvement OWNS its tile
yield, so its tech-conditioned bump rides on the improvement, plot-scope, `enabled` by the team-tech (the tech is
the GIVER/enabler -- never inverted onto the tech). x1 (improvement yields are human-scale, like the base
YieldChanges). RouteYieldChanges -> stays folded onto the ROUTE (curate_route:46) -- the route GOVERNS which
improvements it upgrades (owner 2026-06-20), the one human-governance inversion that's correct.
DROPPED: iHealthPercent -> drop, BALANCE-CUT as a source from improvements (capability kept globally). Categories /
root iDepletionRand / Button (no improvement button — it lives on the worker Build) / MapCategoryTypes (0/266) -> drop.
iAirBombDefense -> defense.plot.air.flat (owner 2026-07-01; the air-bomb defense magnitude). RNG: iFeatureGrowth /
iCultureRange -> identity (intrinsic improvement mechanics read by their OWN CvPlot systems -- feature-regrowth /
culture-seed vestige -- NOT cascade modifiers; owner 2026-07-01 "leave them in identity"). PropertyManipulators ->
top-level `triggers` entries (json.md §5, ruling 8): the RELATION_NEAR pollution pulse becomes an onTurn trigger
whose action carries the spatial intent (#429 reads its target from the action).

EXE-link: 3 DllExport (isGoody, isRequiresRiverSide, getArtInfo) -> bGoody + bRequiresRiverSide EXE-constrained.

  python3 curate_improvement.py --sample IMPROVEMENT_FARM
  python3 curate_improvement.py --write
"""
import os
from collections import OrderedDict

import engine
import curate_common as cc
from store import REPO

# Own per-plot modifier families (override the mapping's wrong city scope -> plot).
IMP_FAMILIES = {
    "YieldChanges":      {"channel": "yield",   "scope": "plot", "kind": "flat", "valueKeys": engine.YIELDS},
    "iDefenseModifier":  {"channel": "defense",  "scope": "plot", "kind": "percent", "member": "amount"},
    "iAirBombDefense":   {"channel": "defense",  "scope": "plot", "kind": "flat", "member": "air"},    # air-bomb defense magnitude (rolled, CvUnit.cpp:7127); owner 2026-07-01
    "iHappiness":        {"channel": "happiness", "scope": "plot", "kind": "flat"},   # #430 gap fix: intrinsic per-radius-improvement city happiness (was a dead identity.happiness leftover; x1 human, read raw by legacy updateFeatureHappiness)
    "iCulture":          {"channel": "culture",  "scope": "plot", "kind": "flat"},
    # Both legacy levers raised how well an observer standing here sees -- one as a radius, one as an elevation
    # tier -- so both ARE elevation (vision.md), and they sum onto the one channel as the two number systems go.
    # Elevation is POSITIONAL: a watchtower raises whoever stands on it, and only while they stand on it.
    "iVisibilityChange": {"channel": "vision",   "scope": "plot", "kind": "flat", "member": "elevation"},
    "iSeeFrom":          {"channel": "vision",   "scope": "plot", "kind": "flat", "member": "elevation"},
}

# Placement bools NOT already in the mapping prereqs (those are auto-dropped) — drop from the default path; requires_fn
# reads them off rec. Conditional yields + per-bonus + properties -> post_process. Inverted/dead -> dropped.
EXTRA_DROP = [
    # placement (requires_fn) not in mapping prereqs:
    "bNoFreshWater", "bWaterImprovement", "bPeakImprovement", "bCanMoveSeaUnits", "bNotOnAnyBonus",
    # post_process:
    "RiverSideYieldChange", "IrrigatedYieldChange", "BonusTypeStructs", "PropertyManipulators",
    # keep-on-source via post_process (tech-conditioned own yields):
    "TechYieldChanges",
    # route governs / balance-cut / dead:
    "RouteYieldChanges", "iHealthPercent", "Categories", "iDepletionRand", "Button",
    "MapCategoryTypes",
]

ID_RENAME = {
    "ImprovementUpgrade": "upgradesTo", "ImprovementPillage": "pillageTo",
    "AlternativeImprovementUpgradeTypes": "alternativeUpgrades", "bUpgradeRequiresFortify": "upgradeRequiresFortify",
    "FeatureChangeTypes": "featureChanges", "bIsZOCSource": "zoneOfControl",
    "bIsUniversalTradeBonusProvider": "universalBonusTrade", "bExtraterresial": "extraterrestrial",
    "iPillageGold": "pillageGold",
}

HAS_RIVER, HAS_IRRIGATION = "HAS_RIVER", "HAS_IRRIGATION"
_PREFIX = ["type", "description", "civilopedia", "help", "quote", "strategy",
           "enables", "obsoletes", "replaces", "disables", "requires"]
_SUFFIX = ["grants", "triggers", "properties", "cost", "ai", "ui", "world", "sound", "mapGeneration", "identity"]


def _bool(node, tag):
    return engine.text(node.find(tag)) in ("1", "true", "True")


def _typelist(rec, wrapper):
    node = rec.find(wrapper)
    if node is None:
        return []
    out = []
    for c in node:
        t = (engine.text(c) or "").strip()
        if t and t != "NONE":
            out.append(t)
    return out


def requires_improvement(rec, store):
    """Build the placement `requires.build` (all / any / noneOf) — the canHaveImprovement gate, owner-ruled."""
    allc, anyset, none = [], [], []
    tech = engine.text(rec.find("PrereqTech"))
    if tech and tech != "NONE":                                  # improvement's OWN placeability tech (also store->tech.enables)
        allc.append(OrderedDict([("type", tech), ("scope", "team")]))
    if _bool(rec, "bWaterImprovement"):                          # domain (both-ways; non-match forbid is engine-structural #430)
        allc.append("IS_WATER")
    if _bool(rec, "bPeakImprovement"):
        allc.append("HAS_PEAK")
    if _bool(rec, "bRequiresIrrigation"):
        allc.append(HAS_IRRIGATION)
    if _bool(rec, "bRequiresFlatlands"):
        allc.append("IS_FLATLANDS")
    if _bool(rec, "bRequiresFeature"):
        allc.append("HAS_FEATURE")
    if _bool(rec, "bCanMoveSeaUnits"):                           # coastal land = IS_LAND ^ HAS_COAST (a land plot adjacent to water)
        allc.append("IS_LAND")
        allc.append("HAS_COAST")
    if _bool(rec, "bRequiresRiverSide"):                          # + one-per-river-side spacing nuance -> #430 FLAG
        allc.append(HAS_RIVER)
    pn = rec.find("PrereqNatureYields")
    if pn is not None:
        ny = engine.named_array(pn, engine.YIELDS)
        if ny:
            allc.append(OrderedDict([("natureYield", ny)]))
    terrains = _typelist(rec, "TerrainMakesValids")
    if terrains:
        anyset.append(OrderedDict([("terrain", terrains)]))
    features = _typelist(rec, "FeatureMakesValids")
    if features:
        anyset.append(OrderedDict([("feature", features)]))
    if _bool(rec, "bHillsMakesValid"):
        anyset.append("HAS_HILLS")
    if _bool(rec, "bFreshWaterMakesValid"):
        anyset.append("HAS_FRESHWATER")
    if _bool(rec, "bRiverSideMakesValid"):
        anyset.append(HAS_RIVER)
    if _bool(rec, "bPeakMakesValid"):
        anyset.append("HAS_PEAK")
    bts = rec.find("BonusTypeStructs")
    if bts is not None:
        mv = [engine.text(s.find("BonusType")) for s in bts.findall("BonusTypeStruct") if _bool(s, "bBonusMakesValid")]
        mv = [b for b in mv if b and b != "NONE"]
        if mv:
            anyset.append(OrderedDict([("bonus", mv)]))
    if _bool(rec, "bNoFreshWater"):
        none.append("HAS_FRESHWATER")
    if anyset:                                                   # bonus-makes-valid OR-group -> nested {any} under all (any = ||)
        allc.append(anyset[0] if len(anyset) == 1 else OrderedDict([("any", anyset)]))
    build = OrderedDict()
    if allc:
        build["all"] = allc
    if none:
        build["noneOf"] = none
    return {"build": build} if build else None


CFG = cc.EntityConfig("ImprovementInfo", families=IMP_FAMILIES, id_rename=ID_RENAME,
                      to_identity={"iAdvancedStartCost": "advancedStart.cost"},
                      # bRequiresRiverSide is ALSO converted to a HAS_RIVER requires.build predicate (requires_improvement);
                      # it is retained here as mapGeneration.requiresRiverSide too, because isRequiresRiverSide() is an
                      # EXE-bound DllExport read (the CvBonusInfo-style map-gen shim leaf, cascade-engine-430.md #3) with
                      # live DLL/Python/UI callers -- the flag and the predicate are not exclusive.
                      map_gen=["iUniqueRange", "iGoodyRange", "iTilesPerGoody", "bGoody", "bRequiresRiverSide"],
                      extra_drop=EXTRA_DROP, requires_fn=requires_improvement,
                      characteristics=['bActsAsCity', 'bBombardable', 'bIsZOCSource'])

# Inbound boosts: Building/Civic ImprovementYieldChanges stay KEEP-ON-SOURCE (the cascade gathers those keyed from the
# building/civic). The TECH's ImprovementYieldChanges is the exception (owner 2026-06-26): a tech BOOSTS an improvement's
# tile yield (engine GET_TEAM::getImprovementYieldChange -> the impTeam addend of calculateImprovementYieldChange), and a
# TECH conditions on the ENABLING axis -> OWN-OUTPUT on the improvement, plot-scope, `enabled` by the tech (modifier.md
# §4). The cascade has no tech-keyed gather, so own-output (which SubstratePlotYield already reads, tech-gate honored) is
# both the spec shape AND the only one the cascade sees. (The improvement's OWN TechYieldChanges is a separate keep-on-self.)
IMP_BOOSTS = [
    ("TechInfo", "ImprovementYieldChanges", "improvements", "yield", engine.YIELDS, "flat", "plot"),
]


def _inject(obj, family, scope, unit, value, enabled=None):
    leaf = obj.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    entry = value if enabled is None else OrderedDict([("value", value), ("enabled", enabled)])
    cur = leaf.get(unit)
    if cur is None:
        leaf[unit] = entry if enabled is None else [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    elif enabled is None:
        leaf[unit] = cur + value
    else:
        leaf[unit] = [cur, entry]


def _reorder(obj):
    fams = [k for k in obj if k not in _PREFIX and k not in _SUFFIX]
    ordered = [f for f in cc.FAMILY_ORDER if f in fams] + [f for f in fams if f not in cc.FAMILY_ORDER]
    new = OrderedDict()
    for k in _PREFIX:
        if k in obj:
            new[k] = obj[k]
    for f in ordered:
        new[f] = obj[f]
    for k in _SUFFIX:
        if k in obj:
            new[k] = obj[k]
    obj.clear()
    obj.update(new)


def post_process(typ, obj, rec, store):
    # Conditional own yields (apply_channel can't express the gate).
    for tag, pred in (("RiverSideYieldChange", HAS_RIVER), ("IrrigatedYieldChange", HAS_IRRIGATION)):
        node = rec.find(tag)
        if node is not None:
            for y, v in engine.named_array(node, engine.YIELDS).items():
                _inject(obj, y, "plot", "flat", v, pred)
    # Tech-conditioned OWN yields: the improvement yields more once the team has the tech (legacy
    # ImprovementInfo.TechYieldChanges). KEEP-ON-SOURCE -- on the improvement, plot-scope, `enabled` by the
    # team-tech (the tech is the GIVER/enabler, never inverted onto the tech; owner 2026-06-20). x1 human-scale,
    # like the base YieldChanges -- no de-scale.
    ty = rec.find("TechYieldChanges")
    if ty is not None:
        for tech, _u, yields in cc._boost_entries(ty, engine.YIELDS, "flat"):
            enabled = OrderedDict([("type", tech), ("scope", "team")])
            for member, v in (yields.items() if isinstance(yields, dict) else []):
                _inject(obj, member, "plot", "flat", v, enabled)
    # Per-bonus nest: yields (HAS_BONUS-gated) -> families ; trade/discoverRand/depletionRand -> identity.bonuses.
    bts = rec.find("BonusTypeStructs")
    if bts is not None:
        bonuses = OrderedDict()
        for s in bts.findall("BonusTypeStruct"):
            b = engine.text(s.find("BonusType"))
            if not b or b == "NONE":
                continue
            yc = s.find("YieldChanges")
            if yc is not None:
                for y, v in engine.named_array(yc, engine.YIELDS).items():
                    _inject(obj, y, "plot", "flat", v, OrderedDict([("HAS_BONUS", b)]))
            rb = OrderedDict()
            if _bool(s, "bBonusTrade"):
                rb["trade"] = True
            for tg, key in (("iDiscoverRand", "discoverRand"), ("iDepletionRand", "depletionRand")):
                t = engine.text(s.find(tg))
                if engine.is_int(t) and int(t) != 0:
                    rb[key] = int(t)
            if rb:
                bonuses[b] = rb
        if bonuses:
            obj.setdefault("identity", OrderedDict()).setdefault("bonuses", OrderedDict()).update(bonuses)
    # PropertyManipulators -> top-level `triggers` entries (json.md §5, ruling 8: trigger -> chance -> action).
    # The improvement's RELATION_NEAR pollution pulse becomes an onTurn trigger whose ACTION carries the spatial
    # intent (on/relation/distance); the (#429) spatial-distribution engine reads its target from there.
    pm = rec.find("PropertyManipulators")
    if pm is not None:
        pulses = [g for g in (engine.property_source_trigger(s) for s in pm if s.tag == "PropertySource") if g]
        if pulses:
            obj.setdefault("triggers", []).extend(pulses)
    _reorder(obj)


if __name__ == "__main__":
    cc.main(CFG, IMP_BOOSTS, os.path.join(REPO, "Assets", "Data", "improvements"), post_process=post_process)
