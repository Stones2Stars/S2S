# HANDOVER — 2026-07-18 (branch `json-data-migration`)

> **TRANSIENT one-time relay** (AGENTS.md handover rules): work done + work upcoming, nothing load-bearing lives
> only here. The DURABLE design is in `docs/plans/structural-cleanup/fixed-point-conformance.md` and the DEC ledger
> (`docs/architecture/decisions.md`) — read THOSE, not this summary. Deletable-without-loss once read.

## DONE this session (wellbeing fixed-point #430 — verified in-game, owner confirmed "numbers correct again")
- **Wellbeing cascade fixed-point conformance (COMPLETE).** ×100 through gather + assemble + every consumer; human
  conversion happens ONLY at reader boundaries (UI / `/computed` / Python via `CyCity`) + the discrete realized
  quantities (`angryPopulation`/`healthRate` = whole citizens). No ×100-variant getter.
- **Root bug (mapped, not guessed) + fixed:** the per-population term's `/100` is the PERCENT DIVISOR
  (`perPopulation` is a percent), NOT the fixed-point reduction — it had been wrongly stripped → 100× on that term
  (London happy 968→**567**, `badHealth` corrected → resolved the DISEASE report). Fix in
  `Sources/Cascade/CvCascadeWellbeing.cpp` assemble: `iPopExtraHappy = iPop * hap.iPpPct / 100;` (+ health twin).
- **Legacy verdict bodies PURGED.** Deleted `happyLevelLegacy/unhappyLevelLegacy/goodHealthLegacy/badHealthLegacy`
  (+ decls). The 4 realized getters are cascade-only (`CascadeAccumulator::wellbeing`, ×100). Endpoint `cityWellbeing`
  + `CyCity` (Python) read the cascade. The what-if (`iExtra`/`bNoAngry`) is an EXPOSED cascade gap (returns the
  base), never legacy.
- **Perf regression fixed as a side effect:** the unit-selection/grouping LAG was legacy's what-if bodies recomputing
  UNCACHED (their serialized caches nuked by #430); gone the instant getters went cascade-only.
- **Consumers wired ×100** across 18 files: `CvCityAI`/`CvPlayerAI` (÷100 each getter, ~91 calls), `CvGame` score,
  `CvPlayer` aggregates, `CvCity` getAdditional*/event-triggers, `CvGameObject` ATTRIBUTE_HAPPINESS, `CvOutcome`,
  `CvDLLWidgetData`+`CvGameTextMgr` UI, `CvHttpServer`, the flipped decomposition getters (getFeature/Bonus/Building*
  → ÷100; they read the cascade).
- **Docs:** `fixed-point-and-scales.md §1` + `DEC-fixedpoint-x100` refined (×100 OUT to consumers, no variant getter);
  added `DEC-no-legacy-masking` + `DEC-legacy-decache-poisons-perf`; `fixed-point-conformance.md` rewritten.

## STATE
- Working tree CLEAN (temp diag removed), **NOT committed** — awaiting owner diff review. `git diff --stat` = 18
  files, ~−120 lines (the `*Legacy` deletion dominates `CvCity.cpp`).
- Deployed Release DLL = the fix + a now-removed-in-tree diag (harmless extra endpoint fields); a rebuild picks up the
  clean tree. Game running on the turn-1337 reference save.
- Build (from `Sources/`): `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" Release build deploy`
  (`Assert build` = ~40s compile check). ⚠ KILL the game before deploy (DLL lock). `agentstart.bat` relaunches;
  poll `http://127.0.0.1:7227/computed/game` for `"turn":1337` (~90s). Verify:
  `/computed/cities/wellbeing?player=0&name=London` — `cascHappy` (×100), `happyLevel` (human).

## NEXT — the wellbeing accumulator CUT (owner-directed, design LOCKED)
Delete the remaining legacy wellbeing ACCUMULATORS, KEEP the per-source breakdown for Python, re-sourced from the
cascade via the **COMBAT-TOOLTIP PATTERN** (`Sources/Engine/CvCombatModel.h` `computeCombatPreview`: ONE producer
returns realized numbers + a `detailLines` vector of `{label, signed value, category}` rows; `CvGameTextMgr` = pure
renderer).
- **Producer:** `CascadeWellbeing::computeBreakdown(city)` → verdicts + happy/health/anger detailLines from the
  cascade terms already in gather/assemble (×100; renderer ÷100). Single-source (patterns.md).
- **Pure renderers:** `setHappyHelp`/`setBadHealthHelp`/`setGoodHealthHelp`/`setAngerHelp` + the endpoint
  decomposition + Python.
- **DELETE the accumulator cluster** (building/bonus/feature ALREADY cut): `extraBuilding`
  (`m_iExtraBuildingGood/BadHappiness`, `…Good/BadHealth`, `…HappinessFromTech`, `…HealthFromTech`), `religion`
  (`m_iReligionGood/BadHappiness` + `updateReligionHappiness`), `specialist` (`m_iSpecialistGood/BadHealth`,
  `m_iSpecialistHappiness/Unhappiness`) — members + `change*`/`update*`/`process*` maintainers + the getters
  (re-point getters to the cascade breakdown). **~106 sites** across `CvCity`/`CvPlayer`(+`CvArea`).
- Full design: `docs/plans/structural-cleanup/fixed-point-conformance.md` → "Wellbeing legacy-accumulator CUT + the
  cascade-sourced breakdown". Do it as ONE focused pass, verified live — no half-cut.

## Owner rulings this session (all ledgered in decisions.md — the durable home)
- **NO LEGACY — purge violently, fail loud** so gaps are visible; legacy MASKING a wrong cascade is worse than legacy
  failing (`DEC-no-legacy-masking`).
- **Legacy-decache poisons ALL perf numbers** — legacy calcs lost their nuked serialized caches, recompute per-read;
  clean perf is only measurable after the full purge (`DEC-legacy-decache-poisons-perf`).
- **×100 out to consumers; blast radius is NEVER a reason to limit the conversion** (`DEC-fixedpoint-x100`, refined).
- On a non-playable branch, a wrong cascade has zero cost — it EXPOSES the defect. Don't gate legacy removal on
  cascade correctness.

## Other channels (later, per plan doc)
Yields / commerce / scalars / properties: the same ×100-to-consumers conformance; dissolve the
`getYieldRate100`/`getYield` split (the "×100 variant" this ruling kills). See `fixed-point-conformance.md` →
"Remaining channels".
