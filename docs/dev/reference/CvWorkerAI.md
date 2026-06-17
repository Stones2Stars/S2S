# CvWorkerAI

**File:** `CvWorkerAI.h` / `CvWorkerAI.cpp`

## Overview

`CvWorkerAI` is the per-player home for worker-unit planning state. Each
`CvPlayer` owns one instance. It currently contains:

1. **`improveBonus`** — the unified planner that sends a worker to improve a
   resource (bonus) tile. Successor to two legacy implementations:
   `CvUnitAI::AI_improveBonus` (deleted) and `CvWorkerService::ImproveBonus`
   (deleted). Both had behaviour gaps relative to each other; merging
   eliminated the divergence.
2. **`BonusEval` cache** — memoizes the per-plot inner sweep so the same plot
   is not re-scored when another worker (or the same worker on a later
   micro-tick) re-plans within the same turn.
3. **`WorkerScoringWeights`** — the magic-number scalars used by the scoring
   formula, extracted so a future world where two players use divergent worker
   weights is a config change rather than a code change.
4. **Target claim ledger** — scaffolding for cross-unit dedup that bypasses
   `MISSIONAI_BUILD`. Not used by `improveBonus` (the existing
   `AI_plotTargetMissionAIs` + `iMaxWorkers` mechanism is the canonical
   dedup), but kept available for future planning paths that lack that
   machinery. The release hooks in `CvSelectionGroupAI::AI_setMissionAI` and
   `CvUnit::killUnconditional` are idempotent no-ops when no claims exist.

5. **Stateless worker helpers** — `shouldImproveCity` and
   `getFastestBuildForImprovementType`, consolidated from the former
   `CvWorkerService` experiment (now deleted). Static because they carry no
   per-player state; they live here so all worker-planning logic sits in one
   module. The dead `CvWorkerService::IsPlotValid` (no callers) was dropped in
   the move.

All instance state is turn-scoped; `onTurnBegin` wipes everything.

## Wiring

| Site | Purpose |
|---|---|
| `CvPlayer::m_workerAI` | owns the per-player instance |
| `CvPlayer::init()` (both paths) | sets owner |
| `CvPlayer::doTurn()` / `doMultiMapTurn()` | calls `onTurnBegin(gameTurn)` |
| `CvSelectionGroupAI::AI_setMissionAI` | releases claims on `MISSIONAI_BUILD` pivots |
| `CvUnit::killUnconditional` | calls `releaseAllClaimsBy(unitId)` — load-bearing, dying units don't transition through `AI_setMissionAI` |
| `CvUnitAI::AI_workerMove` (and related) | calls `getWorkerAI().improveBonus(this, allowedTurns)` |
| `CvUnitAI` friend declaration | grants `CvWorkerAI` access to the protected `AI_plotValid`, `AI_betterPlotBuild`, `AI_connectPlot` |

## `improveBonus(CvUnitAI* unit, int allowedMovementTurns = -1)`

Returns `true` if a mission was pushed onto the unit's group.

### Parameters

| Param | Meaning |
|---|---|
| `unit` | the worker doing the planning |
| `allowedMovementTurns` | `-1` for unbounded reachable-plot scan (legacy behaviour); `>0` to constrain candidates to plots reachable within that many turns (used by chained planners) |

### Algorithm

1. **Setup** — pull invariants (`ePlayer`, options, `bCanRoute`, weights),
   construct `CvReachablePlotSet`, populate upfront.
2. **Outer loop** — iterate the reachable plot set directly. For each plot:
   1. **Outer filters** — ownership, area, `AI_plotValid`, bonus presence.
   2. **Super Forts close-enough check** — for non-owned plots, verify the
      worker isn't being sent too far from any friendly city.
   3. **Accessibility gate** — `getWorkingCity() || bCanRoute || isConnectedToCapital`.
      The third term is lazy-evaluated (only computed if the first two short-circuit).
   4. **`bDoImprove` decision** — is the current improvement (if any)
      replaceable? Allowed cases: no improvement, ruins, city-radius fort
      replacement of a non-bonus-trading fort. `PLAYEROPTION_SAFE_AUTOMATION`
      blocks all non-ruin replacements.
   5. **Inner build pick** — cache-first; on miss, iterate the sparse
      candidate set (see below) and pick the build with highest improvement
      yield, breaking ties on fastest build time.
   6. **Per-plot scoring** — `iValue` accumulates from base bonus value,
      city-radius yield contributions, fort defense value (where applicable),
      and counter-value penalty for existing improvements. Whole-plot
      multipliers (path turns, atPlot, cityRadius, noTradeable) are applied.
   7. **Dedup gate** — `AI_plotTargetMissionAIs < iMaxWorkers` and
      `getBuildTurnsLeft > 2 * iPathTurns - 1`.
   8. **Best-plot tracking** — `iValue > iBestValue` wins.
3. **Mission push** — for the chosen plot, decide `MISSION_MOVE_TO` vs
   `MISSION_ROUTE_TO`, apply `AI_betterPlotBuild`, and push.

### Inner build pick: sparse iteration

The legacy path scanned every entry in `BuildsRepo::improvementBuilds()` and
filtered late. The current path iterates two sparse sources:

| Source | When | Why |
|---|---|---|
| `CvBonusInfo::getProvidedByImprovementTypes(bonus)` then per-improvement `getBuildTypes()` | always | improvements that trade the plot's bonus |
| `BuildsRepo::cultureBuilds()` | when `!bCityRadius` | culture-providing improvements that extend borders, even if they don't trade the bonus |

The two branches both call the file-scope helper `considerCandidate` which
applies the surviving per-build filters (`feature-remove`, ownership / outside
borders, `canBuild`), increments the qualified count, and updates the running
best on `(yield > best.yield) || (yield == best.yield && time > best.time)`.

`bCultureBranch=true` weakens condition B to require only `isOutsideBorders()`
(culture > 0 is guaranteed by the iteration source), avoiding a redundant
check that would fail in the legacy branch's stricter form.

### Scoring

Scoring happens in two phases.

**Phase A — pick the best build for this plot:**

```
yieldSum = Σ over yield types of pLoopPlot->calculateImprovementYieldChange(impr, yieldType, player)
timeScore = weights.timeScoreNumerator / (buildInfo.getTime() + 1)
```

Primary sort: `yieldSum` descending. Tiebreaker: `timeScore` descending
(equivalent to build time ascending). This means **best improvement always
wins**; among multiple builds for the same improvement (typically only one),
the faster build wins.

**Phase B — score this plot vs other candidate plots:**

```
iValue  = AI_bonusVal(bonus)
        + (bCityRadius ? Σ (weights.improvementYieldInCityRadius * yieldChange
                          + weights.natureYieldInCityRadius * natureYield) : 0)
        + (impr.isActsAsCity && !bCityRadius
            ? defenseRaw / weights.defenseDivisor
              + (impr.isZOCSource ? weights.zocSourceBonus : 0)
              + impr.getCulture
           : 0)
        + (impr.isActsAsCity && bCityRadius && getWorkingCity
            ? threatScaledDefense
           : 0)
        - (existingImprovement ? plot.getImprovementCurrentValue : 0)
```

Then if still positive:
```
iValue += max(0, weights.aiObjectiveScale * bonus.getAIObjective)
iValue *= (getNumTradeableBonuses(bonus) == 0
             ? weights.noTradeableBonusMultiplier : 1)
iValue *= weights.bonusValueMultiplier
iValue *= (unit.atPlot(plot) ? weights.atPlotBonus : 1)
iValue /= (iPathTurns + 1)
iValue *= (bCityRadius ? weights.cityRadiusBonus : 1)
```

The plot with the highest `iValue` wins.

## `WorkerScoringWeights`

| Field | Default | Role |
|---|---|---|
| `timeScoreNumerator` | `10000` | numerator in time-score tiebreaker formula |
| `improvementYieldInCityRadius` | `20` | multiplier on `calculateImprovementYieldChange` |
| `natureYieldInCityRadius` | `10` | multiplier on `calculateNatureYield` |
| `defenseDivisor` | `10` | divides air-bomb + defense raw value for fort scoring |
| `zocSourceBonus` | `3` | flat add when fort improvement is `isZOCSource` |
| `aiObjectiveScale` | `100` | multiplies `bonus.getAIObjective` |
| `bonusValueMultiplier` | `1000` | applied once before path-turns division |
| `atPlotBonus` | `3` | multiplier when worker is already on the plot |
| `cityRadiusBonus` | `2` | multiplier when plot is in city radius |
| `noTradeableBonusMultiplier` | `2` | multiplier when player has no other copies of this bonus |

Access via `GET_PLAYER(p).getWorkerAI().weights()` (const) or
`weights()` (mutable). All defaults reproduce the pre-extraction legacy
behaviour exactly.

## `BonusEval` cache

| Field | Meaning |
|---|---|
| `turnComputed` | game turn the entry was filled (validity check) |
| `bonus` | bonus type at the time of computation (validity check) |
| `bestBuild` | the inner-loop winner; `NO_BUILD` when `!qualified` |
| `score` | `yieldSum` of `bestBuild`'s improvement; semantically log-only after the refactor |
| `qualified` | `true` if at least one build qualified |

**Key:** `(plotIdx, unitType)`. `unitType` is part of the key because
`CvUnit::canBuild` varies by unit type — different worker tiers must not share
entries. Player is implicit (the cache lives on a per-player object).

**Validity rules:** an entry is a hit only if
- the key matches,
- `turnComputed == gameTurn` (defensive; `onTurnBegin` should have cleared it),
- and `bonus == currentBonus` (a plot's bonus can reveal mid-turn).

Cache misses pay for the full inner sweep; cache hits skip directly to the
plot-level scoring. `[WAI/build/hit]` and `[WAI/build/winner]` log lines
distinguish the two outcomes.

## Target claim ledger

Methods: `tryClaim`, `isClaimedByOther`, `releaseClaim`, `releaseAllClaimsBy`.

Reserved for future planning paths. **Not used by `improveBonus`** —
`AI_plotTargetMissionAIs(plot, MISSIONAI_BUILD, group)` already provides the
same dedup with team-build awareness (`iMaxWorkers > 1`), and the player-side
`m_missionTargetCache` makes it amortized O(1) per turn.

If a new planning path is added that does not flow through `MISSIONAI_BUILD`,
the ledger is the right home for its claims. Release hooks are already wired.

## Log taxonomy

`improveBonus` emits tagged log lines that map 1:1 to code sections. Tags are
prefixed `[WAI/...]` for grepping (`WAI` = worker AI / improveBonus).

| Tag | Level | Where in code | Fields |
|---|---|---|---|
| `[WAI/begin]` | 1 | function entry | `owner`, `unit`, position, `allowedTurns`, `searchRange`, `canRoute` |
| `[WAI/plotset-empty]` | 1 | after `plotSet.Populate` if empty | `unit` |
| `[WAI/plot/skip]` | 2 / 3 | outer filter rejected the plot | reason code: `ownership`, `plotInvalid`, `areaMismatch`, `noBonus` (lvl3), `notCloseEnough`, `inaccessible`, `visibleEnemy`, `noPath` |
| `[WAI/plot/close]` | 3 | Super Forts close-enough check ran | `bonus`, `closeEnough` |
| `[WAI/build/hit]` | 2 | inner build pick served from cache | `bonus`, `chosen build`, `qualified`, `yield` |
| `[WAI/build/cand]` | 3 | per-candidate inner-loop evaluation | `build`, `impr`, `yield`, `time`, `timeScore`, `(culture)` flag |
| `[WAI/build/winner]` | 2 / 3 | inner loop final pick | `bonus`, `chosen build`, `qualified` count, `yield`, `time` |
| `[WAI/score]` | 2 | per-plot scoring breakdown | `bonus`, `base`, `yield`, `def`, `counter`, `total`, `path`, `maxW`, `others`, `ok` |
| `[WAI/dedup]` | 2 | plot dropped at the dedup / buildtime gate | reason, `others`, `max` |
| `[WAI/best]` | 1 | new best plot during outer iteration | `bonus`, `build` or `ROUTE`, `value`, mode (`improve` / `connect`) |
| `[WAI/mission]` | 2 | mission decision after the loop | `chosen build`, mission type, `value` |
| `[WAI/end]` | 1 | function exit | `result`: `noTarget` / `build` / `route` / `pushFailed` / `fallthrough` |

### Levels

`gPlayerLogLevel` controls verbosity:

| Level | Includes |
|---|---|
| 1 | headlines: `[begin]`, `[end]`, `[best]`, `[plotset-empty]` |
| 2 | per-plot decisions: all of level 1 plus `[plot/skip]`, `[plot/close]` (some), `[build/hit]`, `[build/winner]` (qualified), `[score]`, `[dedup]`, `[mission]` |
| 3 | per-candidate detail: all of level 2 plus `[build/cand]`, `[build/winner]` (NO_BUILD), some `[plot/skip]` (`noBonus`, `noPath`), `[plot/close]` |

### Reading a session log

Typical successful planning for one worker (level 2):

```
[WAI/begin] owner=0 unit=12345 at=(50,30) allowedTurns=4 searchRange=8 canRoute=1
[WAI/plot/skip] at=(45,28) reason=ownership owner=2
[WAI/build/winner] at=(48,32) bonus=BONUS_WHEAT -> BUILD_FARM qualified=2 yield=4 time=5000
[WAI/score] at=(48,32) bonus=BONUS_WHEAT base=10 yield=80 def=0 counter=0 total=90 path=2 maxW=1 others=0 ok=1
[WAI/best] at=(48,32) bonus=BONUS_WHEAT build=BUILD_FARM value=30000 (improve)
[WAI/mission] at=(48,32) build=BUILD_FARM mission=MOVE_TO value=30000
[WAI/end] unit=12345 result=build at=(48,32) build=BUILD_FARM value=30000
```

### Useful greps

| Question | Grep |
|---|---|
| Final decision for one unit | `grep "WAI/end.*unit=12345"` |
| All plot rejections | `grep "WAI/plot/skip"` |
| Cache hit vs miss ratio | `grep -c "WAI/build/hit"` vs `grep -c "WAI/build/winner"` |
| Plots dropped by the dedup gate | `grep "WAI/dedup"` |
| All plots considered worthy enough to be "best" | `grep "WAI/best"` |
| Worker AI invocations per turn | `grep -c "WAI/begin"` |

### Reading a `[WAI/score]` line

Most information-dense entry — shows how `iValue` was assembled before the
whole-plot multipliers:

| Field | Meaning |
|---|---|
| `base` | `AI_bonusVal(bonus)`: bonus's intrinsic value to this player |
| `yield` | sum of `weights.improvementYieldInCityRadius * yieldChange + weights.natureYieldInCityRadius * natureYield` across yield types (city-radius only) |
| `def` | fort defense value (zero unless improvement `isActsAsCity`; threat-scaled inside city radius) |
| `counter` | `plot.getImprovementCurrentValue` (subtracted when replacing an existing improvement) |
| `total` | `base + yield + def - counter`, after `aiObjectiveScale * AIObjective` is added and `noTradeableBonusMultiplier` is applied |
| `path` | `generatePath` turns from unit to plot |
| `maxW` | `AI_calculatePlotWorkersNeeded` (team-build limit) |
| `others` | `AI_plotTargetMissionAIs(plot, MISSIONAI_BUILD, ownGroup)` |
| `ok` | `1` if both dedup and buildTimeVsPath gates passed |

The final `value` in the `[WAI/best]` line is `total * bonusValueMultiplier`
then `/= (path+1)` and possibly `* atPlotBonus` and / or `* cityRadiusBonus`.

## Related repositories

`Sources/Repos/BuildsRepo` indexes all builds by purpose:

| Method | Returns |
|---|---|
| `improvementBuilds()` | every build whose `getImprovement() != NO_IMPROVEMENT` |
| `routeBuilds()` | every build whose `getRoute() != NO_ROUTE` |
| `cultureBuilds()` | every improvement build whose improvement has `getCulture() > 0` |

Populated in `rebuild()` after XML load and after `updateReplacements()`.
`improveBonus` consumes `cultureBuilds()` for its second iteration source.

## Things deliberately not done

- **Persistence (save / load).** All state types are POD-or-stdlib-of-POD so
  `read`/`write` would be mechanical; not wired today because turn-scoped
  state survives a game reload via re-derivation.
- **Per-improvement yieldSum across the culture branch.** Each `cultureBuild`
  is currently treated as having its own improvement (which it does), so
  `yieldSum` is recomputed per build. If two culture builds ever share an
  improvement this would be wasteful; the list is small enough that the
  optimization isn't worth the complexity today.
- **Outer-loop filter caching.** Plot ownership / area / bonus / accessibility
  checks re-run every replan; only the inner build pick is cached. The outer
  filters are individually cheap.
- **`pLoopPlot->setImprovementCurrentValue()` lazy-init refactor.** The
  planner currently mutates plot state when it encounters an uninitialised
  `getImprovementCurrentValue` reading. Proper home is `setImprovementType()`
  or a load-time fixup; left for a future pass since it crosses module
  boundaries.

## `improveCity(CvUnitAI* unit, CvCity* pCity)`

Returns `true` if a mission was pushed onto the unit's group. Successor to
the legacy `CvUnitAI::AI_bestCityBuild` two-pass + `CvUnitAI::AI_improveCity`
mission-decision body; the latter is now a one-line delegate. `AI_bestCityBuild`
itself is retained for non-AI callers (`CvGameInterface` UI hints,
`AI_nextCityToImprove` cross-city probing).

### Algorithm

1. **Outer loop** — iterate `pCity->getNumCityPlots()`. For each plot:
   1. **Outer filters** — `getWorkingCity() == pCity`, `AI_plotValid`,
      `PLAYEROPTION_SAFE_AUTOMATION` block on non-ruin improvements.
   2. **Inner pick** — cache-first via `CityPlotEval`; on miss, materialise
      from `pCity->AI_getBestBuild(iI)` + `pCity->AI_getBestBuildValue(iI)`
      (the per-city precomputed result of `AI_findBestImprovementForPlot`)
      plus the per-unit `canBuild()` filter. The city-AI's precomputed best
      is the analog of `improveBonus`'s `BonusEval` cached inner pick.
   3. **Path / dedup gate** — `generatePath`, `AI_calculatePlotWorkersNeeded`
      vs `AI_plotTargetMissionAIs`. Mirrors the legacy `iMaxWorkers + 10`
      stack bonus for a worker already standing in a city one step away.
   4. **Per-plot scoring** — `iValue * (atPlot ? 3/2 : 1) / (1 + pathTurns)`.
   5. **Best-plot tracking** — strict `iValue > iBestValue` wins.
2. **Mission push** — `MISSION_MOVE_TO` vs `MISSION_ROUTE_TO` chosen by the
   same rules the legacy `AI_improveCity` used (own-city plot, route-builds,
   movement-cost terrain). `AI_betterPlotBuild` substitution kept.

### Persistent target-city commitment

City *selection* lives in `CvUnitAI::AI_workerMove`, not in `improveCity`
(which only ever touches plots whose `getWorkingCity() == pCity`). A worker
re-runs `AI_workerMove` every time its mission queue empties — i.e. after
*every* single build, since `improveCity` queues only one
`MOVE_TO/ROUTE_TO` + one `MISSION_BUILD`. The legacy selection picked the city
from the tile under the worker's feet (`plot()->getWorkingCity()`), so a worker
crossing another city's working radius mid-job (overlapping radii, or just
passing between cities) would abandon its assignment and start improving the
city it strayed into.

The fix is a per-unit commitment, `CvUnitAI::m_iTargetImproveCity`
(serialized; resolved via `AI_getTargetImproveCity`, which drops the commitment
if the city is razed/captured or on a different area). `AI_workerMove` tries
the committed city first; `AI_improveCity` re-commits on every success, and
`AI_nextCityToImprove` commits to the city it routes toward. When the committed
city has no work left for this worker, `improveCity` returns false and the
commitment is cleared so the worker re-picks normally.

### Inner pick: city-precomputed reuse

The city-AI's per-plot best build is already yield-primary scored (see
`CvCityAI::AI_findBestImprovementForPlot`, which now also uses a
yield-primary + time-tiebreaker comparator — the legacy `>=` made
later-iterated improvements silently win ties, the analog of the
sign-inverted comparator that `improveBonus` fixed). `improveCity` reuses
that score directly; it does not re-rank improvements per worker.

### Caching

`CityPlotEval` cache keyed on `(plotIdx, unitType)` and validated against
the current `gameTurn` + `cityId`. Different worker tiers cannot share
entries because `canBuild()` varies by unit type. Entries invalidate
automatically if the plot's working city flips during the turn.

The cache short-circuits the per-unit `canBuild()` call and the indirection
through `CvCityAI` — useful on the second and subsequent workers per turn,
but does NOT short-circuit `generatePath` (which is per-unit-position).

## Known follow-ups

- **Player-level task queue** — the cache helps within-turn re-planning but
  not the cold-cache cost of the first worker per turn. A player-level "what
  plot needs a worker" queue, computed once per turn, would let all workers
  consume from a shared pool. This is the architectural finish line that the
  current scaffolding anticipates.
- **Sparse improvement iteration in `AI_findBestImprovementForPlot`** — still
  iterates `GC.getNumImprovementInfos()` for every plot. The bonus path got a
  sparse iteration source (`getProvidedByImprovementTypes`); the city analog
  would need an indexed `terrain/feature → buildable improvements` map.
