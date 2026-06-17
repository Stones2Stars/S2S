#!/usr/bin/env python3
"""Shared core for the top-down JSON curators (#428) — ONE model across entities.

Every entity curator passes an EntityConfig + a boosts config and gets the same conventions:
top-down `enables`/`obsoletes` (from the store), FLAT modifier FAMILIES at the top level (no `modifiers`
wrapper, §3) — each `<family>.<scope>.<member>.<unit>`, where the old mapping `channel` IS the family and
named keys (food/gold) are members; entity-targeted "boosts" fold into the SAME family, qualified by
`<targetType>.<TARGET>`. Plus an `ai` group (flavours + behaviour), `grants`, clean de-Hungarian names,
boolean flags, the `worth`/`{field}Worth` naming, named yield/commerce keys, and faithful x100 (#432).
Per-entity specifics come from mapping/<Entity>.json + the config.
See Sources/docs/plans/building-cascade-conversion.md -> "THE MODEL (locked 2026-06-14)".
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from store import Store, REPO

MAPDIR = os.path.join(os.path.dirname(__file__), "mapping")

HOIST = {"Description": "description", "Civilopedia": "civilopedia", "Quote": "quote",
         "Help": "help", "Strategy": "strategy"}
# b-flags -> clean boolean keys (no Hungarian). Rename the unclear ones; others just drop the leading 'b'.
B_FLAG_NAMES = {"bTrade": "tradeable", "bGoodyTech": "goodyTech"}
# Unify the display-art reference to `icon` across entities: tech's direct Button path AND bonus/building's
# ArtDefineTag both -> icon. Other art fields (sound/movie/...) just de-Hungarianize.
ART_RENAME = {"Button": "icon", "ArtDefineTag": "icon"}
# ART SUB-BLOCKS (owner 2026-06-16): the SUBSYSTEM is the TOP-LEVEL block (`ui`/`world`/`sound`), with `art` a
# sub-block WITHIN ui/world (so non-art members like key-triggers sit BESIDE art, not under it) — dedicated-block
# rule, modifier-spec §0.8. Solves the icon headache: UI icon (`Button`) -> ui.art.icon, on-map graphic
# (`ArtDefineTag`) -> world.art.icon. SOUND is flat (`sound.footsteps`). Movie video+sound go TOGETHER under
# ui.art.movie.{file,sound}. tag -> full dotted path. Unknown art tag -> ui.art.<de_i> (visible; categorize at its pass).
ART_BLOCK = {
    # --- ui.art (on-screen icons / buttons / movies / glyphs) ---
    # Button + Texture are BOTH UI 2D icons, but DISTINCT slots: among gameplay infos only Specialist carries a
    # <Texture> (the city-screen/advisor citizen icon, Python interface), beside its base-class <Button> (the
    # pedia/WorldBuilder button) — verified both consumed only by interface screens, neither on-map; 2 specialists
    # have differing values, so they must NOT collapse to one key (owner 2026-06-16). (The other <Texture> users are
    # the separate, unmigrated CIV4ArtDefines_Unit art tier, not cc-curated.)
    "Button": "ui.art.icon", "Texture": "ui.art.texture",
    "TechButton": "ui.art.techButton", "GenericTechButton": "ui.art.genericTechButton",
    "Advisor": "ui.art.advisor",
    "MovieFile": "ui.art.movie.file", "MovieSound": "ui.art.movie.sound",
    "VictoryMovie": "ui.art.movie.file", "MovieDefineTag": "ui.art.movie.defineTag",
    "FontButtonIndex": "ui.art.fontButton", "iTGAIndex": "ui.art.tgaIndex",
    # --- ui (non-art: key triggers) ---
    "HotKey": "ui.hotkey", "bAltDown": "ui.altDown", "bCtrlDown": "ui.ctrlDown",
    "bShiftDown": "ui.shiftDown", "iHotKeyPriority": "ui.hotKeyPriority",
    # --- world.art (on-map / 3D graphics, styles) ---
    "ArtDefineTag": "world.art.icon",
    "EffectType": "world.art.effect.type", "iEffectProbability": "world.art.effect.probability",  # cosmetic bird-scatter (async RNG, active-player-only) + its trigger chance — grouped (verified CvUnit.cpp:4996)
    "ArtStyleType": "world.art.style", "UnitArtStyleType": "world.art.unitStyle",
    "DefaultPlayerColor": "world.art.playerColor",  # civ render colour (EXE-bound int FK), beside the civ's world art
    "EntityEvent": "world.art.entityEvent",  # Build: the on-map worker animation (ENTITY_EVENT_SHOVEL/IRRIGATE/…), EXE-bound (getEntityEvent DllExport)
    # --- sound (flat: audio is itself the asset) ---
    "FootstepSounds": "sound.footsteps", "WorldSoundscapeAudioScript": "sound.soundscape",
    "GrowthSound": "sound.growth", "ConstructSound": "sound.construct",
    "CreateSound": "sound.onCompletion",   # Project: audio played when the project is COMPLETED (CvCity.cpp:16205)
    "Sound": "sound.sound", "SoundMP": "sound.soundMP",
    "CivilizationActionSound": "sound.action", "CivilizationSelectionSound": "sound.selection",
    "AudioUnitVictoryScript": "sound.unitVictory", "AudioUnitDefeatScript": "sound.unitDefeat",
    "EraInfoSoundtracks": "sound.soundtracks", "CitySoundscapes": "sound.citySoundscapes",
    # Era: make the era's FIRST listed soundtrack play first on era entry. A sibling flag over `sound.soundtracks`;
    # owner's better future shape is a soundtracks OBJECT carrying a per-track `firstPlayed` (out of scope, #428).
    "bFirstSoundtrackFirst": "sound.introSoundtrack",
    "iSoundtrackSpace": "sound.soundtrackSpace",
    "DiplomacyIntroMusicPeace": "sound.diploIntroMusicPeace", "DiplomacyIntroMusicWar": "sound.diploIntroMusicWar",
    "DiplomacyMusicPeace": "sound.diploMusicPeace", "DiplomacyMusicWar": "sound.diploMusicWar",
}
# All AI-targeting metadata -> one `ai` group, subgroups flavours + behaviour (extensible: attitude/strategy).
AI_BEHAVIOUR = {"iAITradeModifier": "tradeModifier", "iAIWeight": "weight", "iAIObjective": "objective"}
# Base yield/commerce SPLIT into per-identifier families (food/gold/…) — no `yield`/`commerce` wrapper, which
# kills the commerce/production/research member-vs-family ambiguity (owner ruling 2026-06-14). Coherent-concept
# families (maintenance/upkeep/diplomacy/combat/vicinityYield) stay GROUPED with members.
SPLIT_FAMILIES = {"yield", "commerce"}
# Output order for the flat family sections (those present); any not listed (e.g. PROPERTY_*) fall in after.
FAMILY_ORDER = ["food", "production", "commerce", "gold", "research", "culture", "cultureDistance", "espionage",
                "vicinityYield", "buildRate", "buildTime", "happiness", "health", "growth", "techCost",
                "workRate", "movement", "defense", "freeSpecialists", "experience", "maintenance", "upkeep",
                "perEra", "revolution", "diplomacy", "combat", "barbarians"]


def de_i(tag):
    """iGridX -> gridX ; Button -> button (drop a leading i-prefix, else lowercase the first char)."""
    if tag[:1] == "i" and len(tag) > 1 and tag[1:2].isupper():
        return tag[1].lower() + tag[2:]
    return tag[:1].lower() + tag[1:]


def drop_empty_audio(v):
    """Drop empty / 'NONE' audio entries (owner 2026-06-16): an empty footstep script maps to the C++ array's
    DEFAULT (-1 / no sound) — identical to an absent entry — so authoring it carries no information (verified:
    SetVariableListTagPairForAudioScripts InitLists to the default, CvXMLLoadUtilitySet.cpp:2434). Filters empty
    footstep-script entries + empty/'NONE' sound strings; leaves real art untouched. None if it collapses to empty."""
    if isinstance(v, str):
        return None if v in ("", "NONE") else v
    if isinstance(v, dict):
        out = {k: x for k, x in ((k, drop_empty_audio(x)) for k, x in v.items()) if x is not None}
        return out or None
    if isinstance(v, list):
        out = [x for x in (drop_empty_audio(x) for x in v) if x is not None]
        return out or None
    return v


def _set_path(d, dotted, val):
    """Set a (possibly nested) dotted key in a dict: _set_path(identity, 'advancedStart.cost', 24)."""
    keys = dotted.split(".")
    for k in keys[:-1]:
        d = d.setdefault(k, {})
    d[keys[-1]] = val


def put_art(art_blocks, tag, val):
    """Route a raw art value to its ui/world/sound block via ART_BLOCK (tag -> dotted path), dropping empty/'NONE'
    audio (drop_empty_audio). The shared art emitter for BESPOKE curators — mirrors curate()'s art handling so EVERY
    entity lands the SAME top-level ui/world/sound shape (owner art restructure 2026-06-16). `tag` is the ORIGINAL XML
    tag (ART_BLOCK keys on it, not the de-Hungarian name). Unknown art tag -> ui.art.<de_i> (visible; categorize later)."""
    av = drop_empty_audio(val)
    if av is not None:
        _set_path(art_blocks, ART_BLOCK.get(tag, "ui.art." + de_i(tag)), av)


def emit_art(out, art_blocks):
    """Emit the populated ui/world/sound blocks (reserved order) onto the output object — the bespoke-curator twin of
    curate()'s art emit. No-op for an empty block."""
    for blk in ("ui", "world", "sound"):
        if art_blocks.get(blk):
            out[blk] = art_blocks[blk]


def _merge_val(a, b):
    """Additive merge on collision (cascade rule): dicts merge keys (summing shared), numbers sum."""
    if isinstance(a, dict) and isinstance(b, dict):
        out = dict(a)
        for k, v in b.items():
            out[k] = _merge_val(out[k], v) if k in out else v
        return out
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return a + b
    return b


def _boost_entries(node, keys, unit):
    """(refType, unit, value) per entry. Key child = the conditioner ref (<PrereqTech>/<TechType>/
    <BonusType>/any *Type); value = the remaining child(ren). Handles C2C's inconsistent key tags."""
    for entry in list(node):
        ref, vals = None, []
        for c in entry:
            if ref is None and (c.tag in ("PrereqTech", "TechType") or c.tag.endswith("Type")):
                ref = engine.text(c)
            else:
                vals.append(c)
        if not ref or ref == "NONE" or not vals:
            continue
        if len(vals) == 1 and list(vals[0]):
            value = engine.named_array(vals[0], keys) if keys else engine.generic(vals[0])
        elif len(vals) == 1:
            tx = engine.text(vals[0])
            value = int(tx) if engine.is_int(tx) else tx
        else:
            value = {c.tag: engine.generic(c) for c in vals}
        if value not in (None, {}, [], ""):
            yield ref, unit, value


def accumulate_boosts(store, boosts_config):
    """Invert entity-targeted modifiers onto their conditioner (`ref`) in FAMILY-first shape (flat-family
    layout, §3), ready to fold: {ref: {family: {scope: {targetType: {TARGET: {member?: {unit: val}}}}}}}.
    `channel` is the family; named keys become members; `tgt_t` (the entity carrying the field) is the TARGET."""
    boosts = {}
    for cfg in boosts_config:
        src_ent, fld, target_type, family, keys, unit, scope = cfg[:7]
        subt = cfg[7] if len(cfg) > 7 else None            # optional 2nd keyed dimension (e.g. specialists)
        scope = "empire" if scope == "player" else ("world" if scope == "game" else scope)
        for tgt_t, rec in store.table(src_ent).items():
            node = rec.find(fld)
            if node is None:
                continue
            for ref, u, val in _boost_entries(node, keys, unit):
                if subt and isinstance(val, list):         # nest a keyed sub-dimension: targetType.TARGET.subt.{KEY}.unit
                    base = (boosts.setdefault(ref, {}).setdefault(family, {}).setdefault(scope, {})
                            .setdefault(target_type, {}).setdefault(tgt_t, {}).setdefault(subt, {}))
                    for d in val:
                        for sk, sv in (d.items() if isinstance(d, dict) else []):
                            leaf = base.setdefault(sk, {})
                            leaf[u] = _merge_val(leaf[u], sv) if u in leaf else sv
                    continue
                if isinstance(val, dict):                  # named members (food/gold/…)
                    for member, v in val.items():
                        if family in SPLIT_FAMILIES:       # member IS the family: member.scope.targetType.TARGET.unit
                            leaf = (boosts.setdefault(ref, {}).setdefault(member, {}).setdefault(scope, {})
                                    .setdefault(target_type, {}).setdefault(tgt_t, {}))
                        else:                              # grouped: family.scope.targetType.TARGET.member.unit
                            leaf = (boosts.setdefault(ref, {}).setdefault(family, {}).setdefault(scope, {})
                                    .setdefault(target_type, {}).setdefault(tgt_t, {}).setdefault(member, {}))
                        leaf[u] = _merge_val(leaf[u], v) if u in leaf else v
                else:                                      # scalar family
                    leaf = (boosts.setdefault(ref, {}).setdefault(family, {}).setdefault(scope, {})
                            .setdefault(target_type, {}).setdefault(tgt_t, {}))
                    leaf[u] = _merge_val(leaf[u], val) if u in leaf else val
    return boosts


def apply_channel(families, spec, c):
    """Deposit a scope-wide modifier into its FAMILY (flat-family layout, §3): <family>.<scope>[.<member>].<unit>.
    The mapping `channel` IS the family; named valueKeys (food/gold/…) are members; a scalar has no member."""
    scope = spec.get("scope", "city")
    scope = "empire" if scope == "player" else ("world" if scope == "game" else scope)
    family, kind = spec.get("channel"), spec.get("kind", "flat")
    keys = spec.get("valueKeys")
    if family and family.endswith("PerPopulation"):
        family = family[:-len("PerPopulation")]
        kind = "percentPerPopulation" if kind == "percent" else "perPopulation"
    # Keyed-by-Type CONTAINER (e.g. FreeSpecialistCounts: each entry = <SpecialistType> + <iFreeSpecialistCount>).
    # `targetType` in the mapping marks this; the entry is addressed family.scope.<targetType>.<TARGET>.<unit>
    # (modifier-spec §1.2). Reuses _boost_entries (the same ref+value parser used to invert entity-targeted fields).
    target_type = spec.get("targetType")
    if target_type:
        for ref, _u, val in _boost_entries(c, keys, kind):
            if val in (None, {}, [], "") or val == 0:
                continue
            leaf = (families.setdefault(family, {}).setdefault(scope, {})
                    .setdefault(target_type, {}).setdefault(ref, {}))
            leaf[kind] = _merge_val(leaf[kind], val) if kind in leaf else val
        return
    if kind == "enabler":
        if engine.text(c) not in ("1", "true", "True"):
            return
        val = True
    elif keys:
        val = engine.named_array(c, keys)
        if not val:
            return
    else:
        t = engine.text(c)
        if not engine.is_int(t) or int(t) == 0:
            return
        val = int(t)
    if isinstance(val, dict):                  # named members (food/gold/…)
        for member, v in val.items():
            if family in SPLIT_FAMILIES:       # split: the member IS the family (food.scope.unit) — no wrapper
                families.setdefault(member, {}).setdefault(scope, {})[kind] = v
            else:                              # grouped concept: family.scope.member.unit (vicinityYield, …)
                families.setdefault(family, {}).setdefault(scope, {}).setdefault(member, {})[kind] = v
    else:                                      # scalar family: family.scope[.member].unit
        node = families.setdefault(family, {}).setdefault(scope, {})
        member = spec.get("member")            # explicit grouped member (maintenance.distance, movement.cost, …)
        if member:
            node = node.setdefault(member, {})
        node[kind] = val


class EntityConfig:
    """Per-entity config, mostly read from mapping/<Entity>.json (channels/cost/art/prereqs)."""
    def __init__(self, entity, cost_rename=None, grants=None, era_fn=None, extra_drop=None, map_gen=None,
                 families=None, id_rename=None, to_identity=None, requires_fn=None, art_rename=None, allowed_fn=None):
        m = json.load(open(os.path.join(MAPDIR, entity + ".json")))
        self.entity = entity
        # `families` = verified per-field modifier specs (from the classification) that OVERRIDE the first-pass
        # mapping channels — the mapping under-classified real gameplay, so the curator carries the truth.
        self.channels = dict(m.get("channels", {}))
        self.channels.update(families or {})
        self.id_rename = id_rename or {}        # per-entity identity-key renames (tag -> clean name)
        # tag -> dotted identity path: force a field into identity at that (possibly nested) key, OVERRIDING the
        # mapping's cost/art classification. Used to keep the advanced-start mechanic grouped under
        # identity.advancedStart across entities (Handicap/Era/Route/…) until that mechanic is reviewed.
        self.to_identity = to_identity or {}
        # per-entity art-key override (tag -> clean name), taking precedence over the global ART_RENAME. Used
        # when an entity carries MULTIPLE art refs the global map would collide (Terrain: the on-map graphics
        # `ArtDefineTag` must stay distinct from the UI `Button` icon, which both default to `icon`).
        self.art_rename = art_rename or {}
        self.cost_fields = set(m.get("cost", []))
        self.art_fields = set(m.get("art", []))
        # prereqs are DROPPED from the authored object (top-down: they become other entities' `enables`/
        # `obsoletes`, derived in the store). Type is identity-by-filename. Plus any entity-specific extras.
        self.drop = set(m.get("prereqs", [])) | {"Type"} | set(extra_drop or [])
        self.cost_rename = cost_rename or {}
        self.grants = grants or {}
        self.map_gen = set(map_gen or [])   # field tags routed into a `mapGeneration` group instead of identity
        self.era_fn = era_fn or (lambda rec, store: "")
        # requires_fn(rec, store) -> the TARGET-side reversible MEANS gate (enabler-spec §3/§5): the positive
        # `requires.{build,operate}` BoolExpr authored ON this entity from its OWN prereq fields (the fields the
        # store does NOT invert away). Returns a dict or None. Per-entity, since prereq shapes differ.
        self.requires_fn = requires_fn or (lambda rec, store: None)
        # allowed_fn(rec, store) -> the declarative INSTANCE CAP (enabler-spec §5/§13.7): `allowed:{<scope>:N}`
        # (self-cap, scope-keyed) "at most N of me at scope". The real cap number; engine owns enforcement +
        # ignoring it under game options. Returns a dict or None. Per-entity, since cap fields differ.
        self.allowed_fn = allowed_fn or (lambda rec, store: None)


def curate(typ, rec, cfg, store, boosts):
    text_fields, families, cost, art_blocks, identity, grants, map_gen = {}, {}, {}, {}, {}, {}, {}
    ai_behaviour, ai_flavours = {}, None
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag in cfg.drop:
            continue
        if tag in HOIST:
            if t:
                text_fields[HOIST[tag]] = t
        elif tag == "Flavors":
            ai_flavours = engine.generic(c)
        elif tag in AI_BEHAVIOUR:
            if engine.is_int(t) and int(t) != 0:
                ai_behaviour[AI_BEHAVIOUR[tag]] = int(t)
        elif tag in cfg.to_identity:                       # forced into identity, overriding cost/art mapping
            if engine.is_int(t):
                if int(t) != 0:
                    _set_path(identity, cfg.to_identity[tag], int(t))
            elif list(c) or t:
                _set_path(identity, cfg.to_identity[tag], engine.generic(c))
        elif tag in cfg.channels:
            apply_channel(families, cfg.channels[tag], c)
        elif tag in cfg.grants:
            grants[cfg.grants[tag]] = int(t) if engine.is_int(t) else (t or engine.generic(c))
        elif tag in cfg.cost_fields:
            if engine.is_int(t) and int(t) != 0:
                cost[cfg.cost_rename.get(tag, de_i(tag))] = int(t)
        elif tag in cfg.art_fields:
            av = drop_empty_audio(engine.generic(c))   # only emit real sounds; empties default in the EXE (owner)
            if av is not None:
                _set_path(art_blocks, ART_BLOCK.get(tag, "ui.art." + de_i(tag)), av)   # ui/world/sound dotted path
        elif tag[:1] == "b" and len(tag) > 2 and tag[1:2].isupper():
            if t in ("1", "true", "True"):                 # boolean flag -> clean name + true (false omitted)
                name = cfg.id_rename.get(tag) or B_FLAG_NAMES.get(tag, tag[1].lower() + tag[2:])
                (map_gen if tag in cfg.map_gen else identity)[name] = True
        else:
            if list(c) or t:
                name = cfg.id_rename.get(tag) or engine.FIELD_RENAME.get(tag, de_i(tag))
                (map_gen if tag in cfg.map_gen else identity)[name] = engine.generic(c)

    for family, fdata in boosts.items():                   # fold entity-targeted modifiers into their family
        families[family] = _merge_val(families[family], fdata) if family in families else fdata

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "civilopedia", "help", "quote", "strategy"):
        if k in text_fields:
            out[k] = text_fields[k]
    # The `enables` FAMILY (enabler-spec §5/§6): four forward-read-from-HAS sections in reserved order.
    # All come from the store's inverted indexes (keyed by the type you HAVE). `disables` is latent today.
    enables = store.enabled_by(typ)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    obsoletes = store.obsoletes_of(typ)
    if obsoletes:
        out["obsoletes"] = OrderedDict((k, obsoletes[k]) for k in sorted(obsoletes))
    replaces = store.replaces_of(typ)
    if replaces:
        out["replaces"] = OrderedDict((k, replaces[k]) for k in sorted(replaces))
    disables = store.disables_of(typ)
    if disables:
        out["disables"] = OrderedDict((k, disables[k]) for k in sorted(disables))
    # `requires` — the TARGET-side reversible MEANS gate (enabler-spec §3/§5), authored from THIS entity's own
    # prereq fields (those the store does not invert into others' `enables`). Reserved order: after the
    # `enables`-family (enables/obsoletes/replaces/disables), before the modifier families (modifier-spec §1.1).
    requires = cfg.requires_fn(rec, store)
    if requires:
        out["requires"] = requires
    allowed = cfg.allowed_fn(rec, store)                   # declarative instance cap (enabler-spec §5/§13.7)
    if allowed:
        out["allowed"] = allowed
    for family in FAMILY_ORDER:                            # families at TOP LEVEL (no `modifiers` wrapper, §3)
        if family in families:
            out[family] = families[family]
    for family in families:                                # any family outside the ordering (safety)
        if family not in out:
            out[family] = families[family]
    if grants:
        out["grants"] = grants
    if cost:
        out["cost"] = cost
    ai = OrderedDict()
    if ai_behaviour:
        ai["behaviour"] = ai_behaviour
    if ai_flavours:
        ai["flavours"] = ai_flavours
    if ai:
        out["ai"] = ai
    for _blk in ("ui", "world", "sound"):      # art now lives in the ui/world/sound top-level subsystem blocks
        if art_blocks.get(_blk):
            out[_blk] = art_blocks[_blk]
    if map_gen:
        out["mapGeneration"] = map_gen
    if identity:
        out["identity"] = identity
    return out


def run(cfg, boosts_config, store=None, post_process=None):
    store = store or Store()
    boosts = accumulate_boosts(store, boosts_config)
    result = OrderedDict()
    for typ, rec in store.table(cfg.entity).items():
        obj = curate(typ, rec, cfg, store, boosts.get(typ, {}))
        if post_process:                  # per-entity hook to inject SYNTHETIC deposits not sourced from this
            post_process(typ, obj, rec, store)   # entity's own XML (e.g. terrain's HAS_RIVER bonus + the
        result[typ] = (obj, cfg.era_fn(rec, store))   # hills/peak plot-yields moved off YieldInfo). Mutates obj.
    return store, result


def main(cfg, boosts_config, out_dir, post_process=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    _, result = run(cfg, boosts_config, post_process=post_process)
    n = len(result)
    has = lambda k: sum(1 for (o, _) in result.values() if k in o)
    STRUCT = {"type", "description", "civilopedia", "help", "quote", "strategy", "enables", "obsoletes",
              "replaces", "disables", "requires", "allowed", "grants", "cost", "ai", "ui", "world", "sound", "mapGeneration", "identity"}
    fams = lambda o: [k for k in o if k not in STRUCT]
    print("%s curated: %d" % (cfg.entity, n))
    for k in ("enables", "obsoletes", "replaces", "disables", "requires", "grants", "ai"):
        print("  with %-9s: %d" % (k, has(k)))
    print("  with families: %d  | seen: %s"
          % (sum(1 for (o, _) in result.values() if fams(o)),
             ", ".join(sorted({f for (o, _) in result.values() for f in fams(o)}))))
    if args.sample is not None:
        for nm in (args.sample or list(result)[:1]):
            if nm in result:
                print("\n=== %s (era=%s) ===" % (nm, result[nm][1] or "-"))
                print(json.dumps(result[nm][0], indent=1, ensure_ascii=False))
            else:
                print("\n(%s not found)" % nm)
    if args.write:
        for typ, (obj, era) in result.items():
            folder = os.path.join(out_dir, era) if era else out_dir
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d %s JSON files under %s" % (n, cfg.entity, os.path.relpath(out_dir, REPO)))
