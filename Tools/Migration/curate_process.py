#!/usr/bin/env python3
"""Curate Process (#428) — thin config over curate_common. A Process is a per-CITY production-conversion choice
(Wealth/Research/Culture/Espionage): spent hammers convert to commerce at the per-commerce rate. The
`ProductionToCommerceModifiers` array splits into the per-identifier commerce families
(gold/research/culture/espionage) at CITY scope, `percent` unit. `TechPrereq` is the enabler gate (the store
inverts it to the tech's `enables`; Process is an enabler-chain DEPENDENT). No inbound boosts exist.

Verified by the classify-light-batch workflow vs Sources/Infos/CvProcessInfo.{h,cpp} + CvCity::changeProduction
(CvCity.cpp:3807-3825). Stored as a natural percent (30/40/50), NOT x100 fixed-point.

NB (consumer semantic — for the #430 cascade engine to disambiguate, flagged not merged): these commerce
percents are a production->commerce CONVERSION rate (hammers BECOME that commerce), NOT a multiplier on
existing commerce. Same family vocabulary, different math — a reader must not treat `gold.city.percent` here as
"multiply existing gold by X%". DECISION (owner 2026-06-15, option b): KEEP the shared commerce-family shape
(do NOT invent a distinct conversion family) — the conversion-vs-multiply disambiguation is deferred to #430,
which already knows the entity type (a Process ONLY converts). Process is a narrow, special beast; don't expand it.

EXE-LINKS — verified safe (owner cautioned; 2026-06-15): CvProcessInfo has ZERO DllExport methods — it is a
simple DLL-internal class (getTechPrereq + getProductionToCommerceModifier array; consumers are CvCity/CvCityAI/
CvGameTextMgr, never the EXE). The only EXE-bound aspect is the CommerceTypes ENUM (the 4 commerces, hard from
the .exe), already handled by the named-commerce-key convention (readJson maps gold/research/culture/espionage ->
the m_paiProductionToCommerceModifier indices). So the JSON key shape is free; nothing here is EXE-constrained.

ONLY-LATEST SUPERSESSION (owner semantic 2026-07-02): within a process family only the LATEST version whose tech
you hold is choosable. Legacy enforces this IN CODE — the Python `cannotMaintain` callback
(Assets/Python/CvGameUtils.py aMap): a process is refused while any LATER version in its chain is available
(its TechPrereq held; isHasTech(NO_TECH)=true). Data model (enabler-spec §2, the target-side prune): each
non-latest process carries `obsoletedBy.techs` = the enabling techs of every LATER process in its chain — the
cascade's `obsoletedByHeldTech` gate then reproduces the rule with zero process special-casing. PROCESS_IDLE is
chainless (no prereq, always offered — its enables edge lives on the synthetic TECH_GAME_START start node).

  python3 curate_process.py --sample PROCESS_WEALTH
  python3 curate_process.py --write
"""
import os
from collections import OrderedDict

import engine
import curate_common as cc
from store import REPO

CFG = cc.EntityConfig("ProcessInfo")

# The family chains, oldest -> latest (mirrors the CvGameUtils.py aMap; that callback is demolition fodder at
# cutover — this table becomes the sole owner of the chain knowledge).
CHAINS = [
    ["PROCESS_WEALTH_MEAGER", "PROCESS_WEALTH_LESSER", "PROCESS_WEALTH"],
    ["PROCESS_RESEARCH_MEAGER", "PROCESS_RESEARCH_LESSER", "PROCESS_RESEARCH"],
    ["PROCESS_CULTURE_MEAGER", "PROCESS_CULTURE_LESSER", "PROCESS_CULTURE"],
    ["PROCESS_SPY_MEAGER", "PROCESS_SPY_LESSER", "PROCESS_SPY"],
]


def _tech_prereq(store, typ):
    rec = store.table("ProcessInfo").get(typ)
    if rec is not None:
        for c in rec:
            if c.tag == "TechPrereq":
                t = engine.text(c)
                if t and t != "NONE":
                    return t
    return None


def post_process(typ, obj, rec, store):
    for chain in CHAINS:
        if typ in chain:
            later = [_tech_prereq(store, p) for p in chain[chain.index(typ) + 1:]]
            later = [t for t in later if t]
            if later:
                obj["obsoletedBy"] = OrderedDict([("techs", later)])
            return


if __name__ == "__main__":
    cc.main(CFG, [], os.path.join(REPO, "Assets", "Data", "processes"), post_process=post_process)
