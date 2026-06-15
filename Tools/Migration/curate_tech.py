#!/usr/bin/env python3
"""Curate Tech to the top-down model (#428) — thin config over curate_common (the shared core).

  python3 curate_tech.py --sample TECH_LANGUAGE
  python3 curate_tech.py --write
"""
import os
from collections import OrderedDict

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


# --- `requires.build` — the multi-parent CONFIRM (enabler-spec §3/§13.8) ----------------------------------------
# A tech is PROPOSED into the candidate frontier by `enables` from a parent (forward, store-built); `requires.build`
# then CONFIRMS it actually has ALL its parents (the tech-tree AND/OR the flat `enables` cannot express). Authored
# from the tech's OWN prereq fields — which the store flattens into OTHER techs' `enables.techs` for generation but
# does NOT retain as the child's grouping. So we read them back off the child here. Tech is monotonic → `build`
# only (no `operate`/dormancy, §13.8). Atom = {type, scope[, min]}: explicit + self-describing (§6.1).
#   AndPreReqs/PrereqTech      -> build.all  (AND of team-scope tech presence atoms)
#   OrPreReqs/PrereqTech       -> build.any  (one OR-group of tech atoms)
#   PrereqBuildings (AND)      -> build.all  (empire-scope building COUNT atoms, min = iNumBuildingNeeded)  [no data today]
#   PrereqOrBuildings (OR)     -> build.any  (one OR-group of building count atoms)  [2 techs: waterproof-concrete/lead-glass]
# Tech presence is binary (never count) -> tech atoms carry NO `min`; buildings ARE count-capable (getBuildingCount,
# CvPlayer::hasValidBuildings) -> empire scope + explicit `min`. `any` is a LIST OF OR-GROUPS (each group AND-ed with
# the rest), so a tech OR-group and a building OR-group stay DISTINCT requirements (modifier-spec §3 nested form).

def _tech_atom(t):
    return OrderedDict([("type", t), ("scope", "team")])


def _building_atom(node):
    typ = engine.text(node.find("BuildingType"))
    if not typ or typ == "NONE":
        return None
    n = engine.text(node.find("iNumBuildingNeeded"))
    return OrderedDict([("type", typ), ("scope", "empire"), ("min", int(n) if engine.is_int(n) else 1)])


def _techs_of(rec, container):
    node = rec.find(container)
    if node is None:
        return []
    out = []
    for c in node.findall("PrereqTech"):
        t = engine.text(c)
        if t and t != "NONE":
            out.append(_tech_atom(t))
    return out


def _buildings_of(rec, container, child):
    out = []
    for node in rec.findall(container + "/" + child):
        atom = _building_atom(node)
        if atom:
            out.append(atom)
    return out


def requires_fn(rec, store):
    all_atoms = _techs_of(rec, "AndPreReqs") + _buildings_of(rec, "PrereqBuildings", "PrereqBuilding")
    any_groups = []
    # An OR-group is the engine's at-least-one-of set. A SINGLE-member OR-group is logically a hard requirement
    # ("at least one of {X}" == "X"), so it FOLDS into `all` — lossless, and it keeps the output clean (934 of 939
    # techs have a 1-member OrPreReqs; only 5 are genuine multi-way ORs). Multi-member groups stay in `any`.
    for group in (_techs_of(rec, "OrPreReqs"),
                  _buildings_of(rec, "PrereqOrBuildings", "PrereqOrBuilding")):
        if len(group) == 1:
            all_atoms.append(group[0])
        elif group:
            any_groups.append(group)
    build = OrderedDict()
    if all_atoms:
        build["all"] = all_atoms
    if any_groups:
        build["any"] = any_groups
    # bGlobal (owner 2026-06-15) — the religion-uniqueness RACE gate. Not identity: a `requires` NEGATIVE at WORLD
    # scope — "NOT the same tech already researched anywhere" (CvPlayer::canEverResearch: a global tech is barred
    # once countKnownTechNumTeams>0). The 29 bGlobal techs are EXACTLY the 29 religion-prereq techs ("religions go
    # under this heading"). SELF = this same tech; world scope = any team. `noneOf` = the negative clause container.
    g = rec.find("bGlobal")
    if g is not None and engine.text(g) in ("1", "true", "True"):
        build["noneOf"] = [OrderedDict([("type", "SELF"), ("scope", "world")])]
    return {"build": build} if build else None


CFG = cc.EntityConfig("TechInfo", cost_rename={"iCost": "research"}, grants=GRANTS, era_fn=era_fn,
                      requires_fn=requires_fn, extra_drop={"bGlobal"})

if __name__ == "__main__":
    cc.main(CFG, TECH_BOOSTS, os.path.join(REPO, "Assets", "Data", "techs"))
