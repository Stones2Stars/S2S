#!/usr/bin/env python3
"""Curate Civilization to the top-down model (#428) — a SOURCE entity (game-start grants + per-civ policy/art).

NOT a POCO (the first-pass mapping only saw the spawnRate pair; the POCO caveat applies hard). CivilizationInfo
actively GRANTS game-start state, GATES capability, deposits one modifier, and carries the civ identity/art —
hand-written read() (CvWString names, audio-tag sounds, CivicOption-keyed InitialCivics, the Cities list,
pre-base ArtDefineTag merge) → a bespoke curator. Verdicts (light-four-classification):
- TEXT: Description / ShortDescription / Adjective / Civilopedia.
- `grants` (one-shot, at game start; flat — landing scope implicit in the grant TYPE, consumer-side):
  `buildings` ← FreeBuildings (the capital: Palace + civ-class buildings, re-asserted on capital move),
  `civics` ← InitialCivics (empire: one starting civic per civic-option slot),
  `techs` ← FreeTechs (team) + **TECH_GAME_START granted to EVERY civ** (owner ruling 2026-06-25): the cascade's
  declarative clean start point (a JSON-only root tech, no XML source) off which the cascade GENERATES the "stuff
  simply there at start" without the old engine's load-everything-with-no-deps behaviour. The XML FreeTechs proper is
  Neanderthal-ONLY (CIVILIZATION_NPC_NEANDERTHAL: CAVE_DWELLING/GATHERING/NOMADISM/SCAVENGING/LANGUAGE; CvGame:1226);
  TECH_GAME_START rides ON TOP for all civs, prepended in curate() below.
- `spawnRate.empire.{general,npcPeace}.percent` ← iSpawnRateModifier / iSpawnRateNPCPeaceModifier — the ONE
  cascade modifier (PERCENT, (100+mod)/100; CvGame:6359/6363). Barb/NPC civs only.
- `identity` (civ meta, owner 2026-07-01 -- NOT `policies`; a policy is a pure empire STATE, json.md §9): `isNpc`
  (derived from the CIVILIZATION_NPC_* type convention -- no engine flag for the full NPC set), `playable` / `aiPlayable`
  (selectability, load-only), `stronglyRestricted` (the NPC build-lockdown, CvCity:2205/2547 -- a DEFERRED enabler input
  parked here so no context is lost). These describe WHAT/WHO the civ is, so they live in identity, not a policy block.
- `disables.techs` — per-civ research ban (canEverResearch=false while active, CvPlayer:8266). Modeled as a v0.3
  `disables` (a reversible ban), NOT a permanent removal to re-home (owner 2026-06-15: there is no CURRENT in-game
  logic to reverse a tech disable — so today it is effectively permanent — BUT nothing stops us adding reversal
  if we want, so the MODEL treats it as reversible; data leads, the engine catches up). The earlier "permanent,
  re-home it" framing was wrong. Removes the tech from the civ's CAN GET while active; scope = this civ's
  player/empire. `disables` stays uniform (= reversible ban; scope is just a parameter — empire-law vs per-civ).
  The `disables` OBJECT mirrors `grants` so it extends to other kinds later. ONLY 1 tech on the NPC-barb civ:
  CIVILIZATION_NPC_NEANDERTHAL can't research TECH_SEDENTARY_LIFESTYLE (stays nomadic).
- `art`: DefaultPlayerColor / ArtDefineTag / ArtStyleType / UnitArtStyleType + the two civ sounds.
- `identity`: Leaders (civ↔leader eligibility), Cities (`cityNames` pool), DerivativeCiv (civ-split lineage).
No dead structure found (every getter has a live non-class consumer).

  python3 curate_civilization.py --sample CIVILIZATION_AMERICA CIVILIZATION_NPC_NEANDERTHAL
  python3 curate_civilization.py --write
"""
import argparse
import json
import locale
import os
from collections import OrderedDict

import engine
import curate_common as cc
from store import Store, REPO

# The JSON writer uses the DEFAULT (game-matching) encoding on purpose (see write()). A string that cannot be
# encoded in it is BROKEN content the game's loader can't load either (owner 2026-06-15) — e.g. a modder's
# Old-Norwegian special-char city names. Such names are dropped individually; everything encodable stays.
ENC = locale.getpreferredencoding(False)


def _encodable(s):
    try:
        s.encode(ENC)
        return True
    except (UnicodeEncodeError, LookupError):
        return False

TEXT = OrderedDict([
    ("Description", "description"), ("ShortDescription", "shortDescription"),
    ("Adjective", "adjective"), ("Civilopedia", "civilopedia"),
])
GRANTS = OrderedDict([      # container tag -> (child tag, grant key)
    ("FreeTechs", ("FreeTech", "techs")),
    ("FreeBuildings", ("BuildingType", "buildings")),
    ("InitialCivics", ("CivicType", "civics")),
])
SPAWN = OrderedDict([("iSpawnRateModifier", "general"), ("iSpawnRateNPCPeaceModifier", "npcPeace")])
# Civ META booleans -> the `identity` block (owner 2026-07-01), NOT a `policies` block. These describe WHAT/WHO the civ
# IS (selectability + an NPC build-lockdown), not a pure empire STATE a civic/trait enacts (a `policy`, json.md §9), so
# they leave the ambiguous `policies` word entirely -- keeping `policies` = one meaning. `playable`/`aiPlayable` are
# load-only meta; `stronglyRestricted` is a DEFERRED enabler input (the NPC build-lockdown pairs with EnabledCivilization
# -> a `requires` gate when NPC civs are wired, out of scope now) -- parked in identity so no context is lost.
CIV_META = OrderedDict([("bPlayable", "playable"), ("bAIPlayable", "aiPlayable"),
                        ("bStronglyRestricted", "stronglyRestricted")])
# art/audio tags -> ui/world/sound via the canonical curate_common.ART_BLOCK:
#   DefaultPlayerColor->world.art.playerColor, ArtDefineTag->world.art.icon, ArtStyleType->world.art.style,
#   UnitArtStyleType->world.art.unitStyle, Civilization{Selection,Action}Sound->sound.{selection,action}.
ART = ("DefaultPlayerColor", "ArtDefineTag", "ArtStyleType", "UnitArtStyleType",
       "CivilizationSelectionSound", "CivilizationActionSound")
# every tag we knowingly handle — anything else gets flagged (leftover check)
KNOWN = ({"Type", "Cities", "Leaders", "DisableTechs", "DerivativeCiv"} | set(TEXT) | set(GRANTS)
         | set(SPAWN) | set(CIV_META) | set(ART))


def _list(rec, container, child):
    out, node = [], rec.find(container)
    if node is not None:
        for c in node.findall(child):
            t = engine.text(c)
            if t and t != "NONE":
                out.append(t)
    return out


def curate(typ, rec):
    out = OrderedDict([("type", typ)])
    for tag, key in TEXT.items():
        t = engine.text(rec.find(tag))
        if t:
            out[key] = t
    grants = OrderedDict()
    for container, (child, key) in GRANTS.items():
        vals = _list(rec, container, child)
        if key == "buildings":
            # BUILDING_PALACE is redundant in grants.buildings (json.md §5, owner 2026-06-30): the settler unit's
            # grants.buildings already seeds it and the engine realizes it into the capital regardless. Drop only it.
            vals = [v for v in vals if v != "BUILDING_PALACE"]
        if vals:
            grants[key] = vals
    # Every civ starts with TECH_GAME_START -- the cascade's declarative clean START POINT (owner ruling 2026-06-25):
    # a JSON-only root tech (no XML source -- a deliberate cascade construct) off which the cascade GENERATES the
    # "stuff simply there at start" (palace, alpha male/female, brute, Neanderthal culture level) WITHOUT the old
    # engine's "load everything with no deps" special-casing. Authored on the civ as a startup tech grant so the
    # starts are DECLARATIVE (the canonical place to read "what every civ begins with"). Granted to ALL civilizations.
    grants["techs"] = ["TECH_GAME_START"] + [t for t in grants.get("techs", []) if t != "TECH_GAME_START"]
    out["grants"] = grants
    spawn = OrderedDict()
    for tag, member in SPAWN.items():
        v = engine.text(rec.find(tag))
        if engine.is_int(v) and int(v) != 0:
            spawn[member] = {"percent": int(v)}
    if spawn:
        out["spawnRate"] = {"empire": spawn}
    dt = _list(rec, "DisableTechs", "DisableTech")
    if dt:
        out["disables"] = {"techs": dt}   # `disables` OBJECT, symmetric with `grants` — extensible to other kinds later
    art_blocks = {}
    for tag in ART:
        cc.put_art(art_blocks, tag, engine.text(rec.find(tag)))   # -> ui/world/sound via ART_BLOCK (+ drop empty/NONE)
    cc.emit_art(out, art_blocks)
    identity = OrderedDict()
    # isNpc: WHAT this civ IS -- an NPC civ (barbarian/animal/neanderthal). No CvCivilizationInfo flag exists for the
    # full NPC set (only the BARBARIAN_CIVILIZATION define names one), so it derives from the CIVILIZATION_NPC_* type
    # convention the mod uses (npc_barbarian/beast/insectoid/neanderthal/predator/prey; the playable neanderthal has no
    # NPC_ prefix). Descriptive identity (owner 2026-07-01). ⏳ NPC-civ handling proper is a deferred, out-of-scope pass.
    if typ.startswith("CIVILIZATION_NPC_"):
        identity["isNpc"] = True
    # civ meta booleans (selectability + the deferred NPC build-lockdown) -- moved off the retired civ `policies` block.
    for tag, key in CIV_META.items():
        if engine.text(rec.find(tag)) in ("1", "true", "True"):
            identity[key] = True
    leaders = _list(rec, "Leaders", "Leader")
    if leaders:
        identity["leaders"] = leaders
    # cityNames STAY — integral to a city getting a name when founded (owner 2026-06-15). But DROP individual
    # names whose SPECIAL CHARACTERS break the (game-matching) encoding: a modder wrote Scandinavian names in Old
    # Norwegian to be "authentic"; the game's loader can't load those either, so they are broken content. Keep all
    # the encodable names.
    cities = [c for c in _list(rec, "Cities", "City") if _encodable(c)]
    if cities:
        identity["cityNames"] = cities
    dc = engine.text(rec.find("DerivativeCiv"))
    if dc and dc != "NONE":
        identity["derivativeCiv"] = dc
    if identity:
        out["identity"] = identity
    cc.fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("CivilizationInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items())
    leftovers = {}
    for typ, rec in table.items():
        for c in rec:
            if c.tag not in KNOWN and (list(c) or engine.text(c)):
                leftovers.setdefault(c.tag, 0)
                leftovers[c.tag] += 1
    has = lambda k: sum(1 for o in results.values() if k in o)
    npc = sum(1 for o in results.values() if o.get("identity", {}).get("isNpc"))
    print("CivilizationInfo curated: %d  | grants: %d  spawnRate: %d  identity: %d  isNpc: %d  disables: %d"
          % (len(results), has("grants"), has("spawnRate"), has("identity"), npc, has("disables")))
    if leftovers:
        print("  !! UNHANDLED tags (review): %s" % ", ".join("%s×%d" % (t, n) for t, n in sorted(leftovers.items())))
    else:
        print("  (every XML tag classified — no leftovers)")
    dropped = [c for rec in table.values() for c in _list(rec, "Cities", "City") if not _encodable(c)]
    if dropped:
        safe = [c.encode("ascii", "replace").decode() for c in dropped[:8]]
        print("  dropped %d special-char city name(s) the game can't load either (%s): %s%s"
              % (len(dropped), ENC, ", ".join(safe), " …" if len(dropped) > 8 else ""))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            obj = dict(results.get(nm, {"(not found)": nm}))
            if "identity" in obj and "cityNames" in obj["identity"]:   # trim the long name list for display
                obj["identity"] = dict(obj["identity"])
                obj["identity"]["cityNames"] = ["...(%d names)..." % len(obj["identity"]["cityNames"])]
            print(json.dumps(obj, indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "civilizations")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            # Default (game-matching) encoding ON PURPOSE — NOT utf-8 (owner 2026-06-15): if the toolkit can't
            # encode a string, the game's loader can't load it either, so an encode error SURFACES broken content
            # rather than masking it. The special-char city names that triggered this are already dropped
            # INDIVIDUALLY by _encodable (the cityNames pool itself stays), so the remaining content is encodable.
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d CivilizationInfo JSON files under Assets/Data/civilizations" % len(results))


if __name__ == "__main__":
    main()
