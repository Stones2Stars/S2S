#!/usr/bin/env python3
"""Curate Route (#428) — thin config over curate_common. A Route is a small plot-feature entity that lays
movement cost + (optionally) tile yields onto the plot it occupies, ranks itself by iValue, and is GATED by a
prerequisite bonus (so it is a DEPENDENT in the bonus->route enabler chain — store.enabled_by(ROUTE_*) is
empty, no `enables` block).

Modeling calls (light-batch-classification.json, verified vs CvRouteInfo + the live consumers):
- Yields -> the SPLIT base-yield families food/production/commerce at PLOT scope (a tile yield, summed into the
  plot alongside terrain/improvement yields by CvPlot::calculateYield) — a genuine cascade deposit.
- iMovement/iFlatMovement -> IDENTITY (movementCost/flatMovementCost), NOT a `movement` family. A route's
  traversal cost is an intrinsic own-stat read directly per-route by pathfinding (CvPlot::movementCost); it is
  never summed onto a scope accumulator. The cascading movement modifier is the per-TECH route delta, which
  curate_tech already folds onto the tech as movement.team.routes.{ROUTE}.flat. Base stats are identity; only
  summed contributions are modifier families.
- iAdvancedStartCost -> identity (de_i = advancedStartCost). Advanced-start is a separate pre-game points
  currency; Handicap/Era likewise park advanced-start in identity, not the production-`costs` family.
- iValue (tier comparator), bSeaTunnel (water-crossing capability flag) -> identity.
- TechMovementChanges -> DROP: it lives in the route XML but is a per-tech improvement; curate_tech's
  RouteInfo boost row already homes it on the tech. BonusType/PrereqOrBonuses -> DROP: prereqs the store
  inverts to the bonus's enables.routes.

Inbound boost: ImprovementInfo.RouteYieldChanges — an improvement carries a per-route yield bonus applied to
the plot when that improvement sits on a tile with THIS route (CvPlayer::calculatePlotRouteYieldDifference).
Folds onto the route as the SPLIT yield families at PLOT scope, keyed by the source improvement.

  python3 curate_route.py --sample ROUTE_MAGLEV
  python3 curate_route.py --write
"""
import os

import engine
import curate_common as cc
from store import REPO

# Verified own modifier-family fields (override the empty mapping channels).
ROUTE_FAMILIES = {
    "Yields": {"channel": "yield", "scope": "plot", "kind": "flat", "valueKeys": engine.YIELDS},
}

# Intrinsic own-stats -> identity, with clearer names than the default de_i would give.
ROUTE_ID_RENAME = {"iMovement": "movementCost", "iFlatMovement": "flatMovementCost"}

# Inbound entity-targeted modifiers that invert ONTO the route:
#   (sourceEntity, field, targetType, family, valueKeys, unit, scope)
ROUTE_BOOSTS = [
    ("ImprovementInfo", "RouteYieldChanges", "improvements", "yield", engine.YIELDS, "flat", "plot"),
]

# BonusType (single AND prereq bonus) + TechMovementChanges (folds onto the tech) dropped from the authored
# route; PrereqOrBonuses is already in the mapping's prereqs (cfg.drop).
CFG = cc.EntityConfig("RouteInfo", extra_drop=["BonusType", "TechMovementChanges"],
                      families=ROUTE_FAMILIES, id_rename=ROUTE_ID_RENAME,
                      to_identity={"iAdvancedStartCost": "advancedStart.cost"})

if __name__ == "__main__":
    cc.main(CFG, ROUTE_BOOSTS, os.path.join(REPO, "Assets", "Data", "routes"))
