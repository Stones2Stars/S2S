#!/usr/bin/env python3
"""
#428 migration — per-entity distinct-tag extractor.

For each gameplay Info entity, emit Tools/Migration/tags/<Entity>.json: the authoritative list of every
distinct XML child tag with a kind + structural hint + sample + occurrence count. The migration mapping
agents classify THIS list (channel / prereq / cost / art / identity-stay / inversion-out / edge-case),
so they never re-read the multi-MB XML and completeness is guaranteed (every tag is in the list).
"""
import glob
import json
import os
import xml.etree.ElementTree as ET

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
XML = os.path.join(REPO, "Assets", "XML")
OUT = os.path.join(os.path.dirname(__file__), "tags")

# The gameplay relationship surface (blueprint scope). Record-element -> filename glob (under Assets/XML).
ENTITIES = {
    "BuildingInfo":        "Buildings/*CIV4BuildingInfos.xml",
    "SpecialBuildingInfo": "**/CIV4SpecialBuildingInfos.xml",
    "UnitInfo":            "Units/*CIV4UnitInfos.xml",
    "SpecialUnitInfo":     "**/CIV4SpecialUnitInfos.xml",
    "UnitCombatInfo":      "**/*CIV4UnitCombatInfos.xml",
    "PromotionInfo":       "**/CIV4PromotionInfos.xml",
    "PromotionLineInfo":   "**/CIV4PromotionLineInfos.xml",
    "TechInfo":            "**/CIV4TechInfos.xml",
    "BonusInfo":           "**/*CIV4BonusInfos.xml",
    "BonusClassInfo":      "**/CIV4BonusClassInfos.xml",
    "CivicInfo":           "**/CIV4CivicInfos.xml",
    "CivicOptionInfo":     "**/CIV4CivicOptionInfos.xml",
    "ImprovementInfo":     "**/CIV4ImprovementInfos.xml",
    "TerrainInfo":         "**/CIV4TerrainInfos.xml",
    "FeatureInfo":         "**/CIV4FeatureInfos.xml",
    "ReligionInfo":        "**/CIV4ReligionInfo.xml",
    "CorporationInfo":     "**/CIV4CorporationInfo.xml",
    "ProjectInfo":         "**/CIV4ProjectInfo.xml",
    "ProcessInfo":         "**/CIV4ProcessInfo.xml",
    "SpecialistInfo":      "**/CIV4SpecialistInfos.xml",
    "CivilizationInfo":    "**/CIV4CivilizationInfos.xml",
    "LeaderHeadInfo":      "**/CIV4LeaderHeadInfos.xml",
    "TraitInfo":           "**/CIV4TraitInfos.xml",
    "RouteInfo":           "**/CIV4RouteInfos.xml",
    "BuildInfo":           "**/CIV4BuildInfos.xml",
    "VoteInfo":            "**/CIV4VoteInfo.xml",
    "VictoryInfo":         "**/CIV4VictoryInfo.xml",
    "CultureLevelInfo":    "**/CIV4CultureLevelInfo.xml",
    "GameSpeedInfo":       "**/CIV4GameSpeedInfo.xml",
    "EraInfo":             "**/CIV4EraInfos.xml",
    "HurryInfo":           "**/CIV4HurryInfo.xml",
    "HandicapInfo":        "**/CIV4HandicapInfo.xml",
    "PropertyInfo":        "**/CIV4PropertyInfos.xml",
    "HeritageInfo":        "**/HeritageInfos.xml",
}


def strip(t):
    return t.split("}", 1)[1] if isinstance(t, str) and "}" in t else t


def shape(el, depth=0):
    """Compact structural hint, max 3 levels deep."""
    kids = list(el)
    if not kids:
        txt = (el.text or "").strip()
        return txt[:40] if txt else ""
    if depth >= 2:
        return "{...}"
    inner = []
    seen = set()
    for k in kids[:6]:
        tag = strip(k.tag)
        if tag in seen:
            continue
        seen.add(tag)
        s = shape(k, depth + 1)
        inner.append(tag + ("=" + s if s and depth < 1 else ("{" + s + "}" if s else "")))
    same = len(seen) == 1
    return ("[" + inner[0] + "...]") if same and len(kids) > 1 else "{" + ", ".join(inner) + "}"


def kind_of(el):
    kids = list(el)
    if kids:
        tags = set(strip(k.tag) for k in kids)
        return "list" if len(tags) == 1 else "map"
    txt = (el.text or "").strip()
    tag = strip(el.tag)
    if tag.startswith("b") and txt in ("0", "1"):
        return "flag"
    if tag[:1] in ("i", "f") and txt.lstrip("-").replace(".", "", 1).isdigit():
        return "number"
    return "string"


def main():
    if not os.path.isdir(OUT):
        os.makedirs(OUT)
    summary = []
    for entity, pat in ENTITIES.items():
        files = sorted(glob.glob(os.path.join(XML, pat), recursive=True))
        files = [f for f in files if "schema" not in f.lower()]
        tags = {}
        recs = 0
        srcs = []
        for path in files:
            try:
                root = ET.parse(path).getroot()
            except ET.ParseError:
                continue
            srcs.append(os.path.relpath(path, REPO).replace("\\", "/"))
            for rec in root.iter():
                if strip(rec.tag) != entity:
                    continue
                recs += 1
                for c in rec:
                    t = strip(c.tag)
                    e = tags.setdefault(t, {"tag": t, "count": 0, "kind": kind_of(c), "sample": ""})
                    e["count"] += 1
                    if not e["sample"]:
                        s = shape(c)
                        if s:
                            e["sample"] = s
        rows = sorted(tags.values(), key=lambda r: -r["count"])
        with open(os.path.join(OUT, entity + ".json"), "w") as f:
            json.dump({"entity": entity, "records": recs, "files": srcs,
                       "distinctTags": len(rows), "tags": rows}, f, indent=1)
        summary.append((entity, recs, len(rows), len(srcs)))
    print("entity                      records  tags  files")
    for e, r, t, fl in summary:
        print("  %-26s %6d %5d %5d" % (e, r, t, fl))
    print("\nwrote %d per-entity tag files to Tools/Migration/tags/" % len(summary))


if __name__ == "__main__":
    main()
