#!/usr/bin/env python3
"""Curate Route (#428) — thin config over curate_common. A Route is a small plot-feature entity that lays
movement cost + (optionally) tile yields onto the plot it occupies, ranks itself by iValue, and is GATED by a
prerequisite bonus (so it is a DEPENDENT in the bonus->route enabler chain — store.enabled_by(ROUTE_*) is
empty, no `enables` block).

Modeling calls (light-batch-classification.json, verified vs CvRouteInfo + the live consumers):
- Yields -> the SPLIT base-yield families food/production/commerce at PLOT scope (a tile yield, summed into the
  plot alongside terrain/improvement yields by CvPlot::calculateYield) — a genuine cascade deposit.
- iMovement -> the `movement` family (`movement.plot.flat`); iFlatMovement stays identity pending its own
  member name. A route's
  BASE traversal cost is an intrinsic own-stat read directly per-route by pathfinding (CvPlot::movementCost); it
  is never summed onto a scope accumulator. Base stats are identity; only the cascading DELTA (below) is a family.
- TechMovementChanges -> the `movement` family ON THE ROUTE (owner ruling 2026-07-01). Each nonzero
  TechMovementChange[tech] = a per-tech route move-cost delta (CvTeam.cpp:6041 adds it to the route's base
  getMovementCost once TECH_X is researched). The route OWNS its move cost (json §6.2 deliveryguy: plot-substrate
  entities own their plot-scope output), so the delta homes on the route as a tech-gated `movement.plot.flat`
  entry: {value:<delta>, enabled:"TECH_X"} (the tech is the `enabled` conditioner, modifier.md §4). This
  RELOCATES the home from the tech: curate_tech's old RouteInfo->movement.team.routes.{ROUTE} boost row is REMOVED
  in the same pass to avoid double-homing the delta. ⚠ VERIFY leaf: `plot` scope + `flat` unit are solid, but the
  exact leaf sub-address is flagged "pending the movement-subsystem doc" (modifier.md §6) — routes may later model
  move cost under a dedicated leaf; do NOT invent a new scope until that doc lands.
- iAdvancedStartCost -> identity (de_i = advancedStartCost). Advanced-start is a separate pre-game points
  currency; Handicap/Era likewise park advanced-start in identity, not the production-`costs` family.
- iValue (tier comparator), bSeaTunnel (water-crossing capability flag) -> identity.
- BonusType/PrereqOrBonuses -> DROP: prereqs the store inverts to the bonus's enables.routes.

Inbound boost: ImprovementInfo.RouteYieldChanges — an improvement carries a per-route yield bonus applied to
the plot when that improvement sits on a tile with THIS route (CvPlayer::calculatePlotRouteYieldDifference).
Folds onto the route as the SPLIT yield families at PLOT scope, keyed by the source improvement.

  python3 curate_route.py --sample ROUTE_MAGLEV
  python3 curate_route.py --write
"""
import os
from collections import OrderedDict

import engine
import curate_common as cc
from store import REPO

# Verified own modifier-family fields (override the empty mapping channels).
ROUTE_FAMILIES = {
    "Yields": {"channel": "yield", "scope": "plot", "kind": "flat", "valueKeys": engine.YIELDS},
}

# Intrinsic own-stats -> identity, with clearer names than the default de_i would give.
ROUTE_ID_RENAME = {"iFlatMovement": "flatMovementCost"}

# Inbound entity-targeted modifiers that invert ONTO the route:
#   (sourceEntity, field, targetType, family, valueKeys, unit, scope)
ROUTE_BOOSTS = [
    ("ImprovementInfo", "RouteYieldChanges", "improvements", "yield", engine.YIELDS, "flat", "plot"),
]

# TechMovementChanges is now HOMED on the route (post_process below), so it is DROPPED from the raw-identity
# emit (it is emitted as a `movement` family, not parked). BonusType (single AND prereq bonus) is dropped —
# store-inverted to the bonus's enables.routes; PrereqOrBonuses is already in the mapping's prereqs (cfg.drop).
CFG = cc.EntityConfig("RouteInfo", extra_drop=["BonusType", "TechMovementChanges"],
                      families=ROUTE_FAMILIES, id_rename=ROUTE_ID_RENAME,
                      to_identity={"iAdvancedStartCost": "advancedStart.cost"})


def post_process(typ, obj, rec, store):
    """TechMovementChanges -> tech-gated `movement.plot.flat` deposits ON THE ROUTE (owner ruling 2026-07-01).
    Each entry is {PrereqTech, iMovementChange}; the delta homes on the route (json §6.2: a plot-substrate entity
    owns its plot-scope output) as a `movement.plot.flat` list entry gated `enabled` by the team-tech (modifier.md
    §4 — the tech is the enabling conditioner). x1 human-scale (getTechMovementChange is not x100'd) -> no de-scale.
    ⚠ VERIFY leaf: `movement.plot.flat` (scope+unit solid) — the exact sub-address pends the movement-subsystem doc."""
    node = rec.find("TechMovementChanges")
    if node is None:
        return
    added = False
    for tech, _u, delta in cc._boost_entries(node, None, "flat"):
        if not isinstance(delta, int) or delta == 0:
            continue
        enabled = OrderedDict([("type", tech), ("scope", "team")])
        entry = OrderedDict([("value", delta), ("enabled", enabled)])
        leaf = obj.setdefault("movement", OrderedDict()).setdefault("plot", OrderedDict())
        # The route's BASE cost already sits in this slot as a bare number (iMovement -> the `movement` family,
        # owner: "movementCost is already movement"). A leaf is one entry OR a list of entries (json.md par.3.9),
        # and the list IS the formula mechanism -- so the base folds in as the first entry and the tech deltas
        # append after it, summing exactly as any other composite deposit does.
        cur = leaf.get("flat")
        if not isinstance(cur, list):
            leaf["flat"] = [] if cur is None else [cur]
        leaf["flat"].append(entry)
        added = True
    if added:                                       # keep `movement` in its FAMILY_ORDER slot, before the
        _reorder_movement(obj)                       # ui/world/sound/mapGeneration/identity suffix (cosmetic)


# The suffix sections that must stay AFTER the modifier families (curate_common emit order).
_SUFFIX = ("grants", "cost", "ai", "ui", "world", "sound", "mapGeneration", "identity")


def _reorder_movement(obj):
    """post_process appends `movement` at the end (after identity); move it up into its FAMILY_ORDER position,
    ahead of the ui/world/sound/mapGeneration/identity suffix — matches every other family's placement."""
    suffix = [(k, obj[k]) for k in _SUFFIX if k in obj]
    for k, _ in suffix:
        del obj[k]
    for k, v in suffix:                              # re-append the suffix so `movement` (already in obj) precedes it
        obj[k] = v


if __name__ == "__main__":
    cc.main(CFG, ROUTE_BOOSTS, os.path.join(REPO, "Assets", "Data", "routes"), post_process=post_process)
