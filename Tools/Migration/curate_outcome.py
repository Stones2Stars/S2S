#!/usr/bin/env python3
"""Curate OutcomeInfo (OUTCOME_*) to clean JSON -- the #430 outcome-subsystem migration.

The OUTCOME_* info is the mission/kill outcome GATE + identity (name/message) + tier tag. It carries NO reward
payload -- the per-carrier rewards live on the units (curate_unit.py's `outcomes` block, which references the
OUTCOME_* id via `requires.outcome`). This curator emits ONLY the gate:

  - identity.description / identity.message               (Description auto-hoists; Message read below)
  - requires: {build:{all:[...]}}  <- PrereqTech (team) / PrereqCivic (empire) / PrereqBuildings (city) prereqs
  - obsoletedBy: TECH_X                                   <- ObsoleteTech
  - territory: [friendly|neutral|hostile|barbarian]       <- the 4 territory bools, as an allowed-territory whitelist
  - in: "city" | "notCity"  +  coastalCity: true          <- bCity / bNotCity / bToCoastalCity
  - capture: true                                         <- bCapture (fires in a capture context)
  - odds: { PROMOTION_X: extraChance }                    <- ExtraChancePromotions (hunter promos raise the roll)
  - replaces: [ OUTCOME_X ]                               <- ReplaceOutcomes (higher tier prunes lower)

  python3 curate_outcome.py --sample OUTCOME_SUBDUE
  python3 curate_outcome.py --write
"""
import os
from collections import OrderedDict

import engine
import curate_common as cc
from store import REPO

# territory bool tag -> clean whitelist name (owner-approved shape 2026-07-20: a compact enum, not requires.plot preds)
# ⛔ THE THIRD ELEMENT IS THE LEGACY DEFAULT, AND IT IS LOAD-BEARING. The XML read defaulted
# friendly/neutral/hostile to TRUE and barbarian to FALSE:
#     .add(m_bFriendlyTerritory, L"bFriendlyTerritory", true)   ... .add(m_bBarbarianTerritory, L"bBarbarianTerritory")
# so an ABSENT tag means ALLOWED for the first three. Reading absent-as-false inverts the meaning of
# every entry that relies on the default -- and 72 of the 105 outcomes never state bFriendlyTerritory,
# because "allowed in your own borders" was the thing you never had to say. The visible result was
# every outcome mission refused inside your own territory: no butcher on a subdued animal, no captive
# missions, nothing. The tag is only ever written to DENY.
_TERRITORY = [("bFriendlyTerritory", "friendly", True), ("bNeutralTerritory", "neutral", True),
              ("bHostileTerritory", "hostile", True), ("bBarbarianTerritory", "barbarian", False)]

# every field this curator shapes itself -> DROP from the base curate() (else the bool flags leak into identity).
_DROP = ["Message", "PrereqTech", "ObsoleteTech", "PrereqCivic", "bToCoastalCity", "bCity", "bNotCity",
         "bCapture", "PrereqBuildings", "ExtraChancePromotions", "ReplaceOutcomes"] + [t for t, _, _d in _TERRITORY]

CFG = cc.EntityConfig("OutcomeInfo", extra_drop=_DROP)


def _txt(rec, tag):
    v = engine.text(rec.find(tag))
    return v if v and v != "NONE" else None


def _bool(rec, tag, bDefault=False):
    #	An ABSENT tag takes the legacy read's default; only a PRESENT tag is read as a value.
    node = rec.find(tag)
    if node is None:
        return bDefault
    return engine.text(node) in ("1", "true", "True")


def post_process(typ, obj, rec, store):
    # identity.message (Description already hoisted to identity.description by the base curate()).
    msg = _txt(rec, "Message")
    if msg:
        obj.setdefault("identity", OrderedDict())["message"] = msg

    # requires: the apply-time prereqs as one cascade condition tree (bare strings -- scope implied by the id prefix:
    # TECH_->team, CIVIC_->empire, BUILDING_->city). The applier evaluates it via cascadeEvalCondition.
    # ⛔ The condition tree lives INSIDE a TIMING clause, never floating as the base of `requires` (owner): a bare
    # `requires.all` is consumed by no section unit -- CvRequires::parse routes build/operate/spread and sends
    # anything else to the unconsumed-key census. An outcome is a leaf action checked once at the moment it fires,
    # so its timing is `build` (json.md §4.3; the same reason a unit carries build only -- it never goes dormant).
    reqs = []
    pt = _txt(rec, "PrereqTech")
    if pt:
        reqs.append(pt)
    pc = _txt(rec, "PrereqCivic")
    if pc:
        reqs.append(pc)
    for pb in rec.findall("PrereqBuildings"):           # repeated wrappers, one BuildingType each
        b = _txt(pb, "BuildingType")
        if b:
            reqs.append(b)
    if reqs:
        obj["requires"] = OrderedDict([("build", OrderedDict([("all", reqs)]))])

    ot = _txt(rec, "ObsoleteTech")
    if ot:
        obj["obsoletedBy"] = ot

    terr = [name for tag, name, bDefault in _TERRITORY if _bool(rec, tag, bDefault)]
    if terr:
        obj["territory"] = terr

    if _bool(rec, "bCity"):
        obj["in"] = "city"
    elif _bool(rec, "bNotCity"):
        obj["in"] = "notCity"
    if _bool(rec, "bToCoastalCity"):
        obj["coastalCity"] = True

    if _bool(rec, "bCapture"):
        obj["capture"] = True

    odds = OrderedDict()
    for ec in rec.findall("ExtraChancePromotions/ExtraChancePromotion"):
        pr = _txt(ec, "PromotionType")
        ch = engine.text(ec.find("iExtraChance"))
        if pr and engine.is_int(ch):
            odds[pr] = int(ch)
    if odds:
        obj["odds"] = odds

    repl = []
    for ro in rec.findall("ReplaceOutcomes"):           # repeated wrappers, one OutcomeType each
        o = _txt(ro, "OutcomeType")
        if o:
            repl.append(o)
    if repl:
        obj["replaces"] = repl


if __name__ == "__main__":
    cc.main(CFG, [], os.path.join(REPO, "Assets", "Data", "outcomes"), post_process=post_process)
