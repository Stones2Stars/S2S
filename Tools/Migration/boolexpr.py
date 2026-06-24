#!/usr/bin/env python3
"""Shared BoolExpr -> enabler-condition converter (#428 BoolExpr/settler follow-up, owner 2026-06-16).

Converts the XML `BoolExpr` machinery (And/Or/Not/Has[GOM]/Is[TAG]/integer-compare; Sources/BoolExpr.{h,cpp})
into the LOCKED requires-condition vocabulary (enabler-spec §3): `all`/`any`/`noneOf` over `requires` atoms
({type,scope,...}) + bare / parameterized predicates. Used to retrofit the parked building `ConstructCondition`
+ `NewCityFree` and unit `TrainCondition` BoolExprs into `requires` / `grants.foundBuildings`.

COVERAGE — verified against the live, module-INCLUDED data (Tools/Migration/_survey_boolexpr.py): the three
fields use ONLY And/Or of Has over GOM_{TECH,BONUS,BUILDING,FEATURE,TERRAIN}, `Is TAG_COASTAL`, and a single
`GreaterEqual(ATTRIBUTE_POPULATION, N)` (UNIT_IMMIGRANT). Anything outside that RAISES — so a future module
addition is caught, never silently mis-converted (owner: "if parsing is too cumbersome we hand-recreate with
grants by hand"). Extend the maps below (consciously) when a new node/GOM/tag actually appears.

OWNER RULINGS 2026-06-16 (recorded in enabler-cascade-spec §3 + migration-renames):
  - `Has GOM_TECH X`     -> {type:TECH_X,   scope:team}                     (per-candidate confirm; Tech/Building precedent)
  - `Has GOM_BONUS X`    -> {type:BONUS_X,  scope:city, connection:"trade|vicinity"}  (city has the resource)
  - `Has GOM_BUILDING X` -> {type:BUILDING_X, scope:city}                   (in-city building presence; matches requires_building)
  - `Has GOM_FEATURE X`  -> {HAS_FEATURE: FEATURE_X}                        (parameterized predicate, uniform with HAS_BONUS)
  - `Has GOM_TERRAIN X`  -> {HAS_TERRAIN: TERRAIN_X}                        (parameterized predicate)
  - `Is TAG_COASTAL`     -> "IS_COASTAL"                                    (bare city-is-coastal predicate)
  - `And`->all, `Or`->any (one OR-group), `Not`->noneOf
  - `GreaterEqual(ATTRIBUTE_POPULATION, N)` -> {type:POPULATION, scope:city, min:N}   (established count kind, Building #32)
  RESOLVED 2026-06-16 (owner, hole #1): HAS_TERRAIN/HAS_FEATURE/HAS_BONUS are the CANONICAL single-valued predicates;
    Improvement #22's {terrain|feature|bonus:[...]} is the compact membership SUGAR (desugars to any-of-the-predicate, no
    data churn). So this converter's HAS_FEATURE/HAS_TERRAIN are already canonical. COASTAL_LAND unused in real data (0) →
    moot; IS_COASTAL (CvCity::isCoastal) stays distinct. (data-model-spec §2.5, enabler-spec §3.)
"""
from collections import OrderedDict
import engine


def _default_scope(typ):
    # Mirrors the parser's rjDefaultScope (CvCascadeReadJson.cpp): TECH->team, civic/heritage->empire,
    # everything else (building/bonus/religion/corporation/population/...)->city.
    if typ.startswith("TECH_"):
        return "team"
    if typ.startswith(("CIVIC_", "HERITAGE_")):
        return "empire"
    return "city"


def _atom(typ, scope, **kw):
    # Collapse a plain presence to a BARE STRING -- the parser implies scope from the ID's domain, so a redundant
    # {type, scope} only invites authoring bugs (owner 2026-06-23: object-for-object's-sake is the XML's worst
    # annoyance; promote array->object only when a predicate/special-case actually needs it). Object form ONLY for:
    # any kw (connection/role/min/max), a non-default scope, or a plot-substrate predicate type
    # (TERRAIN_/FEATURE_/IMPROVEMENT_/MAPCATEGORY_) the parser routes to a plot predicate by the `type` key.
    is_plot_pred = isinstance(typ, str) and typ.startswith(("TERRAIN_", "FEATURE_", "IMPROVEMENT_", "MAPCATEGORY_"))
    if not kw and not is_plot_pred and scope == _default_scope(typ):
        return typ
    a = OrderedDict([("type", typ), ("scope", scope)])
    a.update(kw)
    return a


def _has_leaf(gom, ident):
    """`Has GOMType ID` -> a requires leaf (atom dict or parameterized predicate)."""
    if not ident or ident == "NONE":
        return None
    if gom == "GOM_TECH":
        return _atom(ident, "team")
    if gom == "GOM_BONUS":
        return _atom(ident, "city", connection="trade|vicinity")
    if gom == "GOM_BUILDING":
        return _atom(ident, "city")
    if gom == "GOM_FEATURE":
        return OrderedDict([("HAS_FEATURE", ident)])
    if gom == "GOM_TERRAIN":
        return OrderedDict([("HAS_TERRAIN", ident)])
    raise ValueError("boolexpr: unhandled <Has> GOMType %s (ID %s) — extend the converter or hand-recreate" % (gom, ident))


def _is_pred(tag):
    """`Is TAG` -> a bare predicate."""
    if tag == "TAG_COASTAL":
        return "HAS_COAST"   # city-context (TAG_COASTAL = CvCity::isCoastal); HAS_COAST resolves per target
    raise ValueError("boolexpr: unhandled <Is> tag %s — extend the converter or hand-recreate" % tag)


def _int_compare(tag, elem):
    """Integer-comparison nodes -> a count atom at CITY scope. Subject is an ATTRIBUTE (e.g. POPULATION, the
    UNIT_IMMIGRANT case) OR a PROPERTY (e.g. PROPERTY_CRIME — a threshold band, the pseudobuilding/PropertyEffect
    case). Greater=> min N+1, GreaterEqual=> min N, Less=> max N-1, LessEqual=> max N, Equal=> min=max=N."""
    cnode = elem.find("Constant")
    const = int(engine.text(cnode)) if cnode is not None and engine.is_int(engine.text(cnode)) else None
    prop = engine.text(elem.find("PropertyType"))
    attr = engine.text(elem.find("AttributeType"))
    if const is not None and (prop or attr):
        typ = prop if prop else ("POPULATION" if attr == "ATTRIBUTE_POPULATION" else attr.replace("ATTRIBUTE_", ""))
        if tag == "Greater":      return _atom(typ, "city", min=const + 1)
        if tag == "GreaterEqual": return _atom(typ, "city", min=const)
        if tag == "Less":         return _atom(typ, "city", max=const - 1)
        if tag == "LessEqual":    return _atom(typ, "city", max=const)
        if tag == "Equal":        return _atom(typ, "city", min=const, max=const)
    raise ValueError("boolexpr: unhandled integer comparison <%s> attr=%s const=%s "
                     "— extend the converter or hand-recreate" % (tag, attr, const))


def convert(node):
    """A BoolExpr NODE -> a normalized condition: a LEAF (atom dict / predicate str / {PRED:ID})
    | {'all':[...]} | {'any':[[...]]} | {'noneOf':[...]}. None for an empty node."""
    if node is None:
        return None
    tag = node.tag
    kids = [k for k in node]
    if tag == "And":
        parts = [p for p in (convert(c) for c in kids) if p is not None]
        if not parts:
            return None
        return parts[0] if len(parts) == 1 else OrderedDict([("all", parts)])
    if tag == "Or":
        parts = [p for p in (convert(c) for c in kids) if p is not None]
        if not parts:
            return None
        return parts[0] if len(parts) == 1 else OrderedDict([("any", parts)])   # any = || over its DIRECT children (NOT a list-of-OR-groups)
    if tag == "Not":
        parts = [p for p in (convert(c) for c in kids) if p is not None]
        if not parts:
            return None
        return OrderedDict([("noneOf", parts)])
    if tag == "Has":
        return _has_leaf(engine.text(node.find("GOMType")), engine.text(node.find("ID")))
    if tag == "Is":
        return _is_pred(engine.text(node))
    if tag in ("Greater", "GreaterEqual", "Less", "LessEqual", "Equal"):
        return _int_compare(tag, node)
    raise ValueError("boolexpr: unhandled node <%s> — extend the converter or hand-recreate" % tag)


def convert_field(field_elem):
    """ENTRY POINT for a field WRAPPER (<ConstructCondition>/<NewCityFree>/<TrainCondition>): the wrapper holds the
    BoolExpr as its single child. Returns the condition node (or None if empty)."""
    if field_elem is None:
        return None
    kids = [k for k in field_elem]
    if not kids:
        return None
    if len(kids) == 1:
        return convert(kids[0])
    parts = [p for p in (convert(k) for k in kids) if p is not None]  # implicit AND of multiple children
    if not parts:
        return None
    return parts[0] if len(parts) == 1 else OrderedDict([("all", parts)])


def _is_structural(node):
    return isinstance(node, dict) and ("all" in node or "any" in node or "noneOf" in node)


def merge_into(node, allc, anyc, none):
    """Fold a converted condition node into requires.build's `all`/`noneOf` lists, IN PLACE. Flattens top-level AND
    into allc; an `any` (OR) node is appended WHOLE into allc as a NESTED `{any:[...]}` child (any = || over its
    members, AND-ed with the rest of allc); a NOT goes to none; a leaf appends to allc. `anyc` is vestigial (the old
    AND-of-ORs accumulator) and is no longer written -- OR-groups live nested under allc now."""
    if node is None:
        return
    if _is_structural(node):
        if "all" in node:
            for m in node["all"]:
                merge_into(m, allc, anyc, none)
        elif "any" in node:
            allc.append(node)          # nested {any:[...]} OR-group under the top-level AND
        else:
            none.extend(node["noneOf"])
    else:
        allc.append(node)


def fold_or_groups(allc, anyc):
    """Move a curator's accumulated OR-GROUPS (`anyc`: a list of groups, each a list of leaves -- the legacy
    `build_any` accumulator) into `allc` as NESTED `{any:[...]}` children, so `any` is always a plain OR and the
    top-level requires is a single AND tree (never the retired `any:[[...]]` list-of-groups). A single-member group
    collapses to its bare leaf."""
    for g in anyc:
        allc.append(g[0] if len(g) == 1 else OrderedDict([("any", g)]))
    del anyc[:]
