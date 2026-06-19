#!/usr/bin/env python3
"""cascade_sim -- offline cascade SIMULATOR prototype (calc-emulator-spec.md §2a; cascade-fixed-point.md).

Feeds the NEW cascade model a real loadout and computes per-turn city YIELDS OFFLINE in INTEGER FIXED-POINT (x100,
"2 decimals" -- cascade-fixed-point.md), reading the migrated (human-readable) Assets/Data JSON deposits, IMPORTING
them to x100, evaluating their conditions, and comparing the full output to LEGACY getYieldRate100. The Python
prototype of the in-game cascade engine ("simulate the simulation"), proven here before porting to the DLL.

Sources modelled (calc-emulator-spec §2a): BUILDINGS (city + empire + area scope, dormancy-gated via requires.operate)
+ CIVICS (empire scope -- incl. negative modifiers). Conditions gated via the spec evaluator (data-model + enabler §8).
The base (plot/trade/free/golden + specialist) rides in from the dump (the legacy pre-modifier base).

Run:  python cascade_sim.py --file samples/m_p0.json          # one city, detailed
      python cascade_sim.py --glob "samples/m_*.json"          # sweep many cities, aggregate parity
"""
import argparse
import glob
import json
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BUILDINGS_DIR = os.path.join(REPO, "Assets", "Data", "buildings")
CIVICS_DIR = os.path.join(REPO, "Assets", "Data", "civics")
TRAITS_DIR = os.path.join(REPO, "Assets", "Data", "traits")
YIELDS = ("food", "production", "commerce")

# type-prefix -> the loadout presence set it checks (spec: prefix discriminates atom type)
PREFIX_SET = (("TECH_", "techs"), ("CIVIC_", "civics"), ("BUILDING_", "buildings"),
              ("BONUS_", "bonuses"), ("TERRAIN_", "terrains"), ("FEATURE_", "features"),
              ("IMPROVEMENT_", "improvements"), ("ROUTE_", "routes"))
PRED_PARAM = {"HAS_TERRAIN": "terrains", "HAS_FEATURE": "features", "HAS_IMPROVEMENT": "improvements",
              "HAS_BONUS": "bonuses", "HAS_ROUTE": "routes"}
STATE_PRED = {"HAS_POWER": "isPowered", "IS_POWERED": "isPowered", "IS_CAPITAL": "isCapital",
              "IS_GOLDEN_AGE": "isGoldenAge", "IS_GOLDENAGE": "isGoldenAge", "HAS_RIVER": "river"}

_JSON_CACHE = {}


def _load(path):
    j = _JSON_CACHE.get(path)
    if j is None:
        with open(path, encoding="utf-8") as fh:
            j = json.load(fh)
        _JSON_CACHE[path] = j
    return j


def building_index():
    idx = {}
    for p in glob.glob(os.path.join(BUILDINGS_DIR, "**", "building_*.json"), recursive=True):
        idx[os.path.basename(p).lower()] = p
    return idx


def civic_index():
    idx = {}
    for p in glob.glob(os.path.join(CIVICS_DIR, "**", "civic_*.json"), recursive=True):
        idx[os.path.basename(p).lower()] = p
    return idx


def type_to_filename(btype):
    return "building_" + btype[len("BUILDING_"):].lower() + ".json"


def civic_to_filename(ct):
    return "civic_" + ct[len("CIVIC_"):].lower() + ".json"


def trait_index():
    idx = {}
    for p in glob.glob(os.path.join(TRAITS_DIR, "**", "trait_*.json"), recursive=True):
        idx[os.path.basename(p).lower()] = p
    return idx


def trait_to_filename(tt):
    return "trait_" + tt[len("TRAIT_"):].lower() + ".json"


def build_context(d):
    """Presence context from the loadout. Vicinity sets derived from the plot list (spec §8)."""
    plots = d.get("plots", [])
    def plot_set(k):
        return set(p[k] for p in plots if p.get(k))
    bonuses = set(d.get("resources", [])) or plot_set("bonus")
    state = d.get("state", {}) or {}
    return {
        "techs": set(d.get("techs", [])),
        "civics": set(d.get("civics", [])),
        "buildings": set(d.get("buildings", [])),
        "bonuses": bonuses,
        "bonusesVicinity": plot_set("bonus"),  # bonuses on the city's workable plots (vicinity) -- for connection:vicinity
        "isPowered": bool(state.get("isPowered")),
        "isCapital": bool(state.get("isCapital")),
        "isGoldenAge": bool(state.get("isGoldenAge")),
        "river": any(p.get("river") for p in plots),
        "terrains": plot_set("terrain"),
        "features": plot_set("feature"),
        "improvements": plot_set("improvement"),
        "routes": plot_set("route"),
    }


# ---------------- the condition evaluator (spec contract: data-model + enabler §8) ----------------

def _eval_atom(atom, ctx, uneval):
    if isinstance(atom, str):
        atom = {atom: True}
    if not isinstance(atom, dict):
        uneval.add("?malformed"); return False
    if "type" in atom:
        t = atom["type"]
        if atom.get("min", 1) > 1 or "max" in atom:
            uneval.add(t + " #count"); return False
        # connection:vicinity -> the bonus must be in the city's WORKABLE radius (legacy hasVicinityBonus), NOT just
        # trade-available. cascade_sim previously fired vicinity deposits for any available bonus -> commerce over-count.
        if t.startswith("BONUS_") and atom.get("connection") == "vicinity":
            return t in ctx["bonusesVicinity"]
        for pfx, key in PREFIX_SET:
            if t.startswith(pfx):
                return t in ctx[key]
        uneval.add(t); return False
    for k, v in atom.items():
        if k in PRED_PARAM:
            return v in ctx[PRED_PARAM[k]]
        if k in STATE_PRED:
            return ctx[STATE_PRED[k]] == bool(v)
        uneval.add(k); return False
    return False


def eval_condition(cond, ctx, uneval):
    if isinstance(cond, str):
        return _eval_atom(cond, ctx, uneval)
    if isinstance(cond, dict) and ("all" in cond or "any" in cond or "noneOf" in cond):
        ok = True
        if "all" in cond:
            ok = ok and all(_eval_atom(a, ctx, uneval) for a in cond["all"])
        if "any" in cond:
            ok = ok and any(all(_eval_atom(a, ctx, uneval) for a in grp) for grp in cond["any"])
        if "noneOf" in cond:
            ok = ok and not any(_eval_atom(a, ctx, uneval) for a in cond["noneOf"])
        return ok
    return _eval_atom(cond, ctx, uneval)


def _deposit_active(item, ctx, uneval):
    en, dis = item.get("enabled"), item.get("disabled")
    ok = True if en is None else eval_condition(en, ctx, uneval)
    if ok and dis is not None:
        ok = not eval_condition(dis, ctx, uneval)
    return ok


# ---------------- the IMPORT (human -> x100) + fixed-point summation ----------------

def to_fixed(human):
    """readJson's sole job (cascade-fixed-point §0/§1.1): human JSON number -> integer x100. 7->700, 0.1->10, 25->2500."""
    return int(round(human * 100))


def sum_unit(unit_obj, ctx, uneval, cond_seen):
    """Sum a flat or percent slot IN x100 (import-converted): scalar | {value,enabled?/disabled?} | array. Active only."""
    total100 = 0
    for it in (unit_obj if isinstance(unit_obj, list) else [unit_obj]):
        if isinstance(it, (int, float)):
            total100 += to_fixed(it)
        elif isinstance(it, dict) and "value" in it:
            if "enabled" in it or "disabled" in it:
                cond_seen[0] += 1
                if not _deposit_active(it, ctx, uneval):
                    continue
            total100 += to_fixed(it["value"])
    return total100


def _entity_deposits(ej, family, scopes, ctx, uneval, cond_seen, acc):
    """Add an entity's <family>.<scope>.{flat,percent} over `scopes` into acc (x100). Sub-scopes (tradeRoute/
    improvements/specialists/perPopulation) are NOT summed here -- later sub-passes."""
    fam = ej.get(family)
    if not isinstance(fam, dict):
        return
    for sc in scopes:
        blk = fam.get(sc)
        if not isinstance(blk, dict):
            continue
        for unit in ("flat", "percent"):
            if blk.get(unit) is not None:
                acc[unit] += sum_unit(blk[unit], ctx, uneval, cond_seen)
    # empire.capital sub-scope: a capital-ONLY modifier (legacy getCapitalYieldRateModifier from civics/traits,
    # implicit IS_CAPITAL) -- summed only when the city is the capital.
    if ctx.get("isCapital") and "empire" in scopes:
        emp = fam.get("empire")
        cap = emp.get("capital") if isinstance(emp, dict) else None
        if isinstance(cap, dict):
            for unit in ("flat", "percent"):
                if cap.get(unit) is not None:
                    acc[unit] += sum_unit(cap[unit], ctx, uneval, cond_seen)


def _building_active(bj, ctx, uneval):
    """DORMANCY (enabler §3): a present building is ACTIVE iff its requires.operate holds (empty = always active).
    The cascade equivalent of legacy hasFullyActiveBuilding -- a dormant building deposits NOTHING."""
    req = bj.get("requires")
    if not isinstance(req, dict):
        return True
    op = req.get("operate")
    return True if op is None else eval_condition(op, ctx, uneval)


def simulate_yields(d):
    bidx = building_index()
    cidx = civic_index()
    tidx = trait_index()
    ctx = build_context(d)
    uneval = set()
    cond_seen = [0]
    sim = {y: {"flat": 0, "percent": 0} for y in YIELDS}
    missing = 0
    dormant = len(d.get("dormantBuildings", []))
    # BUILDINGS: ACTIVE only (the dump's `buildings` = hasFullyActiveBuilding). VERIFIED 2026-06-19 (legacy code
    # trace, 4-agent grounding): legacy removes a DORMANT building's yield MODIFIER and FLAT via processBuilding(-1)
    # (resource-disabled via setDisabledBuilding + religiously-limited via setReligiouslyLimitedBuilding), so
    # getBuildingYieldModifier/m_buildingExtraYield100 reflect ACTIVE buildings only. The earlier all-present approach
    # over-counted dormant modifiers, which MASKED the un-wired bonus/power/trait modifiers (owner was right that
    # "something else was at play").
    present = list(d.get("buildings", []))
    for bt in present:
        path = bidx.get(type_to_filename(bt))
        if path is None:
            missing += 1
            continue
        bj = _load(path)
        for y in YIELDS:
            _entity_deposits(bj, y, ("city", "empire", "area"), ctx, uneval, cond_seen, sim[y])
    # CIVICS: empire-scope yield modifiers (incl. NEGATIVE -- the commerce-reducing civics) roll down to the city.
    for ct in d.get("civics", []):
        path = cidx.get(civic_to_filename(ct))
        if path is None:
            continue
        cj = _load(path)
        for y in YIELDS:
            _entity_deposits(cj, y, ("empire",), ctx, uneval, cond_seen, sim[y])
    # TRAITS: empire-scope yield modifiers (+ empire.capital sub-scope) -- legacy CvPlayer yield modifier = civics +
    # TRAITS. cascade_sim previously never summed traits -> the missing trait/capital percent (4-agent grounding).
    for tt in d.get("traits", []):
        path = tidx.get(trait_to_filename(tt))
        if path is None:
            continue
        tj = _load(path)
        for y in YIELDS:
            _entity_deposits(tj, y, ("empire",), ctx, uneval, cond_seen, sim[y])
    return sim, missing, uneval, cond_seen[0], ctx, dormant


def evaluate(d):
    """Per-family cascade-vs-legacy rows + diagnostics. cascade eff100 = base100 x (10000+Spct100)/10000 + Sflat100."""
    sim, missing, uneval, cond_seen, ctx, dormant = simulate_yields(d)
    dumped = {y["family"]: y for y in d.get("yields", [])}
    rows = {}
    for y in YIELDS:
        s = sim[y]
        dy = dumped.get(y, {})
        base, spec, legacy100 = dy.get("base", 0), dy.get("specialist", 0), dy.get("legacy100", 0)
        base100 = (base + spec) * 100
        casc100 = base100 * (10000 + s["percent"]) // 10000 + s["flat"]
        gap = casc100 - legacy100
        gpct = (100.0 * gap / legacy100) if legacy100 else 0.0
        rows[y] = dict(base=base, spec=spec, pct=s["percent"], flat=s["flat"],
                       casc=casc100, legacy=legacy100, gap=gap, gpct=gpct)
    info = dict(missing=missing, uneval=uneval, cond_seen=cond_seen, dormant=dormant,
                name=d.get("cityName", "?"), player=d.get("player"), pop=d.get("population"),
                nbuild=len(d.get("buildings", [])))
    return rows, info


def _print_one(d):
    rows, info = evaluate(d)
    print("=== cascade_sim [%s p%s pop%s]: NEW cascade (x100, buildings+civics, dormancy-gated) vs LEGACY ==="
          % (info["name"], info["player"], info["pop"]))
    print("  family     base spec |   pct100    flat100 | cascade100   legacy100 |     gap    gap%")
    for y in YIELDS:
        r = rows[y]
        print("  %-10s %4d %4d | %8d %10d | %10d %11d | %+8d  %+.1f%%"
              % (y, r["base"], r["spec"], r["pct"], r["flat"], r["casc"], r["legacy"], r["gap"], r["gpct"]))
    print("  buildings %d (%d dormant, %d no-JSON) | %d conditional deposits | %d unevaluable atom kinds"
          % (info["nbuild"], info["dormant"], info["missing"], info["cond_seen"], len(info["uneval"])))
    if info["uneval"]:
        print("    unevaluable: " + ", ".join(sorted(info["uneval"])[:25]))


def _sweep(paths, tol):
    print("=== cascade_sim SWEEP: %d cities, parity vs legacy (adjacent = within +/-%.0f%%) ===" % (len(paths), tol))
    print("  city                 p  pop |   food%    prod%    comm%  | dorm | worst")
    agg = {y: [] for y in YIELDS}
    nadj = {y: 0 for y in YIELDS}
    for p in sorted(paths):
        d = _load(p)
        if not d.get("yields"):
            continue
        rows, info = evaluate(d)
        worst = max(abs(rows[y]["gpct"]) for y in YIELDS)
        for y in YIELDS:
            agg[y].append(rows[y]["gpct"])
            if abs(rows[y]["gpct"]) <= tol:
                nadj[y] += 1
        print("  %-20s %2s %4s | %+7.1f %+8.1f %+8.1f | %4d | %+.1f%%"
              % (info["name"][:20], info["player"], info["pop"],
                 rows["food"]["gpct"], rows["production"]["gpct"], rows["commerce"]["gpct"],
                 info["dormant"], worst))
    n = len(agg["food"])
    print("\n  AGGREGATE over %d cities (adjacent = within +/-%.0f%%):" % (n, tol))
    for y in YIELDS:
        v = agg[y]
        mean_abs = sum(abs(x) for x in v) / max(1, len(v))
        print("    %-10s  parity-adjacent %d/%d  | mean|gap| %.1f%%  | worst %+.1f%%"
              % (y, nadj[y], n, mean_abs, max(v, key=abs) if v else 0.0))


def main():
    ap = argparse.ArgumentParser(description="cascade_sim -- offline cascade simulator (yields; buildings+civics; x100)")
    ap.add_argument("--file", help="one cityInput dump fixture (detailed)")
    ap.add_argument("--glob", help="glob of fixtures to SWEEP + aggregate, e.g. 'samples/m_*.json'")
    ap.add_argument("--tol", type=float, default=10.0, help="parity-adjacency tolerance, +/- percent")
    args = ap.parse_args()
    if args.glob:
        paths = glob.glob(os.path.join(os.path.dirname(__file__), args.glob)) or glob.glob(args.glob)
        _sweep(paths, args.tol)
    elif args.file:
        _print_one(_load(args.file))
    else:
        ap.error("pass --file or --glob")


if __name__ == "__main__":
    main()
