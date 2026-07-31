#!/usr/bin/env python3
"""Shared core for the top-down JSON curators (#428) — ONE model across entities.

Every entity curator passes an EntityConfig + a boosts config and gets the same conventions:
top-down `enables`/`obsoletes` (from the store), FLAT modifier FAMILIES at the top level (no `modifiers`
wrapper, §3) — each `<family>.<scope>.<member>.<unit>`, where the old mapping `channel` IS the family and
named keys (food/gold) are members; entity-targeted "boosts" fold into the SAME family, qualified by
`<targetType>.<TARGET>`. Plus an `ai` group (flavours + behaviour), `grants`, clean de-Hungarian names,
boolean flags, the `worth`/`{field}Worth` naming, named yield/commerce keys, and faithful x100 (#432).
Per-entity specifics come from mapping/<Entity>.json + the config.
See docs/specs/modifier.md (the flat-family modifier model + deliveryguy/inversion) and docs/specs/json.md
(the JSON shapes this produces).
"""
import argparse
import atexit
import json
import os
import re
from collections import OrderedDict

import engine
from store import Store, REPO

MAPDIR = os.path.join(os.path.dirname(__file__), "mapping")


def gate_entity(out, on_options, off_options):
    """The ENTITY-LEVEL `enabled`/`disabled` applicability gate (json.md §3.9 applied at entity level; owner ruling
    2026-07-08: `enabled: GAMEOPTION_X` "is literally what it is supposed to be" -- the retired `loadPrune`
    invention's replacement, superseded-ideas.md). One shared emitter so every curator authors the same shape:
      on_options  -- legacy OnGameOptions / PrereqGameOption: ALL listed options must be ON  -> `enabled`
      off_options -- legacy NotOnGameOptions / NotGameOption: ANY listed option ON suppresses -> `disabled`
    A single option authors as the bare condition string; several as the {all}/{anyOf} tree (json.md §3.4)."""
    on_options = [x for x in (on_options or []) if x and x != "NONE"]
    off_options = [x for x in (off_options or []) if x and x != "NONE"]
    if on_options:
        out["enabled"] = on_options[0] if len(on_options) == 1 else OrderedDict([("all", list(on_options))])
    if off_options:
        out["disabled"] = off_options[0] if len(off_options) == 1 else OrderedDict([("anyOf", list(off_options))])


def descale100(v):
    """The curator's ONE-TIME x100 -> HUMAN-READABLE de-scale (cascade-fixed-point.md §0/§1.1, owner-LOCKED).
    A legacy XML value stored x100 (a `get...100()` accessor / a `Centi*` or `*100` field that flows into a x100
    accumulator) becomes a plain human number: int when divisible by 100 (700 -> 7), else a 2-decimal float
    (150 -> 1.5). Recurses into split/member dicts. After this single XML->JSON conversion NO x100 representation
    survives in the JSON; readJson then does the uniform human -> integer-x100 conversion. ONE place for every
    curator (building/heritage/promotion/unitcombat/...) so the de-scale can never drift per-entity."""
    if isinstance(v, dict):
        return OrderedDict((k, descale100(x)) for k, x in v.items())
    if isinstance(v, bool):
        return v
    if isinstance(v, int):
        return v // 100 if v % 100 == 0 else round(v / 100.0, 2)
    return v

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
    # ArtDefineTag -> world.art.DEFINE (not "icon"): the ART_DEF_* tag is the link to the FULL on-map model setup
    # (NIF model + KFM animation + textures via ARTFILEMGR), NOT a 2D icon (that is ui.art.icon = Button). Only the
    # tag id lives in JSON; the definition stays in CIV4ArtDefines_* (json.md §7).
    "ArtDefineTag": "world.art.define",
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


# Size Matters (GAMEOPTION_COMBAT_SIZE_MATTERS) -> the dedicated `sizeMatters` block (json.md §9 own-block rule),
# NOT the strength/cargo modifier families or identity.base. The tag SETS differ by entity: promotion + unitcombat
# carry the *Change deltas (shared), the unit carries the base values -- each curator passes its own set below.
SM_FLAT_CHANGE     = {"iStrengthModifier": "sizeModifier", "iMaxHPChange": "maxHP",
                      "iQualityChange": "quality", "iGroupChange": "group"}
SM_COMBATMOD_CHANGE = {"iCombatModifierPerSizeMoreChange": "perSizeMore", "iCombatModifierPerSizeLessChange": "perSizeLess",
                       "iCombatModifierPerVolumeMoreChange": "perVolumeMore", "iCombatModifierPerVolumeLessChange": "perVolumeLess"}
SM_CARGO_CHANGE    = {"iSMCargoChange": "smSpace", "iSMCargoVolumeChange": "volume", "iSMCargoVolumeModifierChange": "volumeModifier"}
SM_COMBATMOD_UNIT  = {"iCombatModifierPerSizeMore": "perSizeMore", "iCombatModifierPerSizeLess": "perSizeLess",
                      "iCombatModifierPerVolumeMore": "perVolumeMore", "iCombatModifierPerVolumeLess": "perVolumeLess"}


def emit_sizematters(out, get_int, flat=None, combatmod=None, cargo=None, base_ranks=None):
    """Build the `sizeMatters` block and place it on `out` (no-op if empty). `get_int(tag)` -> int|None (pass the
    curator-local `lambda t: _int(rec, t)`). `flat`/`combatmod`/`cargo` are tag->key dicts (0/None dropped -- these are
    deltas/mods where 0 = none); `base_ranks` is a preset {qualityBase/groupBase/sizeBase: v} dict the CALLER builds
    (base ranks keep a -10 "unset" sentinel, so 0 is a real value the caller must NOT drop -- filter -10 there)."""
    sm = OrderedDict()
    if base_ranks:
        sm.update(base_ranks)
    for tag, key in (flat or {}).items():
        v = get_int(tag)
        if v:
            sm[key] = v
    cm = OrderedDict()
    for tag, key in (combatmod or {}).items():
        v = get_int(tag)
        if v:
            cm[key] = v
    if cm:
        sm["combatModifier"] = cm
    cg = OrderedDict()
    for tag, key in (cargo or {}).items():
        v = get_int(tag)
        if v:
            cg[key] = v
    if cg:
        sm["cargo"] = cg
    if sm:
        out["sizeMatters"] = sm


# TEXT belongs INSIDE `identity` (json.md §7: "identity — all TEXT"); only `type` (the ID) stays at root, front-and-
# centre. This fold moves any root TEXT keys into out["identity"] (created if absent, TEXT FIRST, existing identity
# fields after) — applied UNIFORMLY by every curator so the whole model is consistent (owner 2026-07-01, no deferral:
# "if deferred it will be forgotten, and we get inconsistency"). Call it right before writing/returning the finished out.
TEXT_KEYS = ("description", "shortDescription", "adjective", "civilopedia", "help", "quote", "strategy", "message")


def fold_text_to_identity(out):
    text = [(k, out.pop(k)) for k in TEXT_KEYS if k in out]
    if not text:
        return out
    ident = OrderedDict(text)
    for k, v in out.get("identity", {}).items():
        ident[k] = v
    out["identity"] = ident
    return out


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


def _deposit(node, unit, val, enabled=None):
    """Write ONE deposit into its slot, honouring the json.md §3.9 entry shape.

    An UNCONDITIONED value stays a bare number. A CONDITIONED one becomes `{value, enabled}`, and a slot holding
    both composes as the entry LIST -- never a sum, because summing a gated value into an ungated one would apply
    it unconditionally. The list IS the formula mechanism (§3.9), so this needs no expression syntax.

    ⛔ A condition is a PREDICATE, never a family of its own ([DEC-conditions-are-predicates]). Without this, a
    conditioned legacy field has nowhere to go but an invented family name the reader does not know -- which is
    exactly how foreignTradeRoute* came to exist.
    """
    if enabled is None:
        prior = node.get(unit)
        if prior is not None and _has_condition(prior):
            node[unit] = ([val] + prior) if isinstance(prior, list) else [val, prior]
        else:
            node[unit] = val                     # unconditioned: unchanged behaviour
        return
    entry = {"value": val, "enabled": enabled}
    prior = node.get(unit)
    if prior is None:
        node[unit] = entry
    else:
        node[unit] = (prior if isinstance(prior, list) else [prior]) + [entry]


def _has_condition(v):
    if isinstance(v, list):
        return any(_has_condition(x) for x in v)
    return isinstance(v, dict) and ("enabled" in v or "disabled" in v)


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


# A CONDITIONER's natural presence scope (where "having it" is read), distinct from the deposit scope: a building is
# present in a CITY, a civic/trait is active EMPIRE-wide, a tech is known at TEAM scope. Used to build the `enabled` atom.
_COND_SCOPE = {"BuildingInfo": "city", "CivicInfo": "empire", "TraitInfo": "empire",
               "BonusInfo": "city", "TechInfo": "team", "ReligionInfo": "city"}


def accumulate_conditioned(store, config):
    """The keep-on-source CONDITIONED inverse of accumulate_boosts (modifier.md §6.5). The source entity is a
    CONDITIONER, NOT a keyed target: the modifier lands on its HOME (the keyed ref -- e.g. the specialist that OWNS
    the output) as a flat/percent deposit `enabled` by the source's PRESENCE. The deposit scope is the config scope;
    the `enabled` atom's scope is the conditioner's own presence scope (_COND_SCOPE), which can DIFFER (a civic boosts
    a specialist's CITY yield but is active at EMPIRE scope). Returns {home: [(family, scope, unit, value, enabled)]} --
    a list of deposits to inject via the caller's _inject_cond (so it merges with the home's own base, int|list-safe).
    config row (7-tuple, target_type ignored): (src_ent, field, _ttype, family, valueKeys, unit, deposit_scope)."""
    out = {}
    for cfg in config:
        src_ent, fld, _ttype, family, keys, unit, scope = cfg[:7]
        scope = "empire" if scope == "player" else ("world" if scope == "game" else scope)
        # OPTIONAL 8th element = an explicit cond_scope override (the conditioner's PRESENCE scope), for the case where
        # it differs from both _COND_SCOPE's default AND the deposit scope -- e.g. a NON-LOCAL building boost deposits
        # EMPIRE-wide and its gate is "the player HAS the building anywhere" (empire), not the in-city default (city).
        cond_scope = (cfg[7] if len(cfg) > 7 else None) or _COND_SCOPE.get(src_ent, scope)
        for cond_t, rec in store.table(src_ent).items():        # cond_t = the conditioner (building/civic/trait)
            node = rec.find(fld)
            if node is None:
                continue
            for ref, u, val in _boost_entries(node, keys, unit):   # ref = the HOME (the specialist that owns the output)
                items = list(val.items()) if isinstance(val, dict) else [(None, val)]
                for member, v in items:
                    fam = member if (member is not None and family in SPLIT_FAMILIES) else family
                    enabled = OrderedDict([("type", cond_t), ("scope", cond_scope)])
                    out.setdefault(ref, []).append((fam, scope, u, v, enabled))
    return out


VISION_PLOT = 100   # one open plot's sight cost -- THE vision scale (vision.md; mirrored in CvInfoKinds.h)


def scale_vision(out):
    """Lift a legacy vision value onto THE VISION SCALE (vision.md).

    Every legacy sight number was a MULTIPLE OF ONE PLOT -- a building's `iLineOfSight` of 2 meant "two plots
    further", a jungle's see-through of 1 meant "one plot's worth harder". One open plot now costs VISION_PLOT,
    so those values scale by it and land in the single unit every vision number shares.

    The point is the room underneath: a baseline of 1 left none, and the data showed exactly what that cost --
    ALL 78 obstruction-authoring features carried the identical `1`, because forest could not be made cheaper
    than jungle. A modder still writes a sensible whole number (owner); the x100 fixed point at the readJson
    boundary is none of their business.
    """
    vision = out.get("vision")
    if not isinstance(vision, dict):
        return
    for scope, node in vision.items():
        if scope not in ("unit", "plot", "city") or not isinstance(node, dict):
            continue
        for key, leaf in node.items():
            if isinstance(leaf, dict) and "flat" in leaf and isinstance(leaf["flat"], int):
                leaf["flat"] *= VISION_PLOT
            elif key == "flat" and isinstance(leaf, int):
                node[key] = leaf * VISION_PLOT

# The 14 legacy hiding METHODS -> their tag names. The method is WHAT a unit hides by, which is type-derived
# membership, so it is a TAG ([tags.md]) and the seeker's qualifier reads it as IS_<TAG>.
def hide_method_tag(invisible_type):
    """INVISIBLE_NAVAL_DISGUISE -> "navalDisguise". MECHANICAL, never a table (owner: special cases are never a
    thing -- we model them into the core set). The engine derives the same name from the same enum, so the two
    sides cannot drift and neither carries a per-type list."""
    parts = invisible_type[len("INVISIBLE_"):].lower().split("_")
    return parts[0] + "".join(w.capitalize() for w in parts[1:])
def specialunit_tag(special_type):
    """SPECIALUNIT_PEOPLE -> "people"; SPECIALUNIT_SEAPLANE -> "seaplane".

    The SPECIALUNIT_* group is pure type-derived MEMBERSHIP, which is exactly a tag ([tags.md] par.8) -- so the
    group becomes the unit's tag and a carrier's restriction reads it as the ordinary {unit: IS_<TAG>} qualifier
    ([modifier.md] par.6). MECHANICAL, never a table: the same derivation as hide_method_tag, so a new group
    needs no curator edit.
    """
    parts = special_type[len("SPECIALUNIT_"):].lower().split("_")
    return parts[0] + "".join(w.capitalize() for w in parts[1:])


# A bare "can see this method" with no graduated strength -- it beats an ordinary hider and no more.
HIDE_SEE_BASELINE = 1
# NegatesInvisibility: the method simply stops working against this seeker.
HIDE_NEGATE_STRENGTH = 10


# ---- unitcombat -> identity TAG mapping (first pass, owner-approved; worklist:
# docs/plans/structural-cleanup/unitcombat-tag-mapping.md). A tag is what a unit IS; a UnitCombat is the
# good/bad-AGAINST stat group ([engine.md] UnitCombat). The mapping is ADDITIVE -- the UnitCombat is KEPT as the
# stat source and merely also says what its carrier IS.
#
# The tag has ONE home: it is authored on the UNITCOMBAT (curate_unitcombat), and the ENGINE unions a unit's
# combat classes' tags into the unit's own at load (CvUnitInfo::deriveAtRegistryComplete, over primary +
# combatClasses -- the same walk the sizeMatters base ranks use). A unit therefore does NOT carry a baked copy:
# baking it would put the same fact in two places, so a modder editing a class's tags would leave every unit
# stale until re-curation.
#
# Only the OBVIOUS identities map; the weapon/size/species/quality/group taxonomy families stay FLAGGED as
# sizeMatters data, never forced. hero/animal/space/police/medic/missile/synthetic/diplomat/entertainer/
# bureaucrat are owner-approved NEW vocabulary.
TAG_BY_UNITCOMBAT = {
    # tech / equipment
    "UNITCOMBAT_GUN": ["gunpowder"], "UNITCOMBAT_SIEGE_GUNPOWDER": ["siege", "gunpowder"],
    "UNITCOMBAT_MOUNTED": ["mounted"], "UNITCOMBAT_MOTILITY_RIDING": ["mounted"],
    "UNITCOMBAT_MOUNT_HORSE": ["mounted"], "UNITCOMBAT_MOUNT_ELEPHANT": ["mounted"], "UNITCOMBAT_MOUNT_CAMEL": ["mounted"],
    "UNITCOMBAT_MOUNT_GIRAFFE": ["mounted"], "UNITCOMBAT_MOUNT_MAMMOTH": ["mounted"], "UNITCOMBAT_MOUNT_ZEBRA": ["mounted"],
    "UNITCOMBAT_MOUNT_DEER": ["mounted"], "UNITCOMBAT_MOUNT_BEAR": ["mounted"], "UNITCOMBAT_MOUNT_LLAMA": ["mounted"],
    "UNITCOMBAT_MOUNT_BISON": ["mounted"], "UNITCOMBAT_MOUNT_RHINO": ["mounted"],
    "UNITCOMBAT_ARMOR_VEHICULAR": ["mechanized", "armored"], "UNITCOMBAT_WHEELED": ["mechanized"],
    "UNITCOMBAT_MOTILITY_DRIVING": ["mechanized"], "UNITCOMBAT_TRACKED": ["mechanized", "armored"],
    "UNITCOMBAT_ASSAULT_MECH": ["mechanized", "armored"],
    # domain — naval
    "UNITCOMBAT_MOTILITY_NAVAL": ["naval"], "UNITCOMBAT_ARMOR_NAVAL": ["naval"], "UNITCOMBAT_NAVAL_COMBATANT": ["naval"],
    "UNITCOMBAT_WOODEN_SHIPS": ["naval"], "UNITCOMBAT_TRANSPORT": ["naval"], "UNITCOMBAT_DIESEL_SHIPS": ["naval"],
    "UNITCOMBAT_STEAM_SHIPS": ["naval"], "UNITCOMBAT_NUCLEAR_SHIPS": ["naval"], "UNITCOMBAT_DESTROYER": ["naval"],
    "UNITCOMBAT_SUBMARINE": ["naval"], "UNITCOMBAT_BATTLESHIP": ["naval"], "UNITCOMBAT_CRUISER": ["naval"],
    "UNITCOMBAT_DREADNOUGHT": ["naval"], "UNITCOMBAT_DROID_SHIPS": ["naval"], "UNITCOMBAT_GRAVITY_DRIVE_SHIPS": ["naval"],
    "UNITCOMBAT_JET_SHIPS": ["naval"], "UNITCOMBAT_LEVITATION_SHIPS": ["naval"], "UNITCOMBAT_SHOCKWAVE_SHIPS": ["naval"],
    "UNITCOMBAT_TROID_SHIPS": ["naval"],
    # domain — air
    "UNITCOMBAT_ARMOR_AIRCRAFT": ["air"], "UNITCOMBAT_MOTILITY_AERIAL": ["air"], "UNITCOMBAT_HELICOPTER": ["air"],
    "UNITCOMBAT_GUNSHIP": ["air"], "UNITCOMBAT_JET_FIGHTERS": ["air"], "UNITCOMBAT_BALLOON": ["air"],
    "UNITCOMBAT_EARLY_FIGHTERS": ["air"], "UNITCOMBAT_BOMBERS": ["air"], "UNITCOMBAT_EARLY_BOMBERS": ["air"],
    "UNITCOMBAT_SUPERSONIC_PLANES": ["air"], "UNITCOMBAT_ORBITAL_AIRCRAFT": ["air", "space"], "UNITCOMBAT_AIR_RECON": ["air", "recon"],
    # type / combat
    "UNITCOMBAT_MELEE": ["melee"], "UNITCOMBAT_ARCHER": ["archery"], "UNITCOMBAT_SIEGE": ["siege"],
    "UNITCOMBAT_SIEGE_FIELD": ["siege"], "UNITCOMBAT_SIEGE_URBAN": ["siege"], "UNITCOMBAT_SIEGE_WOODEN": ["siege"],
    "UNITCOMBAT_SIEGE_DEFENSIVE": ["siege"], "UNITCOMBAT_SIEGE_ROCKETRY": ["siege"], "UNITCOMBAT_SIEGE_ENERGY": ["siege"],
    "UNITCOMBAT_SIEGE_GATECRASHER": ["siege"], "UNITCOMBAT_RECON": ["recon"], "UNITCOMBAT_EXPLORER": ["recon"],
    # role (folds onto existing role tags; outlaw is also handled by the CRIMINAL_COMBAT block above)
    "UNITCOMBAT_CIVILIAN": ["civilian"], "UNITCOMBAT_MISSIONARY": ["missionary"], "UNITCOMBAT_WORKER": ["worker"],
    "UNITCOMBAT_TRADE": ["merchant"], "UNITCOMBAT_CRIMINAL": ["outlaw"], "UNITCOMBAT_SETTLER": ["settler"],
    "UNITCOMBAT_SEA_WORKER": ["worker"], "UNITCOMBAT_SPY": ["spy"], "UNITCOMBAT_COMBAT_WORKER": ["worker"],
    # NEW vocabulary (owner-approved 2026-07-21): hero / animal / space
    "UNITCOMBAT_HERO": ["hero"], "UNITCOMBAT_ANIMAL": ["animal"], "UNITCOMBAT_SEA_ANIMAL": ["animal"],
    # the animal LIFECYCLE states. `wild` is derivable (animal and not tamed) and minted anyway --
    # an extra tag costs nothing and a missing one does (tags.md).
    "UNITCOMBAT_TAMED": ["tamed"], "UNITCOMBAT_WILD": ["wild"],
    "UNITCOMBAT_SEA_ANIMAL_TALE": ["animal"], "UNITCOMBAT_SPACE_WORKER": ["worker", "space"],
    "UNITCOMBAT_EARLY_SPACESHIP": ["space"], "UNITCOMBAT_WORMHOLE_SPACESHIP": ["space"],
    "UNITCOMBAT_SOLAR_SAIL_SPACESHIP": ["space"], "UNITCOMBAT_ANTIMATTER_SPACESHIP": ["space"],
    "UNITCOMBAT_NUCLEAR_SPACESHIP": ["space"],
    # --- flagged-remainder second pass (owner-approved 2026-07-21, unitcombat-tag-mapping.md §FLAGGED) ---
    # map-to-existing vocabulary:
    "UNITCOMBAT_HUNTER": ["recon"], "UNITCOMBAT_STRIKE_TEAM": ["recon"],   # (STRIKE_TEAM also -> outlaw via its CRIMINAL subcombat)
    "UNITCOMBAT_EXECUTIVE": ["merchant"], "UNITCOMBAT_PACIFIST": ["civilian"],
    "UNITCOMBAT_COMMODORE": ["naval"], "UNITCOMBAT_CAPTAIN": ["naval"], "UNITCOMBAT_ROCKET_LAUNCHER": ["siege"],
    # NEW vocabulary (owner-approved 2026-07-21): police / medic / missile / synthetic / diplomat / entertainer
    "UNITCOMBAT_LAW_ENFORCEMENT": ["police"], "UNITCOMBAT_HEALTH_CARE": ["medic"],
    # bureaucrat (owner-approved): the legal / civil-service professions -- JUDGE + LAWYER (primary) and
    # GREAT_STATESMAN (sub-combat). Read by the university free-promotion conditions (POLICING1), which key on
    # this class rather than on law enforcement.
    "UNITCOMBAT_ADMINISTRATOR": ["bureaucrat"],
    "UNITCOMBAT_MISSILE": ["missile"], "UNITCOMBAT_BALLISTIC": ["missile"],
    "UNITCOMBAT_ROBOT": ["synthetic"], "UNITCOMBAT_HITECH": ["synthetic"], "UNITCOMBAT_CLONES": ["synthetic"],
    "UNITCOMBAT_NANITE": ["synthetic"], "UNITCOMBAT_NANOMORPHIC": ["synthetic"],
    "UNITCOMBAT_DIPLOMAT": ["diplomat"], "UNITCOMBAT_ENTERTAINER": ["entertainer"],
    # --- the last FLAGGED individuals (owner: "you can always have more tags, it doesn't hurt to add an extra
    # tag, even though we don't fully know what it does"). An extra tag is inert until something queries it, so
    # certainty is NOT a gate -- a surplus tag costs nothing, while a MISSING one leaves the class stuck doing
    # identifier duty and blocks the purge. Prefer existing vocabulary; mint where nothing fits.
    "UNITCOMBAT_HOVERCRAFT": ["mechanized"],      # a powered vehicle, land/sea hybrid
    "UNITCOMBAT_ATTACHE": ["diplomat"],           # military attache -- a diplomatic posting
    "UNITCOMBAT_RUFFIAN": ["outlaw"], "UNITCOMBAT_EXILE": ["outlaw"],   # criminal-type, like CRIMINAL itself
    "UNITCOMBAT_PIRATE": ["outlaw"],              # criminal at sea; the naval half comes from its ship class
    "UNITCOMBAT_THROWING": ["throwing"],          # thrown-weapon skirmisher -- ranged, but not archery
    "UNITCOMBAT_COMMANDER": ["commander"], "UNITCOMBAT_PRODIGY": ["prodigy"],
    "UNITCOMBAT_NOMAD": ["nomad"], "UNITCOMBAT_IDEA": ["idea"], "UNITCOMBAT_DOOM": ["doom"],
}


def add_tags(out, tags):
    """Append tags to out["tags"] without duplicating or clobbering.

    The block is an always-present ARRAY OF STRINGS ([tags.md]), and two independent sources write it (the
    hide-and-seek METHOD tag and the identity mapping below), so a plain assignment would silently drop
    whichever ran first -- the same clobber that cost the concealment values once already.
    """
    if not tags:
        return
    have = out.setdefault("tags", [])
    for tag in tags:
        if tag not in have:
            have.append(tag)


def collapse_hide_and_seek(out, vision, bTags):
    """Collapse the 13 per-type invisibility tables onto the `vision` family (vision.md §4).

    ONE detection type counters ONE concealment type (owner) -- a PAIRING, and the legacy type IS that pairing:
    the hider's `invisibilityIntensity{X}` and the seeker's `visibilityIntensity{X}` share a key. So the method
    becomes a TAG and both strengths become ordinary vision entries, the seeker's qualified by `IS_<TAG>`.
    Nothing new is minted: it is the same {unit: IS_<TAG>} qualifier cargo uses for what it may carry.

    ⚖ Marginal data loss is ACCEPTED (owner) -- the second reach (`visibilityIntensityRange` and its
    terrain/feature/improvement variants) goes because detection is an ADDENDUM to vision and rides the §2
    budget, and the same-tile bonus and the per-substrate conditional tables go with it. What survives is what
    the data actually uses: the pairing, and graduated strengths including the negatives (the family sums, so
    counter-detection is just a negative deposit).
    """
    tags = []
    conceal = 0
    detect = []

    # the hider: its method, and how well it hides by it
    method = vision.pop("invisible", None)
    if isinstance(method, str) and method.startswith("INVISIBLE_"):
        tags.append(hide_method_tag(method))
        conceal = max(conceal, HIDE_SEE_BASELINE)
    for typ, val in (vision.pop("invisibilityIntensity", None) or {}).items():
        if typ.startswith("INVISIBLE_") and isinstance(val, int):
            tags.append(hide_method_tag(typ))
            conceal += val

    # the seeker: which methods it answers, and how well
    seen = OrderedDict()
    for typ in (vision.pop("seeInvisible", None) or []):
        if typ.startswith("INVISIBLE_"):
            seen[typ] = HIDE_SEE_BASELINE
    for typ, val in (vision.pop("visibilityIntensity", None) or {}).items():
        if typ.startswith("INVISIBLE_") and isinstance(val, int):
            seen[typ] = seen.get(typ, 0) + val
    for typ in (vision.pop("negates", None) or []):
        if typ.startswith("INVISIBLE_"):
            seen[typ] = seen.get(typ, 0) + HIDE_NEGATE_STRENGTH
    for typ, val in seen.items():
        detect.append(OrderedDict([("value", val * VISION_PLOT),
                                   ("unit", "IS_" + hide_method_tag(typ).upper())]))

    # the second reach and the per-substrate conditional tables -- dropped with the mechanic they served
    for dead in ("visibilityIntensityRange", "visibilityIntensitySameTile", "invisibleTerrain",
                 "invisibleFeature", "invisibleImprovement", "visibleTerrainRange", "visibleFeatureRange",
                 "visibleImprovementRange", "visibleImprovement"):
        vision.pop(dead, None)

    if conceal or detect:
        node = out.setdefault("vision", OrderedDict()).setdefault("unit", OrderedDict())
        if conceal:
            node["concealment"] = OrderedDict([("flat", conceal * VISION_PLOT)])
        if detect:
            node["detection"] = detect[0] if len(detect) == 1 else detect
    if bTags:
        add_tags(out, tags)

def merge_vision(out, vision):
    """Merge a leftover `vision` dict into out["vision"] WITHOUT clobbering a scope already filled.

    A plain dict.update() replaces the whole scope sub-dict, so a base-sight write of {"unit": {"flat": N}}
    silently deleted the concealment/detection the hide-and-seek collapse had just put there. Merge per scope.
    """
    if not vision:
        return
    node = out.setdefault("vision", OrderedDict())
    for scope, value in vision.items():
        if isinstance(value, dict) and isinstance(node.get(scope), dict):
            node[scope].update(value)
        else:
            node[scope] = value

def apply_channel(families, spec, c, enabler_block=None):
    """Deposit a scope-wide modifier into its FAMILY (flat-family layout, §3): <family>.<scope>[.<member>].<unit>.
    The mapping `channel` IS the family; named valueKeys (food/gold/…) are members; a scalar has no member."""
    scope = spec.get("scope", "city")
    scope = "empire" if scope == "player" else ("world" if scope == "game" else scope)
    family, kind = spec.get("channel"), spec.get("kind", "flat")
    keys = spec.get("valueKeys")
    if isinstance(family, str) and family.endswith("PerPopulation"):
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
            if family in ("freeSpecialists", "allowedSpecialists"):
                # (A) count families (modifier.md §6.7): the specialist-type IS the leaf, value is a bare count —
                # NOT a `<targetType>.<ref>.<unit>` keyed sub-scope.
                node = families.setdefault(family, {}).setdefault(scope, {})
                node[ref] = _merge_val(node[ref], val) if ref in node else val
                continue
            leaf = (families.setdefault(family, {}).setdefault(scope, {})
                    .setdefault(target_type, {}).setdefault(ref, {}))
            leaf[kind] = _merge_val(leaf[kind], val) if kind in leaf else val
        return
    if kind == "enabler":
        if engine.text(c) not in ("1", "true", "True"):
            return
        # Per-channel `block` override (owner rulings 2026-07-02): a bespoke root block (`canTrade`/`canTradeOn`/
        # `canWorkOn`) instead of the entity default (capabilities/policies). `channel` may be a LIST — ONE legacy
        # flag emitting SEVERAL keys (bOpenBordersTrading -> openBorders + rightOfPassage; bWaterWork -> water +
        # ocean): the legacy coupling lives in DATA, never a hardcoded engine implication (capabilities.md).
        blk = spec.get("block") or enabler_block
        if blk:
            for nm in (family if isinstance(family, list) else [family]):
                families.setdefault(blk, {})[nm] = True
            return
        val = True
    elif kind == "refList":
        # Container of FK refs -> block.channel: [ids] (owner ruling 2026-07-02: TerrainTrades -> the root
        # `canTradeOn` block with REAL TERRAIN_ refs, capabilities.md — previously fell through the scalar-only
        # enabler check and emitted NOTHING, a Gate-1 data gap).
        blk = spec.get("block") or enabler_block
        ids = [engine.text(x) for x in c if engine.text(x) and engine.text(x) != "NONE"]
        if ids and blk:
            families.setdefault(blk, {})[family] = ids
        return
    elif kind == "flexArray":
        # Positional bool array -> discrete per-index capability keys (owner ruling 2026-07-01: commerceFlexible ->
        # canSet<X>Rate booleans; the positional array previously emitted NOTHING). `channelByIndex` maps the
        # array position (stringified CommerceTypes index) to its key; unmapped positions (gold: no slider) skip.
        blk = spec.get("block") or enabler_block
        by_index = spec.get("channelByIndex", {})
        for i, x in enumerate(list(c)):
            nm = by_index.get(str(i))
            if nm and blk and engine.text(x) in ("1", "true", "True"):
                families.setdefault(blk, {})[nm] = True
        return
    elif keys:
        val = engine.named_array(c, keys)
        if not val:
            return
    else:
        t = engine.text(c)
        if not engine.is_int(t) or int(t) == 0:
            return
        val = int(t)
    enabled = spec.get("enabled")              # a PREDICATE gating this deposit (§3.9) -- available to every mapping
    if isinstance(val, dict):                  # named members (food/gold/…)
        for member, v in val.items():
            if family in SPLIT_FAMILIES:       # split: the member IS the family (food.scope.unit) — no wrapper
                _deposit(families.setdefault(member, {}).setdefault(scope, {}), kind, v, enabled)
            else:                              # grouped concept: family.scope.member.unit (vicinityYield, …)
                _deposit(families.setdefault(family, {}).setdefault(scope, {}).setdefault(member, {}), kind, v, enabled)
    else:                                      # scalar family: family.scope[.member].unit
        node = families.setdefault(family, {}).setdefault(scope, {})
        member = spec.get("member")            # explicit grouped member (maintenance.distance, movement.cost, …)
        if member:
            node = node.setdefault(member, {})
        _deposit(node, kind, val, enabled)


class EntityConfig:
    """Per-entity config, mostly read from mapping/<Entity>.json (channels/cost/art/prereqs)."""
    def __init__(self, entity, cost_rename=None, grants=None, era_fn=None, extra_drop=None, map_gen=None,
                 families=None, id_rename=None, to_identity=None, requires_fn=None, art_rename=None, allowed_fn=None,
                 enabler_block=None, characteristics=None):
        m = json.load(open(os.path.join(MAPDIR, entity + ".json")))
        self.entity = entity
        # owner 2026-06-29: where this entity's `kind:"enabler"` boolean channels land — "capabilities" (tech
        # unlocks) / "policies" (civic-enacted). None (default) = the legacy top-level {scope:{enabler:true}} family.
        self.enabler_block = enabler_block
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
        # PLOT SUBSTRATE held booleans -> the `characteristics` block (json.md par.8): what the terrain / feature /
        # improvement / route IS or DOES, out of `identity`, which carries NO effects (owner). Its own block and
        # its own CHARACTERISTIC_ registry rather than the building `attributes` word -- the same key can name a
        # different mechanic per carrier (a BUILDING nukeImmune shields its city; a FEATURE one survives the blast),
        # and one shared block would have merged them.
        self.characteristics = set(characteristics or [])
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
    chars = {}
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
            apply_channel(families, cfg.channels[tag], c, cfg.enabler_block)
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
                (chars if tag in cfg.characteristics else map_gen if tag in cfg.map_gen else identity)[name] = True
        else:
            if list(c) or t:
                name = cfg.id_rename.get(tag) or engine.FIELD_RENAME.get(tag, de_i(tag))
                (chars if tag in cfg.characteristics else map_gen if tag in cfg.map_gen else identity)[name] = engine.generic(c)

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
    if chars:
        out["characteristics"] = [k for k, v in chars.items() if v]
    if map_gen:
        out["mapGeneration"] = map_gen
    if identity:
        out["identity"] = identity
    fold_text_to_identity(out)   # TEXT (emitted at root above) -> identity (json.md §7; owner 2026-07-01)
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


# ---------------------------------------------------------------------------
# THE ADDITIONS OVERLAY IS PART OF THE WRITE -- not a step to remember (owner)
# ---------------------------------------------------------------------------
# `_additions/<type>.json` is deep-merged ON TOP of the curated JSON as the final offline step, and a per-entity
# `--write` CLEARS its folder before rewriting -- so running one curator alone used to silently drop that entity's
# overlay, leaving the committed data and a fresh regen disagreeing. It happened more than once, because it relied
# on the runner remembering to re-run the overlay afterwards.
#
# ⚑ So the re-apply is hooked to the ONE act every writer performs -- clearing its folder -- and runs at process
# exit over exactly the folders this run rewrote. That is what makes skipping it UNSAYABLE rather than forbidden:
# there is no ordering to get wrong and no curator to edit when a new one is added. The merge itself stays the ONE
# implementation in `curate_additions` ([DEC-single-implementation]); this only decides WHEN it runs.
#
# Idempotent by construction (a deep-merge of the same partial is a no-op), so `curate_all`'s closing
# `curate_additions --write` still runs and still lands the same bytes. A `--sample`/dry run clears nothing, so it
# registers nothing and the overlay is not applied.
_REWRITTEN_ENTITY_DIRS = set()


def _reapply_additions_overlay():
    """Re-merge `_additions/<type>.json` into every entity folder this process rewrote."""
    if not _REWRITTEN_ENTITY_DIRS:
        return
    try:
        import curate_additions as _add
    except Exception as e:                                    # never let the overlay hook break a curate run
        print("WARNING: additions overlay NOT re-applied (%s) -- run curate_additions.py --write" % e)
        return
    for dir_path in sorted(_REWRITTEN_ENTITY_DIRS):
        type_name = os.path.basename(dir_path.rstrip(os.sep))
        af = os.path.join(_add.ADDITIONS, type_name + ".json")
        if not os.path.isfile(af):
            continue
        adds = json.load(open(af, encoding="utf-8"), object_pairs_hook=OrderedDict)
        applied = missing = 0
        for entity_id, partial in adds.items():
            ef = _add.find_entity_file(dir_path, entity_id)
            if ef is None:
                print("  ADDITIONS MISSING: %s not found under %s/" % (entity_id, type_name))
                missing += 1
                continue
            d = json.load(open(ef, encoding="utf-8"), object_pairs_hook=OrderedDict)
            _add.deep_merge(d, partial)
            with open(ef, "w", encoding="utf-8") as f:        # match the curators' exact serialization
                json.dump(d, f, indent=1, ensure_ascii=False)
            applied += 1
        print("additions overlay: %s -- %d applied, %d missing" % (type_name, applied, missing))


atexit.register(_reapply_additions_overlay)


def wipe_entity_json(dir_path, recurse=False, expected=None):
    """Drop-before-rewrite (owner ruling 2026-07-21): clear an entity folder's generated JSON before a --write so a
    type no longer emitted (dropped at the store, renamed, or merged away) does NOT linger as a stale file. PRESERVES
    any side/manifest file whose name starts with '_' (e.g. _order.json, the _additions overlay dir). recurse=True
    also clears one level of subfolders -- the era-foldered layout (buildings/<era>/, and any thin curator with eras).

    ⛔ A curator that produced NOTHING never clears. Zero entities is a curator FAILURE (no input, a moved
    source, a content-LOCKED type that no longer curates) -- never an instruction to empty the folder. Without
    this the drop-before-rewrite silently deletes hand-maintained content, which is exactly what it did to the
    370 content-LOCKED trait files."""
    if expected == 0:
        print("REFUSED to clear %s: the curator produced 0 entities (nothing to rewrite)." % dir_path)
        return
    if not os.path.isdir(dir_path):
        return
    for name in os.listdir(dir_path):
        p = os.path.join(dir_path, name)
        if os.path.isdir(p):
            if recurse:
                for fn in os.listdir(p):
                    if fn.endswith(".json") and not fn.startswith("_"):
                        os.remove(os.path.join(p, fn))
        elif name.endswith(".json") and not name.startswith("_"):
            os.remove(p)
    # This folder is about to be rewritten, so its additions overlay must land again (above).
    _REWRITTEN_ENTITY_DIRS.add(os.path.abspath(dir_path))


def main(cfg, boosts_config, out_dir, post_process=None, synthesize=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store, result = run(cfg, boosts_config, post_process=post_process)
    if synthesize:                    # whole-set hook: append SYNTHETIC entities that have no XML record of their
        synthesize(store, result)     # own, derived from the FULL store (the TECH_GAME_START root). Mutates result.
    # Skip the shells: no effect, no unlock, named by nothing (fail-closed + reference-guarded + announced).
    skip_inert(result, store, cfg.entity)
    n = len(result)
    has = lambda k: sum(1 for (o, _) in result.values() if k in o)
    STRUCT = {"type", "description", "civilopedia", "help", "quote", "strategy", "enables", "obsoletes",
              "replaces", "disables", "obsoletedBy", "provides", "requires", "allowed", "grants", "cost", "ai",
              "ui", "world", "sound", "mapGeneration", "identity", "characteristics"}
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
        wipe_entity_json(out_dir, recurse=True, expected=n)   # drop-before-rewrite (all thin curators route through here)
        for typ, (obj, era) in result.items():
            folder = os.path.join(out_dir, era) if era else out_dir
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d %s JSON files under %s" % (n, cfg.entity, os.path.relpath(out_dir, REPO)))


# ---------------------------------------------------------------------------
# INERT-ENTITY SKIP -- "curators should have mechanics to skip dead things" (owner)
# ---------------------------------------------------------------------------
# An entity that produces NO effect and unlocks NOTHING is dead weight: it is
# loaded resident, listed in the manifest, offered in build lists, and scored by
# the AI, all to do nothing. The legacy XML accumulates these because a modder
# authors a shell and the field that would have made it work is dropped, renamed,
# or never read -- so the shell survives every later pass looking plausible.
#
# The mechanism is a STRUCTURAL test plus a REFERENCE guard, and both halves are
# load-bearing:
#
#   1. INERT -- the emitted object carries no section that DOES anything. The test
#      is FAIL-CLOSED: only keys known to be effect-free make an entity droppable,
#      so a section this list has never heard of keeps the entity ALIVE. A new
#      json.md section therefore cannot silently start deleting content; the worst
#      it can do is stop a drop that would have been correct.
#   2. UNREFERENCED -- nothing anywhere names it. An entity can be inert and still
#      load-bearing: a shell whose only job is to be another entity's prereq gate,
#      an obsoletes target, an `enables` entry. Dropping one of those breaks the
#      referrer, so the scan is exhaustive (every XML record's every element, plus
#      Python and the DLL) and it runs over the handful the structural test
#      already narrowed to -- never over the whole category.
#
# A drop is ANNOUNCED, never silent (the triggers.md census discipline: authored
# data that vanishes and reports nothing is invisible on both axes at once).
#
# This is NOT the same tool as store.DROPPED_TYPES, which cuts a Type whole at the
# store because a whole SYSTEM was ruled out. That is a decision; this is a
# detection.

# Keys that never, on their own, make an entity DO anything: its own identity, what
# it costs, what it needs, and what removes it. Everything else -- every modifier
# family, every edge that adds to the tree, every provision, every classification
# block, every bespoke system section -- means the entity is alive.
_INERT_OK_KEYS = frozenset((
    "type",
    "identity", "cost", "ui", "world", "sound", "ai",   # intrinsic self-description (json.md par.7)
    "requires", "allowed", "enabled", "disabled",       # constraints ON it, not effects OF it
    "obsoletedBy", "replacedBy",                        # target-side: what removes THIS
))


# ⛔ `identity` is NOT blanket-inert. json.md par.7 is explicit that it holds "intrinsic flags/values (radii,
# classifications, capability bools, base stats)" alongside the TEXT -- so a plot feature carrying movementCost /
# nukeImmune / noImprovement is doing real work from inside identity. Only these keys are pure self-description or
# display metadata; ANY other identity key makes the entity LIVE, the same fail-closed direction as the section
# test. (Caught by FEATURE_RADIATION_CLOUD, which a blanket-identity rule would have deleted.)
_INERT_OK_IDENTITY = frozenset((
    # text
    "description", "shortDescription", "adjective", "civilopedia", "help", "quote", "strategy", "message", "text",
    # display / pedia placement
    "advisor", "visibilityPriority", "fontButtonIndex", "gridX", "gridY", "graphicalOnly", "appearance", "order",
    # metadata ABOUT the entity, producing nothing on its own
    "conquestProbability", "mapCategories", "forceNoPrereqScaling", "worth", "militaryWorth", "conscription",
))

_ID_RE = re.compile(r"^[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+$")


def is_inert(out):
    """True iff the emitted entity produces no effect and unlocks nothing.

    FAIL-CLOSED twice over: any top-level key not known to be effect-free makes it LIVE, and so does any
    `identity` key outside the pure text/display/self-metadata set."""
    if [k for k in out if k not in _INERT_OK_KEYS]:
        return False
    ident = out.get("identity")
    if isinstance(ident, dict) and [k for k in ident if k not in _INERT_OK_IDENTITY]:
        return False
    return True


def _xml_referenced_ids(store):
    """Every Type id NAMED by some XML record, excluding each record's OWN <Type>
    declaration. One pass over the whole store, cached on it."""
    idx = getattr(store, "_inert_ref_index", None)
    if idx is not None:
        return idx
    idx = set()
    for _ent, table in store.tables.items():
        for _typ, rec in table.items():
            own = rec.find("Type")
            for node in rec.iter():
                if node is own:
                    continue
                text = (node.text or "").strip()
                if text and _ID_RE.match(text):
                    idx.add(text)
    store._inert_ref_index = idx
    return idx


def _named_in_tree(typ, subdirs):
    """Is the literal id mentioned anywhere under these repo subdirectories?"""
    needle = typ.encode("utf-8")
    for sub in subdirs:
        root = os.path.join(REPO, sub)
        for dirpath, _dirnames, filenames in os.walk(root):
            for fn in filenames:
                if not fn.lower().endswith((".py", ".cpp", ".h", ".xml")):
                    continue
                try:
                    with open(os.path.join(dirpath, fn), "rb") as fh:
                        if needle in fh.read():
                            return True
                except OSError:
                    continue
    return False


def skip_inert(results, store, label, keep=()):
    """Drop every entity in `results` that is inert AND referenced by nothing.

    Mutates and returns `results`. Prints what went and, when a candidate is kept,
    WHY -- a near-miss is the interesting case, because it names an entity that
    does nothing on its own and exists only to be pointed at.
    """
    unwrap = lambda v: v[0] if isinstance(v, tuple) else v   # shared main() holds (obj, era)
    candidates = [t for t, v in results.items() if t not in keep and is_inert(unwrap(v))]
    if not candidates:
        return results
    referenced = _xml_referenced_ids(store)
    dropped, held = [], []
    for typ in candidates:
        if typ in referenced or _named_in_tree(typ, ("Assets/Python", "Sources")):
            held.append(typ)
        else:
            dropped.append(typ)
    for typ in dropped:
        del results[typ]
    if dropped:
        print("DROPPED %d inert %s (no effect, no unlock, referenced nowhere): %s"
              % (len(dropped), label, ", ".join(sorted(dropped))))
    if held:
        shown = sorted(held)
        tail = "" if len(shown) <= 12 else " (+%d more)" % (len(shown) - 12)
        print("INERT but REFERENCED, kept %d %s: %s%s"
              % (len(held), label, ", ".join(shown[:12]), tail))
    return results
