#!/usr/bin/env python3
"""Fast-path batch curator for VERIFIED POCO / data-holder infos (#428) — infos with NO gameplay modifiers
(grouping markers, config, simple definitions). They ride the shared curate_common as-is: text hoisted,
`Flavors`->ai, booleans cleaned, everything else -> `identity` with clean de-Hungarian keys. No modifier
families, no boosts, no per-entity curator. Convert + move on; gameplay-bearing infos get careful curators.

NB: a 0-channel mapping is NOT proof of POCO — the first-pass mapping under-classified real gameplay
(Promotion combat bonuses, Terrain yields, UnitCombat combat modifiers, Property diffusion/manipulators,
LeaderHead AI-personality params). This list is the VERIFIED data-holders only.

  python3 curate_pocos.py            # report
  python3 curate_pocos.py --write    # write all
"""
import argparse
import json
import os

import engine
import curate_common as cc
from store import Store, REPO

# 2026-06-14 PM audit (wf verify-pocos): of the 6 fast-pathed "POCOs", only CivicOption was a true text+identity
# holder. The rest got proper curators or defer to their parent monster: CultureLevel->curate_culturelevel,
# Hurry->curate_hurry, Victory->curate_victory, BonusClass->curate_bonusclass, and CivicOption->curate_civicoption
# (split out at its Tier-A turn, info #10, 2026-06-15). PromotionLine->Promotion pass, SpecialBuilding->Building
# pass — these RIDE THEIR PARENT MONSTER. SpecialBuilding is now emitted PROPERLY by curate_building.py
# (curate_special: the per-player GROUP cap iMaxPlayerInstances -> allowed.empire, json §4.4) — it must NOT be
# re-emitted here as a generic-identity placeholder, or the placeholder OVERWRITES the proper output (the group cap
# silently lands in identity.maxPlayerInstances and never loads). PromotionLine remains the one deferred placeholder.
POCOS = ["PromotionLineInfo"]


def folder(entity):
    base = entity[:-4].lower() if entity.endswith("Info") else entity.lower()
    return engine.plural(base)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print first object of each (or named types)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    for entity in POCOS:
        cfg = cc.EntityConfig(entity)
        _, result = cc.run(cfg, [], store)
        fld = folder(entity)
        print("%-22s -> %-16s %d" % (entity, fld, len(result)))
        if args.sample is not None:
            typ = next(iter(result))
            print(json.dumps(result[typ][0], indent=1, ensure_ascii=False))
        if args.write:
            out_dir = os.path.join(REPO, "Assets", "Data", fld)
            if not os.path.isdir(out_dir):
                os.makedirs(out_dir)
            for typ, (obj, _era) in result.items():
                with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                    json.dump(obj, f, indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
