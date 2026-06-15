#!/usr/bin/env python3
"""Curate Heritage (#428) — a per-PLAYER acquired "heritage" (folklore/myth/taxon tiers) stored on
CvPlayer::m_myHeritage, contributing EMPIRE-wide effects. BESPOKE. Verified vs CvPlayer::processHeritage
(CvPlayer.cpp:30973-30992) + the acquisition gate CvPlayer::canAddHeritage (~30914-30930).

Two real modifier sources, both EMPIRE scope (consumed at the player level):
- EraCommerceChanges -> the SPLIT commerce families (gold/research/culture/espionage), each with a `byEra`
  member keyed by the era band. Era is a THRESHOLD CONDITION, not a scope: processHeritage applies a band's
  commerce for every era where `currentEra >= band` (CvPlayer.cpp:30984), so the bands accumulate — authored
  verbatim, the engine applies the threshold. CentiCommerce is x100 fixed-point, carried FAITHFULLY (#432).
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
from curate_common import de_i
from store import Store, REPO

TEXT = {"Description": "description", "Civilopedia": "civilopedia"}
SOURCE_UNIT = {"CONSTANT": "perTurn", "DECAY": "decay"}
DROP = {"PrereqTech", "PrereqOrHeritage"}
FAMILY_ORDER = ["gold", "research", "culture", "espionage"]


def _era_key(era):
    # The era Type verbatim (C2C_ERA_*) — a data-driven Type, referenced like BONUS_*/TECH_* (owner convention),
    # consistent with how techs carry `era: C2C_ERA_*`. (Was a short stem matching neither Type nor filename.)
    return era


def _era_commerce(node, fam):
    """EraCommerceChanges -> <commerce>.empire.byEra.<era>.flat (x100 faithful, zeros dropped)."""
    for entry in list(node):
        era = engine.text(entry.find("EraType"))
        centi = entry.find("CentiCommerce")
        if not era or centi is None:
            continue
        for member, v in engine.named_array(centi, engine.COMMERCES).items():
            (fam.setdefault(member, {}).setdefault("empire", {}).setdefault("byEra", {})
             .setdefault(_era_key(era), {}))["flat"] = v


def _properties(node, props):
    """PropertyManipulators -> {PROPERTY_X: {empire: [ {unit: amount, active?: <gate>} ]}}, gate preserved."""
    for src in node:
        if src.tag != "PropertySource":
            continue
        cp = engine.clean_property_source(src)   # {source, property, amountPerTurn, Active?}
        prop, amount = cp.get("property"), cp.get("amountPerTurn")
        if not prop or amount in (None, "", {}):
            continue
        unit = SOURCE_UNIT.get(cp.get("source", ""), str(cp.get("source", "")).lower())
        dep = OrderedDict()
        dep[unit] = amount
        gate = cp.get("Active")
        if gate not in (None, "", [], {}):
            dep["active"] = gate
        props.setdefault(prop, {}).setdefault("empire", []).append(dep)


def curate(typ, rec, store):
    text, fam, props, art, identity, leftover = {}, {}, {}, {}, {}, []
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
            v = engine.generic(c)
            if v:
                art["icon"] = v
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
    if art:
        out["art"] = art
    if identity:
        out["identity"] = identity
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
