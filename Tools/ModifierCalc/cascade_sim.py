#!/usr/bin/env python3
"""cascade_sim -- offline cascade SIMULATOR prototype (calc-emulator-spec.md §2a).

Feeds the NEW cascade model a real loadout and computes per-turn values OFFLINE, reading the migrated Assets/Data
JSON deposits + evaluating their conditions -- the Python prototype of the in-game cascade engine ("simulate the
simulation"). Condition evaluator ported from the SPEC contract (data-model-spec + enabler-cascade-spec §8), not
reconstructed from code.

Increment 2: the YIELD channel from BUILDINGS, city-scope, WITH condition evaluation. Each building's
`food/production/commerce . city . {flat,percent}` deposit (scalar | {value,enabled?/disabled?} | array) is
evaluated against the loadout's presence context (techs/civics/buildings + plot-derived vicinity bonus/terrain/
feature/improvement/route). Active deposits are summed and compared to the dump's DLL-cascade (cascadeFlat/
cascadePercent). Atoms we cannot resolve offline yet (river, state flags isCapital/isGoldenAge, empire-tally
counts, religions/corps/traits) are TRACKED and the gated deposit is treated as OFF (conservative) -- the report
lists them so we know what loadout data to add next ("start somewhere, find more").

Run:  python cascade_sim.py --file samples/london.json
"""
import argparse
import glob
import json
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BUILDINGS_DIR = os.path.join(REPO, "Assets", "Data", "buildings")
YIELDS = ("food", "production", "commerce")

# type-prefix -> the loadout presence set it checks (spec: prefix discriminates atom type)
PREFIX_SET = (("TECH_", "techs"), ("CIVIC_", "civics"), ("BUILDING_", "buildings"),
              ("BONUS_", "bonuses"), ("TERRAIN_", "terrains"), ("FEATURE_", "features"),
              ("IMPROVEMENT_", "improvements"), ("ROUTE_", "routes"))
# parameterized plot predicates -> the vicinity set scanned (spec §8: default vicinity scope)
PRED_PARAM = {"HAS_TERRAIN": "terrains", "HAS_FEATURE": "features", "HAS_IMPROVEMENT": "improvements",
              "HAS_BONUS": "bonuses", "HAS_ROUTE": "routes"}
# state-boolean predicates -> the ctx flag they read (owner 2026-06-19: model power=on)
STATE_PRED = {"HAS_POWER": "isPowered", "IS_POWERED": "isPowered", "IS_CAPITAL": "isCapital",
              "IS_GOLDEN_AGE": "isGoldenAge", "IS_GOLDENAGE": "isGoldenAge", "HAS_RIVER": "river"}


def building_index():
    idx = {}
    for p in glob.glob(os.path.join(BUILDINGS_DIR, "**", "building_*.json"), recursive=True):
        idx[os.path.basename(p).lower()] = p
    return idx


def type_to_filename(btype):
    return "building_" + btype[len("BUILDING_"):].lower() + ".json"


def build_context(d):
    """Presence context from the loadout. Vicinity sets are derived from the plot list (spec §8: a bonus/terrain/
    feature/improvement is 'in vicinity' if any workable plot has it)."""
    plots = d.get("plots", [])
    def plot_set(k):
        return set(p[k] for p in plots if p.get(k))
    # bonuses: prefer the city's AVAILABLE set (resources = vicinity + trade-connected); fall back to vicinity-only
    # (plot bonuses) for older dumps without the resources field.
    bonuses = set(d.get("resources", [])) or plot_set("bonus")
    state = d.get("state", {})
    return {
        "techs": set(d.get("techs", [])),
        "civics": set(d.get("civics", [])),
        "buildings": set(d.get("buildings", [])),
        "bonuses": bonuses,
        # state booleans (owner 2026-06-19: model power=on). HAS_RIVER is vicinity (any workable plot has a river).
        "isPowered": bool(state.get("isPowered")),
        "isCapital": bool(state.get("isCapital")),
        "isGoldenAge": bool(state.get("isGoldenAge")),
        "river": any(p.get("river") for p in plots),
        "terrains": plot_set("terrain"),
        "features": plot_set("feature"),
        "improvements": plot_set("improvement"),
        "routes": plot_set("route"),
    }


def _eval_atom(atom, ctx, uneval):
    """Three-valued in spirit but returns bool; an atom we cannot resolve offline is recorded in `uneval` and
    treated as False (conservative -- the gated deposit turns OFF, so we under-count rather than fabricate)."""
    if isinstance(atom, str):           # bare predicate -> {PRED: true}
        atom = {atom: True}
    if not isinstance(atom, dict):
        uneval.add("?malformed"); return False
    # presence / count atom with an explicit type
    if "type" in atom:
        t = atom["type"]
        if atom.get("min", 1) > 1 or "max" in atom:     # a real COUNT (empire tally) -- not resolvable offline
            uneval.add(t + " #count"); return False
        for pfx, key in PREFIX_SET:
            if t.startswith(pfx):
                return t in ctx[key]
        uneval.add(t); return False                      # POPULATION / PROPERTY_/ RELIGION_/ CORPORATION_/ TRAIT_/...
    # predicate object: {HAS_TERRAIN: X}, {HAS_POWER: true}, etc.
    for k, v in atom.items():
        if k in PRED_PARAM:
            return v in ctx[PRED_PARAM[k]]
        if k in STATE_PRED:                              # state boolean: {HAS_POWER: true} -> require power on
            return ctx[STATE_PRED[k]] == bool(v)
        uneval.add(k); return False                      # still-unmodelled predicate (stateReligion / latitude / ...)
    return False


def eval_condition(cond, ctx, uneval):
    """all (AND) / any (OR-of-AND-groups) / noneOf (NOT-any) / single atom. (spec contract §2/§6)"""
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
    """active = (enabled missing OR true) AND NOT (disabled present AND true). (spec §6)"""
    en, dis = item.get("enabled"), item.get("disabled")
    ok = True if en is None else eval_condition(en, ctx, uneval)
    if ok and dis is not None:
        ok = not eval_condition(dis, ctx, uneval)
    return ok


def sum_unit(unit_obj, ctx, uneval, cond_seen):
    """Sum a city.flat or city.percent slot: scalar | {value,enabled?/disabled?} | array-of-those. Active only."""
    total = 0
    for it in (unit_obj if isinstance(unit_obj, list) else [unit_obj]):
        if isinstance(it, (int, float)):
            total += int(it)
        elif isinstance(it, dict) and "value" in it:
            if "enabled" in it or "disabled" in it:
                cond_seen[0] += 1
                if not _deposit_active(it, ctx, uneval):
                    continue
            total += int(it["value"])
    return total


def simulate_yields(d):
    idx = building_index()
    ctx = build_context(d)
    uneval = set()
    cond_seen = [0]
    sim = {y: {"flat": 0, "percent": 0} for y in YIELDS}
    missing = 0
    for bt in d.get("buildings", []):
        path = idx.get(type_to_filename(bt))
        if path is None:
            missing += 1
            continue
        with open(path) as fh:
            bj = json.load(fh)
        for y in YIELDS:
            fam = bj.get(y)
            if not isinstance(fam, dict):
                continue
            city = fam.get("city")
            if not isinstance(city, dict):
                continue
            for unit in ("flat", "percent"):
                if city.get(unit) is not None:
                    sim[y][unit] += sum_unit(city[unit], ctx, uneval, cond_seen)
    return sim, missing, uneval, cond_seen[0], ctx


def main():
    ap = argparse.ArgumentParser(description="cascade_sim -- offline cascade simulator (yields, buildings, w/ conditions)")
    ap.add_argument("--file", required=True, help="cityInput dump fixture (loadout + yields)")
    args = ap.parse_args()
    with open(args.file) as fh:
        d = json.load(fh)
    sim, missing, uneval, cond_seen, ctx = simulate_yields(d)
    dumped = {y["family"]: y for y in d.get("yields", [])}

    print("=== cascade_sim [%s]: Python cascade (buildings, city-scope yields, CONDITIONS EVALUATED) vs DLL-cascade ==="
          % d.get("cityName", "?"))
    print("  context: techs %d, civics %d, buildings %d | vicinity bonuses %d terrains %d features %d improvements %d"
          % (len(ctx["techs"]), len(ctx["civics"]), len(ctx["buildings"]),
             len(ctx["bonuses"]), len(ctx["terrains"]), len(ctx["features"]), len(ctx["improvements"])))
    print("  family      py-flat  dll-flat | py-pct  dll-pct | py==DLL?")
    allok = True
    for y in YIELDS:
        s = sim[y]
        dy = dumped.get(y, {})
        dflat, dpct = dy.get("cascadeFlat", 0), dy.get("cascadePercent", 0)
        match = (s["flat"] == dflat and s["percent"] == dpct)
        allok = allok and match
        print("  %-10s %8d %8d | %6d %7d | %s" % (y, s["flat"], dflat, s["percent"], dpct, "OK" if match else "DIFF"))
    if missing:
        print("  [%d present buildings had no JSON file -- naming/module gap]" % missing)
    print("\n  %d conditional deposits evaluated. %d atom kind(s) UNEVALUABLE offline (gated deposits treated OFF):"
          % (cond_seen, len(uneval)))
    if uneval:
        for a in sorted(uneval)[:40]:
            print("    - %s" % a)
        print("  -> these are the loadout-data gaps to close next (river/state flags/empire-tally/religion/corp/trait/...).")
    print("\n  %s" % ("ALL MATCH -- the evaluable conditions reproduce the DLL-cascade." if allok else
                       "DIFF remains -> attributable to the unevaluable atoms above (under-count) + non-building/empire sources."))


if __name__ == "__main__":
    main()
