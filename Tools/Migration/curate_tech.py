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

# Tech-conditioner folds -- MOSTLY RETIRED (owner 2026-06-20, modifier.md §6.5). A tech is the ENABLING giver, never
# a modifier HOME, so a tech-conditioned effect stays KEEP-ON-SOURCE (`enabled:{tech}`) on the entity that owns it:
#   building yield/commerce/happiness/health -> curate_building COND_KEYED (keep-on-building, enabled);
#   improvement yield -> curate_improvement post_process; specialist happiness/health -> curate_specialist (self).
# route TechMovementChanges: RELOCATED to the ROUTE (owner ruling 2026-07-01) — curate_route.post_process now homes
#   it as a tech-gated `movement.plot.flat` deposit (the route owns its move cost, json §6.2; the tech is the
#   `enabled` conditioner). REMOVED from here to avoid double-homing the delta.
# One row REMAINS, FLAGGED for follow-up (still inverted onto the tech meanwhile -- harmless, unconsumed):
#   - building TechSpecialistChanges (freeSpecialists keyed by specialist): a keyed+tech-`enabled` combo the
#     keep-on-building machinery doesn't express yet -> belongs on the building (COND_KEYED follow-up).
TECH_BOOSTS = []

# FreeSpecialistCounts -> freeSpecialists.team.{SPECIALIST_X}: N (owner ruling 2026-07-01; carried in the
# mapping's `channels` with scope:"team"). TEAM scope because a tech grant is team-wide (modifier.md §6
# "Specialist counts"; CvTeam holds the free-specialist accumulator, read into cities at CvCity.cpp:14277).
# ⚠ CARRIED FAITHFULLY BUT INERT — the tech free-specialist WRITE-PATH is UNWIRED: no CvTeam::processTech ever
# calls changeFreeSpecialistCount for a tech's FreeSpecialistCounts (only building/civic/unit/event/era-advance
# do). The ONLY reader is AI valuation (CvPlayerAI.cpp:4776). So this is DEAD DATA — a despair-log candidate.
# (Base XML: exactly ONE tech carries it — SPECIALIST_MERCHANT:1.)

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
    for g in any_groups:                                         # OR-groups -> nested {any} under all (any = ||, not a list-of-groups)
        all_atoms.append(g[0] if len(g) == 1 else OrderedDict([("any", g)]))
    build = OrderedDict()
    if all_atoms:
        build["all"] = all_atoms
    return {"build": build} if build else None


def allowed_fn(rec, store):
    """bGlobal (owner 2026-06-15/2026-06-17) — the religion-uniqueness RACE gate, now the unified `allowed` cap:
    `allowed:{world:1}` = "at most ONE of this tech may be researched anywhere in the world" (CvPlayer::canEverResearch
    bars a global tech once countKnownTechNumTeams>0). The 29 bGlobal techs are EXACTLY the 29 religion-prereq techs
    ("religions go under this heading"). Replaces the old `requires.build.noneOf:[SELF@world]` spelling — one idiom
    for "only N may exist," authoring the real cap (1), engine enforces (enabler-spec §5/§13.7)."""
    g = rec.find("bGlobal")
    if g is not None and engine.text(g) in ("1", "true", "True"):
        return OrderedDict([("world", 1)])
    return None


CFG = cc.EntityConfig("TechInfo", cost_rename={"iCost": "research"}, grants=GRANTS, era_fn=era_fn,
                      requires_fn=requires_fn, allowed_fn=allowed_fn, extra_drop={"bGlobal"},
                      enabler_block="capabilities")   # tech enabler channels are empire CAPABILITIES (owner 2026-06-29)

if __name__ == "__main__":
    cc.main(CFG, TECH_BOOSTS, os.path.join(REPO, "Assets", "Data", "techs"))
