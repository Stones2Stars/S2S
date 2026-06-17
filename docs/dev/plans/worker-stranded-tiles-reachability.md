# Worker reachability for stranded border tiles

## Problem (confirmed from logs, Jun 2026)

AI border cities leave tiles unimproved. Root-caused via `[WAI/city/*]` logging in
`BuildEvaluation.log`:

- **89%** of worker `noPath` skips are on a territorial border (`border=1`: 2681 vs 323).
- **54%** of "built nothing" city visits (`[WAI/city/frontier] found=0`) are border-blocked;
  ~45% are benign late-game saturation.
- Every `noPath` tile is the player's **own** land (`own=1`, 3772/3772) with **no enemy on it**
  (`enemyOnPlot=0`), mostly **close** (2107 at dist≤2, 1104 at 3-5).
- **Gold is not a factor**: `goldShort=0` across 19,497 evaluated builds (the `canBuild`
  gold gate at `CvPlayer.cpp:7507` never fired).

Interpretation: a land worker can't reach its own frontier tiles because the connecting
route is impassable to it — the tile sits in a salient cut off by **closed foreign borders,
water, or peaks**. This is territory fragmentation at the frontier, not gold, enemies, or
worker AI scoring.

## Shared foundation: stranded-tile detection

Both fixes need the same primitive: *which own tiles are unreachable by land, and why.*

Two implementation options:
- **(A) Connectivity components (principled).** BFS the player's worker-traversable land
  (own + open-borders land, excluding water/peaks/closed-foreign) into connected components.
  An own tile in a component that contains no worker source (city/worker) is *stranded-by-land*.
  Position-independent and computed once per player per turn (invalidate on border/OB/tech
  change). Correct, but a new subsystem.
- **(B) Cache the generatePath verdict (cheap).** Memoise per-(player, plot, turn) the
  land-unreachable result already computed in `improveCity`, so repeated evaluations across
  workers/cities in a turn don't re-run pathfinding. Less correct (position-dependent for the
  source worker) but a quick efficiency win.

Recommendation: ship (B) as the immediate efficiency layer; build (A) as the durable core
that both tracks consume.

### Phase 0 — barrier classification (DONE)

`[WAI/city/plot/skip] reason=noPath` now logs `adjOwn / adjForeign / adjWater / adjPeak`
(adjacency of the stranded tile). Aggregating this sizes the sub-cases:
- `adjForeign` dominant, `adjOwn=0` → **Open-Borders** case.
- `adjWater` dominant → **sea-transport** case.
- `adjPeak` dominant → likely **unavoidable**.
- `adjOwn>0` but still noPath → larger stranded salient (whole pocket cut off).

Play a few turns, then:
```
grep 'reason=noPath' BuildEvaluation.log \
  | grep -oE 'adjForeign=[0-9]+ adjWater=[0-9]+ adjPeak=[0-9]+' | sort | uniq -c | sort -rn | head
```
The dominant bucket decides where to invest first.

## Track #1 — efficiency (SHIPPED)

Implemented as a **per-worker, per-turn path-unreachable memo** in `CvWorkerAI`
(`isPathKnownUnreachable`/`markPathUnreachable`, members `m_pathUnreachable` +
`m_pathMemo{UnitId,UnitPlot,Turn}`). In `improveCity`, before the `generatePath` call we
skip plots already found unreachable this pass; on a `generatePath` failure we record the
plot. Key correctness constraints:
- **Negative-only:** `generatePath` has a side effect (sets the unit's current path, read by
  downstream `getPathMovementRemaining()`), so reachable plots always run the real call;
  only failures are memoised (a failed plot is `continue`d, needing no path state).
- **Per-worker, position-scoped:** the memo clears whenever (unit, unit-plot, turn) changes,
  so it never leaks one worker's reachability onto another — critical so a pocket-built
  local worker (Track #3) still discovers it can reach its own tiles.
Collapses the repeated `generatePath` to the same stranded tile across a worker's overlapping
city radii (was ~23x re-evaluated). Frontier counters stay accurate; repeated detailed
noPath log lines are suppressed (less log spam) since the tile was logged on first encounter.

### Original framing (superseded by the above)

Once a tile is known stranded-by-land:
- Skip it in the land worker's `improveCity` evaluation (no repeated `generatePath`).
- Don't let it inflate a city's "needs work" so workers stop being routed toward cities
  they can't actually serve (the `found=0`-after-travel waste).
- Implemented over foundation (A) or (B). Low risk; immediate throughput gain.

## DECISIVE classification (Jun 2026, joined noPath tiles vs PlotSnapshot t1135)

444 distinct stranded tiles, classified by landmass `area` vs their city's area:
- **389 (88%) land-blocked on the SAME landmass as their city** → a land path exists ignoring
  borders → blocked by **closed foreign borders** → **Open Borders is the dominant fix.**
- 51 (11%) the target tile is itself a **peak** (worker can't traverse; e.g. Mountain Mine) —
  separate, tech/ability-gated, mostly accept.
- 4 (1%) sea (different landmass) → **sea transport is NOT worth building. Dropped.**

Also: those 444 tiles generate **10,170 noPath evaluations (~23× redundant each)** and misroute
workers — so the efficiency track is a guaranteed win independent of diplomacy.

**Caveat:** OB only helps where the fragmenting neighbor will agree (neutral/friendly). Hostile
neighbors → tiles stay stranded; inherent, not a bug. This raises the relative value of Track #1.

**Revised priority:** (1) Track #1 efficiency cache (guaranteed); (2) Track #2 Open-Borders
valuation (helps the diplomatically-feasible subset); peaks & sea deprioritized/dropped.

## Track #3 — city self-produces workers (PRIMARY fix, SHIPPED)

User insight: if a city is cut off from the shared worker pool, it should **build its own
worker** — a worker built in the city spawns inside the city's own reachable region, so it
can improve tiles the external pool can't reach. This sidesteps the OB diplomacy ceiling
entirely.

Root mechanism found: worker production is gated on `iWorkersInArea < iNeededWorkersInArea`
(`CvCityAI.cpp`), i.e. supply counted across the whole **landmass area** — which borders
split into unreachable components. A cut-off city sees "area covered" and never self-produces.
Detection signal (clean + self-limiting): a worker is registered to a city only while it
physically stands in that city's work radius (`CvSelectionGroupAI.cpp` setWorkerHave), so a
cut-off city has `getNumWorkers()==0` permanently while `iWorkersInArea` stays high.

Implemented (#12b in `CvCityAI::AI_chooseProduction`): build a local `UNITAI_WORKER` when
`iWorkersNeeded>0 && getNumWorkers()==0 && iWorkersInArea>=iNeededWorkersInArea &&
AI_totalBestBuildValue(area)>0` (+ danger/age guards). Self-limiting: once the worker is in
radius `getNumWorkers()>0` and it stops. Log reason: `"stranded city local worker"`.
This also answers the original "are AIs producing enough workers?" — they were, but the count
was area-pooled and blind to border fragmentation.

## Track #2 — reachability (Open Borders) — now secondary

Route the stranded tiles by barrier type:

- **Sea transport (coastal salients).** A worker with a high-value build on a water-separated
  own tile advertises a transport need through the existing **`CvContractBroker::advertiseWork`**
  (`CvContractBroker.h:116`, `unitCapabilities` flag selects a sea transport). A free/idle
  transport fulfils it and ferries the worker. Reuses the broker that already handles
  escort/contract requests (see `ContractBroker.log`). Likely the highest-yield sub-fix.
- **Open Borders (foreign-interlock salients).** When a player has meaningful own territory
  reachable only through a specific neighbor's closed borders, raise that player's AI valuation
  of an Open-Borders agreement with that neighbor (`CvPlayerAI`/`CvTeamAI` OB-trade valuation).
  Workers already path through OB land once granted, so no pathfinding change — only the
  diplomatic weight. Effectiveness gated on the neighbor agreeing.
- **Peak-locked.** Generally no land fix; mark stranded and stop trying (folds into Track #1).

## Rollout

1. Phase 0 classification logging — **done**; gather the barrier mix.
2. Stranded-tile detector — start with (B) cache for the Track-#1 efficiency win; design (A)
   connectivity core.
3. Track #2 by dominant barrier: sea-transport via ContractBroker first (if `adjWater`
   dominates), then OB valuation (if `adjForeign` dominates).
4. Re-measure `[WAI/city/frontier] found=0` border-blocked share to confirm improvement.

## Diagnostics reference

Tags/fields live in `CvWorkerAI.cpp` city planner; taxonomy in the worker-AI-log-taxonomy
memory. Key lines: `[WAI/city/frontier]` (per-city summary), `reason=noPath` (now with
adj* barrier breakdown), `reason=enemyUnit` (newly surfaced), `goldShort` on eval lines.
