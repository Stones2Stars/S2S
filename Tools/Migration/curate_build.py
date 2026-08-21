#!/usr/bin/env python3
"""Curate Build (#428, Tier C #23) — the worker ACTION that PLACES improvements / lays routes / adds-removes
features / terraforms terrain, CLEANLY SEPARATED from those entities (owner 2026-06-16): it references each by FK
in a `produces` section, never embeds their data. A self-contained leaf action sitting OVER the done Improvement
#22 + Feature #21; enables nothing (store.enabled_by(BUILD_*) empty -> no `enables`).

THE DOUBLE-MAPPING (owner 2026-06-16): the IMPROVEMENT self-gates plot placement (its own requires.build, so an
EVENT can't drop a disallowed improvement, #22); the BUILD gates its OWN worker-action path. Not redundant — two
placement paths. So plot-VALIDITY (terrain/feature/etc.) is NOT duplicated onto the build; it lives on the
improvement. The build's `requires.build` is its OWN tech/bonus MEANS gate (CvPlayer::canBuild:7510).

requires.build (CvPlayer::canBuild + CvPlot::canBuild):
- all: {type:PrereqTech, scope:team} — the build's own tech (CvPlayer:7540). ALSO store-inverts to
  tech.enables.builds (generation) + tech.obsoletes.builds (ObsoleteTech) — this clause is the per-candidate
  CONFIRM, the two coexist (like Improvement/Tech). + per-bonus {type:BONUS_X, scope:plot, connection:trade}
  for the 3 PrereqBonusTypes builds (ALL must be adjacent-plot-group-connected, CvPlot:3398). ⚑ scope: the gate
  is plot-group-local trade reach; exact scope (plot vs city vs team) pins at #430. Edge case (3 geoglyph builds).

produces (the OUTCOME; owner-approved shape 2026-06-16) — outcome FKs + per-outcome tech/time/production:
- improvement / route / terrainChange / featureChange : single-FK outcomes (the improvement it lays, the route it
  lays, terraform-to terrain TerrainChange, feature planted/changed-to FeatureChange).
- features[] : FeatureStructs {feature, tech?, time?, production?, remove?} — the per-feature add/REMOVE. remove=true
  is the CHOP (clears the feature -> +production hammers, +time); a remove=false entry with only a tech is the
  per-feature TECH GATE ("road on a swamp needs Canal Systems"). Per-feature PrereqTech STAYS HERE (it is
  CONDITIONAL on the plot already having that feature — NOT an unconditional requires.build; CvPlayer:7550). It
  ALSO store-inverts to tech.enables.builds (generation). owner 2026-06-16: any edits land in the post-migration phase.
- terraform[] : TerrainStructs {terrain, tech?, time?} — per-terrain terraform time + tech gate (CvPlayer:7556).
Build PLACING A BONUS is a recognized outcome (owner 2026-06-16: "build should have the tooling to place a bonus"),
but the special `PlaceBonusTypes` struct that expressed it is DROPPED (below) and 0/304 builds use it — so no data
to author; the place-bonus tooling is the #430 outcome-system capability, and the Great Farmer (unit) is to be
reworked to ADHERE to that build tooling rather than its own unit-side hijack (Unit pass / post-migration).

FOLDERS (owner 2026-06-16) — builds are organized by PRIMARY outcome into Assets/Data/builds/<folder>/, priority
bonus > forts > routes > features > terraform > improvements > clearing (see folder()). The 5 core buckets the owner
named + terraform (own folder) + clearing (feature/terrain/pollution REMOVERS, no improvement laid; own folder).

cost (owner: a good call — becomes important for BuildingInfo/UnitInfo later): iCost -> cost.gold (gold spent,
CvPlayer::getBuildCost:7609), iTime -> cost.time (base worker-turns, CvPlot::getBuildTime:7568). iTime < 0 is the
build-DISABLED sentinel (CvPlot:3345) — carried faithfully.

ui/world: Button -> ui.art.icon (the build's button; the IMPROVEMENT carries NO button — it lives on the build).
HotKey/iHotKeyPriority/bShift/bCtrl/bAltDown -> ui.{hotkey,hotKeyPriority,shiftDown,ctrlDown,altDown} (CvHotkeyInfo
base; clean: bools only when true, priority only when non-zero — Build is the first hotkey-bearing entity, sets the
shape per ART_BLOCK). EntityEvent -> world.art.entityEvent (the on-map worker animation; EXE-bound,
getEntityEvent DllExport).

identity: bKill -> identity.consumesUnit (the build consumes the worker, isKill; widget CvDLLWidgetData:2943).

DROPPED:
- ObsoleteTech -> store tech.obsoletes.builds (drop from build).
- MissionType -> RUNTIME-assigned (m_iMissionType, NO_MISSION default, setMissionType), NOT XML-backed — never on
  the XML record. (getMissionType is DllExport but reads the runtime value.)
- MapCategoryTypes -> 0/304; a live placement gate (CvPlot:3406 isMapCategory(info)) but SPACEMAP-related (owner) —
  handled via existing tooling when spacemap is fixed properly. No build data today.
- PlaceBonusTypes -> 0/304; a SPECIAL place-a-bonus struct to be DROPPED (owner 2026-06-16). The place-bonus
  capability belongs to the build as canonical tooling (above); the special struct + the Great Farmer's unit-side
  use are the hacks to retire (Unit pass / post-migration). No build data today.
- Categories -> 0/304, no getBuildInfo(...).getCategory consumer — dead ("other"; same as Terrain/Improvement).

EXE-link: 2 DllExport — getEntityEvent (XML -> world.art.entityEvent) + getMissionType (runtime, dropped).

  python3 curate_build.py --sample BUILD_FARM BUILD_ROAD BUILD_REMOVE_FOREST
  python3 curate_build.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import put_art, emit_art, fold_text_to_identity
from store import Store, REPO


def _bool(rec, tag):
    return engine.text(rec.find(tag)) in ("1", "true", "True")


def _txt(rec, tag):
    t = engine.text(rec.find(tag))
    return t if (t and t != "NONE") else None


def _int(rec, tag):
    t = engine.text(rec.find(tag))
    return int(t) if engine.is_int(t) else None


def _requires(rec):
    """The build's OWN reversible MEANS gate (enabler-spec §3/§5) — its tech (CONFIRM) + connected bonus(es)."""
    allc = []
    tech = _txt(rec, "PrereqTech")
    if tech:
        allc.append(OrderedDict([("type", tech), ("scope", "team")]))
    pbt = rec.find("PrereqBonusTypes")
    if pbt is not None:
        for c in pbt.findall("PrereqBonusType"):
            b = engine.text(c)
            if b and b != "NONE":                                # ALL must be connected (CvPlot:3398) -> requires.build.all
                allc.append(OrderedDict([("type", b), ("scope", "plot"), ("connection", "trade")]))
    return OrderedDict([("build", OrderedDict([("all", allc)]))]) if allc else None


def _struct_list(rec, wrapper, item, fk_tag, fk_key, fields):
    """Per-outcome struct list (FeatureStructs/TerrainStructs): [{fk_key:FK, <emitted fields>}], FK first,
    zero/false/NONE members omitted (keep tech even when remove is false — it's the per-feature tech gate)."""
    node = rec.find(wrapper)
    if node is None:
        return []
    out = []
    for s in node.findall(item):
        fk = engine.text(s.find(fk_tag))
        if not fk or fk == "NONE":
            continue
        entry = OrderedDict([(fk_key, fk)])
        for tag, key, kind in fields:
            if kind == "tech":
                v = engine.text(s.find(tag))
                if v and v != "NONE":
                    entry[key] = v
            elif kind == "int":
                v = engine.text(s.find(tag))
                if engine.is_int(v) and int(v) != 0:
                    entry[key] = int(v)
            elif kind == "bool":
                if _bool(s, tag):
                    entry[key] = True
        out.append(entry)
    return out


def _produces(rec):
    p = OrderedDict()
    for tag, key in (("ImprovementType", "improvement"), ("RouteType", "route"),
                     ("TerrainChange", "terrainChange"), ("FeatureChange", "featureChange")):
        v = _txt(rec, tag)
        if v:
            p[key] = v
    features = _struct_list(rec, "FeatureStructs", "FeatureStruct", "FeatureType", "feature",
                            [("PrereqTech", "tech", "tech"), ("iTime", "time", "int"),
                             ("iProduction", "production", "int"), ("bRemove", "remove", "bool")])
    if features:
        p["features"] = features
    terraform = _struct_list(rec, "TerrainStructs", "TerrainStruct", "TerrainType", "terrain",
                             [("PrereqTech", "tech", "tech"), ("iTime", "time", "int")])
    if terraform:
        p["terraform"] = terraform
    return p or None


def _ui(rec, art_blocks):
    """Clean key-trigger block beside ui.art.icon (Build is the first hotkey-bearing entity)."""
    ui = art_blocks.setdefault("ui", OrderedDict())
    hk = _txt(rec, "HotKey")
    if hk and hk != "0":                                         # "0" is the no-hotkey placeholder (65 builds)
        ui["hotkey"] = hk
    pr = _int(rec, "iHotKeyPriority")
    if pr:
        ui["hotKeyPriority"] = pr
    for tag, key in (("bShiftDown", "shiftDown"), ("bCtrlDown", "ctrlDown"), ("bAltDown", "altDown")):
        if _bool(rec, tag):
            ui[key] = True
    if not ui:
        art_blocks.pop("ui", None)


def folder(rec, imp):
    """Classify a build into ONE outcome folder by PRIMARY outcome (owner 2026-06-16). A build can do several
    things at once (BUILD_ROAD lays a route AND clears features); priority picks the primary:
    bonus > forts > routes > features > terraform > improvements > clearing.
    - bonus    : the improvement it lays PLACES a bonus (improvement bPlacesBonus / BonusChange) — BUILD_BONUS_*.
    - forts    : the improvement is bActsAsCity / bMilitaryStructure — forts, bunkers, space bases.
    - routes   : lays a route (RouteType).
    - features : ADDS/plants a feature (FeatureChange).
    - terraform: changes terrain (TerrainChange).
    - improvements : lays a normal improvement (the bulk).
    - clearing : lays no improvement — removes feature/terrain/pollution (drain swamp, clear fallout, …)."""
    impt = _txt(rec, "ImprovementType")
    irec = imp.get(impt) if impt else None
    if irec is not None and (_bool(irec, "bPlacesBonus") or _txt(irec, "BonusChange")):
        return "bonus"
    if irec is not None and (_bool(irec, "bActsAsCity") or _bool(irec, "bMilitaryStructure")):
        return "forts"
    if _txt(rec, "RouteType"):
        return "routes"
    if _txt(rec, "FeatureChange"):
        return "features"
    if _txt(rec, "TerrainChange"):
        return "terraform"
    if impt:
        return "improvements"
    return "clearing"


def curate(typ, rec):
    out = OrderedDict([("type", typ)])
    for tag, key in (("Description", "description"), ("Help", "help")):
        t = _txt(rec, tag)
        if t:
            out[key] = t
    requires = _requires(rec)
    if requires:
        out["requires"] = requires
    produces = _produces(rec)
    if produces:
        out["produces"] = produces
    cost = OrderedDict()
    gold = _int(rec, "iCost")
    if gold:
        cost["gold"] = gold
    time = engine.text(rec.find("iTime"))                        # always emit (incl. 0 and the <0 disabled sentinel)
    if engine.is_int(time):
        cost["time"] = int(time)
    if cost:
        out["cost"] = cost
    art_blocks = OrderedDict()
    put_art(art_blocks, "Button", engine.text(rec.find("Button")))          # -> ui.art.icon
    put_art(art_blocks, "EntityEvent", engine.text(rec.find("EntityEvent")))  # -> world.art.entityEvent (EXE-bound)
    _ui(rec, art_blocks)
    emit_art(out, art_blocks)
    if _bool(rec, "bKill"):
        out["identity"] = OrderedDict([("consumesUnit", True)])
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("BuildInfo")
    imp = store.table("ImprovementInfo")
    results = OrderedDict((typ, (curate(typ, rec), folder(rec, imp))) for typ, rec in table.items())
    n = len(results)
    has = lambda k: sum(1 for (o, _f) in results.values() if k in o)
    from collections import Counter
    fcount = Counter(f for (_o, f) in results.values())
    print("BuildInfo curated: %d" % n)
    for k in ("requires", "produces", "cost", "ui", "world", "identity"):
        print("  with %-9s: %d" % (k, has(k)))
    print("  folders: %s" % ", ".join("%s=%d" % (f, fcount[f]) for f in
          ("bonus", "forts", "routes", "features", "terraform", "improvements", "clearing")))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            if nm in results:
                obj, f = results[nm]
                print("\n=== %s  (folder: %s) ===" % (nm, f))
                print(json.dumps(obj, indent=1, ensure_ascii=False))
            else:
                print("\n(%s not found)" % nm)
    if args.write:
        base = os.path.join(REPO, "Assets", "Data", "builds")
        os.makedirs(base, exist_ok=True)
        for typ, (obj, f) in results.items():
            d = os.path.join(base, f)
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        print("\nwrote %d BuildInfo JSON files under Assets/Data/builds/<folder>" % n)


if __name__ == "__main__":
    main()
