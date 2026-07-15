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


# --- TECH_GAME_START — the synthetic universal ROOT node (enabler-spec §2) --------------------------------------
# The generate tree is FULLY CONNECTED: every entity enters CAN GET via an inbound `enables` edge; there is no
# implicit "no-edge => always available" engine rule, so the start-available entities are authored onto this
# root's `enables`, derived MECHANICALLY here — never hand-listed (superseded-ideas #14). The rule: an entity is
# start-enabled iff NO inbound enables-family ADD edge exists anywhere in the store index ("no prereq in legacy
# => enabled from game start"), minus each kind's never-generated class — the SAME sentinel that kind's own
# curator translates:
#   techs        — minus bDisable placeholders; yields the 3 root successor techs. NB TECH_DUMMY is the legacy
#                  EVENT-GRANT PARKING NODE (owner): entities prereq'd on it (162 promotions, 8 builds,
#                  IMPROVEMENT_CITY) are deliberately unreachable in the tree — an inbound edge from a
#                  never-held tech — and are granted directly into HAVE by events/systems. They correctly do
#                  NOT root; do not "fix" them into the root.
#   civics       — none needed (the 15 start civics)
#   units        — minus spawn-only (iCost == -1 -> identity.spawnOnly, curate_unit) and minus ZERO instance cap
#                  (iMaxGlobal/Player/TeamInstances == 0 -> allowed:{...:0}, curate_unit.allowed_unit): a 0-cap
#                  unit can never be CREATED by training, only granted — UNIT_BAND, the start-only settler every
#                  civ receives at game start (owner ruling 2026-07-14; the first BUILDABLE settler is the
#                  tech-gated UNIT_TRIBE). A settler carrying NO iCost tag (population-based cost) otherwise
#                  stays; yields UNIT_BRUTE
#   buildings    — minus notConstructible (iCost in (None,-1), curate_building): pseudobuildings are placed by
#                  their own systems, never generated. PALACE + the special-building-group members remain — a
#                  group's TechPrereq lives in each member's requires.build (curate_building), so GENERATE stays
#                  conditional-free and the gate holds the tech (enabler-spec §1)
#   builds       — none needed (the BUILD_BONUS_* worker actions; plot validity is the live per-plot gate,
#                  enabler-spec §7.1 worker-builds exception)
#   improvements — minus system-placed (no BuildInfo produces it: city ruins, goody huts, worked markers — map
#                  substrate, reachable by no action)
#   promotions   — none needed: promotion->promotion prereq chains are requires-side level-up gates
#                  (canAcquirePromotion), so the whole no-tech/no-bonus set roots (the COMBAT1-5 class)
# Kinds deliberately NOT root-carried: religions/projects (all tech-gated — the derived set is empty),
# corporations (generated by the HQ-founding mechanic; every TechPrereq is NONE), heritages (mission-acquired),
# traits (leader-selected), bonuses (map substrate / reveal semantics), routes (reachable via their builds),
# promotionLines/specialBuildings (grouping metadata; their gates ride the members' requires).

def _start_enabled(store, ent, buckets, keep=None):
    # BONUS_-sourced enables edges are GATE-view data, never membership (owner ruling 2026-07-15): a bonus you'd
    # rely on a plot group for (traded / manufactured / vicinity) only ever gates via the target's requires atom
    # (which the inversion retains target-side), so it does not count as an inbound MEMBERSHIP edge here. An
    # entity whose only inbound edges are bonuses therefore roots — visible from game start, GREYED on its bonus
    # requirement (json.md §6 "grey on resources"). The plot-bonus→improvement carve-out (enables.builds) needs
    # no exception: those builds all carry tech edges, and their plot-bonus half is the live per-plot gate.
    have_edge = set()
    for src, bmap in store.enables.items():
        if src.startswith("BONUS_"):
            continue
        for b in buckets:
            have_edge |= bmap.get(b, set())
    return sorted(t for t, rec in store.table(ent).items()
                  if t not in have_edge and (keep is None or keep(t, rec)))


def synthesize_game_start(store, result):
    def cost(rec):
        v = engine.text(rec.find("iCost"))
        return int(v) if engine.is_int(v) else None

    def truthy(rec, tag):
        return engine.text(rec.find(tag)) in ("1", "true", "True")

    def cap0(rec):                       # a 0 instance cap at ANY scope = never created by build/train
        for tag in ("iMaxGlobalInstances", "iMaxPlayerInstances", "iMaxTeamInstances"):
            v = engine.text(rec.find(tag))
            if engine.is_int(v) and int(v) == 0:
                return True
        return False

    produced = set()                     # improvements a worker build lays (BuildInfo.ImprovementType)
    for rec in store.table("BuildInfo").values():
        t = engine.text(rec.find("ImprovementType"))
        if t and t != "NONE":
            produced.add(t)

    enables = OrderedDict()              # bucket order mirrors curate()'s sorted(enables)
    for bucket, ent, sbuckets, keep in (
        ("buildings",    "BuildingInfo",    ["buildings"],    lambda t, r: cost(r) not in (None, -1) and not cap0(r)),
        ("builds",       "BuildInfo",       ["builds"],       None),
        ("civics",       "CivicInfo",       ["civics"],       None),
        ("improvements", "ImprovementInfo", ["improvements"], lambda t, r: t in produced),
        ("processes",    "ProcessInfo",     ["processes"],    None),
        ("promotions",   "PromotionInfo",   ["promotions"],   None),
        ("techs",        "TechInfo",        ["techs"],        lambda t, r: not truthy(r, "bDisable")),
        ("units",        "UnitInfo",        ["units"],        lambda t, r: cost(r) != -1 and not cap0(r)),
    ):
        ids = _start_enabled(store, ent, sbuckets, keep)
        if ids:
            enables[bucket] = ids

    obj = OrderedDict([
        ("type", "TECH_GAME_START"),
        ("enables", enables),
        # canSetScienceRate/canSetEspionageRate are UNIVERSAL commerce-slider defaults with no legacy grantor
        # (CIV4CommerceInfo bFlexiblePercent) — their data home is the root (capabilities.md).
        ("capabilities", OrderedDict([("canSetScienceRate", True), ("canSetEspionageRate", True)])),
        ("ui", {"art": {"icon": ",Art/Interface/Buttons/Process/Blank.dds,Art/Interface/Buttons/Beyond_the_Sword_Atlas.dds,8,5"}}),
        ("identity", OrderedDict([
            ("description", "TXT_KEY_TECH_GAME_START"),
            ("civilopedia", "TXT_KEY_TECH_DUMMY_PEDIA"),
            ("era", "C2C_ERA_PREHISTORIC"),
            ("disable", True),           # never in the tech tree; held by every player from game start
        ])),
    ])
    result["TECH_GAME_START"] = (obj, "prehistoric")


if __name__ == "__main__":
    cc.main(CFG, TECH_BOOSTS, os.path.join(REPO, "Assets", "Data", "techs"), synthesize=synthesize_game_start)
