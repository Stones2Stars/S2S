#!/usr/bin/env python3
"""Curate Tech to the top-down model (#428) — thin config over curate_common (the shared core).

  python3 curate_tech.py --sample TECH_LANGUAGE
  python3 curate_tech.py --write
"""
import os

import engine
import curate_common as cc
from store import REPO

# Tech-conditioner boosts (entity-targeted modifiers that fold into the tech's `modifiers.{scope}`):
#   (sourceEntity, field, targetType, channel, valueKeys, unit, scope)
# unit EXPLICIT per field (verified vs the C++ consumer, workflow wf_7bbae202); never derived from the
# misnamed value elements. Building Tech{Yield,Commerce}Changes are x100 fixed-point, carried FAITHFULLY (#432).
TECH_BOOSTS = [
    ("BuildingInfo",    "TechYieldChanges",      "buildings",    "yield",           engine.YIELDS,    "flat",    "city"),
    ("BuildingInfo",    "TechYieldModifiers",    "buildings",    "yield",           engine.YIELDS,    "percent", "city"),
    ("BuildingInfo",    "TechCommerceChanges",   "buildings",    "commerce",        engine.COMMERCES, "flat",    "city"),
    ("BuildingInfo",    "TechCommerceModifiers", "buildings",    "commerce",        engine.COMMERCES, "percent", "city"),
    ("BuildingInfo",    "TechHappinessChanges",  "buildings",    "happiness",       None,             "flat",    "city"),
    ("BuildingInfo",    "TechHealthChanges",     "buildings",    "health",          None,             "flat",    "city"),
    ("BuildingInfo",    "TechSpecialistChanges", "buildings",    "freeSpecialists", None,             "flat",    "city"),
    ("SpecialistInfo",  "TechHappinessTypes",    "specialists",  "happiness",       None,             "flat",    "city"),
    ("SpecialistInfo",  "TechHealthTypes",       "specialists",  "health",          None,             "flat",    "city"),
    ("ImprovementInfo", "TechYieldChanges",      "improvements", "yield",           engine.YIELDS,    "flat",    "team"),
    ("RouteInfo",       "TechMovementChanges",   "routes",       "movement",        None,             "flat",    "team"),
]

GRANTS = {"FirstFreeUnit": "firstFreeUnit", "FirstFreeProphet": "firstFreeProphet", "iFirstFreeTechs": "freeTechs"}


def era_fn(rec, store):
    e = engine.text(rec.find("Era"))
    return (e.replace("C2C_ERA_", "").replace("ERA_", "").lower()) or "none"


CFG = cc.EntityConfig("TechInfo", cost_rename={"iCost": "research"}, grants=GRANTS, era_fn=era_fn)

if __name__ == "__main__":
    cc.main(CFG, TECH_BOOSTS, os.path.join(REPO, "Assets", "Data", "techs"))
