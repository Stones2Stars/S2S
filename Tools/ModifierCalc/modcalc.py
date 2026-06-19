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
import json
import sys
import urllib.request


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


# ----------------------------------------------------------------------------------------------
# CONSUME a /diagnostic/cityInput dump -- the LIVE game-dump comparison (calc-emulator-spec.md §3/§5).
# Reproduces getYieldRate100 EXACTLY from the dumped input vector (the §3a FIDELITY credential -- the proof
# the emulator faithfully maps the legacy calc, which licenses the DESTROY pass) and mirrors the cascade
# calc-flow offline. Engine formula (CvCity::getYieldRate100, verified):
#   legacy100 = min(cap, max(100, (base + specialist) * modifier + 100 * extraYield)).
# NB `modifier` is the FULL percent (100 + sum%); `extraYield` is the x1-TRUNCATED flat-outside term the
# engine actually uses (sub-100 precision is lost before the x100) -- we consume it verbatim so we truncate
# identically. This is the offline twin of the DLL; the same arithmetic, proven here before it ships.
# ----------------------------------------------------------------------------------------------

def legacy_yield100(base, specialist, modifier, extra_yield, cap):
    """Reproduce CvCity::getYieldRate100 (x100) from the dumped input vector. Integer math, engine-exact."""
    return min(cap, max(100, (base + specialist) * modifier + 100 * extra_yield))


def cascade_apply(base, flat, percent, mult100, flow):
    """Mirror Sources/Cascade cascadeModifierApply for the named flow (the offline twin of the DLL dispatch)."""
    if flow == "legacy_outside":
        # CALCFLOW_LEGACY_FLAT_OUTSIDE: base x (100+percent)/100 + flat (multiplier identity in parity mode)
        return base * (100 + percent) // 100 + flat
    # CALCFLOW_UNIFIED_FLAT_INSIDE: (base+flat) x (100+percent)/100 x mult/100 -- step-wise int div, engine-exact
    return (base + flat) * (100 + percent) // 100 * mult100 // 100


def getModifiedIntValue(v, mod):
    """The engine's cost-asymmetric combiner (CvGameCoreDLL.cpp:689)."""
    if mod > 0:
        return v * (100 + mod) // 100
    if mod < 0:
        return v * 100 // (100 - mod)
    return v


def _check(label, emu, live):
    ok = (emu == live)
    print("  %-24s emu=%-12d live=%-12d %s" % (label, emu, live, "OK" if ok else "*** MISMATCH ***"))
    return ok


def reproduce_commerce(d):
    """Reproduce getCommerceRateAtSliderPercent per commerce (legacy-value-calc-map §2). -> (ok, total) or None."""
    rows = d.get("commerce")
    if not rows:
        return None
    cap = int(d.get("cap", 99000000))
    maxY = int(d.get("maxYield100", 1900000000))
    minTol = int(d.get("minTolFalseAccum", -9999))
    yc = int(d.get("yieldCommerce100", 0))
    prod = int(d.get("prodRate", 0))
    disorder = bool(d.get("isDisorder", False))
    ok = 0
    print("\nCOMMERCE (reproduce getCommerceRateAtSliderPercent):")
    for c in rows:
        if disorder:
            emu = 0
        else:
            iRate = min(maxY, yc)
            iRate = iRate * c["slider"] // 100 + min(maxY, c["baseExtra100"])
            if iRate < cap:
                mod = c["totalModifier"]
                iRate = (iRate * mod // 100) if iRate > 0 else (iRate * 100 // mod)
                iRate += prod * c["prodToCommerce"]
            if iRate < 0 and c["family"] in ("culture", "research"):
                emu = 0
            elif iRate < minTol:
                emu = cap
            else:
                emu = min(cap, iRate)
        ok += 1 if _check(c["family"], emu, c["realized100"]) else 0
    return (ok, len(rows))


def reproduce_defense(d):
    df = d.get("defense")
    if not df:
        return None
    total = max(df["buildingDefense"], df["naturalDefense"]) + df["playerCityDefenseModifier"] + df["bonusDefense"]
    if df["isOccupation"]:
        mod = 0
    else:
        maxd = df["maxDefenseDamage"]
        mod = max(df["extraMinDefense"], total * (maxd - df["defenseDamage"]) // maxd)
    print("\nDEFENSE (reproduce getTotalDefense + getDefenseModifier):")
    ok = (1 if _check("totalDefense", total, df["totalDefense"]) else 0)
    ok += (1 if _check("defenseModifier", mod, df["defenseModifier"]) else 0)
    return (ok, 2)


def reproduce_maintenance(d):
    m = d.get("maintenance")
    if not m:
        return None
    pop = int(d.get("population", 0))
    if (not d.get("isDisorder", False)) and (not m["isWeLoveTheKingDay"]) and pop > 0:
        emu = m["eraInitialPercent"] + getModifiedIntValue(m["baseMaint100"], m["effectiveModifier"])
    else:
        emu = m["eraInitialPercent"]
    print("\nMAINTENANCE (reproduce getMaintenanceTimes100):")
    ok = (1 if _check("maintenanceTimes100", emu, m["maintenanceTimes100"]) else 0)
    return (ok, 1)


def reproduce_growth(d):
    g = d.get("growth")
    if not g:
        return None
    thr = getModifiedIntValue(g["playerGrowthThreshold"], g["popGrowthRatePct"])
    thr = max(1, thr // 2) if g["isHominid"] else max(1, thr)
    print("\nGROWTH (reproduce growthThreshold):")
    ok = (1 if _check("growthThreshold", thr, g["growthThreshold"]) else 0)
    # foodDifference raw (engine adds disorder/foodProduction/pop1 clamps) -- informational
    _check("foodDifference(raw)", g["foodProduced"] - g["foodConsumption"], g["foodDifference"])
    return (ok, 1)


def _div100(v):
    """C++ integer division by 100 (truncate toward zero) -- matches the engine's `x / 100`."""
    return v // 100 if v >= 0 else -((-v) // 100)


def reproduce_health(d):
    """Exact: goodHealth = Σ max(0,·); badHealth = unhealthyPop − Σ min(0,·) − max(0,espionage). (legacy-value-calc-map §3)"""
    h = d.get("health")
    if not h:
        return None
    good_only = [h["freshWaterGoodHealth"], h["featureGoodHealth"], h["bonusGoodHealth"],
                 h["totalGoodBuildingHealth"], _div100(h["improvementGoodHealth"]), _div100(h["specialistGoodHealth"])]
    bad_only = [h["featureBadHealth"], h["bonusBadHealth"], h["totalBadBuildingHealth"],
                _div100(h["improvementBadHealth"]), _div100(h["specialistBadHealth"])]
    signed = [h["extraHealth"], h.get("handicapHealth", 0), h["corporationHealth"], h["extraTechHealth"],
              h.get("playerExtraHealth", 0), h.get("playerCivicHealth", 0), h.get("playerCivilizationHealth", 0),
              h.get("playerWorldHealth", 0), h.get("playerProjectHealth", 0)]
    emu_good = sum(max(0, v) for v in good_only) + sum(max(0, v) for v in signed)
    emu_bad = h["unhealthyPopulation"] - (sum(min(0, v) for v in bad_only) + sum(min(0, v) for v in signed)) - max(0, h["espionageHealthCounter"])
    print("\nHEALTH (reproduce goodHealth / badHealth):")
    ok = (1 if _check("goodHealth", emu_good, h["goodHealth"]) else 0)
    ok += (1 if _check("badHealth", emu_bad, h["badHealth"]) else 0)
    return (ok, 2)


def reproduce_happiness(d):
    """Exact happyLevel (Σ max(0,·) + temp); unhappyLevel informational (foreign/tax/city-limit terms are its own pass)."""
    hp = d.get("happiness")
    if not hp:
        return None
    good_terms = [
        hp.get("revSuccessHappiness", 0), hp["largestCityHappiness"], hp["militaryHappiness"],
        hp["stateReligionHappiness"], hp["buildingGoodHappiness"], hp.get("extraBuildingGoodHappiness", 0),
        hp["featureGoodHappiness"], hp["bonusGoodHappiness"], hp["religionGoodHappiness"], hp["commerceHappiness"],
        hp.get("areaBuildingHappiness", 0), hp.get("playerBuildingHappiness", 0),
        hp["extraHappiness"] + hp.get("playerExtraHappiness", 0),   # summed THEN max(0,·)
        hp.get("handicapHappy", 0), hp.get("vassalHappiness", 0), hp.get("civicHappiness", 0),
        _div100(hp["specialistHappiness"]), hp.get("playerWorldHappiness", 0), hp.get("playerProjectHappiness", 0),
        hp.get("corporationHappiness", 0), hp.get("extraTechHappiness", 0),
    ]
    emu = sum(max(0, v) for v in good_terms)
    if hp.get("happinessTimer", 0) > 0:
        emu += hp.get("tempHappy", 0)
    emu = max(0, emu)
    print("\nHAPPINESS (reproduce happyLevel; unhappyLevel informational):")
    ok = (1 if _check("happyLevel", emu, hp["happyLevel"]) else 0)
    anger = (hp["overcrowdingAnger"] + hp["noMilitaryAnger"] + hp["cultureAnger"] + hp["religionAnger"]
             + hp["hurryAnger"] + hp["conscriptAnger"] + hp["warWearinessAnger"] + hp["revIndexAnger"])
    print("  unhappyLevel live=%d  angerPct-sum=%d (x pop / %d)  [informational]" % (hp["unhappyLevel"], anger, hp["percentAngerDivisor"]))
    return (ok, 1)


def reproduce_greatpeople(d):
    """Exact: getGreatPeopleRate = base × totalModifier / 100 (disorder -> 0). (legacy-value-calc-map §9.5)"""
    gp = d.get("greatPeople")
    if not gp:
        return None
    emu = 0 if d.get("isDisorder", False) else gp["baseGreatPeopleRate"] * gp["totalGPRateModifier"] // 100
    print("\nGREAT PEOPLE (reproduce getGreatPeopleRate):")
    ok = (1 if _check("greatPeopleRate", emu, gp["greatPeopleRate"]) else 0)
    return (ok, 1)


def reproduce_player_gold(d):
    """Exact: finalExpense = anarchy?0 : preInflated × inflationMod/10000; baseNetGold = commerceGold + deals − finalExpense. (§11.1)"""
    g = d.get("gold")
    if not g:
        return None
    fe = 0 if d.get("isAnarchy", False) else g["preInflatedCosts"] * g["inflationMod10000"] // 10000
    base = g["commerceGold"] + g["goldPerTurnDeals"] - fe
    print("\nPLAYER GOLD (reproduce finalExpense + baseNetGold):")
    ok = (1 if _check("finalExpense", fe, g["finalExpense"]) else 0)
    ok += (1 if _check("baseNetGold", base, g["baseNetGold"]) else 0)
    return (ok, 2)


def reproduce_player_science(d):
    """Exact: baseNetResearch = getModifiedIntValue(BASE + commerce(research), nationalTechMod + researchModifier). (§11.1)"""
    s = d.get("science")
    if not s:
        return None
    emu = getModifiedIntValue(s["baseResearchRate"] + s["commerceResearch"], s["nationalTechMod"] + s["researchModifier"])
    print("\nPLAYER SCIENCE (reproduce calculateBaseNetResearch):")
    ok = (1 if _check("baseNetResearch", emu, s["baseNetResearch"]) else 0)
    return (ok, 1)


def report_player_demographics(d):
    dm = d.get("demographics")
    if not dm:
        return
    print("\nPLAYER DEMOGRAPHICS (readings; power is AI-relevant):")
    for k in ("power", "techPower", "unitPower", "assets", "totalPopulation", "realPopulation",
              "totalLand", "totalLandScored", "numMilitaryUnits"):
        if k in dm:
            print("  %-22s %d" % (k, dm[k]))


def report_properties(d):
    """Informational: each PROPERTY_* current value (the city-state reading; per-turn delta = CvPropertySolver, spatial #429)."""
    pr = d.get("properties")
    if pr is None:
        return
    nz = [p for p in pr if p["value"] != 0]
    print("\nPROPERTIES (current values; per-turn delta = solver, reproduction is a follow-up):")
    if not nz:
        print("  (all zero)")
    for p in nz:
        print("  %-28s %d" % (p["type"], p["value"]))


def _load_dump(args):
    if args.file:
        with open(args.file, "r") as fh:
            return json.load(fh)
    try:
        with urllib.request.urlopen(args.url, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except Exception as e:
        return {"error": "fetch failed: %s -- is the game up with Autolog__HttpServer on, and the new DLL "
                         "(with /diagnostic/cityInput) rebuilt+deployed?" % e}


def consume(args):
    d = _load_dump(args)
    if "error" in d:
        print("dump error: %s" % d["error"])
        return 1
    cap = int(d.get("cap", 99000000))
    rows = d.get("yields", [])
    is_player = ("yields" not in d) and ("gold" in d or "demographics" in d)

    if is_player:
        print("=== modcalc consume [playerInput]: player %s ===" % d.get("player", "?"))
    else:
        print("=== modcalc consume [cityInput]: %s (player %s, city %s), pop %s, %d buildings ==="
              % (d.get("cityName", "?"), d.get("player", "?"), d.get("city", "?"),
                 d.get("population", "?"), len(d.get("buildings", []))))

    fid_ok = mir_ok = 0
    if rows:
        print("\nFIDELITY -- emulator-legacy vs live getYieldRate100 (the DESTROY-pass credential):")
        print("  family      base  spec   mod%   extra | emu-legacy   live-legacy |  result")
        for y in rows:
            emu = legacy_yield100(y["base"], y["specialist"], y["modifier"], y["extraYield"], cap)
            live = y["legacy100"]
            ok = (emu == live)
            fid_ok += 1 if ok else 0
            print("  %-10s %5d %5d %6d %7d | %10d  %10d |  %s"
                  % (y["family"], y["base"], y["specialist"], y["modifier"], y["extraYield"],
                     emu, live, "OK" if ok else "*** MISMATCH ***"))

        print("\nCASCADE-FLOW MIRROR -- offline cascadeModifierApply[%s] vs the dumped engine cascade:" % args.flow)
        print("  family      flat  pct  mult100 | emu-cascade  dumped | result")
        for y in rows:
            emu_c = cascade_apply(y["base"], y["cascadeFlat"], y["cascadePercent"], y["cascadeMult100"], args.flow)
            dumped = y["cascade"]
            ok = (emu_c == dumped)
            mir_ok += 1 if ok else 0
            print("  %-10s %5d %4d %7d | %10d  %6d | %s"
                  % (y["family"], y["cascadeFlat"], y["cascadePercent"], y["cascadeMult100"],
                     emu_c, dumped, "OK" if ok else "*** MISMATCH ***"))

        print("\nFORMULA DELTA -- legacy vs cascade (informational; pilot cascade = city-scope buildings only, x1 vs x100):")
        for y in rows:
            print("  %-10s legacy100=%d  cascade=%d" % (y["family"], y["legacy100"], y["cascade"]))

    # ---- channels: exact guards (city + player); health/happiness exact too; properties/demographics informational ----
    extra = []
    for fn, nm in ((reproduce_commerce, "commerce"), (reproduce_defense, "defense"),
                   (reproduce_maintenance, "maintenance"), (reproduce_growth, "growth"),
                   (reproduce_health, "health"), (reproduce_happiness, "happiness"),
                   (reproduce_greatpeople, "greatPeople"),
                   (reproduce_player_gold, "playerGold"), (reproduce_player_science, "playerScience")):
        r = fn(d)
        if r is not None:
            extra.append((nm, r[0], r[1]))
    report_properties(d)
    report_player_demographics(d)

    n = len(rows)
    extra_ok = sum(o for _, o, _ in extra)
    extra_tot = sum(t for _, _, t in extra)
    chan_str = ", ".join("%s %d/%d" % (nm, o, t) for nm, o, t in extra) or "none"
    if rows:
        allok = fid_ok == n and mir_ok == n and extra_ok == extra_tot
        print("\nOVERALL: yields-fidelity %s (%d/%d) | cascade-mirror %s (%d/%d) | channels [%s]"
              % ("PASS" if fid_ok == n else "FAIL", fid_ok, n, "PASS" if mir_ok == n else "FAIL", mir_ok, n, chan_str))
    else:
        allok = extra_tot > 0 and extra_ok == extra_tot
        print("\nOVERALL: channels [%s]" % chan_str)
    return 0 if allok else 1


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

    cs = sub.add_parser("consume", help="validate a /diagnostic/cityInput dump (the live game-dump comparison)")
    cs.add_argument("--url", default="http://127.0.0.1:7227/diagnostic/cityInput?player=0",
                    help="cityInput endpoint URL (default: player 0's capital)")
    cs.add_argument("--file", default="", help="read the dump from a JSON file instead of fetching the URL")
    cs.add_argument("--flow", choices=FLOWS.keys(), default="legacy_outside",
                    help="which cascade calc-flow to mirror (default: the active legacy_outside)")
    cs.set_defaults(func=consume)

    args = p.parse_args()
    sys.exit(args.func(args) or 0)


if __name__ == "__main__":
    main()
