# Enabler frontier perf — kill the every-turn full rebuild (owner ruling 2026-07-06)

> **The problem (measured, live `[MODIFIER/perf]`):** ~5.2M condition evaluations per turn, dominated by
> `ceOperatingBuildings` 2.8M, `ceFrontU` 1.4M, `ceFrontB` 1.2M. Frontier FILL counts per turn: **unit 446, building 149,
> pp 42**. Turns are far over the 1-minute goal. This is the "way too many needless calcs."

## The model (owner's original design intent, reconciled with the data)

- **The walk DOWN (GENERATE — the `enables` frontier / the buildable+trainable sets) is a pure function of HAVE.**
  It should be computed **once per HAVE-change**, not every-time. Today it is torn down and rebuilt hundreds of
  times per turn by events that mostly don't affect it.
- **The walk UP (GATE — `requires`) is the dynamic part** and **stays** — the data proves it must: across the
  curated set, **~75% of building requires and the large majority of unit requires are AND** (multi-condition,
  often at different scopes with live predicates — connected / IS_CAPITAL / count thresholds). A pure top-down
  single-enable inversion (enabler.md §5 "the load-bearing asymmetry") **cannot** flatten AND, so the up-walk is
  kept — but it re-runs **INCREMENTALLY over only the affected candidates**, not the whole frontier.
- **The reverse index is what makes the up-walk incremental:** at load, invert every `requires` into
  "HAVE-atom → the entities that reference it," so a HAVE-change re-gates only its dependents.
- **The per-slice rebuild is the correctness NET.** `playerSliceRebuild` already marks every city's packages
  all-dirty once per player per turn (frontier stays lazy → rebuilt on first read). That is the "down once"
  floor. Over-inclusion in the reverse index is safe (a few extra re-checks); a MISSED class self-heals at the
  slice boundary (at worst a one-slice staleness — the accepted wellbeing-lag class). **Validation: the legacy
  shadow re-finds divergences after the cut** (owner 2026-07-06: "do it completely, then we re-find divergences,
  that is why we still have the shadow legacy").

## Root cause (mapped, `file:line`)

`buildingProcessed` (`CvCascadeAccumulator.cpp:816`) — fires on **every building added/removed in a city** —
marks `CPK_ALL & ~(YSPEC|CSPEC|SCSPEC|FRONT_B)` dirty, which **includes `CPK_FRONT_U` + `CPK_FRONT_PP` + the operating buildings
fixpoint**. Buildings got an incremental path (`onBuildingChanged`, `CvCascadeBuildingCascade.cpp:338`, so
`FRONT_B` is excluded — building fills are only 149). **Units never got one** — `CvUnit.cpp` never dirties the
cascade; the only unit-box maintenance is a targeted erase on train-order (`CvCity.cpp:16113`). So every building
completion (+ city growth `setPopulation:7019`, religion `setHasReligion:15404`, corp `setHasCorporation:15609`)
blanket-dirties the whole unit frontier, and the next AI `canTrain` read forces a full **~3340-unit re-walk**
(`UnitCascade::trainable`, `CvCascadeUnitCascade.cpp:73`, one `cascadeEvalCondition(requiresBuild)` per unit at
`:96`). 446 of those/turn = the 1.4M `ceFrontU`. The operating buildings fixpoint
(`EnablerKernel::recomputeOperatingBuildingsInto`, `CvCascadeEnablerKernel.cpp:199`) rides the SAME triggers, so it
recomputes alongside → the 2.8M `ceOperatingBuildings`.

## The reverse-index state (owner's original ask — all confirmed)

- `bc_buildIndices()` is built **LAZILY** on first `onBuildingChanged` (`CvCascadeBuildingCascade.cpp:166`),
  NOT at load.
- `s_bcBuildingDeps` (building→dependent-buildings) is the **ONLY** bucket consumed (`onBuildingChanged` `:350`).
- The HAVE-atom→dependents buckets (`s_bcTech`/`s_bcBonus`/`s_bcPop`/`s_bcReligion`/`s_bcCorp`/`s_bcPower`/
  `s_bcGolden`/`s_bcStateRel`/`s_bcCivic`, `:162-191`) are **populated but READ NOWHERE** — built but dead.
- **Units have NO reverse index at all.**

## The fix — do all of it (owner: complete)

### A — fill the reverse indices at LOAD (not lazily)
Move `bc_buildIndices()` off its lazy first-`onBuildingChanged` trigger to a load-end hook — the eager cache
warm-up block `CvGame::onFinalInitialized` (`CvGame.cpp:632-646`), where plot/city caches already warm. Build a
**UNIT reverse index** there too (mirror the `s_bc*` scan over unit `requiresBuild` atoms). Idempotent
(`s_bcIdxBuilt`), so a load-end call is safe.

### B — unit incremental path + STOP the blanket dirty (the biggest lever)
- Add `UnitCascade::onUnitChanged` mirroring `BuildingCascade::onBuildingChanged` (`:338`): a unit trained/lost
  re-checks only the changed unit + its dependents (units whose `capped()` count / cap depends on it) via the
  unit reverse index, in place — no full rebuild.
- Add a unit HAVE-change recheck (mirror `bc_recheckBuildings` `:299`) over the unit reverse-index bucket.
- **REMOVE `CPK_FRONT_U` + `CPK_FRONT_PP` from `buildingProcessed`'s blanket dirty** (`CvCascadeAccumulator.cpp:816`).
  A building change affects unit trainability **only** via a shared prereq or a `provides.bonuses` a unit needs
  in-vicinity — so instead, on building change, re-check only the units the building's provides/enables reference
  (via the unit reverse index for provided bonuses). Correctness held by the index + the slice net.

### C — consume the building HAVE-atom buckets
On a specific HAVE change, call `bc_recheckBuildings` over **only** that bucket's ids, instead of dirtying
`CPK_FRONT_B`/`CPK_FRONTIER` broadly. Wire the event sites: `setPopulation` (`CvCity.cpp:7019`, → `s_bcPop`),
`setHasReligion` (`:15404`, → `s_bcReligion`), `setHasCorporation` (`:15609`, → `s_bcCorp`), tech
(`CvTeam.cpp:4970`, → `s_bcTech[id]`), civic (`CvPlayer.cpp:14370`, → `s_bcCivic`), golden-age
(`CvPlayer.cpp:9459`, → `s_bcGolden`), power (→ `s_bcPower`), state-religion (→ `s_bcStateRel`). Same for the
unit reverse index.

### D — narrow the operating buildings co-dirtying
Operating buildings is co-dirtied with the frontiers by the same events, so A-C already cut it. Additionally: a completed
building only changes the active/dormant/`provides` status of buildings sharing its dormant-triggers or
`provides.bonuses`, so `recomputeOperatingBuildingsInto` should be gated to the affected subset rather than a full
`markAllDirty` (`CvCascadeAccumulator.cpp:817`) + full recompute (`CvCity::refreshOperatingBuildings` ignores its mask,
`CvCity.cpp:11375` — give it a real one). Same reverse-index principle applied to the operating buildings fixpoint.

## Safety + validation
- The **per-slice rebuild stays** as the net (never removed). Mid-turn incremental is an optimization on top; a
  miss self-heals at the slice boundary.
- **Validate via the legacy shadow** after: re-find divergences (cascade `can*` vs legacy `can*`) to catch any
  stale-frontier correctness regression, then fix.
- **Measure:** the **wall** metric (end-turn press → next-turn start, StoneBase perf store) before/after, plus the
  `[MODIFIER/perf]` component counters (`condEval`, the `ceX` split, `frontUFills`/`frontBFills`, the frontier
  stopwatches) which are idle-independent. Target: `ceFrontU`/`ceFrontB`/most of `ceOperatingBuildings` (~4.8M of 5.2M) go
  away as the fill counts collapse from hundreds to ~one-per-city-per-turn.

## Stage 2 — the operating buildings as a delta-maintained package (the keystone, owner greenlit 2026-07-06)

> **Why this is the real fix (owner):** ">1M calcs for any unit/building evaluation is nutty; the yield/percentage
> PACKAGE concept should obviate a lot of that." Stage-1 (reverse-index) cut the frontier re-walk FREQUENCY but each
> fill still RE-EVALUATES. The operating buildings fixpoint (`ceOperatingBuildings` ~2M, the biggest condEval caller) is the keystone: it is the
> **active building set** (`requiresOperate` holds ∧ no dormant successor ∧ present) + the **in-vicinity provided
> bonuses**, and it feeds BOTH the enabler frontier AND the modifier packages. Today it is a full least-fixpoint
> recompute (`recomputeOperatingBuildingsInto`, `CvCascadeEnablerKernel.cpp:199`) on every operating buildings-dirty. Make it a
> **maintained package** ([modifier-substrate.md](modifier-substrate.md) §1 applied to the operating buildings): reads O(1), a
> HAVE-change applies a bounded DELTA, no full recompute.

**The active↔provides fixpoint is the hard part** — a building's active state can depend on a bonus another active
building `provides` in-vicinity, so an active-flip ripples. The delta is a **work-list incremental fixpoint**, bounded
by the ripple:

1. **An OPERATE-specific reverse index (load-time).** For each building's `requiresOperate`, record which HAVE-atom
   classes it references (tech/civic/religion/corp/bonus/pop/power/GA) AND which bonuses it CONSUMES in-vicinity
   (the `connection:"vicinity"` bonus atoms — the provides-ripple edges). Distinct from the Stage-1 `s_bc*` (which
   mixes build+operate); this is operate-only. Build a per-bonus "consumers" map for the ripple.
2. **A reference-counted provided set.** The provided bonuses are a UNION over active buildings' `provides.bonuses`;
   store a per-bonus COUNT so removing one active building only un-provides a bonus when the count hits 0.
3. **The delta on a HAVE-change H.** Seed the work-list with the buildings whose `requiresOperate` references H
   (operate reverse index) + (if H is a building change) the changed building. Pop each; re-evaluate its active
   verdict under the CURRENT provided set; if it FLIPS, update the active set + the provided counts, and push the
   consumers of any bonus whose provided-count crossed 0/1 (via the per-bonus consumers map). Runs until the
   work-list drains — O(affected + ripple), not O(all present buildings).
4. **Narrowed dirty triggers fall out.** Operating buildings is only touched when H is in the operate reverse index — so
   tech/civic/GA/religion/corp that no building's `requiresOperate` references stop triggering operating buildings work entirely
   (a big frequency win on top of the per-event cost win).
5. **Dynamic predicates → a bounded per-turn re-check.** `requiresOperate` clauses that read live non-HAVE state
   (connection, IS_CAPITAL, count thresholds) can't be tracked by HAVE-atom deltas; collect the (small) set of
   buildings with such operate clauses and re-check ONLY them once per turn (the bounded pass). The per-slice
   rebuild remains the ultimate net.
6. **Load-time init.** Build the package once at load (`onFinalInitialized` warm-up, beside the Stage-1 indices) —
   the one full computation, per the capstone "full rebuild = LOAD ONLY."

**⛔ VALIDATION built in — the full recompute is the SHADOW ORACLE.** Keep `recomputeOperatingBuildingsInto` and, under the
shadow gate, diff the delta-maintained (active, provided) against a fresh full recompute per city per turn; emit
`operatingBuildingsDiverging` on the `[MODIFIER/perf]` line (mirror the heal-diff). Drive it to 0 before trusting the delta;
the full recompute is the oracle exactly as the legacy shadow is for the enabler. This is the owner's "do it, then
re-find divergences" applied to the operating buildings.

**Measure:** `ceOperatingBuildings` (target: ~2M → a fraction) + total condEval + the fill counts (the frontier reads get cheaper
operating buildings) + `wall` once StoneBase is up.

## See also
- [enabler.md](../../specs/enabler.md) §5 (the load-bearing AND asymmetry — up-walk kept) · §7 (recompute cadence).
- [state-repositories.md](../../architecture/state-repositories.md) (the `CvDerivedCache`/dirty model + the
  turn-end unified rebuild end-state this feeds) · [DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king).
