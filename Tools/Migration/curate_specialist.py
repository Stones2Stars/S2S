#!/usr/bin/env python3
"""Curate Specialist (#428) — a sub-city LEAF entity that, as a modifier SOURCE, deposits every effect at CITY
scope (CvCity::processSpecialist, CvCity.cpp:5130-5198, applies each field per specialist-count). BESPOKE.
Verified vs that consumer + the classify-light-batch workflow.

Own scope-wide families (all CITY / flat):
- Yields / Commerces -> the SPLIT base-yield & base-commerce families.
- iGreatPeopleRateChange -> greatPeopleRate (singleton).
- iHealthPercent / iHappinessPercent -> health / happiness. CORRECTION: FLAT despite the "Percent" name —
  CvCity.cpp:5169-5184 routes them to changeSpecialistGoodHealth/BadHealth & Happiness/Unhappiness as raw flat
  adds split by sign; the /100 in CvCityAI is AI weighting only.
- iInvestigation / iInsidiousness / iExperience -> their singleton families (the last two are module-rare).

Own target-keyed modifier:
- UnitCombatExperienceTypes -> experience.city.unitCombats.{UNITCOMBAT}.flat. The unitcombat is the TARGET that
  gets free XP (not a conditioner), so per the model this stays on the SOURCE keyed by target (like a building's
  unitProduction.percent.{UNIT}). The GAMEOPTION_UNIT_XP_FROM_SPECIALISTS Null-twin gate is a runtime rule; only
  the real {unitCombat: modifier} pairs are authored.

DROPPED (inverted onto their CONDITIONER tech — curate_tech already folds them via TECH_BOOSTS, authoring
tech.happiness/health.city.specialists.{SPECIALIST}.flat; authoring here too would double-count):
- TechHappinessTypes, TechHealthTypes.

Inbound boosts (fold ONTO the specialist; the source entity drops them when curated, per the inversion
convention — no double-authoring):
- Building Specialist{Yield,Commerce}Change + Local variants (city, flat), Civic Specialist{Yield,Commerce}
  PercentChanges (city, percent), Trait Specialist{Yield,Commerce}Change (empire, flat).
- FreeSpecialistCount (Civic/Tech/Event) is NOT folded here: it grants N free specialists of this type — a
  capability/grant that belongs on the SOURCE, not a per-turn modifier on the specialist (flagged).

PropertyManipulators -> per-PROPERTY_* family at CITY scope (gated list, like Heritage). GreatPeopleUnitType,
Categories, bSlave, bVisible -> identity. Texture/Button -> art. Flavors -> ai. Runtime m_iMissionType not
XML-backed. Specialists enable nothing (a leaf), so no `enables`.

  python3 curate_specialist.py --sample SPECIALIST_MERCHANT SPECIALIST_SLAVES
  python3 curate_specialist.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import curate_common as cc
from curate_common import de_i
from store import Store, REPO

# tag -> (family, scope, member, unit, valueKeys). valueKeys set => SPLIT base family (member = the identifier).
FAMILIES = {
    "Yields":                ("yield",           "city", None, "flat", engine.YIELDS),
    "Commerces":             ("commerce",        "city", None, "flat", engine.COMMERCES),
    "iGreatPeopleRateChange":("greatPeopleRate", "city", None, "flat", None),
    "iHealthPercent":        ("health",          "city", None, "flat", None),   # FLAT (corrected)
    "iHappinessPercent":     ("happiness",       "city", None, "flat", None),   # FLAT (corrected)
    "iInvestigation":        ("investigation",   "city", None, "flat", None),
    "iInsidiousness":        ("insidiousness",   "city", None, "flat", None),
    "iExperience":           ("experience",      "city", None, "flat", None),
}
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Help": "help"}
# Texture + Button -> ui.art.texture / ui.art.icon via ART_BLOCK (kept DISTINCT: 2 specialists differ; both UI).
ART = {"Texture", "Button"}
IDENTITY = {"GreatPeopleUnitType": "greatPeopleUnit", "Categories": "categories"}
BOOL_ID = {"bSlave": "slave", "bVisible": "visible"}
SOURCE_UNIT = {"CONSTANT": "perTurn", "DECAY": "decay"}
# TechHappiness/HealthTypes -> KEEP-ON-SELF (owner 2026-06-20, Phase-F): the specialist OWNS the output it
# produces, so its tech-conditioned happiness/health bump rides on the specialist, city-scope, `enabled` by the
# team-tech (the tech is the GIVER/enabler -- never inverted onto the tech). Read raw post-loop (still in DROP so the
# default loop skips them; the explicit block below emits keep-on-self). YieldChanges is DEAD structure:
# read() only reads <Yields> (addYields -> m_piYieldChange); <YieldChanges> populates no member and is unread.
DROP = {"TechHappinessTypes", "TechHealthTypes", "YieldChanges"}
FAMILY_ORDER = ["food", "production", "commerce", "gold", "research", "culture", "espionage",
                "greatPeopleRate", "health", "happiness", "experience", "investigation", "insidiousness"]

# inbound entity-targeted modifiers: (sourceEntity, field, targetType, family, valueKeys, unit, scope)
# NB the container tags are PLURAL (rec.find returns the container, _boost_entries iterates its entries);
# C2C names them inconsistently but Building/Trait both wrap entries in <…Changes>.
SPECIALIST_BOOSTS = [
    ("BuildingInfo", "SpecialistYieldChanges",          "buildings", "yield",    engine.YIELDS,    "flat",    "city"),
    ("BuildingInfo", "SpecialistCommerceChanges",       "buildings", "commerce", engine.COMMERCES, "flat",    "city"),
    ("BuildingInfo", "LocalSpecialistYieldChanges",     "buildings", "yield",    engine.YIELDS,    "flat",    "city"),
    ("BuildingInfo", "LocalSpecialistCommerceChanges",  "buildings", "commerce", engine.COMMERCES, "flat",    "city"),
    ("CivicInfo",    "SpecialistYieldPercentChanges",   "civics",    "yield",    engine.YIELDS,    "percent", "city"),
    ("CivicInfo",    "SpecialistCommercePercentChanges","civics",    "commerce", engine.COMMERCES, "percent", "city"),
    ("TraitInfo",    "SpecialistYieldChanges",          "traits",    "yield",    engine.YIELDS,    "flat",    "empire"),
    ("TraitInfo",    "SpecialistCommerceChanges",       "traits",    "commerce", engine.COMMERCES, "flat",    "empire"),
]


def _put(fam, family, scope, member, unit, val):
    node = fam.setdefault(family, {}).setdefault(scope, {})
    if member:
        node = node.setdefault(member, {})
    node[unit] = val


def _inject_cond(fam, family, scope, unit, value, enabled):
    """Keep-on-self conditioned deposit: family.scope.unit gets [{value, enabled}, ...] (modifier.md §6.5)."""
    node = fam.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    entry = OrderedDict([("value", value), ("enabled", enabled)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


def _apply_family(fam, spec, c):
    family, scope, member, unit, keys = spec
    if keys:                                   # SPLIT base family: the identifier IS the family name
        for ident, v in engine.named_array(c, keys).items():
            _put(fam, ident, scope, None, unit, v)
    else:
        t = engine.text(c)
        if engine.is_int(t) and int(t) != 0:
            _put(fam, family, scope, member, unit, int(t))


def _unit_combat_xp(node, fam):
    """UnitCombatExperienceTypes -> experience.city.unitCombats.{UNITCOMBAT}.flat (target-keyed)."""
    for entry in list(node):
        uc = engine.text(entry.find("UnitCombatType"))
        mod = engine.text(entry.find("iModifier"))
        if uc and engine.is_int(mod) and int(mod) != 0:
            (fam.setdefault("experience", {}).setdefault("city", {}).setdefault("unitCombats", {})
             .setdefault(uc, {}))["flat"] = int(mod)


def _properties(node, props):
    """PropertyManipulators -> PROPERTY_X.<scope>.<unit> via the shared v3 converter (property = a yield-like family;
    CONSTANT->flat, DECAY->percent, attribute-scaled->per [reduced], Active BoolExpr->enabled). Multiple sources to one
    (prop,scope,unit) accumulate as a LIST of entries; a lone source stays a scalar."""
    for src in node:
        if src.tag != "PropertySource":
            continue
        conv = engine.property_source_v3(src)
        if conv is None:
            continue
        prop, scope, unit, value = conv
        leaf = props.setdefault(prop, OrderedDict()).setdefault(scope, OrderedDict())
        if unit in leaf:
            if not isinstance(leaf[unit], list):
                leaf[unit] = [leaf[unit]]
            leaf[unit].append(value)
        else:
            leaf[unit] = value


def curate(typ, rec, boosts):
    text, fam, props, art_blocks, identity, ai, leftover = {}, {}, {}, {}, {}, {}, []
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in FAMILIES:
            _apply_family(fam, FAMILIES[tag], c)
        elif tag == "UnitCombatExperienceTypes":
            _unit_combat_xp(c, fam)
        elif tag == "PropertyManipulators":
            _properties(c, props)
        elif tag == "Flavors":
            v = engine.generic(c)
            if v:
                ai["flavours"] = v
        elif tag in ART:
            cc.put_art(art_blocks, tag, engine.generic(c))   # Texture->ui.art.texture, Button->ui.art.icon (ART_BLOCK)
        elif tag in IDENTITY:
            if t or list(c):
                identity[IDENTITY[tag]] = engine.generic(c)
        elif tag in BOOL_ID:
            if t in ("1", "true", "True"):
                identity[BOOL_ID[tag]] = True
        else:
            if list(c) or t:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(c)

    # Tech-conditioned OWN happiness/health -> KEEP-ON-SELF (the specialist owns the output it produces; the tech
    # is the enabling GIVER). city-scope, `enabled` by the team-tech. x1, no de-scale. (owner 2026-06-20, §6.5)
    for tag, family in (("TechHappinessTypes", "happiness"), ("TechHealthTypes", "health")):
        node = rec.find(tag)
        if node is not None:
            for tech, _u, val in cc._boost_entries(node, None, "flat"):
                _inject_cond(fam, family, "city", "flat", val, OrderedDict([("type", tech), ("scope", "team")]))

    # Conditioned keep-on-source deposits (building/civic/trait boosts -> THIS specialist's OWN output, `enabled` by
    # the source's presence; modifier.md §6.5). INJECTED (not merged) so each lands as a {value, enabled} entry that
    # coexists with the specialist's own base value (int|list-safe), instead of the old keyed-by-conditioner sub-scope.
    for fam_name, scope, unit, value, enabled in boosts:
        _inject_cond(fam, fam_name, scope, unit, value, enabled)

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "civilopedia", "help"):
        if k in text:
            out[k] = text[k]
    for family in FAMILY_ORDER:
        if family in fam:
            out[family] = fam[family]
    for family in fam:
        if family not in out:
            out[family] = fam[family]
    for prop in sorted(props):
        out[prop] = props[prop]
    if ai:
        out["ai"] = ai
    cc.emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    return out, leftover


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    boosts = cc.accumulate_conditioned(store, SPECIALIST_BOOSTS)
    table = store.table("SpecialistInfo")
    results, all_leftover = OrderedDict(), set()
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec, boosts.get(typ, []))
        results[typ] = obj
        all_leftover.update(leftover)
    print("SpecialistInfo curated: %d" % len(results))
    seen = sorted({k for o in results.values() for k in o
                   if k not in ("type", "description", "civilopedia", "help", "ai", "ui", "world", "sound", "identity")})
    print("  families/props seen: %s" % ", ".join(seen))
    if all_leftover:
        print("  leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "specialists")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d SpecialistInfo JSON files under Assets/Data/specialists" % len(results))


if __name__ == "__main__":
    main()
