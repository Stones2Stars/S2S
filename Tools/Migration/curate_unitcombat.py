#!/usr/bin/env python3
"""Curate UnitCombat (#428, Tier D #29) — the unit-COMBAT-CLASS (UNITCOMBAT_MELEE/ARCHER/...). A unit-plane
stat SOURCE like Promotion: it deposits onto a unit via CvUnit::processUnitCombat's changeExtra*/change*Count
stack (the §5 self-accumulator). **REUSES the Promotion #28 unit-stat vocabulary VERBATIM** (imported tables) —
this is the entity that, with Promotion, DEFINES that vocabulary (modifier-spec §5). EXE-link 0 DllExport.

SAME as Promotion (imported): the `*Change` stat fields -> the same families (combat/withdrawal/firstStrike/
air/collateral/heal/movement/experience/workRate/cargo/upkeep/vision/capture/poison/espionage/trap/...), the
CAPABILITIES boolean group, the vision/LOS resolver, properties -> scoped deposits (property_source_v3), the
vs-keyed combat modifiers (under DIFFERENT XML container names: TerrainAttackChangeModifiers vs Promotion's
TerrainAttacks — same {Type,value} shape). OnGameOptions/NotOnGameOptions -> the entity-level enabled/disabled gate (owner 2026-07-08).

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
celebrity: iCelebrityHappy's numeric AMOUNT is DROPPED (owner 2026-07-01: "not a random field on a unit") ->
  a boolean base SKILL (skills.celebrity=true when non-zero); CvCity is fixed POST-MIGRATION to scan for
  celebrity-skilled units and award the happiness itself. (Getter/XML-reader exist in-engine; 0 UCs set it today.)
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
from curate_common import (FAMILY_ORDER, collapse_hide_and_seek, merge_vision, put_art, emit_art, descale100, fold_text_to_identity, gate_entity,
                           TAG_BY_UNITCOMBAT, add_tags,
                           emit_sizematters, SM_FLAT_CHANGE, SM_COMBATMOD_CHANGE, SM_CARGO_CHANGE, wipe_entity_json)
# REUSE the Promotion unit-stat vocabulary (the shared §5 definition) + helpers.
from curate_promotion import (COMBAT_MODS, FAMILIES, NEGATE_TAGS, CAP_BOOL, CAP_PAIR, CAP_COUNT, VISION_PAIRS,
                              VISION_STRUCTS, _txt, _int, _simple_list, _pairs)

# UnitCombat-specific extensions to the shared tables.
# bRBombardDirect/bRBombardForceAbility DROP with the DCM-range removal ruling (see BASE_PLAIN note).
CAP_BOOL_X = {"bCannotMergeSplit": "cannotMergeSplit", "bInvisible": "alwaysInvisible", "bHealsAs": "healsAs"}
CAP_COUNT_X = {"iNoCaptureChange": "noCapture"}
VISION_PAIRS_X = {"VisibilityIntensitySameTileChangeTypes": "visibilityIntensitySameTile"}
# vs-keyed combat modifiers — UnitCombat's container names (struct-vectors {Type, iModifier}); same homes as
# Promotion's VS_KEYED. (family, keyword, member|None, unit)
VS_KEYED = {
    "TerrainAttackChangeModifiers":   ("combat", "terrain", "attack", "percent"),
    "TerrainDefenseChangeModifiers":  ("combat", "terrain", "defense", "percent"),
    "FeatureAttackChangeModifiers":   ("combat", "feature", "attack", "percent"),
    "FeatureDefenseChangeModifiers":  ("combat", "feature", "defense", "percent"),
    "UnitCombatChangeModifiers":      ("combat", "unitCombat", None, "percent"),
    "DomainMods":                     ("combat", "domain", None, "percent"),
    "FlankingStrengthbyUnitCombatTypesChanges": ("combat", "flanking", None, "percent"),  # true XML container (CvUnitCombatInfo.cpp:1804 reader); child iModifier keyed by UnitCombat. Zero authorings today.
    "TerrainWorkChangeModifiers":     ("workRate", "terrain", None, "percent"),
    "FeatureWorkChangeModifiers":     ("workRate", "feature", None, "percent"),
    "BuildWorkChangeModifiers":       ("workRate", "build", None, "percent"),
    "TrapAvoidanceUnitCombatTypes":   ("trap", "avoidance", None, "flat"),
}
CAP_LIST = {"TrapImmunityUnitCombatTypes": "trapImmunity"}
# doubleMove is HALF MOVEMENT COST on that ground (owner) -- a keyed MOVEMENT modifier, never a skill; see
# curate_promotion for the ruling. Same magnitude on both carriers.
DOUBLE_MOVE_KEYED = {
    "TerrainDoubleMoveChangeTypes": "terrain",
    "FeatureDoubleMoveChangeTypes": "feature",
}
DOUBLE_MOVE_PERCENT = -50
# *Base -> identity.base (§0.6 create-unit base data). quality/group/size use a -10 "unset" sentinel.
# *Base ranks -> the sizeMatters block (json.md §9), NOT identity.base. -10 = "unset" sentinel (0 is a real rank).
BASE_SENTINEL10 = {"iQualityBase": "qualityBase", "iGroupBase": "groupBase", "iSizeBase": "sizeBase"}
# DCM RANGE BOMBARD: the whole mod is ruled FULLY REMOVED (structural-cleanup.md Tier 2, owner) -- ranged
# attack is GONE until/unless rebuilt siege-only. The five *Base fields DROP (absent reads 0 -> canRBombard
# gates off); the C++ system removal is the recorded follow-up.
BASE_PLAIN = {}
ID_REF = {"ReligionType": "religion", "CultureType": "culture", "EraType": "era"}
ID_BOOL = {"bForMilitary": "forMilitary", "bForNavalMilitary": "forNavalMilitary"}
ID_LIST = {"GGptsforUnitTypes": "ggPointsForUnits", "DefaultStatusTypes": "defaultStatuses"}
# DROP: dead/handled-elsewhere. FeatureAttacks/FeatureDefenses (2 recs) + iWithdrawalProb (1) are WRONG-TAG
# entries the engine ignores (it reads FeatureAttackChangeModifiers / iWithdrawalChange) -> dead in-game (the
# Promotion iStealthCombatModifier-typo pattern). Button handled via put_art.
# bSpy: spy is a TAG ONLY (owner 2026-06-23) -- not a skill; the `spy` tag derives at the UNIT level (curate_unit,
# from UNITAI_SPY) and the legacy bSpy CAP is dropped there too. So DROP it here as well (curate_unit already does).
DROP = {"Type", "Description", "Help", "Categories", "FeatureAttacks", "FeatureDefenses", "iWithdrawalProb", "bSpy",
        # the DCM-range removal ruling (see BASE_PLAIN note):
        "iRBombardDamageBase", "iRBombardDamageLimitBase", "iRBombardDamageMaxUnitsBase",
        "iDCMBombRangeBase", "iDCMBombAccuracyBase", "bRBombardDirect", "bRBombardForceAbility"}


def curate(typ, rec, store):
    out = OrderedDict([("type", typ)])
    for tag, key in (("Description", "description"), ("Help", "help")):
        t = _txt(rec, tag)
        if t:
            out[key] = t

    fams, caps, vision, identity = OrderedDict(), OrderedDict(), OrderedDict(), OrderedDict()

    def fam_unit(family):
        return fams.setdefault(family, OrderedDict()).setdefault("unit", OrderedDict())

    # --- shared scalar combat-modifier members + other families (imported tables; UC-absent tags skip).
    # Strength MODIFIERS -> the combat family (ruling 5); `strength` holds only a unit's base value. ---
    for tag, (member, unit) in COMBAT_MODS.items():
        v = _int(rec, tag)
        if v:
            node = fam_unit("combat")
            if member:
                node = node.setdefault(member, OrderedDict())
            node[unit] = v
    for tag, (family, member, unit) in FAMILIES.items():
        v = _int(rec, tag)
        if tag.endswith("100"):                # one-time x100 -> human de-scale (cascade-fixed-point.md §2; iExtraUpkeep100)
            v = descale100(v) if v is not None else v
        if v and tag in NEGATE_TAGS:           # sign-normalized (upgrade discount -> negative costs.upgrade)
            v = -v
        if v:
            node = fam_unit(family)
            if member:
                node = node.setdefault(member, OrderedDict())
            node[unit] = v

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
        v = _int(rec, tag)
        if v:
            caps[name] = v > 0   # sign-aware: negative = REVOKE (see curate_promotion -- the WANTED find)
    for tag, name in CAP_LIST.items():
        node = rec.find(tag)
        if node is not None:
            for k in _simple_list(node):
                caps.setdefault(name, OrderedDict())[k] = True
    for tag, kw in DOUBLE_MOVE_KEYED.items():
        node = rec.find(tag)
        if node is not None:
            for k in _simple_list(node):
                fam_unit("movement").setdefault(kw, OrderedDict()).setdefault(k, OrderedDict())["percent"] = DOUBLE_MOVE_PERCENT
    # celebrity: a unit-combat defines a unit's BASE abilities, so celebrity is a base SKILL here too. The numeric
    # iCelebrityHappy amount is DROPPED (owner 2026-07-01: "not a random field on a unit") -> boolean skill when
    # non-zero. CvCity is fixed POST-MIGRATION to scan for celebrity-skilled units and award the happiness itself.
    # (The CvUnitCombatInfo.getCelebrityHappy() getter + XML reader exist in-engine; no UC in the current data
    #  actually sets it, but the field is consumed here consciously so it can never surface as UNHANDLED.)
    if _int(rec, "iCelebrityHappy"):
        caps["celebrity"] = True

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

    # --- outcomes (KillOutcomes + Actions) -> the CLEAN verb-per-payload schema, shared with curate_unit (#430).
    # A unitcombat is the second CvOutcome carrier; the runtime merges its lists with the unit's, so it emits the
    # identical shape the unit poco parses (curate_unit.emit_outcomes -- adapt-unwrap, cascade conditions, verbs).
    from curate_unit import emit_outcomes
    outcomes = emit_outcomes(typ, rec) or OrderedDict()

    # --- entity-level enabled/disabled gate (game options; owner 2026-07-08) ---
    gate_on, gate_off = [], []   # entity-level enabled/disabled (owner 2026-07-08)
    for tag, dst in (("OnGameOptions", gate_on), ("NotOnGameOptions", gate_off)):
        node = rec.find(tag)
        if node is not None:
            dst.extend(_simple_list(node) or [])

    # --- identity: *Base create-unit data (§0.6) + refs + AI tags + parked lists ---
    sm_base = OrderedDict()   # the *Base ranks -> sizeMatters (below), keeping the -10 "unset" sentinel out
    for tag, key in BASE_SENTINEL10.items():
        v = _int(rec, tag)
        if v is not None and v != -10:
            sm_base[key] = v
    base = OrderedDict()      # BASE_PLAIN (rangedBombard/dcm create-unit stats) stays in identity.base
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
    # --- tags: what this class says its CARRIER is (the identity half of the distillation, [tags.md]) ---
    # A UnitCombat holds the good/bad-AGAINST stats; the tag says what the unit IS. Authored HERE, once: the
    # engine unions a unit's combat classes' tags into the unit at load, so no unit carries a baked copy.
    # Only the OBVIOUS identities map -- the taxonomy families (weapon/size/species/quality/group) carry none.
    add_tags(out, TAG_BY_UNITCOMBAT.get(typ, ()))
    # A unitcombat is TYPE-DERIVED, so it may carry the hiding-METHOD tag too (a promotion may not -- tags are
    # not promotion-grantable, [tags.md]).
    collapse_hide_and_seek(out, vision)
    merge_vision(out, vision)
    if outcomes:
        out["outcomes"] = outcomes
    gate_entity(out, gate_on, gate_off)
    emit_sizematters(out, lambda t: _int(rec, t), flat=SM_FLAT_CHANGE, combatmod=SM_COMBATMOD_CHANGE,
                     cargo=SM_CARGO_CHANGE, base_ranks=sm_base)
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out


# The genuinely-dead unit-combats: verified 0-reference everywhere (Assets/Data JSON beyond each class's own file,
# all module/legacy XML unit/promotion assignments, Python, Sources) AND no runtime attr (era/religion/culture) --
# re-verified 2026-07-21 (owner-approved conservative purge, merge-candidates.md §3). Dropped outright: every
# CvUnitCombatInfo is loaded resident whether referenced or not, so under the 32-bit ~3.2GB ceiling this is a direct
# memory win. 9 empty taxonomy stubs + 6 orphaned payload classes whose content a referenced sibling already carries.
DROP_DEAD_UNITCOMBATS = {
    "UNITCOMBAT_DISASSEMBLY", "UNITCOMBAT_HOLOGRAPHIC_DIVERSIONS", "UNITCOMBAT_IMPROVED_HOLOGRAPHIC_DIVERSIONS",
    "UNITCOMBAT_MAMMAL_BAT", "UNITCOMBAT_MOUNT_MULE", "UNITCOMBAT_REPTILE_DINOSAUR", "UNITCOMBAT_SEA_LARGE",
    "UNITCOMBAT_SEA_SMALL", "UNITCOMBAT_WHALE", "UNITCOMBAT_AMPHIBIAN_SALAMANDER", "UNITCOMBAT_ANTIGRAV_CRAFT",
    "UNITCOMBAT_CARRIER", "UNITCOMBAT_CORVETTE", "UNITCOMBAT_CUTTER", "UNITCOMBAT_SWARMSHIP",
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("UnitCombatInfo")
    # Culture unit-combats are redundant double-data and are DROPPED outright (owner 2026-07-19): the
    # culture<->unit identity is owned by the culture BONUS (BONUS_X.enables.units + identity.bonusClassType
    # BONUSCLASS_CULTURE) and gated on the units (requires.build: BONUS_X); the UNITCOMBAT_CULTURE_* shell
    # only re-points at it via identity.culture, whose engine getter (CvUnitCombatInfo::getCulture) has ZERO
    # consumers, and it attaches to no unit. A culture record carrying REAL combat content would be a data
    # bug -- guarded below, never silently dropped.
    results = OrderedDict()
    dropped_culture = []
    dropped_dead = []
    for typ, rec in table.items():
        if typ in DROP_DEAD_UNITCOMBATS:   # the owner-approved conservative purge (merge-candidates.md §3)
            dropped_dead.append(typ)
            continue
        out = curate(typ, rec, store)
        if out.get("identity", {}).get("culture"):
            extra = [k for k in out if k not in ("type", "ui", "identity")]
            id_extra = [k for k in out["identity"] if k not in ("description", "help", "culture")]
            assert not extra and not id_extra, (
                "culture UC %s is not a pure shell (extra=%s identity_extra=%s) -- review before dropping"
                % (typ, extra, id_extra))
            dropped_culture.append(typ)
            continue
        results[typ] = out
    n = len(results)
    if dropped_culture:
        print("DROPPED %d culture unit-combats (redundant double-data; owner 2026-07-19)" % len(dropped_culture))
    if dropped_dead:
        print("DROPPED %d genuinely-dead unit-combats (0-ref conservative purge; owner 2026-07-21): %s"
              % (len(dropped_dead), ", ".join(dropped_dead)))
    # COVERAGE CHECK
    handled = (set(COMBAT_MODS) | set(FAMILIES) | set(VS_KEYED) | set(CAP_BOOL) | set(CAP_BOOL_X)
               | set(CAP_PAIR) | set(CAP_COUNT) | set(CAP_COUNT_X) | set(CAP_LIST) | set(VISION_PAIRS)
               | set(VISION_PAIRS_X) | set(VISION_STRUCTS) | set(BASE_SENTINEL10) | set(BASE_PLAIN)
               | set(ID_REF) | set(ID_BOOL) | set(ID_LIST) | DROP
               | set(SM_FLAT_CHANGE) | set(SM_COMBATMOD_CHANGE) | set(SM_CARGO_CHANGE)   # consumed by emit_sizematters (json.md §9) -- were mis-reported UNHANDLED
               | {"PropertyManipulators", "KillOutcomes", "Actions", "OnGameOptions", "NotOnGameOptions",
                  "Button",
                  "iCelebrityHappy"})   # -> skills.celebrity (boolean), amount dropped (owner 2026-07-01)
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
              "enabled", "disabled", "identity"}
    seen = sorted({f for o in results.values() for f in o if f not in STRUCT})
    print("UnitCombatInfo curated: %d" % n)
    for k in ("obsoletes", "skills", "vision", "outcomes", "identity"):
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
        # drop-before-rewrite THROUGH THE SHARED WIPE -- also what registers the folder for the additions-overlay
        # re-apply at exit (curate_common). A bespoke in-place write skips both (the curate_unit precedent).
        wipe_entity_json(base, expected=n)
        for typ, obj in results.items():
            with open(os.path.join(base, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        print("\nwrote %d UnitCombatInfo JSON files under Assets/Data/unitcombats" % n)


if __name__ == "__main__":
    main()
