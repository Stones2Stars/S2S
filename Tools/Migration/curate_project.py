#!/usr/bin/env python3
"""Curate Project (#428) — a buildable wonder/megaproject. BESPOKE (per-field scopes), modelled on curate_era.
Its effects are deposited on completion by CvTeam::processProjectChange (CvTeam.cpp:4508-4577), which pins the
scope of every field — verified line-by-line against that function:

- TEAM scope: iNukeInterception (changeNukeInterception, 4513), iTechShare (changeTechShareCount, 4517).
- EMPIRE scope (per-player ON the team, inside `player.getTeam()==getID()`): iGlobalHappiness/iGlobalHealth,
  iInflationModifier, the four maintenance modifiers, CommerceModifiers (4559-4570).
- WORLD scope (EVERY alive player, OUTSIDE the team check): iWorldHappiness/iWorldHealth/iWorldTradeRoutes
  (4572-4574). The first-pass mapping mis-scoped these as team — the C++ is explicit they are world.
  ⚠ They emit the PLURAL TARGET `world.empires`, never a bare `world` flat: WORLD is CONFIG and carries no
  package, so "grants something to every player" is the §3.3 empires fan that lands in EACH PLAYER's package
  (state-repositories.md — this was the flagged mis-scoped-data curator fix). A bare `world.<unit>` here would
  compile to a slot no scope stores and the value would simply vanish.

Modeling calls made this pass (light-batch-classification.json + the C++):
- The victory-launch cluster (VictoryThresholds/VictoryMinThresholds per-victory + iVictoryDelayPercent +
  iSuccessRate) is a NON-CASCADE structural `victory` section, NOT a modifier family: these don't sum down a
  scope spine or deposit onto leaves — they are per-victory launch parameters read by victory resolution
  (CvTeam::getVictoryDelay/getLaunchSuccessRate, CvGame::testVictory). Forcing them into the additive-family
  vocabulary with a fake scope/unit would misrepresent them.
- iCost -> a `cost` section ({create: N}): the project's intrinsic base hammer cost. The universal `costs`
  family (GameSpeed/Era costs.world.create.percent) MULTIPLIES this base; the base lives on the info.
- BonusProductionModifiers -> buildRate.self (build THIS project faster WHILE a bonus is present; owner 2026-06-16).
  SELF build-rate gated by bonus presence, unified with building/unit into the buildRate family. (The earlier
  "DROP, the Bonus curator folds it" note was STALE: curate_bonus's BONUS_BOOSTS fold was removed under the §6
  keep-on-source ruling, so the project's build-faster-with-bonus was being silently LOST — restored here.)
- YieldModifiers -> DROP: DEAD structure. CvProjectInfo has no YieldModifier member/getter/consumer; the
  <YieldModifiers> XML element is never read (only <CommerceModifiers> is). bTechShareWithHalfCivs -> DROP: DEAD
  (no consumer outside CvProjectInfo's own getter/checksum).
- TechPrereq -> DROP, store inverts to tech.enables.projects. PrereqProjects -> DROP, store inverts to the
  prerequisite project's enables.projects (the SS_* parts need PROJECT_APOLLO_PROGRAM), so this project emits an
  `enables` block via store.enabled_by. NB the per-edge iNeeded count (all 1 today) is NOT carried by the
  set-based enables index; a count>1 would need a count-bearing edge. AnyonePrereqProject -> DROP (unused in
  current data; if ever set it needs its own store edge). VictoryPrereq -> identity.launchesVictory (the victory
  this project's creation LAUNCHES — not a build prereq; the reverse 'launchedBy' is a cold-path derived edge).
- EveryoneSpecialUnit -> grants (one-shot makeSpecialUnitValid on completion).
- EveryoneSpecialBuilding -> enables.specialBuildings (owner 2026-07-01): completion flips the game-wide
  SpecialBuildingValid flag (CvGame::makeSpecialBuildingValid) — it UNLOCKS constructibility, hands out no
  instance, so it is an `enables` edge (the specialBuildings enables bucket, json.md §4.1), NOT a grant.

Family/member names for nukeInterception (combat) and techShare (diplomacy) are PROVISIONAL (owner: nail down
later); techShare sits with handicap's diplomacy.noTechTrade/techTradeKnown tech-diffusion members.

  python3 curate_project.py --sample PROJECT_SDI PROJECT_SS_ENGINE PROJECT_THE_INTERNET
  python3 curate_project.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import curate_common as cc
from curate_common import de_i
from store import Store, REPO

# tag -> (family, scope, member, unit). member None = singleton family.
FAMILIES = {
    "iNukeInterception":              ("combat",      "team",   "nukeInterception", "percent"),
    "iTechShare":                     ("diplomacy",   "team",   "techShare",        "flat"),
    "iGlobalMaintenanceModifier":     ("maintenance", "empire", "all",              "percent"),
    "iDistanceMaintenanceModifier":   ("maintenance", "empire", "distance",         "percent"),
    "iNumCitiesMaintenanceModifier":  ("maintenance", "empire", "numCities",        "percent"),
    "iConnectedCityMaintenanceModifier": ("maintenance", "empire", "connectedCity", "percent"),
    "iInflationModifier":             ("inflation",   "empire", None,               "percent"),
    "iGlobalHappiness":               ("happiness",   "empire", None,               "flat"),
    "iGlobalHealth":                  ("health",      "empire", None,               "flat"),
    "iWorldHappiness":                ("happiness",   "world",  "empires",               "flat"),
    "iWorldHealth":                   ("health",      "world",  "empires",               "flat"),
    "iWorldTradeRoutes":              ("tradeRoutes", "world",  "empires",               "flat"),   # ruling 11: kind 0 IS the route count (memberless -- the reconciliation micro-fix)
}
# CommerceModifiers: SPLIT per-identifier commerce families (gold/research/culture/espionage), empire/percent.
SPLIT_COMMERCE = {"CommerceModifiers": ("empire", "percent")}
GRANTS = {"EveryoneSpecialUnit": "grantsSpecialUnit"}
TEXT = {"Description": "description", "Civilopedia": "civilopedia"}
# art tags -> ui/world/sound via ART_BLOCK (CreateSound -> sound.onCompletion; MovieDefineTag -> ui.art.movie.defineTag).
ART = {"Button", "MovieDefineTag", "CreateSound"}
# intrinsic identity, with clean names. iCost handled separately (own `cost` section).
IDENTITY = {"VictoryPrereq": "launchesVictory", "MapCategoryTypes": "mapCategories", "Categories": "categories"}
# dead structure (no consumer) + derived enabler/prereq edges (store-inverted) -> never authored.
# (AnyonePrereqProject is NOT dropped -- it maps to a requires.build world-scope atom, handled below.)
DROP = {"YieldModifiers", "bTechShareWithHalfCivs",
        "TechPrereq", "PrereqProjects"}
# the per-victory launch cluster -> a non-cascade `victory` section.
VICTORY_KEYED = {"VictoryThresholds": "thresholds", "VictoryMinThresholds": "minThresholds"}
VICTORY_SCALAR = {"iVictoryDelayPercent": "delayPercent", "iSuccessRate": "successRate"}
FAMILY_ORDER = ["buildRate", "combat", "diplomacy", "maintenance", "upkeep", "happiness", "health", "tradeRoutes",
                "gold", "research", "culture", "espionage"]


def _put(fam, family, scope, member, unit, val):
    node = fam.setdefault(family, {}).setdefault(scope, {})
    if member:
        node = node.setdefault(member, {})
    node[unit] = val


def _keyed_ints(node):
    """<Foo><FooEntry><XType>K</XType><iVal>n</iVal></FooEntry>...> -> {K: n} (first *Type child = key)."""
    out = OrderedDict()
    for entry in list(node):
        key, val = None, None
        for c in entry:
            if key is None and c.tag.endswith("Type"):
                key = engine.text(c)
            elif engine.is_int(engine.text(c)):
                val = int(engine.text(c))
        if key and val is not None:
            out[key] = val
    return out


def curate(typ, rec, store):
    text, fam, grants, art_blocks, identity, victory, cost, requires, leftover = {}, {}, {}, {}, {}, {}, {}, {}, []
    allowed = OrderedDict()
    enables_special = []   # EveryoneSpecialBuilding -> enables.specialBuildings (unlocks constructibility)
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in FAMILIES:
            if engine.is_int(t) and int(t) != 0:
                family, scope, member, unit = FAMILIES[tag]
                _put(fam, family, scope, member, unit, int(t))
        elif tag in SPLIT_COMMERCE:
            scope, unit = SPLIT_COMMERCE[tag]
            for member, v in engine.named_array(c, engine.COMMERCES).items():  # member IS the family (split)
                _put(fam, member, scope, None, unit, v)
        elif tag == "BonusProductionModifiers":
            # build THIS project faster WHILE a bonus is present -> buildRate.self (owner 2026-06-16; unified with
            # building/unit). SELF build-rate gated by bonus presence, NOT folded onto the bonus (BONUS_BOOSTS gone).
            lst = fam.setdefault("buildRate", {}).setdefault("self", {}).setdefault("percent", [])
            for bonus, v in _keyed_ints(c).items():
                lst.append(OrderedDict([("value", v),
                                        ("enabled", OrderedDict([("type", bonus), ("scope", "city"), ("min", 1)]))]))
        elif tag == "iCost":
            if engine.is_int(t) and int(t) != 0:
                cost["create"] = int(t)
        elif tag in VICTORY_KEYED:
            keyed = _keyed_ints(c)
            if keyed:
                victory[VICTORY_KEYED[tag]] = keyed
        elif tag in VICTORY_SCALAR:
            if engine.is_int(t) and int(t) != 0:
                victory[VICTORY_SCALAR[tag]] = int(t)
        elif tag == "EveryoneSpecialBuilding":
            # completion UNLOCKS constructibility (CvGame::makeSpecialBuildingValid) -> enables edge, not a grant.
            if t and t != "NONE":
                enables_special.append(t)
        elif tag in GRANTS:
            v = int(t) if engine.is_int(t) else (t or None)
            if v not in (None, "", "NONE"):
                grants[GRANTS[tag]] = v
        elif tag == "iMaxGlobalInstances":
            # -> allowed:{world:N} (enabler-spec §5; Gate-1 find 2026-07-02: parked in identity, canCreate over-offered
            # already-built world projects). -1 = unlimited -> omitted.
            if engine.is_int(t) and int(t) > 0:
                allowed["world"] = int(t)
        elif tag == "iMaxTeamInstances":
            if engine.is_int(t) and int(t) > 0:
                allowed["team"] = int(t)
        elif tag == "AnyonePrereqProject":
            # a SINGLE project that must be built by ANY player -> a world-scope presence prereq (owner 2026-07-01;
            # CvPlayer.cpp:6868 blocks when getProjectCreatedCount(project)==0). NOT the `any` combinator (one project).
            if t and t != "NONE":
                requires["build"] = OrderedDict([("type", t), ("scope", "world")])
        elif tag in ART:
            cc.put_art(art_blocks, tag, engine.generic(c))   # -> ui/world/sound via ART_BLOCK
        elif tag in IDENTITY:
            if t or list(c):
                identity[IDENTITY[tag]] = engine.generic(c)
        elif tag[:1] == "b":                              # capability flag (bSpaceship/bAllowsNukes) -> identity bool
            if t in ("1", "true", "True"):
                identity[tag[1].lower() + tag[2:]] = True
        else:
            if list(c) or t:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(c)

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "civilopedia"):
        if k in text:
            out[k] = text[k]
    enables = dict(store.enabled_by(typ))                  # project -> project (PrereqProjects; Apollo -> SS_* parts)
    if enables_special:                                    # completion unlocks these SPECIALBUILDING_* for construction
        enables["specialBuildings"] = enables_special
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    if requires:
        out["requires"] = requires
    if allowed:
        out["allowed"] = allowed
    for family in FAMILY_ORDER:
        if family in fam:
            out[family] = fam[family]
    for family in fam:
        if family not in out:
            out[family] = fam[family]
    if victory:
        out["victory"] = victory
    if grants:
        out["grants"] = grants
    if cost:
        out["cost"] = cost
    cc.emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    cc.fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out, leftover


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("ProjectInfo")
    results, all_leftover = OrderedDict(), set()
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec, store)
        results[typ] = obj
        all_leftover.update(leftover)
    print("ProjectInfo curated: %d" % len(results))
    if all_leftover:
        print("  leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "projects")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d ProjectInfo JSON files under Assets/Data/projects" % len(results))


if __name__ == "__main__":
    main()
