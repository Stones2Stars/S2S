"""Locust census of Assets/Data: every top-level modifier-family key with its authored structure.

For each non-reserved, object-valued top-level key (json.md section 1: that IS a modifier family),
collect: entity-kind usage counts, the scopes authored under it, the member/target keys under each
scope, and the unit keys seen. Output a compact per-family summary for the enum-group definition.
"""
import json
import os
import sys
from collections import defaultdict

DATA_DIR = sys.argv[1] if len(sys.argv) > 1 else r"C:\code\s2s\s2s\Assets\Data"

RESERVED = {
    "type", "identity", "cost", "ui", "world", "sound", "ai",
    "enables", "obsoletes", "obsoletedBy", "replaces", "replacedBy", "disables",
    "requires", "allowed", "grants", "provides", "whenObsolete",
    "skills", "tags", "state", "attributes", "capabilities", "policies",
    "shrine", "headquarters", "succession", "excludes", "produces",
    "condition", "effect", "vision", "outcomes", "mapGeneration",
    "promotionLine", "buildUp", "properties", "voteSource", "threshold",
    "role", "victory", "targetLevel", "conversion", "cityFounding",
    "unitCapability", "canTrade", "canTradeOn", "canWorkOn", "spread",
    "sizeMatters", "combatClass", "combatClasses", "builds", "missions",
    "enabled", "disabled",
}
SCOPES = {"world", "team", "empire", "area", "city", "plot", "self", "building", "specialist", "unit"}
UNITS = {"flat", "percent", "multiplier", "postMultiplier", "rawPercent"}
PLURAL_TARGETS = {"plots", "units", "cities", "areas", "empires"}
KEYED_TARGETS = {"improvements", "terrains", "features", "bonus", "bonuses", "buildings",
                 "domains", "unitCombats", "specialists", "routes"}


class Family:
    def __init__(self):
        self.count = 0
        self.kinds = defaultdict(int)          # entity folder -> count
        self.scopes = defaultdict(int)         # scope -> count
        self.members = defaultdict(int)        # scope.member -> count (non-unit, non-target keys)
        self.targets = defaultdict(int)        # scope.target -> count
        self.units = defaultdict(int)          # unit key -> count
        self.odd_keys = defaultdict(int)       # top-of-family keys that are not scopes


families = defaultdict(Family)


def walk_scope(fam, scope, node):
    if not isinstance(node, dict):
        return
    for key, val in node.items():
        if key in UNITS:
            fam.units[key] += 1
        elif key in PLURAL_TARGETS:
            fam.targets[scope + "." + key] += 1
            walk_scope(fam, scope, val)
        elif key in KEYED_TARGETS and isinstance(val, dict):
            fam.targets[scope + "." + key + ".{}"] += 1
            for tval in val.values():
                walk_scope(fam, scope, tval)
        elif key in ("enabled", "disabled", "per", "value", "ai", "interval", "chance"):
            continue
        else:
            fam.members[scope + "." + key] += 1
            walk_scope(fam, scope, val)


count_files = 0
for root, dirs, names in os.walk(DATA_DIR):
    if "_additions" in root:
        continue
    kind = os.path.relpath(root, DATA_DIR).split(os.sep)[0]
    for name in names:
        if not name.endswith(".json") or name == "_order.json":
            continue
        count_files += 1
        try:
            with open(os.path.join(root, name), "r", encoding="utf-8-sig") as handle:
                doc = json.load(handle)
        except (ValueError, OSError):
            continue
        if not isinstance(doc, dict):
            continue
        for key, val in doc.items():
            if key in RESERVED or not isinstance(val, dict):
                continue
            fam = families[key]
            fam.count += 1
            fam.kinds[kind] += 1
            for scope_key, scope_val in val.items():
                if scope_key in SCOPES:
                    fam.scopes[scope_key] += 1
                    walk_scope(fam, scope_key, scope_val)
                else:
                    fam.odd_keys[scope_key] += 1


def fmt(counter):
    return ", ".join(k + ":" + str(v) for k, v in sorted(counter.items(), key=lambda p: -p[1]))


print("files=%d families=%d" % (count_files, len(families)))
print()
for name in sorted(families, key=lambda n: -families[n].count):
    fam = families[name]
    print("== %s  (%d entities)" % (name, fam.count))
    print("   kinds:   %s" % fmt(fam.kinds))
    print("   scopes:  %s" % fmt(fam.scopes))
    if fam.members:
        print("   members: %s" % fmt(fam.members))
    if fam.targets:
        print("   targets: %s" % fmt(fam.targets))
    print("   units:   %s" % fmt(fam.units))
    if fam.odd_keys:
        print("   ODD:     %s" % fmt(fam.odd_keys))
    print()
