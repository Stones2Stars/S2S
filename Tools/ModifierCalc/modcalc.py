#!/usr/bin/env python3
"""
modcalc -- the #430 modifier OLD-vs-NEW formula calculator (owner 2026-06-19).

WHY: legacy's actual value arithmetic was never deliberately designed (only "modifiers flow
top down"); the cascade is the first deliberate design of the formula. This is the offline
FORMULA SANDBOX -- it implements BOTH the legacy combine and the cascade calc-flows and feeds
them the SAME input, so any output delta is PURELY formula-attributable (input parity is
trivial -- the inputs are just the raw contribution lists). Use it to see where/how-much the
formulas diverge and tune the cascade toward PARITY-ADJACENT (close enough that the played
game stays recognizable), and to prototype a new calc-flow before wiring it into the DLL.

This is NOT a game simulator -- no game state, no JSON. It is pure arithmetic on input vectors.

Scope it to the in-DLL model: integer math (matches the engine; no floats), ×1 yields. Legacy
getYieldRate100 (CvCity.cpp) in ×1 terms:
    legacy = (base + specialist) * (100 + Sum(percent)) // 100 + Sum(flat)
(specialist rides INSIDE the percent; building flats are added OUTSIDE -- the "extraYield" term).

Run:
    python modcalc.py spot   --base 5 --spec 0 --flats 3,1,2 --percents 25,50
    python modcalc.py sweep  --tol 10          # grid sweep, report adjacency within +/-10%
    python modcalc.py sweep  --flow unified     # compare the unified-flat-inside flow instead
"""

import argparse
import itertools


# ----------------------------------------------------------------------------------------------
# The formulas. SAME input vector to every one -> the delta is the formula, nothing else.
# An input is: base (int), specialist (int), flats (list[int]), percents (list[int]).
# ----------------------------------------------------------------------------------------------

def legacy(base, specialist, flats, percents):
    """Legacy getYieldRate100 in x1 terms: (base+specialist) x (100+Sum%)/100 + Sum(flat).
    Specialist INSIDE the percent; building flats OUTSIDE. The accidental-but-actual old formula."""
    return (base + specialist) * (100 + sum(percents)) // 100 + sum(flats)


def cascade_legacy_flat_outside(base, specialist, flats, percents):
    """CALCFLOW_LEGACY_FLAT_OUTSIDE -- the CURRENT cascade flow. Matches legacy's placement (flat
    outside), but note: the pilot does NOT yet feed specialist as a deposit, so to see the pure
    FLOW difference we pass the same inputs; the specialist-coverage gap is called out separately."""
    return base * (100 + sum(percents)) // 100 + sum(flats)


def cascade_unified_flat_inside(base, specialist, flats, percents):
    """CALCFLOW_UNIFIED_FLAT_INSIDE -- the spec's unified model (deferred; needs a data rebalance).
    Flats fold INTO the base before the percent multiply -> every flat is scaled by the percent."""
    return (base + sum(flats)) * (100 + sum(percents)) // 100


FLOWS = {
    "legacy_outside": cascade_legacy_flat_outside,
    "unified":        cascade_unified_flat_inside,
}


# ----------------------------------------------------------------------------------------------
# Reporting
# ----------------------------------------------------------------------------------------------

def rel_pct(new, old):
    """Signed relative delta of new vs old, in percent. old==0 -> None (undefined)."""
    if old == 0:
        return None
    return 100.0 * (new - old) / old


def spot(args):
    flats = [int(x) for x in args.flats.split(",")] if args.flats else []
    percents = [int(x) for x in args.percents.split(",")] if args.percents else []
    old = legacy(args.base, args.spec, flats, percents)
    new = FLOWS[args.flow](args.base, args.spec, flats, percents)
    rp = rel_pct(new, old)
    print("input: base=%d spec=%d flats=%s (sum %d) percents=%s (sum %d)"
          % (args.base, args.spec, flats, sum(flats), percents, sum(percents)))
    print("legacy           = %d" % old)
    print("cascade[%s] = %d" % (args.flow, new))
    print("delta            = %+d  (%s)" % (new - old, ("%+.1f%%" % rp) if rp is not None else "n/a"))


def sweep(args):
    flow = FLOWS[args.flow]
    # A coarse grid over the input axes. Flats/percents modelled as a SUM and a COUNT (so "many small
    # sources" vs "few big" is covered) -- the formulas only ever use the sums, so a sum-grid suffices.
    bases     = [0, 2, 5, 10, 25, 50, 100, 250, 550]
    specs     = [0, 2, 10, 50]
    flatsums  = [0, 1, 5, 20, 100, 500, 2000, 5000]
    pctsums   = [0, 10, 25, 50, 100, 200, 500]

    n = 0
    within = 0
    worst = []  # (abs_rel, signed_rel, row)
    inversions = 0  # cascade > legacy while it deposits FEWER sources -> suspicious
    for base, spec, fs, ps in itertools.product(bases, specs, flatsums, pctsums):
        old = legacy(base, spec, [fs], [ps])
        new = flow(base, spec, [fs], [ps])
        rp = rel_pct(new, old)
        n += 1
        if rp is None:
            continue
        if abs(rp) <= args.tol:
            within += 1
        if new > old:
            inversions += 1
        worst.append((abs(rp), rp, (base, spec, fs, ps, old, new)))

    worst.sort(reverse=True)
    print("=== modcalc sweep: legacy vs cascade[%s] ===" % args.flow)
    print("combinations         : %d" % n)
    print("within +/-%d%% (adjacent): %d (%.1f%%)" % (args.tol, within, 100.0 * within / max(1, n)))
    print("cascade > legacy     : %d  (over-shoot count)" % inversions)
    print()
    print("worst divergences (|rel%| desc):")
    print("  base spec  flat   pct |   legacy  cascade |   delta   rel%")
    for _absrp, rp, row in worst[:args.top]:
        base, spec, fs, ps, old, new = row
        print("  %4d %4d %6d %5d | %8d %8d | %+7d  %+.1f%%"
              % (base, spec, fs, ps, old, new, new - old, rp))


def main():
    p = argparse.ArgumentParser(description="modcalc -- #430 modifier old-vs-new formula calculator")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("spot", help="one input -> legacy vs cascade")
    sp.add_argument("--base", type=int, default=5)
    sp.add_argument("--spec", type=int, default=0)
    sp.add_argument("--flats", default="", help="comma list, e.g. 3,1,2")
    sp.add_argument("--percents", default="", help="comma list, e.g. 25,50")
    sp.add_argument("--flow", choices=FLOWS.keys(), default="legacy_outside")
    sp.set_defaults(func=spot)

    sw = sub.add_parser("sweep", help="grid sweep -> adjacency + worst divergences")
    sw.add_argument("--flow", choices=FLOWS.keys(), default="legacy_outside")
    sw.add_argument("--tol", type=float, default=10.0, help="adjacency tolerance, +/- percent")
    sw.add_argument("--top", type=int, default=15, help="how many worst rows to print")
    sw.set_defaults(func=sweep)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
