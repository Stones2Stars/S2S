#!/usr/bin/env python3
"""XML-as-DB store (#428) — the queryable foundation for the top-down JSON migration.

Loads every gameplay Info from BOTH the base XML (Assets/XML) AND the modules (Assets/Modules),
merges module overlays by Type (preservation invariant: module data is BAKED INTO core, never dropped),
and exposes per-entity tables plus a generic ENABLER reverse-index (who-enables-what), built by
inverting the prereq references. The top-down curators query this; they never re-walk the XML.

See docs/specs/enabler.md (the top-down enabler topology this reverse-index realizes).

  python3 store.py            # load + print coverage/module stats
  python3 store.py --enables TECH_LANGUAGE   # show what a type enables
"""
import argparse
import copy
import glob
import os
import re
import xml.etree.ElementTree as ET
from collections import OrderedDict

import engine  # same dir: strip_ns, text, is_int, keyed_entries, named_array, unit_of, generic, ...

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
XML_DIR = os.path.join(REPO, "Assets", "XML")
MOD_DIR = os.path.join(REPO, "Assets", "Modules")

# ARCHIVED curator INPUT. `SourceArchive/Assets/**` mirrors the two live roots above, and holds legacy XML that was
# removed from `Assets/` once its JSON landed. It is read HERE and nowhere else: the archive is curator input only,
# never a game load path ([DEC-no-xml-into-game]) and never a source of engine code (the red-ratchet ban is on
# reviving a `CvXInfo` from `SourceArchive/Infos/`, a different thing entirely).
# ⚠ The archive roots are searched ALONGSIDE the live ones, not instead of them, so an entity whose XML is still
# live is unaffected; only a category whose XML lives solely in the archive is served from it.
ARCHIVE_XML_DIR = os.path.join(REPO, "SourceArchive", "Assets", "XML")
ARCHIVE_MOD_DIR = os.path.join(REPO, "SourceArchive", "Assets", "Modules")

# Module sub-paths EXCLUDED from the migration — confirmed CASE BY CASE against the MLF authority
# (`Assets/Modules/MLF_CIV4ModularLoadingControls.xml`, config Modules_Main_1 + the nested per-module MLFs), owner
# 2026-06-15. The MLF `bLoad` flag is the truth for what the GAME loads. Verdicts (per-module sweep):
#   INCLUDE (bLoad=1): Cultures, Pepper2000, Thunderbrd (all under its loaded `Traits` sub), Alt_Timelines (all
#     punk subs =1), NotSoGood.
#   EXCLUDE (bLoad=0): zWIP; Bad_Karma (top-level on, but ALL content subs — Building_Meltdown / Fantasy[/
#     KillerRabbit] / War_Of_The_Worlds / Locusts_Normal / Andromeda_Strain / Bandits_and_Pirates — are bLoad=0,
#     so nothing it carries loads; owner: "bad_karma should be removed").
#   EXCLUDE — P2K_Multimaps_Test (bLoad=0): RESOLVED 2026-06-15 — its 92 "space" units are a 100% DUPLICATE of
#     Pepper2000 units (bLoad=1, loaded); ZERO are P2K-unique (owner's hunch: "someone copied p2k's units into the
#     main mod" — confirmed, into Pepper2000). So the space content is fully in-game via Pepper2000 regardless;
#     excluding the dead duplicate makes the migration use the game's canonical Pepper2000 definitions instead of
#     merging them with the disabled copy. Net roster change: zero. (Owner: "yes, do that.")
# Match is on the normalised (forward-slash, lowercase) path.
EXCLUDED_MODULE_SUBPATHS = ["/modules/zwip/", "/modules/bad_karma/", "/modules/p2k_multimaps_test/"]

# entity record-element -> filename glob (matched under BOTH Assets/XML and Assets/Modules).
ENTITIES = {
    "TechInfo":        "*CIV4TechInfos.xml",
    "BuildingInfo":    "*CIV4BuildingInfos.xml",
    "UnitInfo":        "*CIV4UnitInfos.xml",
    "BuildInfo":       "*CIV4BuildInfos.xml",
    "SpecialistInfo":  "*CIV4SpecialistInfos.xml",
    "ImprovementInfo": "*CIV4ImprovementInfos.xml",
    "RouteInfo":       "*CIV4RouteInfos.xml",
    "CivicInfo":         "*CIV4CivicInfos.xml",
    "TraitInfo":         "*CIV4TraitInfos.xml",
    "ReligionInfo":      "*CIV4ReligionInfo.xml",
    "BonusInfo":         "*CIV4BonusInfos.xml",
    "CorporationInfo":   "*CIV4CorporationInfo.xml",
    "ProjectInfo":       "*CIV4ProjectInfo.xml",
    "ProcessInfo":       "*CIV4ProcessInfo.xml",
    "PromotionInfo":     "*CIV4PromotionInfos.xml",
    "PromotionLineInfo": "*CIV4PromotionLineInfos.xml",
    "UnitCombatInfo":    "*CIV4UnitCombatInfos.xml",
    "SpecialUnitInfo":   "*CIV4SpecialUnitInfos.xml",   # #33, rides the Unit pass (cargo-load rules + combat deposits)
    "HeritageInfo":      "*HeritageInfos.xml",
    "SpecialBuildingInfo": "*CIV4SpecialBuildingInfos.xml",
    # ART DEFINES -- curator INPUT ONLY, read to DERIVE a building's on-map presence (json.md §7). The defines
    # are the ART carve-out: nothing here migrates or emits them, and the game keeps resolving them through
    # ARTFILEMGR. Reading one HERE is the ordinary curator-input relationship every legacy XML has
    # ([DEC-no-xml-into-game] bans loading a replaced info's XML into the GAME, never reading one offline).
    "BuildingArtInfo":   "*CIV4ArtDefines_Building.xml",
    # config/global entities — participate in NO enabler edges (enable nothing); registered for
    # module-merged table access by their curators.
    "HandicapInfo":      "*CIV4HandicapInfo.xml",
    "GameSpeedInfo":     "*CIV4GameSpeedInfo.xml",
    "EraInfo":           "*CIV4EraInfos.xml",
    "WorldInfo":         "*CIV4WorldInfo.xml",   # map-size config (#430 item 15); enables nothing
    # Config / category entities (the former "POCO" batch). The 2026-06-14 PM audit (plan doc "AUDIT DONE")
    # proved only CivicOption is a pure text+identity holder; BonusClass/Hurry/Victory/PromotionLine carry real
    # config/gameplay and get proper curators. Registered here for table access.
    "BonusClassInfo":    "*CIV4BonusClassInfos.xml",
    "CivicOptionInfo":   "*CIV4CivicOptionInfos.xml",
    "HurryInfo":         "*CIV4HurryInfo.xml",
    "VictoryInfo":       "*CIV4VictoryInfo.xml",
    "CultureLevelInfo":  "*CIV4CultureLevelInfo.xml",   # per-city-level config; enables buildings (PrereqCultureLevel)
    "PropertyInfo":      "*CIV4PropertyInfos.xml",       # defines the PROPERTY_* channels (crime/education/…)
    "VoteInfo":          "*CIV4VoteInfo.xml",            # diplo-vote resolutions (config for the voting subsystem)
    "CivilizationInfo":  "*CIV4CivilizationInfos.xml",   # game-start grants + per-civ policy/art (source entity)
    "LeaderHeadInfo":    "*CIV4LeaderHeadInfos.xml",     # AI personality/diplo params (-> ai) + leader trait grants
    "TerrainInfo":       "*CIV4TerrainInfos.xml",        # plot-leaf TARGET (forms the plot's base yields); enables nothing
    "FeatureInfo":       "*CIV4FeatureInfos.xml",        # plot-leaf TARGET / deliveryguy (modifies the plot it sits on); enables nothing
    "YieldInfo":         "*CIV4YieldInfos.xml",          # the 3 yields' config; source of the hills/peak/river plot-yield deltas (migrated onto terrains)
    "OutcomeInfo":       "*CIV4OutcomeInfos.xml",        # the OUTCOME_* gate/tier infos (name/message/requirements/odds) -- #430 outcome-subsystem migration
}

# Enabler reverse-index config: a record of <sourceEntity> that references type X via <fieldPath> means
# "X ENABLES this record" -> enables[X][bucket].add(record). bucket = the kind of thing X enables.
# This is generic: tech->building, tech->tech, AND building->building (the Culture/national-wonder
# gating the punk buildings), bonus->building, etc. all flow through the same index.
PREREQ_FIELDS = [
    ("TechInfo",     "OrPreReqs/PrereqTech",                "techs"),
    ("TechInfo",     "AndPreReqs/PrereqTech",               "techs"),
    ("BuildingInfo", "PrereqTech",                          "buildings"),
    ("BuildingInfo", "TechTypes/PrereqTech",                "buildings"),
    ("UnitInfo",     "PrereqTech",                          "units"),
    ("UnitInfo",     "TechTypes/PrereqTech",                "units"),
    ("BuildInfo",    "PrereqTech",                          "builds"),
    # building -> building (generic in-city building prereqs; punk buildings use these too):
    ("BuildingInfo", "PrereqInCityBuildings/BuildingType",  "buildings"),
    ("BuildingInfo", "PrereqAmountBuildings/BuildingType",  "buildings"),
    ("BuildingInfo", "PrereqOrBuildings/BuildingType",      "buildings"),
    # building -> unit (a unit requires a building -> the building enables that unit):
    ("UnitInfo",     "PrereqAndBuildings/BuildingType",     "units"),
    ("UnitInfo",     "PrereqOrBuildings/BuildingType",      "units"),
    # bonus -> building/unit. This is the ENABLE half of the Culture chain: a Culture national wonder
    # GRANTS a bonus via ExtraFreeBonuses (a building->bonus provision, captured at building curation),
    # and that bonus ENABLES the punk buildings via PrereqAndBonus (<Bonus>) / PrereqOrBonuses.
    ("BuildingInfo", "Bonus",                "buildings"),   # PrereqAndBonus (direct <Bonus> child)
    ("BuildingInfo", "PrereqBonuses/Bonus",  "buildings"),   # PrereqOrBonuses
    ("BuildingInfo", "VicinityBonus",        "buildings"),   # singular AND vicinity
    ("BuildingInfo", "RawVicinityBonus",     "buildings"),   # singular AND raw-vicinity
    ("BuildingInfo", "PrereqVicinityBonuses/VicinityBonus",    "buildings"),  # OR vicinity list
    ("BuildingInfo", "PrereqRawVicinityBonuses/VicinityBonus", "buildings"),
    # units use the <BonusType> spelling, NOT <Bonus> (using <Bonus> was a building-tag bug -> empty enables.units):
    ("UnitInfo",     "BonusType",                       "units"),   # PrereqAndBonus
    ("UnitInfo",     "PrereqBonuses/BonusType",         "units"),   # PrereqOrBonuses
    ("UnitInfo",     "VicinityBonusType",               "units"),   # vicinity AND
    ("UnitInfo",     "PrereqVicinityBonuses/BonusType", "units"),   # vicinity OR
    # NB: CorporationInfo PrereqBonuses/PrereqBuildings are NOT enabler edges (v0.3, owner 2026-06-15): a corp is
    # GENERATED by its tech (TechPrereq below); its prereq bonuses are the per-bonus OUTPUT scaling basis (read on
    # the corp as `per:{anyOf:[…]}`, curate_corporation) + the HQ FOUND-requirement (authored on the
    # FoundsCorporation HQ building at the Building pass). So they do not invert to bonus/building.enables.corporations.
    # corp -> building: a building gated by an active corporation (PrereqCorporation) -> corp.enables.buildings
    ("BuildingInfo",    "PrereqCorporation",            "buildings"),
    # religion -> building/unit: a building/unit gated by the city having a specific religion (GOM_RELIGION
    # construct/train requirement, isHasReligion) -> religion.enables.buildings/units (parallel to PrereqCorporation).
    ("BuildingInfo",    "PrereqReligion",               "buildings"),
    ("UnitInfo",        "PrereqReligion",               "units"),
    # culture-level -> building: a building gated by the city's culture level (PrereqCultureLevel) ->
    # cultureLevel.enables.buildings. CultureLevel is the CONDITIONER (a level you must HAVE), so it inverts onto
    # the level (owned by CultureLevel, a config entity migrated before the Building monster).
    ("BuildingInfo",    "PrereqCultureLevel",           "buildings"),
    # civic -> building/unit: a building/unit gated by an active civic (PrereqCivic, direct or inside the
    # And/Or containers) -> civic.enables.buildings/units (parallel to PrereqReligion/PrereqCorporation; the
    # And-vs-Or composition collapses in the display reverse-index, by design — the real gate stays in C++).
    ("BuildingInfo",    "PrereqCivic",                  "buildings"),
    ("BuildingInfo",    "PrereqAndCivics/PrereqCivic",  "buildings"),
    ("BuildingInfo",    "PrereqOrCivics/PrereqCivic",   "buildings"),
    ("UnitInfo",        "PrereqCivic",                  "units"),
    ("UnitInfo",        "PrereqAndCivics/PrereqCivic",  "units"),
    ("UnitInfo",        "PrereqOrCivics/PrereqCivic",   "units"),
    # trait -> trait + tech -> trait: a developing-leaders trait is enabled by its prereq trait(s) and/or a
    # tech. Inverted to top-down `enables.traits` on the prereq trait / tech (the dependent trait drops these
    # fields). OR-pair is schema-present/data-empty today; the trait's PromotionLine carries a second tech gate
    # (owned by PromotionLineInfo's own PrereqTech edge above). #428 Trait curation (curate_trait.py).
    ("TraitInfo",    "TraitPrereq",     "traitsAnd"),   # single AND prereq -> getPrereqTrait (bucket-split keeps AND vs OR distinct; reverse-mapped at load)
    ("TraitInfo",    "TraitPrereqOr1",  "traitsOr"),     # OR pair -> getPrereqOrTrait1/2
    ("TraitInfo",    "TraitPrereqOr2",  "traitsOr"),
    ("TraitInfo",    "PrereqTech",      "traits"),        # tech->trait prereq (on the prereq TECH's enables) -> getPrereqTech
    ("RouteInfo",    "BonusType",                       "routesAnd"),   # single AND prereq -> distinct bucket (getPrereqBonus); reverse-mapped at load
    ("RouteInfo",    "PrereqOrBonuses/BonusType",       "routes"),      # OR-list -> getPrereqOrBonuses
    ("BuildInfo",    "PrereqBonusTypes/PrereqBonusType", "builds"),
    ("PromotionInfo", "PrereqBonusTypes/BonusType",     "promotions"),
    ("PromotionInfo", "PrereqPlotBonusTypes/PrereqPlotBonusType", "promotions"),
    # promotion -> promotion chains (owner 2026-07-17): the PromotionPrereq(Or) chains store-invert onto the
    # PREREQ promotion's enables.promotions -- holding the prereq PROPOSES the successor (OR is native to
    # multiple enabling edges). The earlier drop's "line+priority carries it" premise failed: 74 chained promos
    # have NO line (HEROIC/LEADER family) + every cross-line prereq was uncovered, and with no inbound edge the
    # root synthesis start-enabled them all (the autogyro-Heroic find). Same-line tier order stays doubly
    # gated by the CvUnit line+priority succession check.
    ("PromotionInfo", "PromotionPrereq",    "promotions"),
    ("PromotionInfo", "PromotionPrereqOr1", "promotions"),
    ("PromotionInfo", "PromotionPrereqOr2", "promotions"),
    # civic / religion gated by a founding tech (TechPrereq; "NONE" = ungated, filtered out):
    ("CivicInfo",    "TechPrereq",           "civics"),
    ("ReligionInfo", "TechPrereq",           "religions"),
    # other whole categories a tech enables (each verified tech-gated against a C++ consumer, wf_7bbae202):
    ("CorporationInfo",    "TechPrereq",                             "corporations"),
    ("ProjectInfo",        "TechPrereq",                             "projects"),
    # project -> project: a project requiring N of another (PrereqProjects/iNeeded; the SS_* parts need Apollo).
    # The count (iNeeded, all 1 today) is NOT carried by the set-based index — a count>1 would need a
    # count-bearing edge (a threshold minority, like OR/NOT); presence is faithful for current data.
    ("ProjectInfo",        "PrereqProjects/PrereqProject/ProjectType", "projects"),
    ("ProcessInfo",        "TechPrereq",                             "processes"),
    ("PromotionInfo",      "TechPrereq",                             "promotions"),
    ("PromotionLineInfo",  "PrereqTech",                             "promotionLines"),
    ("HeritageInfo",       "PrereqTech",                             "heritages"),
    # heritage -> heritage succession (a Taxon tier requires a Folklore tier via PrereqOrHeritage/<Type>):
    ("HeritageInfo",       "PrereqOrHeritage/Type",                  "heritages"),
    ("SpecialBuildingInfo", "TechPrereq",                            "specialBuildings"),
    ("SpecialBuildingInfo", "TechPrereqAnyone",                      "specialBuildings"),
    ("ImprovementInfo",    "PrereqTech",                             "improvements"),
    ("BuildInfo", "FeatureStructs/FeatureStruct/PrereqTech",         "builds"),
    ("BuildInfo", "TerrainStructs/TerrainStruct/PrereqTech",         "builds"),
    ("BonusInfo",          "TechReveal",                             "bonuses"),
    ("BonusInfo",          "TechCityTrade",                          "bonuses"),
]

# --- The `enables` FAMILY (enabler-spec §5/§6): four source-side, forward-read-from-HAS objects that
# together GENERATE the candidate frontier: CAN GET = enables - (disables u obsoletes u replaces) over HAS.
# enables = ADD (constructive); obsoletes/replaces/disables = SUBTRACT (destructive). All four are built by
# inverting the source XML so the resulting index is keyed by the thing you HAVE and points forward at what
# that thing enables/removes — never an upward "who-affects-me" query (that view is the cold-path pedia index).

# Obsolete edges — top-down: a tech OBSOLETES these targets (reverse of <entity>.ObsoleteTech).
# Buildings MIGRATED to TARGET-side `obsoletedBy` (owner ruling 2026-06-22): the building authors its own
# obsoleting tech (ObsoleteTech) + superseding building (ObsoletesToBuilding) directly; the cascade builds the
# reverse map at load. So buildings are no longer inverted here (units/builds/bonuses/… still source-side).
OBSOLETE_FIELDS = [
    ("UnitInfo",     "ObsoleteTech", "units"),
    ("BuildInfo",    "ObsoleteTech", "builds"),
    ("BonusInfo",    "TechObsolete", "bonuses"),
    ("CorporationInfo", "ObsoleteTech", "corporations"),  # latent (no corp sets it today); hardens the edge
    ("PromotionInfo", "ObsoleteTech", "promotions"),      # a tech can obsolete a promotion (#428 #28; owner-approved)
]

# Replace edges — `replaces` stays a DEFINED concept but is UNUSED (owner 2026-06-23, engine-verified). Legacy
# `ReplacementBuildings` on predecessor A (A lists [B] that supersede it) is NOT removal: the engine DISABLES A
# (setDisabledBuilding, CvCity.cpp:14413) while B is present and re-enables it when B is gone — reversible DORMANCY,
# never "A removed" (the old "CvCity:14465 removed when hasBuilding(B)" note was wrong). So the building curator
# mirrors it as A.requires.operate.dormant: [B] (target-side, no inversion), NOT a `replaces`/`replacedBy` edge.
# `replaces` keeps its enables-family slot for a future genuine-removal source; there is none today.
REPLACE_FIELDS = []

# Disable edges — DESTRUCTIVE reversible BANS (enabler-spec §5). LATENT: there is NO live generic XML source
# today. The only converted `disables` is per-civ `CivilizationInfo.DisableTechs` (CvPlayer.cpp:8266 inside
# canEverResearch) — a load-stable per-civ research OVERRIDE, re-homed to loadPrune/`obsoletes`-shaped at the
# Civilization pass, NOT a reversible empire-scope ban. Real player-law bans (slavery/prostitution/…) are
# interim-implemented as pseudobuilding/autobuild with disable/enable (existing machinery, kept; promoted to
# the empire/team-scope `disables`-building tier later). Kept here as a defined-but-empty edge so the four
# `enables`-family objects are structurally distinct and a real source is a one-line add when it lands.
DISABLE_FIELDS = []


# --- CUT SYSTEMS (owner ruling 2026-07-21) -----------------------------------------------------------------
# The astrological-influence + ancient-way trait/wonder/promotion system is unfleshed past the Renaissance, so it
# is CUT WHOLE and documented for reimplementation (docs/plans/parked/astrological-ancient-way-traits.md). The
# cut is applied HERE, at the store, so it is invisible to EVERY curator AND to the enable/obsolete inversion below
# -- i.e. the granting wonders' `PrereqTech` never inverts into a tech's `enables`, so no dangling FK is produced
# (a per-curator output drop would leave exactly that dangle). Art defines are KEPT (owner). The source XML is left
# in place as the reimplementation reference; git history + the park doc are the record.
DROPPED_TYPE_PREFIXES = (
    "TRAIT_ASTROLOGICAL_INFLUENCE_OF_", "TRAIT_ANCIENT_WAY_OF_THE_",
    "BUILDING_ASTROLOGICAL_INFLUENCE_OF_", "BUILDING_ANCIENT_WAY_OF_THE_",
    "PROMOTION_INFLUENCE_OF_", "PROMOTION_WAY_OF_THE_",
)
DROPPED_TYPES = frozenset((
    "SPECIALBUILDING_GROUP_ASTROLOGICAL_INFLUENCES",
    "SPECIALBUILDING_GROUP_ANCIENT_WAYS",
))


def is_dropped_type(typ):
    """A Type cut whole at the store (see CUT SYSTEMS above) -- no curator or inversion ever sees it."""
    return typ in DROPPED_TYPES or typ.startswith(DROPPED_TYPE_PREFIXES)


def complex_variant_id(baseType):
    """The COMPLEX-set id of a trait, derived from the SIMPLE id -- the ONE definition, used by both callers.

    The prefix STATES THE SET ([naming.md]: `TRAIT_` is a simple trait, `TRAIT_COMPLEX_` a complex one), and the
    STEM is the base trait's own name (owner: "use the simple names as base, because that is the base of the
    names"). Two callers must agree on it or the sets drift: the replacement variant keyed at `_load`, and the
    re-key of a complex-ONLY record in `trait_rekey`.

    ⚑ Deriving it -- rather than reading an authored id -- is what makes `complex/` a SUPERSET of `simple/` BY ID,
    so a stored trait resolves into the active set by inserting `COMPLEX_` and nothing else.
    """
    if not baseType.startswith("TRAIT_") or baseType.startswith("TRAIT_COMPLEX_"):
        return baseType
    return "TRAIT_COMPLEX_" + baseType[len("TRAIT_"):]


def _refs(rec, path):
    """Collect referenced Type-strings at fieldPath ('PrereqTech' or nested 'TechTypes/PrereqTech')."""
    nodes = [rec]
    for part in path.split("/"):
        nxt = []
        for n in nodes:
            nxt.extend(n.findall(part))
        nodes = nxt
    out = []
    for n in nodes:
        t = engine.text(n)
        if t:
            out.append(t)
        else:  # a container element: take its children's text
            for c in n:
                ct = engine.text(c)
                if ct:
                    out.append(ct)
    return out


def _merge(base, mod):
    """Module overlay onto a base record: a tag present in the module replaces the base's same-tag
    child(ren); tags absent from the module are left untouched; new Types are added by the caller.
    v1 approximation of C2C copyNonDefaults (scalar override + add). List-merge/append fidelity and
    exact module load-order are refinements (flagged); content is preserved (no Type or value lost)."""
    for child in list(mod):
        if child.tag == "Type":
            continue
        for ex in base.findall(child.tag):
            base.remove(ex)
        base.append(child)


class Store:
    def __init__(self, entities=None):
        self.tables = {}      # entity -> OrderedDict{Type: merged element}
        self.provenance = {}  # entity -> {Type: [relpath, ...]}  (module attribution)
        self.module_added = {}  # entity -> set(Type) first defined in a module
        self.enables = {}     # referencedType -> {bucket: set(referrerType)}  (CONSTRUCTIVE: ADD to CAN GET)
        self.obsoletes = {}   # referencedType -> {bucket: set(referrerType)}  from ObsoleteTech edges (SUBTRACT)
        self.replaces = {}    # successorType  -> {bucket: set(predecessorType)} from ReplacementBuildings (SUBTRACT)
        self.disables = {}    # bannerType     -> {bucket: set(bannedType)}  reversible bans — LATENT today (SUBTRACT)
        self.replacements = {}  # entity -> {baseType: {"replacement": replId, "condition": <elem>}}
        self._culture_bonuses = None
        for ent, glb in (entities or ENTITIES).items():
            self._load(ent, glb)
        self._index()

    def _files(self, glb):
        found = []
        for base in (XML_DIR, MOD_DIR, ARCHIVE_XML_DIR, ARCHIVE_MOD_DIR):
            found.extend(glob.glob(os.path.join(base, "**", glb), recursive=True))
        out = []
        for f in found:
            norm = f.replace("\\", "/").lower()
            if "schema" in norm or any(ex in norm for ex in EXCLUDED_MODULE_SUBPATHS):
                continue
            out.append(f)
        return sorted(out)

    def _load(self, ent, glb):
        table, prov, modadd = OrderedDict(), {}, set()
        repl = self.replacements.setdefault(ent, {})
        for path in self._files(glb):
            try:
                root = engine.strip_ns(ET.parse(path).getroot())
            except ET.ParseError:
                continue
            rel = os.path.relpath(path, REPO).replace("\\", "/")
            is_module = (os.sep + "Modules" + os.sep) in path
            for rec in root.iter(ent):
                typ = engine.text(rec.find("Type"))
                if not typ:
                    continue
                if is_dropped_type(typ):
                    continue   # CUT SYSTEM (see DROPPED_TYPE_PREFIXES) -- invisible to every curator + the inversion
                # Conditional replacement (AIAndy's CvInfoReplacements): a record carrying <ReplacementID> is NOT
                # a module override of `typ`. The engine keeps it apart and swaps it in FOR `typ` only when the
                # <ReplacementCondition> holds — read as a FRESH full Info, no base merge
                # (CvXMLLoadUtilitySet.cpp:1587-1604; CvInfoReplacements::updateReplacements). So re-key it under
                # the ReplacementID as its OWN Type and record the base -> replacedBy edge; the base stays clean.
                # (Only TraitInfo populates this today; the handling is generic for Building/Unit later.)
                rid_node = rec.find("ReplacementID")
                rid = engine.text(rid_node) if rid_node is not None else ""
                if rid and rid != "NONE" and not typ.startswith("TRAIT_"):
                    # ⛔ A NON-TRAIT replacement keeps its AUTHORED ReplacementID as its own Type -- the
                    # CULTURELEVEL_ALT_POOR shape json.md §9 specs (`replacedBy`, the conditional whole-entity
                    # swap), and what the committed data carries. The trait re-key below must not reach it:
                    # `complex_variant_id` answers a NON-trait id UNCHANGED, so falling through MERGED the gated
                    # variant into its base and silently dropped the gate (measured: a module's complex
                    # PROMOTION_AGGRESSIVE variant landed +20 attack unconditionally on the base promotion).
                    # Only TRAITS were ruled into derived-id folders; everything else stays authored-id.
                    repl[typ] = {"replacement": rid, "condition": rec.find("ReplacementCondition")}
                    for t in ("ReplacementID", "ReplacementCondition"):
                        e = rec.find(t)
                        if e is not None:
                            rec.remove(e)
                    tn = rec.find("Type")
                    if tn is not None:
                        tn.text = rid
                    key = rid
                    if key in table:
                        _merge(table[key], rec)
                    else:
                        table[key] = rec
                        if is_module:
                            modadd.add(key)
                    prov.setdefault(key, []).append(rel)
                    continue
                if rid and rid != "NONE":
                    # ⛔ THE VARIANT'S ID IS DERIVED FROM THE BASE IT REPLACES, NEVER FROM THE AUTHORED
                    # <ReplacementID> (owner: use the simple names as base -- "that is the base of the names").
                    # The authored id is whatever stem the modder felt like (TRAIT_NOMAD names TRAIT_COMPLEX_NOMADIC,
                    # TRAIT_FANATICAL names TRAIT_COMPLEX_ZEALOUS), and it is NOT EVEN UNIQUE: TRAIT_EXCESSIVE and
                    # TRAIT_EXCESSIVE1 both name TRAIT_COMPLEX_EXCESSIVE, so keying on it sent two different rungs
                    # to one key and the `_merge` below folded the rung-1 payload into the base's -- the complex
                    # set simply lost a rung, with nothing reporting it.
                    # ⚑ None of that was visible while the engine hot-swapped these in memory: the id was only ever
                    # FOLLOWED, never read, so a duplicate name cost nothing. It costs a whole record the moment the
                    # two sets are separated BY ID.
                    # ⚑ Deriving from the base makes the id unique by construction (base Types are unique) and makes
                    # `complex/` a true SUPERSET of `simple/` BY ID -- which is what lets a save resolve into the
                    # active set by pure prefix insertion, with no table and no does-it-exist test
                    # ([modifier.md] par.4; `sm_resolveStoredType`).
                    key = complex_variant_id(typ)
                    repl[typ] = {"replacement": key, "condition": rec.find("ReplacementCondition")}
                    for t in ("ReplacementID", "ReplacementCondition"):
                        e = rec.find(t)
                        if e is not None:
                            rec.remove(e)
                    tn = rec.find("Type")
                    if tn is not None:
                        tn.text = key
                else:
                    key = typ
                if key in table:
                    _merge(table[key], rec)
                else:
                    table[key] = rec
                    if is_module:
                        modadd.add(key)
                prov.setdefault(key, []).append(rel)

        # ⛔ A REPLACEMENT IS AN OVERLAY ON ITS BASE, NOT A STANDALONE RECORD (owner) -- so the variant is built
        # as BASE + the replacement's own tags on top, exactly as an ordinary module override is.
        # ⚑ THE LEGACY ENGINE DOES NOT DO THIS, AND THAT IS THE BUG BEING FIXED, NOT A DIVERGENCE TO AVOID.
        # `CvXMLLoadUtilitySet::SetGlobalClassInfo` handles both cases feet apart: a plain module override runs
        # `pClassInfo->copyNonDefaults(aInfos[uiExistPosition])` and so inherits the base, while a record carrying
        # a <ReplacementID> reaches `addReplacement` with a FRESH `new T()` read from its own XML alone. The merge
        # in the replacement path (`copyNonDefaults(pExisting->getInfo())`) stacks a SECOND replacement of the same
        # id onto an earlier one -- module-on-module, never replacement-on-base -- so rung 1 inherits nothing while
        # every rung above it inherits. `CvInfoReplacements::updateReplacements` then swaps the object in whole.
        # ⚑ THE DATA PROVES THE AUTHORING INTENT AGAINST THE ENGINE: 304 of 305 complex trait records carry NO
        # ShortDescription, while 0 of 65 simple ones lack it -- and the single exception is the base-FILLED
        # TRAIT_COMPLEX_BARBARIAN. A mandatory field missing from 100% of the whole-swapped records and 0% of the
        # base-filled ones is not a design choice; nobody authors a 54k-character redefinition and omits its name.
        # The replacements were written expecting the base underneath, and the engine never supplied it.
        for baseType, info in repl.items():
            key = info["replacement"]
            if baseType not in table or key not in table:
                continue
            merged = copy.deepcopy(table[baseType])
            _merge(merged, table[key])          # the replacement's tags win; the base's survive where it is silent
            typeNode = merged.find("Type")
            if typeNode is not None:
                typeNode.text = key             # _merge skips Type, so restore the VARIANT's identity
            table[key] = merged

        self.tables[ent] = table
        self.provenance[ent] = prov
        self.module_added[ent] = modadd

    def _index(self):
        self.enables = self._build_index(PREREQ_FIELDS)
        self.obsoletes = self._build_index(OBSOLETE_FIELDS)
        self.replaces = self._build_index(REPLACE_FIELDS)
        self.disables = self._build_index(DISABLE_FIELDS)  # empty today (no live source); see DISABLE_FIELDS
        self._inherit_group_obsoletes()

    def _inherit_group_obsoletes(self):
        """SpecialBuilding GROUP obsoletion inheritance (data-model §7, the building-group wrangle): a MEMBER
        building inherits its group's ObsoleteTech. The cascade reads tech.obsoletes.buildings, so expand each
        SpecialBuilding's ObsoleteTech onto its member buildings (those whose SpecialBuildingType is that group).
        1 group today (SPECIALBUILDING_MONASTERY -> TECH_MODERN_PHYSICS); hardens the edge for future groups.
        (The group TechPrereq inheritance is curator-side, curate_building.requires_building.)"""
        sb_obs = {}
        for sb, rec in self.tables.get("SpecialBuildingInfo", {}).items():
            ot = engine.text(rec.find("ObsoleteTech"))
            if ot and ot != "NONE":
                sb_obs[sb] = ot
        if not sb_obs:
            return
        for typ, rec in self.tables.get("BuildingInfo", {}).items():
            sb = engine.text(rec.find("SpecialBuildingType"))
            if sb in sb_obs:
                self.obsoletes.setdefault(sb_obs[sb], {}).setdefault("buildings", set()).add(typ)

    def _build_index(self, fields):
        idx = {}
        for ent, fld, bucket in fields:
            for typ, rec in self.tables.get(ent, {}).items():
                for ref in _refs(rec, fld):
                    if not ref or ref == "NONE":
                        continue
                    idx.setdefault(ref, {}).setdefault(bucket, set()).add(typ)
        return idx

    # --- query API ---
    def table(self, ent):
        return self.tables.get(ent, OrderedDict())

    def get(self, ent, typ):
        return self.tables.get(ent, {}).get(typ)

    def trait_rung_zero(self):
        """{TRAIT_COMPLEX_X: TRAIT_COMPLEX_X1} -- the RUNG-0 demotion, defined ONCE here.

        ⛔ A COMPLEX GAME HAS NEVER USED RUNG 0 OF ANY TRAIT -- a line is 1 -> 2 -> 3 (owner,
        [DEC-trait-sets-separate]), so the complex set carries no un-digited record for a line that has rungs.
        Anything that NAMED that record -- a leaderhead's assignment, a tech's gate edge -- has to name rung 1
        instead, or it points at an id nothing defines.

        ⚠ It is deliberately SEPARATE from `trait_rekey` even though both rewrite a trait id: that one answers
        WHICH SET a record belongs to, this one answers WHICH RUNG. Folding them would make a record's set
        depend on its ladder position.
        ⚑ It lives on the store for the same reason the re-key does -- a trait id is named from several
        curators, and one definition is what stops an edge naming an id no record defines
        ([enabler.md]: a severed rung gate is silent).
        """
        if getattr(self, "_trait_rung_zero_cache", None) is not None:
            return self._trait_rung_zero_cache
        final_ids = set(self.trait_rekey().values())
        table = self.tables.get("TraitInfo", {})
        repl = self.replacements.get("TraitInfo", {})
        rid_to_base = dict((v["replacement"], b) for b, v in repl.items())
        for typ in table:
            if typ in rid_to_base:
                final_ids.add(complex_variant_id(rid_to_base[typ]))
            elif typ.startswith("TRAIT_COMPLEX_"):
                final_ids.add(typ)
        ladder_stems = set(re.sub(r"\d+$", "", i) for i in final_ids if re.search(r"\d+$", i))
        out = {}
        for stem in ladder_stems:
            if stem in final_ids or True:          # the base need not itself be emitted to be NAMED by an edge
                if stem + "1" in final_ids:
                    out[stem] = stem + "1"
        self._trait_rung_zero_cache = out
        return out

    def trait_rekey(self):
        """{oldTraitId: newTraitId} -- the COMPLEX-SET re-key, defined ONCE here.

        EVERY record in the complex set carries `TRAIT_COMPLEX_`, with no exceptions (owner): the prefix STATES
        THE SET, so folder and prefix agree by construction. A record whose line has no simple counterpart is
        re-keyed exactly like one that has -- "if it was built as complex, it's complex, no matter what".

        The reason is that the alternative makes a wrong set UNDETECTABLE: if a plain `TRAIT_` id were legal
        inside the complex set, a held `TRAIT_EFFICIENT1` would be indistinguishable from a simple-set leak, and
        no consumer, log line or check could tell the two apart.

        It lives on the STORE because a trait id is named from several curators -- above all the TECH edge that
        GATES a rung, without which every upper rung is permanently unreachable and silently so. One definition,
        applied where the inverted edges are handed out, so no curator can emit an id nothing defines.
        """
        if getattr(self, "_trait_rekey_cache", None) is not None:
            return self._trait_rekey_cache
        table = self.tables.get("TraitInfo", {})
        repl = self.replacements.get("TraitInfo", {})
        complex_ids = set(v["replacement"] for v in repl.values())
        rid_to_base = dict((v["replacement"], b) for b, v in repl.items())

        def _is_complex(typ, rec):
            if typ in complex_ids:
                return True
            og = rec.find("OnGameOptions")
            if og is not None:
                for x in og:
                    for txt in [engine.text(x)] + [engine.text(c) for c in x]:
                        if txt and "COMPLEX" in txt:
                            return True
            return False

        out = {}
        for typ, rec in table.items():
            if typ in rid_to_base or not _is_complex(typ, rec):
                continue
            if typ.startswith("TRAIT_COMPLEX_") or not typ.startswith("TRAIT_"):
                continue
            out[typ] = complex_variant_id(typ)
        self._trait_rekey_cache = out
        return out

    def enabled_by(self, typ):
        """What does `typ` enable? -> {bucket: [sorted referrer types]}."""
        rekey = self.trait_rekey()
        return {b: sorted(rekey.get(t, t) for t in s) for b, s in self.enables.get(typ, {}).items()}

    def obsoletes_of(self, typ):
        """What does `typ` obsolete? -> {bucket: [sorted referrer types]}."""
        return {b: sorted(s) for b, s in self.obsoletes.get(typ, {}).items()}

    def replaces_of(self, typ):
        """What does `typ` REPLACE (succeed)? -> {bucket: [sorted predecessor types]}. Forward-read from the
        successor you HAVE: having `typ` removes these predecessors from CAN GET (enabler-spec §6)."""
        return {b: sorted(s) for b, s in self.replaces.get(typ, {}).items()}

    def disables_of(self, typ):
        """What does `typ` BAN (disable, destructive/reversible)? -> {bucket: [sorted banned types]}. LATENT
        today — no live XML source (see DISABLE_FIELDS); always {} until a real ban source lands."""
        return {b: sorted(s) for b, s in self.disables.get(typ, {}).items()}

    def replacement_of(self, typ):
        """If base `typ` is conditionally replaced (AIAndy CvInfoReplacements), return
        {'replacement': replId, 'condition': <ReplacementCondition element or None>}, else None."""
        for ent_repl in self.replacements.values():
            if typ in ent_repl:
                return ent_repl[typ]
        return None

    def culture_bonuses(self):
        """Bonuses granted by a Culture national wonder (SpecialBuildingType SPECIALBUILDING_C2C_CULTURE,
        or a BUILDING_CULTURE_* type, via ExtraFreeBonuses) — the removal-candidate set."""
        if self._culture_bonuses is None:
            s = set()
            for B, rec in self.tables.get("BuildingInfo", {}).items():
                sb = engine.text(rec.find("SpecialBuildingType"))
                if sb == "SPECIALBUILDING_C2C_CULTURE" or B.startswith("BUILDING_CULTURE_"):
                    for fb in rec.iter("FreeBonus"):
                        t = engine.text(fb)
                        if t:
                            s.add(t)
            self._culture_bonuses = s
        return self._culture_bonuses


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--enables", help="print what this Type enables")
    args = ap.parse_args()
    s = Store()
    print("entity              records  module-added")
    for ent in ENTITIES:
        t = s.tables.get(ent, {})
        print("  %-18s %6d %6d" % (ent, len(t), len(s.module_added.get(ent, set()))))
    # enables-family index coverage (the four forward-read-from-HAS generation objects)
    techs = s.table("TechInfo")
    enabling = sum(1 for T in techs if s.enables.get(T))
    print("\ntechs that enable >=1 thing: %d / %d" % (enabling, len(techs)))
    print("enables-family index sizes: enables=%d obsoletes=%d replaces=%d disables=%d (keys = the HAVE-side type)"
          % (len(s.enables), len(s.obsoletes), len(s.replaces), len(s.disables)))
    if args.enables:
        T = args.enables
        print("\n%s enables:" % T)
        for b, items in s.enabled_by(T).items():
            print("  %-10s %3d  e.g. %s" % (b, len(items), ", ".join(items[:5])))
        for label, q in (("obsoletes", s.obsoletes_of), ("replaces", s.replaces_of), ("disables", s.disables_of)):
            res = q(T)
            if res:
                print("%s %s:" % (T, label))
                for b, items in res.items():
                    print("  %-10s %3d  e.g. %s" % (b, len(items), ", ".join(items[:5])))


if __name__ == "__main__":
    main()
