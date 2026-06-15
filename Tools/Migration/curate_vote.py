#!/usr/bin/env python3
"""Curate Vote to the top-down model (#428) — CONFIG for the existing diplo-VOTE subsystem (resolutions).

NOT a POCO. Like Victory, a Vote is structural data for an EXISTING subsystem, NOT a cascade source: it carries
ZERO cascade modifiers. Traced 2026-06-14 PM: a resolution is raised (CvGame::doDiploVote :6920), tallied
(countVote :2893), and on pass its result is recorded (setVoteOutcome :5218) and its effects applied DIRECTLY by
**CvGame::processVote** (:7917-8023) / doVoteResults (:9500) — a hardcoded isX() ladder (changeTradeRoutes,
changeFreeTradeCount, changeForceCivicCount, isOpenBorders/isForceWar/isAssignCity, …). NB: this is NOT the
`CvOutcome` kill/action system (VoteInfo has no OutcomeList; the only "outcome" here is m_paiVoteOutcome = the
vote RESULT). So the effect fields below feed processVote, not a cascade.

**A vote (really a "DiplomaticProposal" — rename DEFERRED to another day, owner 2026-06-14) participates in
NEITHER cascade: no cascade modifier, no cascade enabler. It is a self-contained PROPOSAL — all its data lives ON
it as intrinsic config for the EXISTING vote subsystem (CvGame voting), and it is "relatively neatly packaged":**
- `voteSource` — which VoteSource(s)/council may raise this proposal (DiploVotes; DIPLOVOTE_UN/POPE/CVIENNA).
  Intrinsic to the proposal (which council it belongs to), read by `isVoteSourceType`; NOT a cascade enabler.
- `threshold` — pass rules: `iPopulationThreshold` (% of eligible votes), `iMinVoters` (min eligible),
  `iStateReligionVotePercent` (per-player vote-weight bonus when state religion matches; 0 dropped).
- EXACTLY ONE of (mutually exclusive in the data):
  - `role` — `secretaryGeneral` (SG/Pope/Chair election) or `victory` (the diplo-victory resolution). The
    resolution CLASS, not an effect.
  - `effect` — the on-pass OUTCOME applied by processVote "on the other side of the voting" (owner): a boolean
    toggle (`freeTrade`/`noNukes`/`defensivePact`/`openBorders`/`forcePeace`/`forceNoTrade`/`forceWar`/
    `assignCity`), `tradeRoutes` (+N to the game trade-route pool — an OUTCOME, NOT a cascade modifier), or
    `forceCivics` (force the listed civic(s)).
- `mode` (`cityVoting`/`civVoting`) — tally mode; LIVE consumers (CvPlayer::getVotes) but all-zero in data ->
  emitted only if true (nothing today; kept live, NOT dropped as dead).

  python3 curate_vote.py --sample VOTE_OPEN_BORDERS VOTE_SECRETARY_GENERAL VOTE_SINGLE_CURRENCY VOTE_UNIVERSAL_SUFFRAGE
  python3 curate_vote.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from store import Store, REPO

THRESHOLDS = OrderedDict([
    ("iPopulationThreshold", "population"), ("iMinVoters", "minVoters"),
    ("iStateReligionVotePercent", "stateReligionPercent"),
])
# on-pass boolean OUTCOMES (one per vote) -> effect keys (read by CvGame::processVote / doVoteResults)
EFFECT_FLAGS = OrderedDict([
    ("bFreeTrade", "freeTrade"), ("bNoNukes", "noNukes"), ("bDefensivePact", "defensivePact"),
    ("bOpenBorders", "openBorders"), ("bForcePeace", "forcePeace"), ("bForceNoTrade", "forceNoTrade"),
    ("bForceWar", "forceWar"), ("bAssignCity", "assignCity"),
])
MODE_FLAGS = OrderedDict([("bCityVoting", "cityVoting"), ("bCivVoting", "civVoting")])


def _list(rec, container, child):
    out = []
    node = rec.find(container)
    if node is not None:
        for c in node.findall(child):
            t = engine.text(c)
            if t and t != "NONE":
                out.append(t)
    return out


def _true(rec, tag):
    return engine.text(rec.find(tag)) in ("1", "true", "True")


def curate(typ, rec):
    out = OrderedDict([("type", typ)])
    desc = engine.text(rec.find("Description"))
    if desc:
        out["description"] = desc
    vs = _list(rec, "DiploVotes", "DiploVote")
    if vs:
        out["voteSource"] = vs
    threshold = OrderedDict()
    for tag, key in THRESHOLDS.items():
        v = engine.text(rec.find(tag))
        if engine.is_int(v) and int(v) != 0:
            threshold[key] = int(v)
    if threshold:
        out["threshold"] = threshold
    # role (SG / victory) XOR effect (outcome) -- mutually exclusive in the data
    if _true(rec, "bSecretaryGeneral"):
        out["role"] = "secretaryGeneral"
    elif _true(rec, "bVictory"):
        out["role"] = "victory"
    else:
        effect = OrderedDict()
        for tag, key in EFFECT_FLAGS.items():
            if _true(rec, tag):
                effect[key] = True
        tr = engine.text(rec.find("iTradeRoutes"))
        if engine.is_int(tr) and int(tr) != 0:
            effect["tradeRoutes"] = int(tr)
        fc = _list(rec, "ForceCivics", "ForceCivic")
        if fc:
            effect["forceCivics"] = fc
        if effect:
            out["effect"] = effect
    mode = OrderedDict()
    for tag, key in MODE_FLAGS.items():
        if _true(rec, tag):
            mode[key] = True
    if mode:
        out["mode"] = mode
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 4)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("VoteInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items())
    has = lambda k: sum(1 for o in results.values() if k in o)
    print("VoteInfo curated: %d  | voteSource: %d  role: %d  effect: %d  mode: %d"
          % (len(results), has("voteSource"), has("role"), has("effect"), has("mode")))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:4]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "votes")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d VoteInfo JSON files under Assets/Data/votes" % len(results))


if __name__ == "__main__":
    main()
