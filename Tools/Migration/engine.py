#!/usr/bin/env python3
"""
#428 migration ENGINE — mapping-driven XML -> JSON database emitter.

Consumes the per-entity mapping files (Tools/Migration/mapping/<Entity>.json, produced by the mapping
workflow) plus the source XML, and emits the whole JSON database under Assets/Data/<entity>/, with every
cross-entity reference HOMED onto its conditioner (inversions accumulated centrally, then merged into the
destination records). This is the deterministic core; the mapping files are the data, the rules live there.

  python3 Tools/Migration/engine.py --dry     # process, report, write nothing
  python3 Tools/Migration/engine.py --write    # write the JSON database
"""
import argparse
import glob
import json
import os
import xml.etree.ElementTree as ET
from collections import OrderedDict

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
XML = os.path.join(REPO, "Assets", "XML")
MAPDIR = os.path.join(os.path.dirname(__file__), "mapping")
OUT = os.path.join(REPO, "Assets", "Data")

YIELDS = ["food", "production", "commerce"]
COMMERCES = ["gold", "research", "culture", "espionage"]
# named keys per channel BASE (after the kind suffix is stripped off)
LEAF_KEYS = {"commerce": COMMERCES, "yield": YIELDS, "vicinityYield": YIELDS}

# Universal clean-name renames for intrinsic fields, SHARED by every curator.
# WORTH CONVENTION: bare `worth` = the entity's OVERALL worth (the player's "Assets" demographic, iAsset);
# a domain-specific worth is `{field}Worth` — so iPower (the "Power"/military demographic) is `militaryWorth`.
# Never `asset` (reads as an art asset) or bare `power` (collides with the electricity mechanic).
FIELD_RENAME = {"iAsset": "worth", "iPower": "militaryWorth",
                "TerrainBooleans": "validTerrains", "FeatureBooleans": "validFeatures",
                "FeatureTerrainBooleans": "validPlacementOn"}


def unit_of(tag):
    """The UNIT of modification comes from the VALUE element name, NOT the outer tag. 'TechCommerceChanges'
    sounds flat but actually carries <CommercePercents><iCommercePercent> -> percent. iCommerce -> flat,
    iCommercePercentPerPopulation -> perPopulation. This is the flat/percent/per-unit distinction."""
    t = tag.lower()
    if "perpop" in t or "perpopulation" in t:
        return "perPopulation"
    if "permilitary" in t:
        return "perMilitaryUnit"
    if "percent" in t or "modifier" in t:
        return "percent"
    return "flat"


def channel_base(leaf):
    """Strip the kind-implying suffix so the unit is carried separately: commerceModifier -> commerce."""
    for suf in ("Modifier", "Percent", "PerPopulation", "PerPop"):
        if leaf.endswith(suf):
            return leaf[:-len(suf)]
    return leaf


def strip_ns(root):
    for el in root.iter():
        if isinstance(el.tag, str) and "}" in el.tag:
            el.tag = el.tag.split("}", 1)[1]
    return root


def text(e):
    return (e.text or "").strip() if e is not None else ""


def is_int(s):
    try:
        int(s); return True
    except (ValueError, TypeError):
        return False


# property-amount formula operators: <iAmountPerTurn><Mult><AttributeType>..</><Constant>..</></Mult></>
FORMULA_OPS = {"Mult", "Div", "Add", "Subtract", "Min", "Max", "Power", "Modulo"}


def formula_node(op):
    """Render a property-amount formula node as {operator: [operands]}, preserving the operator (Mult/Div)."""
    operands = []
    for c in op:
        if c.tag == "AttributeType":
            operands.append({"attribute": text(c)})
        elif c.tag in FORMULA_OPS:
            operands.append(formula_node(c))
        else:
            t = text(c)
            operands.append(int(t) if is_int(t) else (t if t else generic(c)))
    return {op.tag.lower(): operands}


def generic(elem):
    """Faithful XML->JSON for sections we preserve as-is (de-asses uniform repeated children to lists)."""
    kids = list(elem)
    if not kids:
        t = text(elem)
        return int(t) if is_int(t) else t
    if len(kids) == 1 and kids[0].tag in FORMULA_OPS:
        return formula_node(kids[0])  # don't collapse the operator into a bare list
    # key/value pair: <Flavor><FlavorType>FLAVOR_SCIENCE</><iFlavor>4</></> -> {FLAVOR_SCIENCE: 4}
    if len(kids) == 2:
        tk = [k for k in kids if k.tag.endswith("Type")]
        if len(tk) == 1:
            v = kids[0] if kids[1] is tk[0] else kids[1]
            return {text(tk[0]): generic(v)}
    if len(set(k.tag for k in kids)) == 1:
        return [generic(k) for k in kids]
    return {k.tag: generic(k) for k in kids}


# property-source field renames -> clean, de-prefixed keys (the rest de-i-prefix + lowercase the leading char)
PROP_RENAME = {"PropertySourceType": "source", "PropertyType": "property", "GameObjectType": "on",
               "RelationType": "relation", "iAmountPerTurn": "amountPerTurn", "iAmount": "amount",
               "ObjectType": "object", "TargetType": "target"}


def clean_property_source(src):
    out = OrderedDict()
    for c in src:
        key = PROP_RENAME.get(c.tag) or (c.tag[1].lower() + c.tag[2:] if c.tag[:1] == "i" and len(c.tag) > 1 else c.tag)
        v = generic(c)
        if c.tag == "PropertySourceType" and isinstance(v, str):
            v = v.replace("PROPERTYSOURCE_", "")
        out[key] = v
    return out


# --- v3 property-source converter (#428, the STANDARD, owner 2026-06-15 "make it the new version at once") -------
# ONE shared converter for <PropertySource> -> a v3 modifier deposit, used by EVERY entity carrying
# PropertyManipulators (Property/Civic now; Heritage/Building/Unit later) so property deposits are uniform with
# the modifier vocabulary (modifier-spec §1.3/§4), never a bespoke perTurn/mult shape.
PROP_SCOPE = {"GAMEOBJECT_CITY": "city", "GAMEOBJECT_PLOT": "plot", "GAMEOBJECT_PLAYER": "empire",
              "GAMEOBJECT_UNIT": "unit", "GAMEOBJECT_AREA": "area"}
PROP_SOURCE_UNIT = {"PROPERTYSOURCE_DECAY": "percent",            # % move toward targetLevel
                    "PROPERTYSOURCE_CONSTANT": "flat",            # flat per-turn add (scalar or Mult-scaled)
                    "PROPERTYSOURCE_ATTRIBUTE_CONSTANT": "flat"}  # flat per-attribute add


def property_source_v3(src):
    """Convert one <PropertySource> to (property, scope, unit, value) in the v3 modifier vocabulary, or None.
    value = an int, OR {value, per:{type,scope}} when scaled by a per-count ATTRIBUTE (POPULATION). DECAY->percent
    (iPercent), CONSTANT/ATTRIBUTE_CONSTANT->flat (iAmountPerTurn; Mult or AttributeType => `per`). scope from
    GameObjectType. RELATION_ASSOCIATED (a source applying to the player's associated cities) is the cascade
    default and dropped; any OTHER RelationType raises (none exist today — surface it rather than silently mis-map)."""
    prop = text(src.find("PropertyType"))
    if not prop or prop == "NONE":
        return None
    rel = text(src.find("RelationType"))
    if rel and rel not in ("NONE", "RELATION_ASSOCIATED"):
        raise ValueError("property_source_v3: unhandled RelationType %r on %s" % (rel, prop))
    stype = text(src.find("PropertySourceType"))
    unit = PROP_SOURCE_UNIT.get(stype)
    if unit is None:
        raise ValueError("property_source_v3: unhandled PropertySourceType %r on %s" % (stype, prop))
    scope = PROP_SCOPE.get(text(src.find("GameObjectType")), "city")
    per_type = None
    if unit == "percent":
        v = text(src.find("iPercent"))
        value = int(v) if is_int(v) else None
    else:
        amt = src.find("iAmountPerTurn")
        if amt is None:
            amt = src.find("iAmount")
        attr = src.find("AttributeType")
        if attr is not None:                                   # ATTRIBUTE_CONSTANT: value=amount, per=attribute
            per_type = text(attr)
            value = int(text(amt)) if (amt is not None and is_int(text(amt))) else None
        elif amt is not None and amt.find("Mult") is not None:  # CONSTANT with Mult(attribute, constant)
            mult = amt.find("Mult")
            per_type = text(mult.find("AttributeType"))
            c = text(mult.find("Constant"))
            value = int(c) if is_int(c) else None
        else:                                                  # plain scalar per-turn amount
            value = int(text(amt)) if (amt is not None and is_int(text(amt))) else None
    if value is None or value == 0:
        return None
    if per_type:                                               # ATTRIBUTE_POPULATION -> POPULATION catch-all token
        # `each` = "per how many of `type`" (the quantum). The property engine computes attribute x amount
        # (CvPropertySource: getAttribute(eAttr) * iAmountPerTurn) => per EACH 1 of the attribute, so each=1 here;
        # `value` is the per-quantum magnitude. effect = value * (count(type) / each). (modifier-spec §4)
        value = OrderedDict([("value", value),
                             ("per", OrderedDict([("type", per_type.replace("ATTRIBUTE_", "")),
                                                  ("each", 1), ("scope", scope)]))])
    return prop, scope, unit, value


def named_array(elem, keys):
    """<YieldChanges><iYield>1</iYield>...</> -> {food:1,...} (positional short keys, zeros dropped)."""
    vals = [int(text(c)) for c in elem if is_int(text(c))]
    out = {}
    for i, v in enumerate(vals):
        if v != 0 and i < len(keys):
            out[keys[i]] = v
    return out


def keyed_entries(parent, keys):
    """<Foo><FooChange><XType>K</XType><value...></FooChange>...> -> [(K, unit, value), ...].
    The unit (flat/percent/perPopulation) is read from the inner VALUE element name, not the outer tag."""
    out = []
    for entry in list(parent):
        k, rest = None, []
        for c in entry:
            if k is None and c.tag.endswith("Type"):
                k = text(c)
            else:
                rest.append(c)
        if not k:
            continue
        if len(rest) == 1 and list(rest[0]):
            inner = list(rest[0])
            unit = unit_of(inner[0].tag) if inner else "flat"
            val = named_array(rest[0], keys) if keys else generic(rest[0])
        elif len(rest) == 1:
            unit = unit_of(rest[0].tag)
            tx = text(rest[0])
            val = int(tx) if is_int(tx) else tx
        else:
            unit = "flat"
            val = {c.tag: generic(c) for c in rest}
        if val not in (None, {}, []):
            out.append((k, unit, val))
    return out


def parse_shape(shape):
    """'buildingBoosts.{BUILDING}.commerce' -> (boostChannel='buildingBoosts', leaf='commerce')."""
    parts = shape.split(".")
    boost = parts[0]
    leaf = parts[-1]
    if leaf.startswith("{"):
        leaf = "value"
    return boost, leaf


def cv_to_record(cv):
    return cv[2:] if cv.startswith("Cv") else cv


def plural(name):
    if name.endswith(("s", "x", "z")):   # bonus->bonuses, process->processes (NOT tech->teches)
        return name + "es"
    if name.endswith("y") and len(name) > 1 and name[-2] not in "aeiou":
        return name[:-1] + "ies"
    return name + "s"


class Engine:
    def __init__(self):
        self.maps = {}
        for f in sorted(glob.glob(os.path.join(MAPDIR, "*.json"))):
            m = json.load(open(f))
            self.maps[m["entity"]] = m
        # inversions[destRecordElem][destType][boostChannel][srcType][leaf] = value
        self.inversions = {}
        self.records = {}   # entity -> [ (type, json) ]
        self.stats = {"records": 0, "channels": 0, "inversions": 0, "identity": 0}

    def home(self, dest_cv, dest_type, boost, src_type, base, unit, value):
        # deposit onto the destination as channel -> type-of-unit -> value (mirrors the modifiers shape)
        rec = cv_to_record(dest_cv)
        (self.inversions.setdefault(rec, {}).setdefault(dest_type, {})
         .setdefault(boost, {}).setdefault(src_type, {}).setdefault(base, {}))[unit] = value
        self.stats["inversions"] += 1

    def convert_record(self, rec, m):
        HOIST_TEXT = {"Description": "description", "Help": "help", "Civilopedia": "civilopedia", "Strategy": "strategy"}
        typ = text(rec.find("Type"))
        modifiers, prereqs, cost, art, identity = {}, {}, {}, {}, {}
        text_fields, properties, flavors = {}, None, None
        channels, inv = m.get("channels", {}), m.get("inversionsOut", {})
        enablers = m.get("enablerEdges", {})
        pset, cset, aset = set(m.get("prereqs", [])), set(m.get("cost", [])), set(m.get("art", []))
        for c in rec:
            tag, t = c.tag, text(c)
            if tag == "Type":
                continue
            if tag in HOIST_TEXT:
                if t:
                    text_fields[HOIST_TEXT[tag]] = t
            elif tag == "PropertyManipulators":
                properties = [clean_property_source(s) if s.tag == "PropertySource" else {s.tag: generic(s)} for s in c]
            elif tag == "Flavors":
                flavors = generic(c)  # AI-targeting metadata (leader likes / advisor suggests) -> top-level section
            elif tag in channels:
                self.apply_channel(modifiers, channels[tag], c)
            elif tag in inv:
                spec = inv[tag]
                boost, leaf = parse_shape(spec["shape"])
                base = channel_base(leaf)
                keys = spec.get("valueKeys") or LEAF_KEYS.get(base)
                for dest_type, unit, val in keyed_entries(c, keys):
                    self.home(spec["destEntity"], dest_type, boost, typ, base, unit, val)
            elif tag in enablers:
                identity.setdefault("_enablerEdges", {})[tag] = generic(c)  # rare; preserve for review
            elif tag in pset:
                prereqs[tag] = generic(c)
            elif tag in cset:
                cost[tag] = generic(c)
            elif tag in aset:
                art[tag] = generic(c)
            else:
                if list(c) or t:
                    identity[tag] = generic(c)
                    self.stats["identity"] += 1
        out = OrderedDict()
        out["type"] = typ
        for k in ("description", "civilopedia", "help", "strategy"):
            if k in text_fields:
                out[k] = text_fields[k]
        if modifiers:  out["modifiers"] = modifiers
        if prereqs:    out["prerequisites"] = prereqs
        if cost:       out["cost"] = cost
        if flavors:    out["flavors"] = flavors
        if properties: out["properties"] = properties
        if art:        out["art"] = art
        if identity:   out["identity"] = identity
        return typ, out

    def apply_channel(self, modifiers, spec, c):
        scope, channel, kind = spec.get("scope", "city"), spec.get("channel"), spec.get("kind", "flat")
        if scope == "player":
            scope = "empire"  # the empire-wide scope is named 'empire' (matches the handicaps prototype)
        keys = spec.get("valueKeys")
        # per-population was baked into the channel NAME with kind=percent; lift it to a real unit so the
        # flat/percent/per-unit distinction is first-class (perPopulation vs percentPerPopulation).
        if channel and channel.endswith("PerPopulation"):
            channel = channel[:-len("PerPopulation")]
            kind = "percentPerPopulation" if kind == "percent" else "perPopulation"
        if kind == "enabler":
            if text(c) not in ("1", "true", "True"):
                return
            val = True
        elif keys:
            val = named_array(c, keys)
            if not val:
                return
        else:
            t = text(c)
            if not is_int(t) or int(t) == 0:
                return
            val = int(t)
        modifiers.setdefault(scope, {}).setdefault(channel, {})[kind] = val
        self.stats["channels"] += 1

    def process(self):
        for entity, m in self.maps.items():
            paths = sorted(glob.glob(os.path.join(XML, m.get("glob", "")), recursive=True))
            paths = [p for p in paths if "schema" not in p.lower()]
            recs = []
            for path in paths:
                try:
                    root = strip_ns(ET.parse(path).getroot())
                except ET.ParseError:
                    continue
                for rec in root.iter(m["rootElement"]):
                    if rec.find("Type") is None:
                        continue
                    typ, out = self.convert_record(rec, m)
                    recs.append((typ, out))
                    self.stats["records"] += 1
            self.records[entity] = recs

    def merge_inversions(self):
        """Attach homed boosts onto their destination records: dest.{boost}.{srcType}.{channel}.{unit}."""
        by_type = {}
        for entity, recs in self.records.items():
            for typ, out in recs:
                by_type[(entity, typ)] = out
        merged = 0
        for rec_elem, dests in self.inversions.items():
            for dest_type, boosts in dests.items():
                out = by_type.get((rec_elem, dest_type))
                if out is None:
                    continue  # dest record not in scope (e.g. enum target) -- engine reports below
                for boost, srcmap in boosts.items():
                    out.setdefault(boost, {})
                    for src_type, basemap in srcmap.items():
                        out[boost][src_type] = basemap  # {channel: {unit: value}}
                        merged += 1
        return merged

    def write(self):
        # Agreed layout: 1 JSON per Info, loose (raw FindFirstFile only sees loose), in per-entity
        # folders; buildings additionally foldered by era (derived from their PrereqTech's era).
        tech_era = {}
        for typ, out in self.records.get("TechInfo", []):
            e = (out.get("identity") or {}).get("Era")
            if e:
                tech_era[typ] = e.replace("C2C_ERA_", "").replace("ERA_", "").lower()
        for entity, recs in self.records.items():
            if entity == "HandicapInfo":
                continue  # keep the hand-built prototype at Assets/Data/handicaps/ (richer ai-override structure)
            ekey = plural(entity[:-4].lower() if entity.endswith("Info") else entity.lower())
            for typ, out in recs:
                if entity == "BuildingInfo":
                    pt = (out.get("prerequisites") or {}).get("PrereqTech")
                    folder = os.path.join(OUT, ekey, tech_era.get(pt, "none"))
                else:
                    folder = os.path.join(OUT, ekey)
                if not os.path.isdir(folder):
                    os.makedirs(folder)
                with open(os.path.join(folder, typ.lower() + ".json"), "w") as f:
                    json.dump(out, f, indent=1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry", action="store_true")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--sample", help="print first 2 converted records of this entity (e.g. CivicInfo)")
    args = ap.parse_args()

    eng = Engine()
    print("loaded %d entity mappings: %s" % (len(eng.maps), ", ".join(sorted(eng.maps))))
    if not eng.maps:
        print("no mapping files yet (waiting on the mapping workflow)."); return
    eng.process()
    merged = eng.merge_inversions()
    print("records=%d  channels=%d  inversions-homed=%d (merged onto %d dest records)  identity-fields=%d"
          % (eng.stats["records"], eng.stats["channels"], eng.stats["inversions"], merged, eng.stats["identity"]))
    # orphan inversions (dest record not found)
    orphans = {}
    by_type = {(e, t) for e, recs in eng.records.items() for t, _ in recs}
    for rec_elem, dests in eng.inversions.items():
        for dt in dests:
            if (rec_elem, dt) not in by_type:
                orphans.setdefault(rec_elem, set()).add(dt)
    if orphans:
        print("\norphan inversion targets (dest Type not found in scope -- enum target or missing entity):")
        for rec_elem, types in sorted(orphans.items()):
            print("  %-20s %d types e.g. %s" % (rec_elem, len(types), ", ".join(sorted(types)[:4])))
    if args.sample and args.sample in eng.records:
        print("\n=== %s sample (post-merge) ===" % args.sample)
        for typ, out in eng.records[args.sample][:2]:
            print(json.dumps(out, indent=1))
    if args.write:
        eng.write()
        print("\nwrote JSON database under Assets/Data/")


if __name__ == "__main__":
    main()
