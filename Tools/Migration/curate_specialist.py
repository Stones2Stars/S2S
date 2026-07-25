#!/usr/bin/env python3
"""Curate Specialist (#428) — rebuilt from scratch against the spec (2026-06-27).

A specialist is a sub-city LEAF entity: as a modifier SOURCE it deposits its effects at CITY scope, applied
per assigned specialist of that type (`CvCity::processSpecialist`, CvCity.cpp:5142-5211). It enables nothing,
so there is no `enables` block. Every classification below is grounded in the consumer model
(legacy-value-calc-map §1.5 for yields; CvCity.cpp for the rest), not in the field name.

SCALE — the ONE non-trivial point (verified at the CONSUMER, not the store):
  - intrinsic `iHealthPercent` / `iHappinessPercent` are stored RAW by processSpecialist (5184-5196) but the
    REALIZED `goodHealth()`/`badHealth()`/`happyLevel()`/`unhappyLevel()` read them `/100`
    (CvCity.cpp:5714/5848/5876/5654 — the legacy latent /100, scale-registry §4c). So they de-scale ÷100 →
    human (celebrity 200→2, doctor 150→1.5, slave −40→−0.4). The "Percent" in the tag name is a misnomer:
    after ÷100 the unit is FLAT (a flat happiness/health add split by sign at read), never a percent.
  - EVERYTHING ELSE is ×1 human, emitted as-is: getYieldChange / getCommerceChange (engine multiplies by 100
    on deposit — scale-registry §4a), getGreatPeopleRateChange, getExperience, getInsidiousness,
    getInvestigation, UnitCombat iModifier, and the Tech happiness/health keep-on-self values (read RAW via
    getExtraTech{Happiness,Health}Total — NO /100, CvCity.cpp:5719/5850).

NEW KEY -> LEGACY FIELD (the old->new map IS this curator, curators/README):
  yield.{food|production|commerce}.city.flat = `<Yields>`     (split base family; positional food/prod/commerce)
  {gold|research|culture|espionage}.city.flat = `<Commerces>` (split base family; the commerce-type IS the family)
  greatPeopleRate.city.flat                  = `iGreatPeopleRateChange`
  health.city.flat                           = `iHealthPercent`     (÷100)
  happiness.city.flat                        = `iHappinessPercent`  (÷100)
  underworld.city.investigation.flat         = `iInvestigation`    (merged into the shared underworld family, ruling 3)
  underworld.city.insidiousness.flat         = `iInsidiousness`    (module-rare; absent in current data)
  experience.city.flat                       = `iExperience`        (free unit XP; module-rare; absent today)
  experience.city.unitCombats.{UC}.flat      = `UnitCombatExperienceTypes` (the UNITCOMBAT is the TARGET that
                                               gets free XP, kept on the source keyed by target — modifier §5)
  PROPERTY_*.<scope>.<unit>                  = `PropertyManipulators` (shared v3 converter; CONSTANT→flat,
                                               DECAY→percent, attribute-scaled→per, Active→enabled)

KEEP-ON-SELF, conditioned (modifier §6.5 — the specialist OWNS the output; the conditioner is the GIVER, never
inverted onto it). Each lands as a {value, enabled} entry coexisting with the base (int|list-safe):
  happiness/health.city.flat += `TechHappinessTypes`/`TechHealthTypes`  enabled {tech, scope:team}   (×1)
  yield/commerce inbound boosts (SPECIALIST_BOOSTS below) folded from buildings/civics.

INBOUND BOOSTS folded onto this specialist (the source entity drops them when curated — no double-authoring):
  - Building `SpecialistYieldChange`/`SpecialistCommerceChange` NON-LOCAL → EMPIRE flat, gate = player HAS the
    building anywhere (`{BUILDING, scope:empire}`); `Local…` variants → CITY flat, gate = this city has it.
    (owner ruling 2026-06-26, engine-verified: non-local → CvPlayer::changeExtraSpecialistYield empire-wide;
    local → CvCity::changeLocalSpecialistExtraYield this-city. calc-map §1.5 "perType (building)"/"local".)
  - Civic `Specialist{Yield,Commerce}PercentChanges` → CITY percent (read Σpercent/100 — calc-map §1.5 "pct").

NOT folded here (deliberate):
  - ⛔ Trait `Specialist{Yield,Commerce}Changes` STAY ON THE TRAIT keyed by specialist
    (`{y}.empire.specialists.{SPEC}.flat`), because the simple/complex sets carry DIFFERENT per-set values for
    the same specialist and the specialist is ONE shared file (Option-B ruling 2026-06-25, modifier.md trait
    callout; calc-map §1.5 "perType (trait)"). curate_trait owns them.
  - Civic/Trait `SpecialistExtraYields` (per ANY specialist) → on the civic/trait
    (`{y}.empire.specialist.perSpecialist`, × the city's TOTAL specialists, calc-map §1.5 "all"). Not a
    per-type fact about this specialist.
  - `TechHappiness/HealthTypes` inverted onto the tech: NO — the specialist owns the output (keep-on-self above).
  - `FreeSpecialistCount` (Civic/Tech/Event): a grant of N free specialists — a capability on the SOURCE, not a
    per-turn modifier on the specialist.

DEAD: `<YieldChanges>` — read() reads only `<Yields>` (addYields → m_piYieldChange); `<YieldChanges>` populates
no member and is unread (1 stray occurrence). Dropped.

  python3 curate_specialist.py --sample SPECIALIST_CELEBRITY SPECIALIST_SLAVES
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

# tag -> (family, valueKeys, descale).  valueKeys set => SPLIT base family (the identifier IS the family name).
# descale=True => ÷100 (the latent-/100 intrinsic health/happiness); everything else is ×1 human.
# All of these deposit scope-wide at CITY, unit `flat`.
FAMILIES = {
    "Yields":                 ("yield",           engine.YIELDS,    False),
    "Commerces":              ("commerce",        engine.COMMERCES, False),
    "iGreatPeopleRateChange": ("greatPeopleRate", None,             False),
    "iHealthPercent":         ("health",          None,             True),   # ÷100 (latent /100)
    "iHappinessPercent":      ("happiness",       None,             True),   # ÷100 (latent /100)
    "iExperience":            ("experience",      None,             False),
}
# iInvestigation/iInsidiousness -> members of the shared `underworld` family (ruling 3, info-rebuild.md: the
# stray singleton investigation/insidiousness families MERGE into underworld -- the in-city criminal game,
# kinds insidiousness + investigation, city scope; uniform with curate_building).
UNDERWORLD = {"iInvestigation": "investigation", "iInsidiousness": "insidiousness"}
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Help": "help"}
ART = {"Texture", "Button"}                                   # ui.art.texture / ui.art.icon (kept DISTINCT)
IDENTITY = {"GreatPeopleUnitType": "greatPeopleUnit", "Categories": "categories"}
BOOL_ID = {"bSlave": "slave", "bVisible": "visible"}
# Tech keep-on-self + the dead structure: skipped by the default loop, TechHappiness/Health handled explicitly below.
DROP = {"TechHappinessTypes", "TechHealthTypes", "YieldChanges"}
FAMILY_ORDER = ["food", "production", "commerce", "gold", "research", "culture", "espionage",
                "greatPeopleRate", "health", "happiness", "experience", "underworld"]

# Inbound conditioned boosts folded onto this specialist (accumulate_conditioned 7/8-tuple:
# src_ent, field, _ttype, family, valueKeys, unit, deposit_scope[, cond_scope]). The cond_scope 8th element
# overrides the conditioner's PRESENCE scope where it differs from the deposit scope (non-local building = empire).
SPECIALIST_BOOSTS = [
    ("BuildingInfo", "SpecialistYieldChanges",           "buildings", "yield",    engine.YIELDS,    "flat",    "empire", "empire"),
    ("BuildingInfo", "SpecialistCommerceChanges",        "buildings", "commerce", engine.COMMERCES, "flat",    "empire", "empire"),
    ("BuildingInfo", "LocalSpecialistYieldChanges",      "buildings", "yield",    engine.YIELDS,    "flat",    "city"),
    ("BuildingInfo", "LocalSpecialistCommerceChanges",   "buildings", "commerce", engine.COMMERCES, "flat",    "city"),
    ("CivicInfo",    "SpecialistYieldPercentChanges",    "civics",    "yield",    engine.YIELDS,    "percent", "city"),
    ("CivicInfo",    "SpecialistCommercePercentChanges", "civics",    "commerce", engine.COMMERCES, "percent", "city"),
]


def _human(raw):
    """÷100 de-scale → human number: int when whole (200→2), else 2-decimal float (150→1.5, −40→−0.4)."""
    v = raw / 100.0
    return int(v) if v == int(v) else round(v, 2)


def _put(fam, family, member, unit, val):
    """family.city[.member].unit = val (scope is always city for the scope-wide families)."""
    node = fam.setdefault(family, OrderedDict()).setdefault("city", OrderedDict())
    if member:
        node = node.setdefault(member, OrderedDict())
    node[unit] = val


def _inject_cond(fam, family, scope, unit, value, enabled):
    """Keep-on-self conditioned deposit: family.scope.unit gets [{value, enabled}, ...] coexisting with the
    base value (int|list-safe — modifier.md §6.5)."""
    node = fam.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    entry = OrderedDict([("value", value), ("enabled", enabled)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


def _apply_family(fam, tag, c):
    """Scope-wide CITY family from a top-level tag. SPLIT base (valueKeys) → each identifier is its own family."""
    family, keys, descale = FAMILIES[tag]
    if keys:                                          # SPLIT: the positional identifier IS the family name
        for ident, v in engine.named_array(c, keys).items():
            _put(fam, ident, None, "flat", v)         # yields/commerce are ×1 (never de-scaled)
    else:
        t = engine.text(c)
        if engine.is_int(t) and int(t) != 0:
            _put(fam, family, None, "flat", _human(int(t)) if descale else int(t))


def _unit_combat_xp(node, fam):
    """UnitCombatExperienceTypes → experience.city.unitCombats.{UNITCOMBAT}.flat (target-keyed, ×1)."""
    for entry in list(node):
        uc = engine.text(entry.find("UnitCombatType"))
        mod = engine.text(entry.find("iModifier"))
        if uc and engine.is_int(mod) and int(mod) != 0:
            (fam.setdefault("experience", OrderedDict()).setdefault("city", OrderedDict())
             .setdefault("unitCombats", OrderedDict()).setdefault(uc, OrderedDict()))["flat"] = int(mod)


def _properties(node, props):
    """PropertyManipulators → PROPERTY_X.<scope>.<unit> via the shared v3 converter. Multiple sources to one
    (prop, scope, unit) accumulate as a LIST; a lone source stays scalar."""
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
    text, fam, props, art_blocks, identity, ai, leftover = {}, OrderedDict(), OrderedDict(), {}, {}, {}, []
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in FAMILIES:
            _apply_family(fam, tag, c)
        elif tag in UNDERWORLD:
            if engine.is_int(t) and int(t) != 0:
                _put(fam, "underworld", UNDERWORLD[tag], "flat", int(t))
        elif tag == "UnitCombatExperienceTypes":
            _unit_combat_xp(c, fam)
        elif tag == "PropertyManipulators":
            _properties(c, props)
        elif tag == "Flavors":
            v = engine.generic(c)
            if v:
                ai["flavours"] = v
        elif tag in ART:
            cc.put_art(art_blocks, tag, engine.generic(c))     # Texture→ui.art.texture, Button→ui.art.icon
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

    # Tech-conditioned OWN happiness/health → KEEP-ON-SELF (the specialist owns the output; the team-tech is the
    # enabling GIVER). city-scope, ×1 (read RAW by getExtraTech…Total), `enabled` by the team-tech.
    for tag, family in (("TechHappinessTypes", "happiness"), ("TechHealthTypes", "health")):
        node = rec.find(tag)
        if node is not None:
            for tech, _u, val in cc._boost_entries(node, None, "flat"):
                _inject_cond(fam, family, "city", "flat", val, OrderedDict([("type", tech), ("scope", "team")]))

    # Inbound building/civic boosts → THIS specialist's OWN output, `enabled` by the source's presence.
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
    for family in fam:                                          # any family not in the explicit order
        if family not in out:
            out[family] = fam[family]
    for prop in sorted(props):
        out[prop] = props[prop]
    if ai:
        out["ai"] = ai
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
