#!/usr/bin/env python3
"""
curate_order.py -- emit the per-category ORDER MANIFEST (`Assets/Data/<folder>/_order.json`).

WHY: the JSON loader (LoadGlobalClassInfoJson) registers a category's engine ids in its load-sort order.
Without a manifest that sort is alphabetical-by-type, which reshuffled every id-ordered UI surface against
the legacy game -- most painfully the unit level-up promotion popup, whose button layout follows promotion
ids (the legacy XML grouped each promotion line's tiers adjacently, and mod-era additions appended at the
END of the document, so the base layout was stable and new lines showed up last). The manifest restores the
LEGACY DOCUMENT ORDER: each `_order.json` is the category's type list in the store's merged enumeration
(base `Assets/XML` document order, then module additions -- the same sequence every curator consumes).

The loader sorts a category's entities by manifest position; a type ABSENT from the manifest (synthetic
TECH_GAME_START, future hand-authored entities) sorts AFTER every listed one, alphabetically -- reproducing
the legacy new-stuff-appends behaviour with zero authoring burden.

Each manifest is the legacy XML order INTERSECTED with the types actually emitted as JSON -- a legacy type a
curator intentionally drops (e.g. the redundant culture unit-combats) is NOT listed. This is safe for every
category: the loader (CvXMLLoadUtilitySet::LoadGlobalClassInfoJson) reads `_order.json` into `order[type]=index`
and sorts only the PRESENT entity files by that index -- a fileless phantom never becomes an entity, so it never
gets an id, and dropping it shifts only the absolute index VALUES, never the RELATIVE order of present types.
So the assigned ids are identical with or without phantoms -- the manifest's job is purely to re-impose the
legacy ORDER that per-file JSON cannot carry intrinsically, and it does that on exactly the files that exist.

Derived artifact -- regenerate + commit freely (the manifests are curator OUTPUT, never hand-edited):
    python3 curate_order.py --write
"""
import argparse
import glob
import json
import os
import xml.etree.ElementTree as ET

import engine
import store as store_mod

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(REPO, "Assets", "Data")

# entity record-element -> the Assets/Data folder its curator writes (only the categories the JSON
# loader registers ids for; TraitInfo is deliberately absent -- the simple/complex folder split shares
# engine ids and no id-ordered UI surface reads traits).
FOLDERS = {
    "TechInfo": "techs",
    "BuildingInfo": "buildings",
    "UnitInfo": "units",
    "BuildInfo": "builds",
    "SpecialistInfo": "specialists",
    "ImprovementInfo": "improvements",
    "RouteInfo": "routes",
    "CivicInfo": "civics",
    "CivicOptionInfo": "civicoptions",
    "ReligionInfo": "religions",
    "BonusInfo": "bonuses",
    "CorporationInfo": "corporations",
    "ProjectInfo": "projects",
    "ProcessInfo": "processes",
    "PromotionInfo": "promotions",
    "PromotionLineInfo": "promotionlines",
    "UnitCombatInfo": "unitcombats",
    "HeritageInfo": "heritages",
    "CultureLevelInfo": "culturelevels",
    "PropertyInfo": "properties",
    "TerrainInfo": "terrains",
    "FeatureInfo": "features",
    # #430: the 11 uniformity types are now JSON-loaded (LoadGlobalClassInfoJson) too, so they need the manifest
    # to keep the legacy XML document order. Handicap/Era especially are INDEX-referenced (not name-remapped in
    # saves), so alphabetical load-order corrupts the save's difficulty/era index -> wrong values applied.
    "GameSpeedInfo": "gamespeeds",
    "EraInfo": "eras",
    "HandicapInfo": "handicaps",
    # #430 item 15: WorldInfo is JSON-loaded too. The manifest is LOAD-BEARING here beyond UI order: the
    # compiled WorldSizeTypes enum stops at HUGE (6 labels) while the data authors 8 sizes, so the engine
    # ids are registration-driven -- the manifest is what keeps DUEL=0..GIGANTIC=7 aligned with the enum's
    # first six values and the saves' stored world-size index.
    "WorldInfo": "worlds",
    "CivilizationInfo": "civilizations",
    "VictoryInfo": "victories",
    "VoteInfo": "votes",
    "HurryInfo": "hurries",
    "BonusClassInfo": "bonusclasses",
    "SpecialBuildingInfo": "specialbuildings",
    "SpecialUnitInfo": "specialunits",
    "LeaderHeadInfo": "leaderheads",
    # #430: the OUTCOME_* gate/tier infos are JSON-loaded too (Assets/Data/outcomes), so they take the manifest on
    # the same rule -- no OutcomeTypes is save-serialized, but the loader registers their ids like any other
    # category and legacy document order is the id order every other category keeps.
    "OutcomeInfo": "outcomes",
}


def _files(glb):
    """The LEGACY registration order: BASE `Assets/XML` files first, THEN the modules -- the legacy loader
    registered the base document first and appended module additions, which is exactly the id order players'
    muscle memory formed on. (The store's own file sort puts Modules/ before XML/ -- right for value-merging,
    WRONG for id ordering -- hence this local enumeration. Module-vs-module order approximates the MLF config
    alphabetically; base-vs-module and per-file document order are the load-bearing parts.)"""
    out = []
    for base in (store_mod.XML_DIR, store_mod.MOD_DIR):
        found = sorted(glob.glob(os.path.join(base, "**", glb), recursive=True))
        for f in found:
            norm = f.replace("\\", "/").lower()
            if "schema" in norm or any(ex in norm for ex in store_mod.EXCLUDED_MODULE_SUBPATHS):
                continue
            out.append(f)
    return out


def order_of(ent, glb):
    seen = set()
    types = []
    for path in _files(glb):
        try:
            root = engine.strip_ns(ET.parse(path).getroot())
        except ET.ParseError:
            continue
        for rec in root.iter(ent):
            typ = engine.text(rec.find("Type"))
            # a conditional-replacement record registers under its ReplacementID (store.py's rule): the base
            # Type stays a separate, earlier record -- both get their own document position.
            rid_node = rec.find("ReplacementID")
            rid = engine.text(rid_node) if rid_node is not None else ""
            if rid and rid != "NONE":
                typ = rid
            if not typ or typ in seen:
                continue
            seen.add(typ)
            types.append(typ)
    return types


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    for ent, folder in sorted(FOLDERS.items()):
        types = order_of(ent, store_mod.ENTITIES[ent])
        # The manifest orders the FILES that exist, in legacy XML order: intersect the legacy type sequence with
        # the types actually emitted as JSON, so a curator-dropped legacy type (e.g. culture unit-combats) is not
        # listed. Provably a no-op for engine ids -- a fileless phantom never gets an id (docstring). Guarded: if
        # the folder has not been generated yet, keep the full order rather than emptying the manifest.
        present = {os.path.splitext(os.path.basename(p))[0].upper()
                   for p in glob.glob(os.path.join(OUT, folder, "**", "*.json"), recursive=True)
                   if os.path.basename(p) != "_order.json"}
        if present:
            types = [t for t in types if t in present]
        print("%-20s -> %s/_order.json  (%d types; first: %s)"
              % (ent, folder, len(types), ", ".join(types[:3])))
        if args.write:
            path = os.path.join(OUT, folder)
            os.makedirs(path, exist_ok=True)
            with open(os.path.join(path, "_order.json"), "w") as f:
                json.dump(types, f, indent=1)
    if args.write:
        print("\nwrote %d order manifests under Assets/Data/" % len(FOLDERS))


if __name__ == "__main__":
    main()
