#!/usr/bin/env python3
"""Curate Heritage (#428) — a per-PLAYER acquired "heritage" (folklore/myth/taxon tiers) stored on
CvPlayer::m_myHeritage, contributing EMPIRE-wide effects. BESPOKE. Verified vs CvPlayer::processHeritage
(CvPlayer.cpp:30973-30992) + the acquisition gate CvPlayer::canAddHeritage (~30914-30930).

Two real modifier sources, both EMPIRE scope (consumed at the player level):
- EraCommerceChanges -> the SPLIT commerce families (gold/research/culture/espionage), each with a `byEra`
  member keyed by the era band. Era is a THRESHOLD CONDITION, not a scope: processHeritage applies a band's
  commerce for every era where `currentEra >= band` (CvPlayer.cpp:30984), so the bands accumulate — authored
  verbatim, the engine applies the threshold. CentiCommerce is x100 legacy fixed-point -> DE-SCALED to human here
  (the curator's one-time job, cascade-fixed-point.md §2; readJson re-applies the uniform human->x100).
  Era keys are the era Type (C2C_ERA_*) VERBATIM — era is a data-driven Type, so it is referenced like every
  other Type (BONUS_*/TECH_*), matching how techs carry `era: C2C_ERA_*`; resolve to the era file via type.lower().
- PropertyManipulators -> per-PROPERTY_* family, empire scope, as a LIST of gated source deposits. Unlike
  Handicap (unconditional, scalar-collapsed) a Heritage source carries an <Active><Has>{GOM_TECH:…}</Has></Active>
  CONDITIONAL gate, and several gated sources target the SAME property — so the deposit must be a list that
  preserves the gate. (The scalar-vs-list property-family shape across Handicap/Heritage is a flagged
  reconciliation for the property pass; here faithfulness wins.)

Acquisition prereqs (NOT modifiers):
- PrereqTech -> DROP: the store inverts it to tech.enables.heritages.
- PrereqOrHeritage -> DROP: the store now derives the heritage->heritage succession edge
  (folklore enables taxon) into enables.heritages. The heritage's own `enables` block is emitted from that.
- bNeedLanguage -> identity.needsLanguage (PRESERVED, flagged): a player-state acquisition gate with no source
  entity to invert onto, so it cannot be a top-down enabler edge; kept until the prereq-model reconciliation.
- MissionType (m_iMissionType) is runtime-assigned, not XML-backed -> never appears, nothing to drop.

  python3 curate_heritage.py --sample HERITAGE_FOLKLORE_ANIMAL
  python3 curate_heritage.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import curate_common as cc
from curate_common import de_i
from store import Store, REPO

TEXT = {"Description": "description", "Civilopedia": "civilopedia"}
SOURCE_UNIT = {"CONSTANT": "perTurn", "DECAY": "decay"}
DROP = {"PrereqTech", "PrereqOrHeritage"}
FAMILY_ORDER = ["gold", "research", "culture", "espionage"]


_ERA_ORDINAL = None


def _era_ordinal(era):
    """era Type (C2C_ERA_*) -> its 1-based counter index (1..X), the engine era sequence (Store/XML order)."""
    global _ERA_ORDINAL
    if _ERA_ORDINAL is None:
        _ERA_ORDINAL = {typ: i + 1 for i, typ in enumerate(Store().table("EraInfo"))}
    return _ERA_ORDINAL.get(era)


def _era_commerce(node, fam):
    """EraCommerceChanges -> <commerce>.empire.flat conditioned on the ERA COUNTER (owner ruling 2026-06-28: era is a
    plain counter 1..X, NOT a bespoke byEra key). processHeritage applies a band for every era >= it (CvPlayer:30984),
    so each band becomes a flat gated `enabled:{type:ERA, min:<era ordinal>}`; they ACCUMULATE through normal deposit
    summation (every entry whose min <= the current era applies). CentiCommerce x100 -> human de-scaled, zeros dropped."""
    for entry in list(node):
        era = engine.text(entry.find("EraType"))
        centi = entry.find("CentiCommerce")
        if not era or centi is None:
            continue
        ordinal = _era_ordinal(era)
        if ordinal is None:
            continue
        for member, v in engine.named_array(centi, engine.COMMERCES).items():
            v = cc.descale100(v)                          # one-time x100 -> human (cascade-fixed-point.md §2)
            if not v:
                continue
            lst = fam.setdefault(member, {}).setdefault("empire", {}).setdefault("flat", [])
            lst.append(OrderedDict([("value", v),
                                    ("enabled", OrderedDict([("type", "ERA"), ("min", ordinal)]))]))


def _properties(node, props):
    """PropertyManipulators -> PROPERTY_X.<scope>.<unit> via the shared v3 converter (property = a yield-like family;
    CONSTANT->flat, DECAY->percent, attribute-scaled->per, Active BoolExpr->enabled). Multiple sources to the same
    (prop,scope,unit) accumulate as a LIST of entries (modifier-spec §1.3); a lone source stays a scalar."""
    for src in node:
        if src.tag != "PropertySource":
            continue
        conv = engine.property_source_v3(src)
        if conv is None:
            continue
        prop, scope, unit, value = conv
        leaf = props.setdefault(prop, OrderedDict()).setdefault(scope, OrderedDict())
        if unit in leaf:                                  # 2nd+ source to the same leaf -> make/extend a list
            if not isinstance(leaf[unit], list):
                leaf[unit] = [leaf[unit]]
            leaf[unit].append(value)
        else:
            leaf[unit] = value


def curate(typ, rec, store):
    text, fam, props, art_blocks, identity, leftover = {}, {}, {}, {}, {}, []
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag == "EraCommerceChanges":
            _era_commerce(c, fam)
        elif tag == "PropertyManipulators":
            _properties(c, props)
        elif tag == "Button":
            cc.put_art(art_blocks, tag, engine.generic(c))   # -> ui.art.icon via ART_BLOCK
        elif tag == "bNeedLanguage":
            if t in ("1", "true", "True"):
                identity["needsLanguage"] = True
        else:
            if list(c) or t:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(c)

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "civilopedia"):
        if k in text:
            out[k] = text[k]
    enables = store.enabled_by(typ)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    for family in FAMILY_ORDER:
        if family in fam:
            out[family] = fam[family]
    for prop in sorted(props):                 # PROPERTY_* families after the commerce families
        out[prop] = props[prop]
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
    table = store.table("HeritageInfo")
    results, all_leftover = OrderedDict(), set()
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec, store)
        results[typ] = obj
        all_leftover.update(leftover)
    print("HeritageInfo curated: %d" % len(results))
    print("  with enables: %d" % sum(1 for o in results.values() if "enables" in o))
    if all_leftover:
        print("  leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "heritages")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d HeritageInfo JSON files under Assets/Data/heritages" % len(results))


if __name__ == "__main__":
    main()
