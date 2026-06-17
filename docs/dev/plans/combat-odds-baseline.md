# Combat-Odds Refactor — Pre-Refactor Baseline (Phase 0)

Reference snapshot of the **current** (post-fix) combat-odds behaviour, captured
before the CvCombatModel unification. Later phases diff against this.

- Captured: turn ~453 (PlotSnapshot_turn_t453.csv), deployed DLL 2026-06-02 12:13
  (FinalRelease, includes the getCombatOdds needed-rounds>=1 clamp and the
  AI_huntRange win-prob gate).
- Source: `HunterAI.log` diagnostics. Numbers are internal (strength = display*100
  under COMBAT_SIZE_MATTERS; odds out of 100).
- These are the **per-unit AI odds path** (`AI_attackOddsAtPlotInternal`, logged as
  `[HAI/oddscalc]`, gate odds<60 && isAnimal). Phase 2 (route AI through engine) must
  reproduce `final` for the same `ourStr/theirStr/dmg*/nr*/climit` inputs.

## A) AI per-unit odds across defender strength (low -> high theirStr)
Format: def ourStr theirStr dmgUs dmgThem nrUs nrThem climit hitLimit base wd rep kb -> final

```
GORILLA       276   240   7  9 15 10 150 0 64 10 0 0 -> 58
INDIAN_RHINO  361   551   7  6 17  8 100 0 59 50 0 0 -> 58
BENGALTIGER   286  1016  11  3 26  7 225 0 52 20 0 0 -> 29
GREATWHITE    200  1284  13  6 24  6 150 0 39 42 0 0 -> 31
BUFFALO_CAPE  600  1705  11  8 26 10 225 0 48 10 0 0 -> 34
GREATWHITE    480  2164  13  7 22  8 150 0 38  0 0 0 -> 22
ORCA          200  2910  16  4 34  5 150 0 32 22 0 0 -> 17
GREATWHITE    300  3213  17  6 25  7 150 0 25  0 0 0 -> 9
GREATWHITE    442  4239  14  7 33  7 225 0 33  0 0 0 -> 9
BUFFALO_CAPE  400  4590  13  5 45  6 225 0 40 50 0 0 -> 32
GREATWHITE    480  5612  17  6 24  6 150 0 26  0 0 0 -> 8
GREATWHITE    480  6257  17  5 30  7 150 0 25  0 0 0 -> 8
GREATWHITE    360  7400  19  5 30  6 150 0 20  0 0 0 -> 4
SEALION_SEA   164  8474  16  4 34  3 150 0 18  0 0 0 -> 1
ORCA          210 10245  21  1 60  4 225 0 24 32 0 0 -> 17
GREATWHITE    300 11886  22  4 38  5 150 0 17  0 0 0 -> 2
SEALION_SEA   111 14744  17  4 37  2 150 0 13  0 0 0 -> 0
GREATWHITE    235 19237  22  4 38  4 150 0 11  0 0 0 -> 1
```

## B) Weak prey now resolve to a high win% (the bug this session fixed)
Lizard/duck/pheasant/kookaburra (defCur 0-1) vs hunters (atkCur ~300-1260) now read
`odds=99` and get `[HAI/target/best]` + `[HAI/engage]` (172 engage events this run),
instead of the old ~6%. Representative enriched skips (non-best candidates):

```
DUCK        defBase=2 defCur=0 atkBase=333 atkCur=276   (odds ~99)
KOOKABURRA  defBase=1 defCur=0 atkBase=1050 atkCur=945  (odds ~99)
```

## How to diff after each phase
1. `grep "[HAI/oddscalc]" HunterAI.log` for the same matchups; `final` should match
   table A (Phases 1-2) — or change only in the known-correct direction.
2. Weak-prey cases should keep reading ~99 and engaging (table B).
3. Phase 3: real combat outcomes (win-rate over many seeded battles) unchanged; no OOS.
4. Preview path (`getCombatOdds`/ACO) is not in these logs — spot-check via the
   in-game combat hover for a couple of matchups, or add a temporary log if needed.
```
