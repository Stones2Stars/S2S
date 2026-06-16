#!/usr/bin/env python3
"""Curate PromotionLine (#428, Tier D #27) — a GROUPING/HIERARCHY axis for promotions (the "promotion line"
chains), units-only. NOT a modifier source (the individual PROMOTION owns yield/commerce/property modifiers —
audit 2026-06-14) and enables nothing (store.enabled_by(PROMOTIONLINE_*) empty → no `enables`). It "rides
Promotion" (ranking #27): its applicability gates are unit-plane vocabulary the PROMOTION pass (#28) defines, so
those are PARKED faithfully here and re-homed there. EXE-link: 0 DllExport (unconstrained). 334 records
(262 base + 72 module). Bespoke curator; no cascade families.

The line's applicability gates are consumed by CvPromotionInfo (cpp:2080) + CvUnit (17664/25585) to decide which
units may take the line's promotions — i.e. unit-plane enabling, deferred to the Promotion pass (owner 2026-06-16:
"we need a broader picture on the enabling"). Parked in `identity` until then:
- UnitCombatPrereqTypes -> identity.unitCombats      (the unit-combats the line applies to)
- NotOnDomainTypes      -> identity.notOnDomains      (excluded domains)
- NotOnUnitCombatTypes  -> identity.notOnUnitCombats  (excluded unit-combats)

`buildUp` — a DEDICATED OBJECT MODULE (owner 2026-06-16: "treat buildup as a module, create a home for it"; the
owner dislikes the mechanic but it must be kept, so it gets an isolated, purgeable home — modifier-spec §0.8).
OBJECT-MODULE ACTIVATION CONVENTION (owner 2026-06-16): top-level keys are CATEGORY OBJECTS (not bare scalars), and
a module's object IS its on/off signal — active iff the object EXISTS and is NON-EMPTY; absent/empty ⇒ false.
- bBuildUp -> buildUp.active:true — the live isBuildUp() build-up-mechanic flag (28 lines). Build-up lines carry NO
  other line-level data: WHAT a line builds up (defense/chasedown/…) lives on its PROMOTIONS, so the buildUp object
  is enriched with the built-up aspect at the Promotion pass (#28); `active` is the interim marker until then
  (it then becomes redundant under the presence convention and can be dropped). Buildups have NO property (owner).

loadPrune.onGameOptions <- OnGameOptions (load-stable game-option gate; WB toggling enable/disable is the
mechanism, engine removes disabled promotions, owner — enabler-spec §6/§10; CultureLevel/Trait precedent).
NotOnGameOptions -> loadPrune.notOnGameOptions (0 populated).

DROPPED ENTITY (owner 2026-06-16): PROMOTIONLINE_AFFLICTION_DISEASE_COMMON_COLD — a STONE-DEAD vestige of the
purged Outbreaks-and-Afflictions mod (orphaned: 0 promotions/buildings/traits reference it; only its GameText
keys remain). It was the SOLE PropertyType carrier (bPoison was already empty), so dropping it removes PropertyType
+ bPoison from the model entirely — `buildUp` is then only ever `{active:true}`. (A targeted, owner-directed drop;
the other 4 true-orphan lines — BARBARIAN/MARAUDER/MEDIC/SNEAK — are dead too but carry no special fields, so they
ride the separate post-migration content-purge pass per the §0 boundary, NOT dropped here.)

DROP (fields): PrereqTech -> store tech.enables.promotionLines (1 line, single tech → no `requires`; owner #4: a
lone tech needs no requires, only a multi-tech AND would). ObsoleteTech (0; not store-registered, moot). Categories
(0, checksum-excluded, dead). UnitCombat/TechContractChanceChanges (0, dead unimplemented system — ranking #27).
Post-load Promotions/Buildings = derived reverse indexes (CvPromotionInfo.getPromotionLine /
CvBuildingInfo.getPromotionLineType), NOT XML — emit nothing.

  python3 curate_promotionline.py --sample PROMOTIONLINE_COMBAT PROMOTIONLINE_BUILD_UP_FIELD_HOSPITAL PROMOTIONLINE_AFFLICTION_DISEASE_COMMON_COLD
  python3 curate_promotionline.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import put_art, emit_art
from store import Store, REPO


# Stone-dead Outbreaks-and-Afflictions mod vestige — orphaned, sole PropertyType carrier (owner 2026-06-16, drop).
DROP_TYPES = {"PROMOTIONLINE_AFFLICTION_DISEASE_COMMON_COLD"}


def _bool(rec, tag):
    return engine.text(rec.find(tag)) in ("1", "true", "True")


def _txt(rec, tag):
    t = engine.text(rec.find(tag))
    return t if (t and t != "NONE") else None


def _list(rec, wrapper):
    """All non-NONE child Type-strings under a wrapper element (the child tag varies by list)."""
    node = rec.find(wrapper)
    if node is None:
        return []
    out = []
    for c in node:
        t = (engine.text(c) or "").strip()
        if t and t != "NONE":
            out.append(t)
    return out


def curate(typ, rec):
    out = OrderedDict([("type", typ)])
    for tag, key in (("Description", "description"), ("Help", "help")):
        t = _txt(rec, tag)
        if t:
            out[key] = t
    # loadPrune — load-stable game-option availability gate (WB toggle enable/disable).
    lp = OrderedDict()
    on = _list(rec, "OnGameOptions")
    if on:
        lp["onGameOptions"] = on
    noton = _list(rec, "NotOnGameOptions")
    if noton:
        lp["notOnGameOptions"] = noton
    if lp:
        out["loadPrune"] = lp
    # buildUp — dedicated object module (presence+non-empty = active; absent/empty = false). Only ever {active:true}
    # at this pass (the built-up aspect comes from the promotions, added at the Promotion pass). No property/poison
    # (the sole carrier, the affliction line, is dropped below).
    if _bool(rec, "bBuildUp"):
        out["buildUp"] = OrderedDict([("active", True)])
    # ui.art.icon
    art_blocks = OrderedDict()
    put_art(art_blocks, "Button", engine.text(rec.find("Button")))
    emit_art(out, art_blocks)
    # identity — unit-plane applicability gates, PARKED (deferred to the Promotion pass).
    ident = OrderedDict()
    for wrapper, key in (("UnitCombatPrereqTypes", "unitCombats"),
                         ("NotOnDomainTypes", "notOnDomains"),
                         ("NotOnUnitCombatTypes", "notOnUnitCombats")):
        vals = _list(rec, wrapper)
        if vals:
            ident[key] = vals
    if ident:
        out["identity"] = ident
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("PromotionLineInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items() if typ not in DROP_TYPES)
    n = len(results)
    has = lambda k: sum(1 for o in results.values() if k in o)
    print("PromotionLineInfo curated: %d" % n)
    for k in ("loadPrune", "buildUp", "ui", "identity"):
        print("  with %-9s: %d" % (k, has(k)))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        base = os.path.join(REPO, "Assets", "Data", "promotionlines")
        for typ, obj in results.items():
            d = os.path.join(base, "buildups") if "buildUp" in obj else base   # build-up lines in their own folder (owner 2026-06-16)
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d PromotionLineInfo JSON files under Assets/Data/promotionlines (build-ups in buildups/)" % n)


if __name__ == "__main__":
    main()
