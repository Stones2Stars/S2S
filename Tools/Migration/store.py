#!/usr/bin/env python3
"""XML-as-DB store (#428) — the queryable foundation for the top-down JSON migration.

Loads every gameplay Info from BOTH the base XML (Assets/XML) AND the modules (Assets/Modules),
merges module overlays by Type (preservation invariant: module data is BAKED INTO core, never dropped),
and exposes per-entity tables plus a generic ENABLER reverse-index (who-enables-what), built by
inverting the prereq references. The top-down curators query this; they never re-walk the XML.

See Sources/docs/plans/building-cascade-conversion.md -> "THE MODEL (locked 2026-06-14)".

  python3 store.py            # load + print coverage/module stats
  python3 store.py --enables TECH_LANGUAGE   # show what a type enables
"""
import argparse
import glob
import os
import xml.etree.ElementTree as ET
from collections import OrderedDict

import engine  # same dir: strip_ns, text, is_int, keyed_entries, named_array, unit_of, generic, ...

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
XML_DIR = os.path.join(REPO, "Assets", "XML")
MOD_DIR = os.path.join(REPO, "Assets", "Modules")

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
    "HeritageInfo":      "*HeritageInfos.xml",
    "SpecialBuildingInfo": "*CIV4SpecialBuildingInfos.xml",
    # config/global entities — participate in NO enabler edges (enable nothing); registered for
    # module-merged table access by their curators.
    "HandicapInfo":      "*CIV4HandicapInfo.xml",
    "GameSpeedInfo":     "*CIV4GameSpeedInfo.xml",
    "EraInfo":           "*CIV4EraInfos.xml",
    # Config / category entities (the former "POCO" batch). The 2026-06-14 PM audit (plan doc "AUDIT DONE")
    # proved only CivicOption is a pure text+identity holder; BonusClass/Hurry/Victory/PromotionLine carry real
    # config/gameplay and get proper curators. Registered here for table access.
    "BonusClassInfo":    "*CIV4BonusClassInfos.xml",
    "CivicOptionInfo":   "*CIV4CivicOptionInfos.xml",
    "HurryInfo":         "*CIV4HurryInfo.xml",
    "VictoryInfo":       "*CIV4VictoryInfo.xml",
    "CultureLevelInfo":  "*CIV4CultureLevelInfo.xml",   # per-city-level config; enables buildings (PrereqCultureLevel)
    "VoteInfo":          "*CIV4VoteInfo.xml",            # diplo-vote resolutions (config for the voting subsystem)
    "CivilizationInfo":  "*CIV4CivilizationInfos.xml",   # game-start grants + per-civ policy/art (source entity)
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
    ("CorporationInfo", "PrereqBonuses/BonusType",      "corporations"),
    ("CorporationInfo", "PrereqBuildings/BuildingType", "corporations"),  # a building required to found the corp
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
    ("TraitInfo",    "TraitPrereq",     "traits"),
    ("TraitInfo",    "TraitPrereqOr1",  "traits"),
    ("TraitInfo",    "TraitPrereqOr2",  "traits"),
    ("TraitInfo",    "PrereqTech",      "traits"),
    ("RouteInfo",    "BonusType",                       "routes"),
    ("RouteInfo",    "PrereqOrBonuses/BonusType",       "routes"),
    ("BuildInfo",    "PrereqBonusTypes/PrereqBonusType", "builds"),
    ("PromotionInfo", "PrereqBonusTypes/BonusType",     "promotions"),
    ("PromotionInfo", "PrereqPlotBonusTypes/PrereqPlotBonusType", "promotions"),
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

# Obsolete edges — top-down: a tech OBSOLETES these targets (reverse of <entity>.ObsoleteTech).
OBSOLETE_FIELDS = [
    ("BuildingInfo", "ObsoleteTech", "buildings"),
    ("UnitInfo",     "ObsoleteTech", "units"),
    ("BuildInfo",    "ObsoleteTech", "builds"),
    ("BonusInfo",    "TechObsolete", "bonuses"),
    ("CorporationInfo", "ObsoleteTech", "corporations"),  # latent (no corp sets it today); hardens the edge
]


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
        self.enables = {}     # referencedType -> {bucket: set(referrerType)}
        self.obsoletes = {}   # referencedType -> {bucket: set(referrerType)} from ObsoleteTech edges
        self.replacements = {}  # entity -> {baseType: {"replacement": replId, "condition": <elem>}}
        self._culture_bonuses = None
        for ent, glb in (entities or ENTITIES).items():
            self._load(ent, glb)
        self._index()

    def _files(self, glb):
        found = []
        for base in (XML_DIR, MOD_DIR):
            found.extend(glob.glob(os.path.join(base, "**", glb), recursive=True))
        return sorted(f for f in found if "schema" not in f.lower())

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
                # Conditional replacement (AIAndy's CvInfoReplacements): a record carrying <ReplacementID> is NOT
                # a module override of `typ`. The engine keeps it apart and swaps it in FOR `typ` only when the
                # <ReplacementCondition> holds — read as a FRESH full Info, no base merge
                # (CvXMLLoadUtilitySet.cpp:1587-1604; CvInfoReplacements::updateReplacements). So re-key it under
                # the ReplacementID as its OWN Type and record the base -> replacedBy edge; the base stays clean.
                # (Only TraitInfo populates this today; the handling is generic for Building/Unit later.)
                rid_node = rec.find("ReplacementID")
                rid = engine.text(rid_node) if rid_node is not None else ""
                if rid and rid != "NONE":
                    repl[typ] = {"replacement": rid, "condition": rec.find("ReplacementCondition")}
                    for t in ("ReplacementID", "ReplacementCondition"):
                        e = rec.find(t)
                        if e is not None:
                            rec.remove(e)
                    tn = rec.find("Type")
                    if tn is not None:
                        tn.text = rid
                    key = rid
                else:
                    key = typ
                if key in table:
                    _merge(table[key], rec)
                else:
                    table[key] = rec
                    if is_module:
                        modadd.add(key)
                prov.setdefault(key, []).append(rel)
        self.tables[ent] = table
        self.provenance[ent] = prov
        self.module_added[ent] = modadd

    def _index(self):
        self.enables = self._build_index(PREREQ_FIELDS)
        self.obsoletes = self._build_index(OBSOLETE_FIELDS)

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

    def enabled_by(self, typ):
        """What does `typ` enable? -> {bucket: [sorted referrer types]}."""
        return {b: sorted(s) for b, s in self.enables.get(typ, {}).items()}

    def obsoletes_of(self, typ):
        """What does `typ` obsolete? -> {bucket: [sorted referrer types]}."""
        return {b: sorted(s) for b, s in self.obsoletes.get(typ, {}).items()}

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
    # enabler index coverage
    techs = s.table("TechInfo")
    enabling = sum(1 for T in techs if s.enables.get(T))
    print("\ntechs that enable >=1 thing: %d / %d" % (enabling, len(techs)))
    if args.enables:
        print("\n%s enables:" % args.enables)
        for b, items in s.enabled_by(args.enables).items():
            print("  %-10s %3d  e.g. %s" % (b, len(items), ", ".join(items[:5])))


if __name__ == "__main__":
    main()
