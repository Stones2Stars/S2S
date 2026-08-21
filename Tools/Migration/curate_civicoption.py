#!/usr/bin/env python3
"""Curate CivicOption (#428) — the civic SLOT / CATEGORY axis. Thin config over curate_common.

In plain terms (owner 2026-06-15): it is just the ORGANIZER for the civics in the CIVIC SCREEN — the
slot/heading each civic is grouped under (Government, Economy, Religion, …). Nothing more.

NOT inert, but correctly text+identity (2026-06-14 PM audit, wf verify-pocos — the lone genuine "POCO" of the
fast-path batch). A CivicOption defines a civic SLOT/category (Government, Economy, Religion, …): the C++ uses
`CivicOptionTypes` as the per-slot active-civic enum index (`CvPlayer::getCivics`/`setCivics`), and the civics
page is grouped Python-side by `getCivicOptionType` (`CivicData.py`). So the categorization is consumed
CIVIC-side (each civic carries its `CivicOptionType`); the OPTION entity itself carries only `type` + the
display `description`. The XML sets NO other field (only Type + Description appear); `m_bPolicy` is a dead C++
member (shadowed by per-civic `isPolicy`) and is absent from the data → nothing to drop. No DllExport on
CvCivicOptionInfo (not EXE-bound; verified 2026-06-15). No enables/cascade — a pure structural axis.

  python3 curate_civicoption.py --sample CIVICOPTION_GOVERNMENT
  python3 curate_civicoption.py --write
"""
import os

import curate_common as cc
from store import REPO

CFG = cc.EntityConfig("CivicOptionInfo")

if __name__ == "__main__":
    cc.main(CFG, [], os.path.join(REPO, "Assets", "Data", "civicoptions"))
