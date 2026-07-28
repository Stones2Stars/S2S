#!/usr/bin/env python3
"""Curate Feature (#428, Tier C #21) — a plot-leaf TARGET / DELIVERYGUY that MODIFIES the plot it sits on.

Unlike Terrain (which SEEDS the plot's base), a feature ADDS its values onto the terrain-seeded accumulator
(CvPlot.cpp movementCost 4559 +=, defenseModifier 4404 +=, recalculateBaseYield 8081 += ) — a genuine per-plot
DELTA, so its own effects are feature-owned PLOT-scope modifier families. Enables nothing
(store.enabled_by(FEATURE_*) empty). All inbound feature-keyed modifiers stay KEEP-ON-SOURCE (the civic/unit/
promotion that delivers them owns them, conditioned on the feature) — Feature carries no inbound boost.
Per-field dispositions: classifications/feature-classification.json (adversarially verified, wf waugnsq1x).

Modeling calls (verified vs CvFeatureInfo + CvPlot::calculateYield/movementCost/getDefenseModifier + CvCity):
- YieldChanges      -> SPLIT food/production/commerce .plot.flat (forest -food/+hammers, jungle -food, oasis +).
- RiverYieldChange  -> SPLIT yield .plot.flat, each entry HAS_RIVER-gated (forest-on-river); the feature's EXTRA
                       river yield, compounding with the terrain's river bonus (enabler-spec §3; first HAS_RIVER use).
- iHealthPercent    -> health.plot.percent (Feature OWNS health; Terrain dropped it precisely because it lives here).
- iDefense          -> defense.plot.amount.percent (feature defense %, additive onto the plot).
- iMovement         -> the `movement` family (`movement.plot.flat`). The RESOLVER is bespoke; its INPUTS are
                       The feature's extra traversal cost is read per-(unit,edge) by the movement resolver
                       (CvPlot::movementCost, additive onto terrain), not summed down a scope spine — matching
                       curate_route.py / curate_terrain.py. Only cascading deltas (promotion discount/credit) are families.
- iCultureDistance  -> cultureDistance.plot.flat (summed into the city culture-distance total).
- iSeeThrough       -> `vision` block: vision.plot.seeThrough.flat (line-of-sight; grouped for the coming vision
                       rework — owner 2026-06-16; modifier-spec §0.8 dedicated-block rule).
- iWarmingDefense   -> DROP. Dead: GLOBAL_WARMING is `// #define`d out (compiled out); a future global-warming
                       system gets its OWN base object, not a feature field (owner; issue #436, global-warming-mod.md).
- PropertyManipulators -> top-level `triggers` entries (json.md §5, ruling 8): the RELATION_NEAR pollution pulse
                       is { trigger:"onTurn", action:{ PROPERTY_X: N, on, relation, distance } }, via
                       engine.property_source_trigger. The (#429) spatial-distribution engine reads its target
                       from the action. (Replaces the former grants.repeatable / parked raw `properties` block.)
- iAppearance/iDisappearance/iGrowth/iSpread/iPopDestroys + bCanGrow.../bRequires.../bNo.../placement flags
                    -> identity (world-gen RNG / lifecycle / placement config). bGraphicalOnly -> identity flag.
- ArtDefineTag (on-map art) / EffectType / iEffectProbability / GrowthSound / FootstepSounds / WorldSoundscape -> art.
- OnUnitChangeTo    -> grants (a feature that transforms a unit on entry; module-only, 0 in base XML). FLAG: grants
                       vs a dedicated transform edge (owner pass if it ever carries data).

EXE-link: 1 DllExport on CvFeatureInfo (getArtInfo, an art FK) — unconstrained for data.

  python3 curate_feature.py --sample FEATURE_FOREST
  python3 curate_feature.py --write
"""
import os
from collections import OrderedDict

import engine
import curate_common as cc
from store import REPO

# Feature's OWN per-plot modifier families (override the mapping's wrong city scope -> plot).
FEATURE_FAMILIES = {
    "YieldChanges":     {"channel": "yield",          "scope": "plot", "kind": "flat", "valueKeys": engine.YIELDS},
    "iHealthPercent":   {"channel": "health",         "scope": "plot", "kind": "percent"},
    "iDefense":         {"channel": "defense",        "scope": "plot", "kind": "percent", "member": "amount"},
    "iCultureDistance": {"channel": "cultureDistance","scope": "plot", "kind": "flat"},
    "iSeeThrough":      {"channel": "obstruction",    "scope": "plot", "kind": "flat"},   # what the ground costs to see THROUGH -- a feature's see-through value IS its obstruction (jungle 2, open ground 1); vision.md
}

# RiverYieldChange + PropertyManipulators are dropped from the DEFAULT path and rebuilt in post_process (the first
# is HAS_RIVER-conditional, which apply_channel can't express; the second becomes `triggers` entries, §5).
# iWarmingDefense is dead. Prereqs (none) come from the mapping.
FEATURE_DROP = ["iWarmingDefense", "RiverYieldChange", "PropertyManipulators", "iPopDestroys"]

# iMovement -> intrinsic `identity.movementCost` via to_identity (resolver-subsystem; matches terrain/route).
# to_identity OVERRIDES the Feature mapping (which classifies iMovement as a movement/city channel) because
# curate() checks to_identity BEFORE the channel mapping -- so iMovement lands intrinsic, not as a stray family.
CFG = cc.EntityConfig("FeatureInfo", extra_drop=FEATURE_DROP, families=FEATURE_FAMILIES,
                      grants={"OnUnitChangeTo": "onUnitChangeTo"},
                      id_rename={'bNoCity': 'unfoundable', 'bNoImprovement': 'unimprovable',
                                 'bNoBonus': 'prohibitsBonus'},
                      characteristics=['bNukeImmune', 'bCountsAsPeak', 'bIgnoreTerrainCulture',
                                       'bNoCity', 'bNoImprovement', 'bNoBonus'])

# No inbound boosts: a feature is never the deliveryguy for another entity's modifier (modifier-spec §6.1).
FEATURE_BOOSTS = []

HAS_RIVER = "HAS_RIVER"   # bare-string predicate shorthand (enabler-spec §3)
_PREFIX = ["type", "description", "civilopedia", "help", "quote", "strategy",
           "enables", "obsoletes", "replaces", "disables", "requires"]
_SUFFIX = ["grants", "triggers", "properties", "cost", "ai", "ui", "world", "sound", "mapGeneration", "identity"]


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
    # RiverYieldChange -> HAS_RIVER-conditional SPLIT yield deposit (the feature's extra river-side yield).
    node = rec.find("RiverYieldChange")
    if node is not None:
        for y, v in engine.named_array(node, engine.YIELDS).items():
            _inject(obj, y, "plot", "flat", v, HAS_RIVER)
    # PropertyManipulators -> top-level `triggers` entries (json.md §5, ruling 8: trigger -> chance -> action).
    # The feature's RELATION_NEAR pollution pulse becomes an onTurn trigger whose ACTION carries the spatial
    # intent (on/relation/distance); the (#429) spatial-distribution engine reads its target from there.
    pm = rec.find("PropertyManipulators")
    if pm is not None:
        pulses = [g for g in (engine.property_source_trigger(s) for s in pm if s.tag == "PropertySource") if g]
        if pulses:
            obj.setdefault("triggers", []).extend(pulses)
    # iPopDestroys -> a TRIGGER on the city's POPULATION (owner). The chain is ordinary containment: a city
    # knows its plot, the plot carries the feature, so the feature reads the city's population fact and dies --
    # "dead feature". SEVT_POPULATION_CHANGED already carries it. The ACTION's subject is the entity the
    # trigger is authored on, implicit exactly as a `grants` happening is (json.md par.5), so no SELF-target
    # vocabulary is needed.
    # ONE condition replaces the legacy THREE branches: the engine destroyed at founding when the value was 0
    # or 1 (CvCity::init) and at `newPop >= value` thereafter (CvCity::setPopulation), which is uniformly
    # "city population >= value" -- a founded city always has pop >= 1, so 0 and 1 both fire at founding and
    # normalize to min:1. -1 means never, and emits nothing.
    pdNode = rec.find("iPopDestroys")
    pdText = engine.text(pdNode) if pdNode is not None else None
    if pdText is not None and engine.is_int(pdText) and int(pdText) > -1:
        obj.setdefault("triggers", []).append(OrderedDict([
            ("trigger", OrderedDict([("type", "POPULATION"), ("scope", "city"),
                                     ("min", max(1, int(pdText)))])),
            ("action", OrderedDict([("destroy", "self")])),
        ]))
    _reorder(obj)


if __name__ == "__main__":
    cc.main(CFG, FEATURE_BOOSTS, os.path.join(REPO, "Assets", "Data", "features"), post_process=post_process)
