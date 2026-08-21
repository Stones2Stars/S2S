#!/usr/bin/env python
"""verify-plane-c-routes -- every AUTHORED deposit gate must have a way to be withdrawn.

A conditioned deposit is applied when its SOURCE arrives and withdrawn when its ATOM's verdict crosses
(DEC-maintained-sum, plane C). The crossing reaches the packages through the modifier consumer's dependency
routes. A predicate with no route is not a stale gate -- the gate itself reads live and stays correct -- it is a
deposit that is applied once and NEVER withdrawn: the phantom contribution nothing clears, compounding on every
repetition (state-repositories.md).

That failure is silent by construction. The number stays plausible, no build breaks, and the only symptom is a
value drifting upward over a session. So this is a CHECK rather than a rule to remember -- the same move
verify-savemigration and verify-spine-fields make for their own silent classes.

WHAT IT FAILS ON: a predicate that authored data uses as a deposit gate (under an `enabled`/`disabled` subtree)
while the consumer carries no route for it. That is the combination that actually corrupts a number.

WHAT IT ONLY REPORTS: a predicate with no route that no data gates a deposit on. That is headroom, not a defect
-- routing it would be machinery for a gate nobody authors (triggers.md: a verb with zero authorings is an
example, not live data). It lights up as a FAILURE the day someone authors one, which is the whole point.

⛔ If it fires, add the ROUTE -- never widen the tool, and never silence it by moving a predicate into WAIVED
without a reason that is true.
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CONDITION_PARSE = os.path.join(ROOT, "Sources", "Infos", "CvJsonConditionParse.cpp")
CONSUMER        = os.path.join(ROOT, "Sources", "Cascade", "CvModifierConsumer.cpp")
PLOT_CONTEXT    = os.path.join(ROOT, "Sources", "Engine", "PlotContext.cpp")
DATA_ROOT       = os.path.join(ROOT, "Assets", "Data")

# Predicates that need no dependency route of their own, each with the reason it is true.
# ⛔ A row here is a CLAIM. Adding one to quiet the tool, rather than because the claim holds, defeats it.
WAIVED = {
    "CASC_PRED_UNKNOWN":
        "the parse sentinel -- not a predicate",
    "CASC_PRED_LATITUDE":
        "a plot's latitude is fixed for the life of the map; no fact can move it",
    "CASC_PRED_HAS_TERRAIN":
        "a deposit KEY, not a gate -- a terrain change re-applies through the keyed/targeted route",
    "CASC_PRED_HAS_IMPROVEMENT":
        "a deposit KEY, not a gate -- an improvement change re-applies through the keyed/targeted route",
    "CASC_PRED_IS_TAG":
        "the UNIT plane: a unit's resolved values move on promotion / combat-class change, never through a "
        "scope package (state-repositories.md -- UNIT is resolved values, not a package)",
    "CASC_PRED_IS_AIR":
        "a unit DOMAIN test, evaluated against the deposit's unit target; not a scope verdict that crosses",
    "CASC_PRED_IS_SPACE":  "the space plane is unmodelled (json.md)",
    "CASC_PRED_IS_LUNAR":  "the space plane is unmodelled (json.md)",
    "CASC_PRED_IS_MARS":   "the space plane is unmodelled (json.md)",
}


def read(path):
    handle = open(path, "r")
    try:
        return handle.read()
    finally:
        handle.close()


def authored_names():
    """The authored STRING -> CASC_PRED_* map, straight from the one parser."""
    text = read(CONDITION_PARSE)
    pairs = re.findall(r'if\s*\(\s*s\s*==\s*"([A-Z_0-9]+)"\s*\)\s*return\s+(CASC_PRED_[A-Z_0-9]+)', text)
    return dict((name, enum) for name, enum in pairs)


def routed_predicates():
    """Predicates the consumer carries an explicit dependency route for."""
    text = read(CONSUMER)
    return set(re.findall(r'gatedByPredicate\(\s*(CASC_PRED_[A-Z_0-9]+)', text))


def plot_plane_predicates():
    """The PLOT bits, covered GENERICALLY: the plot announces its own verdict crossing carrying the predicate
    id, so one route in the consumer serves every bit and a new bit needs no new case."""
    consumer = read(CONSUMER)
    if "mc_applyPlotPredicate" not in consumer:
        return set(), False
    rules = read(PLOT_CONTEXT)
    block = re.search(r"s_plotBitRules\[\]\s*=\s*\{(.*?)\n\s*\};", rules, re.S)
    if block is None:
        return set(), False
    return set(re.findall(r"(CASC_PRED_[A-Z_0-9]+)", block.group(1))), True


def gate_usage_in_data(names):
    """Predicate strings appearing under an `enabled` / `disabled` subtree -- i.e. as a DEPOSIT GATE.

    Deliberately NOT a bare text search: the same spelling under `outcomes[].requires` gates an outcome at
    mission-execute time, which is a different plane with a different applier and needs no package route.
    """
    used = {}

    def walk(node, under_gate, path):
        if isinstance(node, dict):
            for key, value in node.items():
                walk(value, under_gate or key in ("enabled", "disabled"), path)
        elif isinstance(node, list):
            for item in node:
                walk(item, under_gate, path)
        elif isinstance(node, str) and under_gate:
            token = node[1:] if node.startswith("!") else node
            if token in names:
                used.setdefault(names[token], set()).add(path)

    for current, _dirs, files in os.walk(DATA_ROOT):
        for name in files:
            if not name.endswith(".json") or name == "_order.json":
                continue
            full = os.path.join(current, name)
            try:
                parsed = json.loads(read(full))
            except (ValueError, IOError):
                continue   # a malformed file is readJson's fail-loud census to report, not this tool's
            walk(parsed, False, os.path.relpath(full, ROOT))
    return used


def main():
    if not os.path.isdir(DATA_ROOT):
        sys.stderr.write("verify-plane-c-routes: Assets/Data not found\n")
        return 2

    names = authored_names()
    if not names:
        sys.stderr.write("verify-plane-c-routes: could not read the predicate vocabulary -- "
                         "CvJsonConditionParse.cpp changed shape\n")
        return 2

    routed = routed_predicates()
    plot_covered, plot_route_present = plot_plane_predicates()
    if not plot_route_present:
        sys.stderr.write("verify-plane-c-routes: the GENERIC plot-predicate route is gone from the consumer -- "
                         "every plot bit just lost its withdrawal path\n")
        return 1

    covered = routed | plot_covered | set(WAIVED)
    used = gate_usage_in_data(names)

    failures = []
    for enum in sorted(set(names.values())):
        if enum in covered:
            continue
        if enum in used:
            failures.append((enum, sorted(used[enum])))

    headroom = sorted(e for e in set(names.values()) if e not in covered and e not in used)

    print("predicates: %d authored-nameable, %d explicitly routed, %d covered by the generic plot route, "
          "%d waived" % (len(set(names.values())), len(routed), len(plot_covered), len(WAIVED)))

    if headroom:
        print("no route, and no data gates a deposit on them (headroom, not a defect):")
        for enum in headroom:
            print("    %s" % enum)

    if failures:
        print("")
        print("FAIL -- a deposit is gated on a predicate whose verdict crossing reaches no package.")
        print("       The deposit applies when its source arrives and is NEVER withdrawn (compounding).")
        for enum, files in failures:
            print("  %s" % enum)
            for path in files[:6]:
                print("      %s" % path)
            if len(files) > 6:
                print("      ... and %d more" % (len(files) - 6))
        return 1

    print("plane-C routes clean -- every authored deposit gate has a withdrawal path.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
