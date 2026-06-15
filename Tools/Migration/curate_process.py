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

  python3 curate_process.py --sample PROCESS_WEALTH
  python3 curate_process.py --write
"""
import os

import curate_common as cc
from store import REPO

CFG = cc.EntityConfig("ProcessInfo")

if __name__ == "__main__":
    cc.main(CFG, [], os.path.join(REPO, "Assets", "Data", "processes"))
