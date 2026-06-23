#!/usr/bin/env python3
"""Curate UnitCombat (#428, Tier D #29) — the unit-COMBAT-CLASS (UNITCOMBAT_MELEE/ARCHER/...). A unit-plane
stat SOURCE like Promotion: it deposits onto a unit via CvUnit::processUnitCombat's changeExtra*/change*Count
stack (the §5 self-accumulator). **REUSES the Promotion #28 unit-stat vocabulary VERBATIM** (imported tables) —
this is the entity that, with Promotion, DEFINES that vocabulary (modifier-spec §5). EXE-link 0 DllExport.

SAME as Promotion (imported): the `*Change` stat fields -> the same families (strength/withdrawal/firstStrike/
air/collateral/heal/movement/experience/workRate/cargo/upkeep/vision/capture/poison/espionage/trap/...), the
CAPABILITIES boolean group, the vision/LOS resolver, properties -> scoped deposits (property_source_v3), the
vs-keyed combat modifiers (under DIFFERENT XML container names: TerrainAttackChangeModifiers vs Promotion's
TerrainAttacks — same {Type,value} shape). loadPrune from OnGameOptions.

UnitCombat-SPECIFIC:
- The `*Base` ranks (iQualityBase/iGroupBase/iSizeBase [-10 sentinel] + iRBombardDamage/DCMBomb *Base) are the
  CLASS's intrinsic CREATE-UNIT base stats -> identity.base (§0.6 "create-unit-subroutine data, kept as-is"
  pending the Size-Matters pass; NOT cascade modifiers, NOT the `*Change` deltas Promotion carries).
- `outcomes` (KillOutcomes + Actions): the CvOutcomeList kill/action-mission system (subdue-animal/capture-on-
  kill, gather, ...) -> carried faithfully (engine.generic) into an `outcomes` section; its own system, refined
  later (#430 / outcome-system pass).
- ReligionType/CultureType/EraType (textual refs), bForMilitary/bForNavalMilitary (AI tags), GGptsforUnitTypes
  (great-general points per killed unit-type), DefaultStatusTypes (auto-applied statuses) -> identity (PARKED,
  flagged: their proper homes — enabler? grants? — settle at the unit-plane enabling / Unit pass).
- Extra capability bools: spy/cannotMergeSplit/rBombardDirect/rBombardForceAbility/alwaysInvisible(bInvisible)/
  healsAs ; extra count: noCapture.
DROP: Categories (dead).

  python3 curate_unitcombat.py --sample UNITCOMBAT_MELEE UNITCOMBAT_ARCHER UNITCOMBAT_ANIMAL
  python3 curate_unitcombat.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from store import Store, REPO
from curate_common import FAMILY_ORDER, put_art, emit_art, descale100
# REUSE the Promotion unit-stat vocabulary (the shared §5 definition) + helpers.
from curate_promotion import (STRENGTH, FAMILIES, CAP_BOOL, CAP_PAIR, CAP_COUNT, VISION_PAIRS,
                              VISION_STRUCTS, _txt, _int, _simple_list, _pairs)

# UnitCombat-specific extensions to the shared tables.
CAP_BOOL_X = {"bSpy": "spy", "bCannotMergeSplit": "cannotMergeSplit", "bRBombardDirect": "rBombardDirect",
              "bRBombardForceAbility": "rBombardForceAbility", "bInvisible": "alwaysInvisible", "bHealsAs": "healsAs"}
CAP_COUNT_X = {"iNoCaptureChange": "noCapture"}
VISION_PAIRS_X = {"VisibilityIntensitySameTileChangeTypes": "visibilityIntensitySameTile"}
# vs-keyed combat modifiers — UnitCombat's container names (struct-vectors {Type, iModifier}); same homes as
# Promotion's VS_KEYED. (family, keyword, member|None, unit)
VS_KEYED = {
    "TerrainAttackChangeModifiers":   ("strength", "terrain", "attack", "percent"),
    "TerrainDefenseChangeModifiers":  ("strength", "terrain", "defense", "percent"),
    "FeatureAttackChangeModifiers":   ("strength", "feature", "attack", "percent"),
    "FeatureDefenseChangeModifiers":  ("strength", "feature", "defense", "percent"),
    "UnitCombatChangeModifiers":      ("strength", "unitCombat", None, "percent"),
    "DomainMods":                     ("strength", "domain", None, "percent"),
    "FlankingStrengthbyUnitCombatTypeChange": ("strength", "flanking", None, "percent"),
    "TerrainWorkChangeModifiers":     ("workRate", "terrain", None, "percent"),
    "FeatureWorkChangeModifiers":     ("workRate", "feature", None, "percent"),
    "BuildWorkChangeModifiers":       ("workRate", "build", None, "percent"),
    "TrapAvoidanceUnitCombatTypes":   ("trap", "avoidance", None, "flat"),
}
CAP_LIST = {"TerrainDoubleMoveChangeTypes": "terrainDoubleMove",
            "FeatureDoubleMoveChangeTypes": "featureDoubleMove",
            "TrapImmunityUnitCombatTypes": "trapImmunity"}
# *Base -> identity.base (§0.6 create-unit base data). quality/group/size use a -10 "unset" sentinel.
BASE_SENTINEL10 = {"iQualityBase": "quality", "iGroupBase": "group", "iSizeBase": "size"}
BASE_PLAIN = {"iRBombardDamageBase": "rangedBombardDamage", "iRBombardDamageLimitBase": "rangedBombardLimit",
              "iRBombardDamageMaxUnitsBase": "rangedBombardMaxUnits", "iDCMBombRangeBase": "dcmRange",
              "iDCMBombAccuracyBase": "dcmAccuracy"}
ID_REF = {"ReligionType": "religion", "CultureType": "culture", "EraType": "era"}
ID_BOOL = {"bForMilitary": "forMilitary", "bForNavalMilitary": "forNavalMilitary"}
ID_LIST = {"GGptsforUnitTypes": "ggPointsForUnits", "DefaultStatusTypes": "defaultStatuses"}
# DROP: dead/handled-elsewhere. FeatureAttacks/FeatureDefenses (2 recs) + iWithdrawalProb (1) are WRONG-TAG
# entries the engine ignores (it reads FeatureAttackChangeModifiers / iWithdrawalChange) -> dead in-game (the
# Promotion iStealthCombatModifier-typo pattern). Button handled via put_art.
DROP = {"Type", "Description", "Help", "Categories", "FeatureAttacks", "FeatureDefenses", "iWithdrawalProb"}


def curate(typ, rec, store):
    out = OrderedDict([("type", typ)])
    for tag, key in (("Description", "description"), ("Help", "help")):
        t = _txt(rec, tag)
        if t:
            out[key] = t

    fams, caps, vision, identity = OrderedDict(), OrderedDict(), OrderedDict(), OrderedDict()

    def fam_unit(family):
        return fams.setdefault(family, OrderedDict()).setdefault("unit", OrderedDict())

    # --- shared scalar strength members + other families (imported tables; UC-absent tags skip) ---
    for tag, (member, unit) in STRENGTH.items():
        v = _int(rec, tag)
        if v:
            node = fam_unit("strength")
            if member:
                node = node.setdefault(member, OrderedDict())
            node[unit] = v
    for tag, (family, member, unit) in FAMILIES.items():
        v = _int(rec, tag)
        if tag.endswith("100"):                # one-time x100 -> human de-scale (cascade-fixed-point.md §2; iExtraUpkeep100)
            v = descale100(v) if v is not None else v
        if v:
            node = vision if family == "vision" else fam_unit(family)
            if member:
                node = node.setdefault(member, OrderedDict())
            node[unit] = v
    if "vision" in fams and not fams["vision"]["unit"]:
        fams.pop("vision")

    # --- vs-keyed combat/work modifiers (UC container names) ---
    for tag, (family, kw, member, unit) in VS_KEYED.items():
        node = rec.find(tag)
        if node is None:
            continue
        for k, v in _pairs(node):
            base = fam_unit(family).setdefault(kw, OrderedDict()).setdefault(k, OrderedDict())
            if member:
                base = base.setdefault(member, OrderedDict())
            base[unit] = v

    # --- capabilities (shared + UC extras) ---
    for tag, name in dict(CAP_BOOL, **CAP_BOOL_X).items():
        if engine.text(rec.find(tag)) in ("1", "true", "True"):
            caps[name] = True
    for tag, (name, grant) in CAP_PAIR.items():
        if engine.text(rec.find(tag)) in ("1", "true", "True"):
            caps[name] = grant
    for tag, name in dict(CAP_COUNT, **CAP_COUNT_X).items():
        if _int(rec, tag):
            caps[name] = True
    for tag, name in CAP_LIST.items():
        node = rec.find(tag)
        if node is not None:
            for k in _simple_list(node):
                caps.setdefault(name, OrderedDict())[k] = True

    # --- vision / LOS resolver (shared pairs + UC SameTile + shared struct-vectors) ---
    for tag, name in dict(VISION_PAIRS, **VISION_PAIRS_X).items():
        node = rec.find(tag)
        if node is not None:
            for k, v in _pairs(node):
                vision.setdefault(name, OrderedDict())[k] = v
    for tag, (name, child_tags) in VISION_STRUCTS.items():
        node = rec.find(tag)
        if node is None:
            continue
        rows = []
        for item in list(node):
            row = OrderedDict()
            for ct in child_tags:
                t = engine.text(item.find(ct))
                if ct[:1] == "i":
                    if engine.is_int(t):
                        row[ct[1].lower() + ct[2:]] = int(t)
                elif t and t != "NONE":
                    row[ct[:-4].lower() if ct.endswith("Type") else ct] = t
            if row:
                rows.append(row)
        if rows:
            vision[name] = rows

    # --- properties -> scoped modifier deposits (v3, SAME_PLOT = containment default) ---
    pm = rec.find("PropertyManipulators")
    if pm is not None:
        for src in pm.findall("PropertySource"):
            res = engine.property_source_v3(src)
            if res:
                prop, scope, unit, value = res
                node = fams.setdefault(prop, OrderedDict()).setdefault(scope, OrderedDict())
                if unit in node and isinstance(node[unit], int) and isinstance(value, int):
                    node[unit] += value
                else:
                    node[unit] = value

    # --- outcomes (KillOutcomes + Actions): the CvOutcomeList system, carried faithfully ---
    outcomes = OrderedDict()
    for tag, key in (("KillOutcomes", "kill"), ("Actions", "actions")):
        node = rec.find(tag)
        if node is not None and list(node):
            outcomes[key] = engine.generic(node)

    # --- loadPrune ---
    loadprune = OrderedDict()
    for tag, key in (("OnGameOptions", "onGameOptions"), ("NotOnGameOptions", "notOnGameOptions")):
        node = rec.find(tag)
        if node is not None:
            lst = _simple_list(node)
            if lst:
                loadprune[key] = lst

    # --- identity: *Base create-unit data (§0.6) + refs + AI tags + parked lists ---
    base = OrderedDict()
    for tag, key in BASE_SENTINEL10.items():
        v = _int(rec, tag)
        if v is not None and v != -10:
            base[key] = v
    for tag, key in BASE_PLAIN.items():
        v = _int(rec, tag)
        if v:
            base[key] = v
    if base:
        identity["base"] = base
    for tag, key in ID_REF.items():
        v = _txt(rec, tag)
        if v:
            identity[key] = v
    for tag, key in ID_BOOL.items():
        if engine.text(rec.find(tag)) in ("1", "true", "True"):
            identity[key] = True
    for tag, key in ID_LIST.items():
        node = rec.find(tag)
        if node is not None:
            lst = _simple_list(node)
            if lst:
                identity[key] = lst

    # --- art ---
    art_blocks = OrderedDict()
    put_art(art_blocks, "Button", engine.text(rec.find("Button")))   # the combat-class icon

    # --- assemble (reserved order) ---
    obsoletes = store.obsoletes_of(typ)
    if obsoletes:
        out["obsoletes"] = OrderedDict((k, obsoletes[k]) for k in sorted(obsoletes))
    ordered = [f for f in FAMILY_ORDER if f in fams] + [f for f in fams if f not in FAMILY_ORDER]
    for f in ordered:
        out[f] = fams[f]
    if caps:
        out["skills"] = caps
    if vision:
        out["vision"] = vision
    if outcomes:
        out["outcomes"] = outcomes
    if loadprune:
        out["loadPrune"] = loadprune
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("UnitCombatInfo")
    results = OrderedDict((typ, curate(typ, rec, store)) for typ, rec in table.items())
    n = len(results)
    # COVERAGE CHECK
    handled = (set(STRENGTH) | set(FAMILIES) | set(VS_KEYED) | set(CAP_BOOL) | set(CAP_BOOL_X)
               | set(CAP_PAIR) | set(CAP_COUNT) | set(CAP_COUNT_X) | set(CAP_LIST) | set(VISION_PAIRS)
               | set(VISION_PAIRS_X) | set(VISION_STRUCTS) | set(BASE_SENTINEL10) | set(BASE_PLAIN)
               | set(ID_REF) | set(ID_BOOL) | set(ID_LIST) | DROP
               | {"PropertyManipulators", "KillOutcomes", "Actions", "OnGameOptions", "NotOnGameOptions",
                  "Button"})
    from collections import Counter
    leftover = Counter()
    for _typ, rec in table.items():
        for c in rec:
            if c.tag not in handled:
                leftover[c.tag] += 1
    print("COVERAGE: all XML tags handled." if not leftover else
          "UNHANDLED tags (count): %s" % ", ".join("%s=%d" % (t, c) for t, c in leftover.most_common()))
    has = lambda k: sum(1 for o in results.values() if k in o)
    STRUCT = {"type", "description", "help", "obsoletes", "skills", "vision", "outcomes",
              "loadPrune", "identity"}
    seen = sorted({f for o in results.values() for f in o if f not in STRUCT})
    print("UnitCombatInfo curated: %d" % n)
    for k in ("obsoletes", "skills", "vision", "outcomes", "loadPrune", "identity"):
        print("  with %-12s: %d" % (k, has(k)))
    print("  families seen: %s" % ", ".join(seen))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            if nm in results:
                print("\n=== %s ===" % nm)
                print(json.dumps(results[nm], indent=1, ensure_ascii=False))
            else:
                print("\n(%s not found)" % nm)
    if args.write:
        base = os.path.join(REPO, "Assets", "Data", "unitcombats")
        os.makedirs(base, exist_ok=True)
        for typ, obj in results.items():
            with open(os.path.join(base, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        print("\nwrote %d UnitCombatInfo JSON files under Assets/Data/unitcombats" % n)


if __name__ == "__main__":
    main()
