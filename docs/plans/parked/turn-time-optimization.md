# Turn-Time Optimization — Investigation & Hot-Path Map

**Status:** Measured; **#1 hotspot FIXED.** The CABV PreLoop (was ~30% of the whole late-game
turn) is gone: within-turn memoization (3.6× CABV) + the load-time **static enabler
reverse-index** (#195 Phase 1, PR #314 — PreLoop ~390×, ~11.7 s/turn → ~0.05 s, set-identical
shadow-verified).
**Every whole-turn number below predates #314** — treat the measurement sections as the
historical record that located the cost, and the "Current state & next levers" section as the
live plan.

**Question that started this:** late-game turns take *significantly* longer than early
turns — what scales worse than "more cities"?

---

## CABV investigation (historical — the machinery it measured is gone)

**`CalculateAllBuildingValues` and `AI_FlushBuildingValueCache` no longer exist in `Sources/`** —
the building-value recompute machinery this whole investigation chased has since been replaced
by the enabler cascade ([`enabler.md`](../../specs/enabler.md)) and the maintained-sum repository
model ([`state-repositories.md`](../../cascade.md)). Everything that
follows through "Three orthogonal optimization tracks" below (originally several sessions'
worth of `[PERF]` measurement against that machinery — ranked levers, the CABV PreLoop
root-cause hunt, the reverse-prereq-index false leads) is now pure history: do not plan against
any of its file:line citations or ms figures.

**What is durable from it, if a future turn-time investigation revisits this class of problem:**

- **Measure before refactoring.** Every "obvious" big-O hot loop bet in this investigation
  (`.NotDeveloping`, `.Sea`, reverse-prereq indices) turned out to be ~0.4% of the actual cost;
  the real hotspot (a redundant per-call rebuild of a set that barely changed turn-to-turn) was
  invisible to a static code read and only found by per-dimension `[PERF]` instrumentation.
- **A per-turn full rebuild of a set that changes rarely is the recurring waste pattern** —
  the fix was retention/memoization across calls (cache the set, invalidate on the actual
  triggering event), not a cheaper rebuild.
- **The `[PERF]` stopwatch instrumentation itself is durable infrastructure** (`PERF_SCOPE`/
  `PERF_ACCUM` in `BetterBTSAI.h`, gated by `gPlayerLogLevel`/`gPerfLogLevel`, logging to
  `Performance.log`) — see "Measuring: `[PERF]` stopwatches" below, which is still the live
  measurement method for any future turn-time work, independent of what it once measured.

> **⚠ MEASUREMENT BLIND SPOT FOUND (2026-06-10, owner stopwatch):** the scoreboard above covers
> the **doTurn tree only**. A stopwatch on the full end-turn → responsive span measured
> **~88 s wall-clock** against ~18 s of scoped phases — ~70 s happens in the **frame-driven
> span**: AI unit movement runs from `CvGame::update` → `updateMoves` → `autoMission` /
> `AI_unitUpdate` (selection-group cascades + pathfinding) per rendered frame BETWEEN turn
> boundaries, outside every doTurn scope. (The old "doTurnUnits ~1.2 s ⇒ units are not the
> bottleneck" conclusion only ever covered upkeep — movement was never in the tree.) New
> instrumentation: `turn.wall` (true wall between turn boundaries), `game.update.accum`
> (DLL compute in the frame span), `game.updateMoves.accum` + `moves.autoMission/`
> `AI_unitUpdate/brokerPP.accum` sub-splits, all logged once per turn at `CvGame::doTurn`.
> `wall − update.accum` ≈ engine/render residual — distinguishes "unit AI compute" from
> "engine frame pacing" as the owner of the missing ~70 s. **Next session decides the next
> target.**

**FRAME SPAN MEASURED (2026-06-10, tier-2 instrumentation, steady-state per prompt-ended
~98 s turn):** it IS compute, not pacing (`update.accum` ~80 s; engine/render residual ~18 s).
Ranking: `pathGen` ~30 s avg / **130–145k calls/turn**; `AI_unitUpdate` ~37–44 s (fully
explained by the `[PERF/unitai]` per-group table); **`UNITAI_CITY_DEFENSE` 15.2 s / 14,556
re-decides/turn**; sea cluster ~9 s (`RESERVE/ATTACK/EXPLORE/SETTLER/WORKER_SEA` — matches
`sea-ai-rework`); `reachable` 7.4 s / 61k ctors; broker post-process 3–8 s. Exonerated:
per-slice Python `gameUpdate` (~5 ms), `updateScore`, `testAlive`; `plotPaging` load-only.

**CITY_DEFENSE churn — investigation log (hypothesis 1 FALSIFIED):**

- *Attempt 1 (2026-06-10):* capped the `ACTIVITY_SLEEP → setForceUpdate` auto-wake in
  `CvSelectionGroupAI::AI_update` to once per turn (`m_iLastSleepWakeTurn`, transient).
  **Measured: NO effect** — CITY_DEFENSE still 13–19 s / 14–25k calls per turn. The parked
  groups evidently are not arriving via the sleep path. The cap stays (correct and free for
  genuinely sleeping groups), but it is not the churn mechanism.
- *Known per-turn re-armer:* `CvSelectionGroup::doTurn` (~`CvSelectionGroup.cpp:290`) sets
  `setForceUpdate(true)` for EVERY AI group not on `ACTIVITY_MISSION`, every turn — explains
  one full cascade per parked group per turn, but not the ~10× slice multiplier.
- *Discriminators measured (force/awake/exitReady on [PERF/unitai]):* CITY_DEFENSE stable at
  `force≈4,000` (the doTurn re-armer: one forced cascade per parked group per turn),
  `exitReady≈3,000`, `awake≈3,400` — a **2× cascade multiplier**, with the rest of n being
  cheap drive-by visits. Same signature on HEALER/SEE_INVISIBLE/INVESTIGATOR/INFILTRATOR.
- *ROOT CAUSE (hypothesis 2, CONFIRMED by the numbers): `CvUnitAI::processContracts` tail
  (`CvUnitAI.cpp:~21358`).* On a unit's FIRST call of the turn it advertises for work, finds
  none, and returned **true** — terminating the cascade with the unit awake, no mission,
  moves intact → `readyToMove()` stays true → the slice driver re-visits → the whole cascade
  re-runs (second pass returns false and reaches a real terminal). Every advertising unit
  paid two full cascades + two broker scans per turn; the per-slice
  `postProcessUnitsLookingForWork` re-scans of ~6k advertising units are also the brokerPP
  3–8 s/turn.
- *FIX 1 (processContracts advertise-path, VERIFIED):* the advertise-no-work path returns
  **false** (cascade continues to its real terminal in one pass; broker post-process remains
  the within-turn work-delivery channel). Measured: brokerPP 3–8 → **0.8–1.5 s**, exitReady
  −75%, awake re-visits −70%, reachable ctors −20%. **But CITY_DEFENSE ms unchanged** — the
  surviving cascades run deeper (the path-heavy low-priority terminals now execute every
  pass), so the cost moved, not vanished. Residual ~600/turn exitReady with
  `missionAI=GUARD_CITY`, no mission — same bug class, smaller, logged for later.
- *THE ISOLATED STRUCTURAL COST:* `force≈3,950` — one full re-plan-from-scratch per parked
  defender group per turn (the `CvSelectionGroup::doTurn` re-armer) at ~3.5 ms + ~30 path
  calls each; spin samples show 25-unit garrison stacks where each unit individually
  re-derives "keep guarding" with pathfinding, every turn.
- *FIX 2 (targeted re-plan stagger) — shipped, measured INSUFFICIENT alone.* force fell only
  ~4k → ~2–2.7k and awake re-visits ROSE to ~2.4–3.5k: net cascades unchanged. Reason
  (the keystone finding): **garrisons park via `MISSION_SKIP`, which only holds for ONE
  turn** (`ACTIVITY_HOLD`, re-awakened by the next `doTurn`) — so parked defenders re-enter
  the decision cascade every turn regardless of any force/wake gating. The stagger only
  governs groups in PERSISTENT states.
- *FIX 3 (the convergent root fix — shipped, awaiting verification): garrison terminals
  park persistently.* `AI_guardCityBestDefender`, `AI_guardCityMinDefender` (in-place) and
  `AI_guardCity` (garrisonHere) now push **`MISSION_FORTIFY`** (`canFortify()` fallback to
  SKIP) — vanilla BTS semantics. Fortify → `ACTIVITY_SLEEP`, persistent; re-planning is now
  governed by the staggered doTurn re-armer + danger + events (fixes 0–2 line up behind
  this). Side bugfix: AI garrisons actually accrue their fortification defense bonus, which
  the SKIP idiom had silently denied them. The redundant sleep-auto-wake in
  `CvSelectionGroupAI::AI_update` is removed (the doTurn re-armer is the single auto-waker).
  Expected: CITY_DEFENSE cascades → ~1k/turn (stagger share + danger), ms → ~4 s, pathGen
  count finally drops. Playtest watch: garrison redistribution lag ≤4 turns absent danger;
  city defense strength slightly UP (fortify bonuses).
- *FIX 3 measured — PLATEAU at ~15–18 s, two causes now precisely understood:*
  (1) **the danger override pins most garrisons to every-turn re-plans** —
  `AI_getAnyPlotDanger(plot, 2)` reads potential-enemy danger, and late-game border cities
  are perpetually "in danger" (where danger doesn't pin, the stagger works:
  PROPERTY_CONTROL force halved, HEALER −50%); (2) **the expensive cascades belong to
  SURPLUS defenders** — in an over-stacked garrison only one unit takes the cheap
  best-defender terminal and a few cover minDefenders; the rest fail every cheap terminal
  and fall through to all-cities path scans + vicinity `generatePath` loops on every
  re-plan. The perf cost and the defender over-stacking behaviour bug are the same thing.
  Also shipped: garrison park fallback is FORTIFY → SLEEP → SKIP (non-fortifying conscripts
  park persistently too).
- **NEXT CAMPAIGN (design work, not patches): the defender economy.** Garrison demand
  accounting + assignment via the repository's **city-declared needs** (cities publish
  defense needs once per change; units consume the published number; no per-unit map
  searches). Demand side already improved by the fortify-bonus fix (garrisons now read at
  full strength, easing the over-stacking attractor and the conscription of non-fortifying
  units). Tied plans: [`state-repositories.md`](../../cascade.md), `unit-ai-valuation.md` (defender
  production glut), `ai-architecture-north-star.md` (per-UNITAI modules).
- *STEP 1 SHIPPED (#384 — garrison tiers, see "City garrison tiers" in `AGENTS.md` and
  `Sources/AI/CvUnitAI.cpp`):* garrisoning no
  longer retypes units to CITY_DEFENSE (the retype both corrupted the count-based demand
  gates — fake defenders suppressed real training — and fed mis-suited units into the
  expensive CITY_DEFENSE relocation cascade). Auxiliary members keep their own UNITAI and
  park persistently; the leave-a-defender-behind attack paths got the FORTIFY park idiom
  (their one-turn `MISSION_SKIP` was this same churn class); release hysteresis
  (`GARRISON_RELEASE_MARGIN_PERCENT` 125) implements the owner's retention ruling.
  Expected effects to verify: `[UNT/role] -> 10` conversions disappear; CITY_DEFENSE
  `[PERF/unitai]` n drops toward true defender count; `[UNT/garrison]` shows stable
  membership. Existing saves bleed their historic mis-typed population via a categorical
  demotion (`noDefensiveBonus()` CITY_DEFENSE units revert to XML default at re-plan,
  `[UNT/act] demoteUnsuitedDefender`). The city-declared-needs repository channel remains
  the campaign's main body.
- *Playtest find (#384, live-traced via the /units endpoint):* `AI_guardCity`'s vicinity
  guard-spot loop used `rect(NUM_CITY_PLOTS_2=21)` — a plot COUNT as a RADIUS — scanning a
  43×43 box per re-plan (part of the measured "vicinity generatePath loops" cost) and
  marching "garrison" units up to 21 tiles from their city. Fixed to `rect(2,2)`; the
  in-city recall preference now also covers garrison members (auxiliaries are not
  city-AI-typed anymore, so type-keyed recall missed them).

Deprioritized (measured negligible): visibility stickytape, property solver, trade routes,
the unit-choose family.

---

## MEASURED CONCLUSION (historical — pre-#314; located the cost, since fixed)

The turn is **not** spread across the plot/unit-scaling passes we first suspected. It is
~80% **one chain**, and within it a single loop dominates. Per-turn averages from a clean
late-game run (`gPerfLogLevel=1`, Autolog off, ~9 turns, ~37 s/turn wall clock):

```
CvPlayer::doTurn               32.6 s   ← essentially the whole turn
└─ doTurn.cities               26.4 s   (81% of the player turn)
   └─ city.doProduction        22.8 s
      └─ AI_chooseProduction    21.9 s
         └─ CalculateAllBuildingValues 16.9 s
            ├─ PreLoop (canConstruct sweep)  11.2 s  ◄── #1: ~30% of the ENTIRE turn
            ├─ building valuation             3.8 s
            └─ all other dimensions          ~1.9 s
```

Everything else is rounding error by comparison: `recalculateAllResourceConsumption` 1.9 s,
`game.autoSave` 1.4 s (periodic), `doTurnUnits` 1.2 s, `AI_doDiplo` 0.6 s, visibility
"stickytape" ~0.035 s, property solver ~0.18 s.

**The hotspot is the CABV PreLoop** (`CvCityAI.cpp:12599`) — the `canConstruct()` sweep over
all building types plus the enabler inner loop (`O(enablers × buildings)` of
`BoolExpr::evaluateChange`). It runs **~91×/turn** (≈ once per city cache-build) at **~123 ms
each = 11.2 s/turn**.

**The #1 lever: cross-turn retention of the constructible set.** The PreLoop is flushed and
fully rebuilt every turn, but its output barely changes — measured per-city `setSize` is
**flat** turn-over-turn (city 24596: 117–123; city 8198: 166–168; city 24585: 214–216).
That flatness is the proof the rebuild is redundant: constructibility only changes on
discrete events (tech, building completed, civic, bonus). Retaining the set across turns with
**event-driven invalidation** plausibly removes most of the 11.2 s/turn (~25–30% off the
whole turn). **This is exactly step 1 of the derived-data repository.** Second lever: a Game-level
reverse-index ("which buildings' construct-conditions reference building X") to prune the
quadratic enabler loop (repository §6.3).

**On "slower and slower":** investigated and **closed — it is episodic/state-driven, not a
monotonic leak.** One heavy stretch (a war/build spree) showed steep linear creep (R²=0.94),
but a later equally-long stretch was flat (R²=0.03), and per-city `setSize` does not grow.
Reload clears a transient component but there is no steady accumulator to chase. Tooling for
re-checking shipped: `Tools/turn-perf-trend.awk` (least-squares creep verdict) and
`[PERF/cabvset]` (set-size log). Do **not** prioritize the degradation; optimize the absolute
hotspot (the PreLoop) instead.

---

## TL;DR ranked suspects (STATIC pre-measurement hypothesis — mostly DISPROVEN)

> **Historical.** This big-O table was the *starting* map from a static code read. Keep it
> for the plot/unit-scaling analysis, but the **measured** verdict above overrides it: #1
> (visibility) measured ~35 ms, #8 (property) ~180 ms, #3 (units) ~1.2 s — none is the cost.
> The real hotspot (CABV PreLoop) is an economic/per-city path that did **not** appear here.

| # | Hot path | File:line | Scales with | Per-turn frequency |
|---|----------|-----------|-------------|--------------------|
| 1 | **Full-map visibility clear + `updateSight`** ("stickytape") | `CvGame.cpp:5869` | plots × units | once / game turn |
| 2 | **`AI_doEnemyUnitData` visible-plot scan** | `CvPlayerAI.cpp:24686` | visible plots × units-on-plot | once / AI player |
| 3 | **Per-unit pathfinding × group re-decide cascades** | `CvSelectionGroupAI.cpp:149` + `CvPathGenerator.cpp:819` | units × decision-depth × path cost | once / unit (×1–50 cascade) |
| 4 | **Found-value clear** (cheap) + lazy recompute (expensive) | `CvPlayerAI.cpp:1189` | plots (clear); revealed plots (recompute, gated) | once / AI player |
| 5 | **`AI_doDiplo`** tech/bonus/deal trading | `CvPlayerAI.cpp:17394` | players² × max(techs, bonuses, deals) | once / AI player |
| 6 | **Resource-consumption recompute** | `CvPlayer.cpp:27065` | bonuses × cities × buildings/city | once / player |
| 7 | **`clearCanConstructCache(NO_BUILDING)`** | `CvPlayer.cpp:27773` | buildingTypes × cities | once / player |
| 8 | **Property solver** (3-phase, single-pass) | `CvPropertySolver.cpp:447` | cities + units + plots (objects w/ manipulators) | once / map |
| 9 | **`updateTradeRoutes`** | `CvPlayer.cpp:4257` | cities² | once / player |
| 10 | **Plot-group flood-fill** (when dirty) | `CvPlayer.cpp:4095` | plots | dirty-driven |

Items that scale with **plots** (1, 2, 4, 10) or **units** (1, 2, 3) are the ones that grow
"beyond just more cities," because a maturing game reveals most of the map and fields large
armies even when city count plateaus. Items 5–7, 9 scale with tech/bonus/building counts,
which also climb over a game independently of cities.

---

## The per-turn pipeline (where the time goes)

Once-per-game-turn driver: `CvGame::update` (`CvGame.cpp:2270`) → `CvGame::doTurn`
(`CvGame.cpp:5788`). Per living player: `CvPlayer::doTurn` (`CvPlayer.cpp:3676`) and the AI
hooks `CvPlayerAI::AI_doTurnPre/Post` and `AI_doTurnUnitsPre/Post`.

```
CvGame::doTurn (CvGame.cpp:5788)
├─ per team:   CvTeam::doTurn                       (espionage loop O(MAX_TEAMS²))
├─ per map:    CvPropertySolver::doTurn  +  CvMap::doTurn (O(plots) per map)
├─ doGlobalWarming                                  (O(plots), feature-gated)
├─ doHeadquarters / doDiploVote
└─ FULL-MAP visibility clear + updateSight  ← suspect #1  (CvGame.cpp:5869)

CvPlayer::doTurn (CvPlayer.cpp:3676)
├─ doUpdateCacheOnTurn → clearCanConstructCache     ← suspect #7  (O(buildings[×cities]))
├─ AI_doTurnPre                                     (CvPlayerAI.cpp:391)
│   ├─ AI_doEnemyUnitData                           ← suspect #2  (CvPlayerAI.cpp:24686)
│   └─ AI_doDiplo (via AI_doTurnPost)               ← suspect #5  (CvPlayerAI.cpp:17394)
├─ recalculatePopulationgrowthratepercentage        (O(buildings×cities))
├─ recalculateAllResourceConsumption                ← suspect #6  (CvPlayer.cpp:27065)
├─ per city:  CvCity::doTurn                        (O(cities); many sub-passes)
├─ doTurnUnits                                      ← suspect #3
│   ├─ O(plots) owned-plot guardable scan           (CvPlayer.cpp:3980)
│   └─ 4 domain passes × all selection groups → group/unit AI + pathfinding
└─ updateTradeRoutes                                ← suspect #9  (O(cities²))

CvPlayerAI::AI_doTurnUnitsPre (CvPlayerAI.cpp:539)
├─ plotDangerCache.clear()                          (TTL = 1 turn; see notes)
└─ AI_updateFoundValues(true)                       ← suspect #4  (clear only here)
```

---

## Detailed findings

### 1. Full-map visibility recompute ("stickytape") — `CvGame.cpp:5869`

Every game turn, `doTurn` clears visibility counts on **every plot** and then calls
`GC.getMap().updateSight(true, false)`, which re-applies sight for every unit/city.

```cpp
// CvGame.cpp:5865  — comment is verbatim from the source
// Recalculate vision on load (a stickytape - can't find where it's skewing visibility counts)
// Hopefully won't create a noteable delay but it may
for (int iJ = 0; iJ < GC.getMap().numPlots(); iJ++)
    GC.getMap().plotByIndex(iJ)->clearVisibilityCounts();
GC.getMap().updateSight(true, false);
```

- **Cost:** O(plots) clear + O(units × sightRange²) re-apply, **every turn, for the whole game**.
- **Why it's the #1 suspect:** it is an acknowledged hack working around a visibility-count
  drift bug, not a designed per-turn cost. If the underlying drift were fixed (or this were
  gated to load-only, as the comment's "on load" intent suggests), it could potentially be
  removed entirely. **Verify with the profiler first** (look for `updateSight` self-time).

### 2. `AI_doEnemyUnitData` — `CvPlayerAI.cpp:24686`

Scans **every plot visible to the team**, and for each, iterates the units on it to tally
enemy strength used by AI threat assessment.

- **Cost:** O(visible plots × units-per-plot). Grows as the map is revealed and armies grow.

### 3. Per-unit pathfinding × group re-decide cascades

Two multiplied costs:

- **Pathfinder** `CvPathGenerator::generatePath` (`CvPathGenerator.cpp:819`): A* with a hard
  ceiling of 20 000 iterations (`CEILING_ITERATIONS`, `CvPathGenerator.cpp:22`) and a
  distance-scaled soft optimization limit. Per query ranges from cheap to expensive on long
  routes / complex terrain. Path caching (lines ~872–900) reuses routes for units continuing
  along the same path.
- **Group re-decide loop** `CvSelectionGroupAI::AI_update` (`CvSelectionGroupAI.cpp:149`):
  `while (... readyToMove())` re-runs the head unit's `AI_update()` until it stops being
  ready, with a **hard safety cap of 50** iterations (`iTempHack >= 50`, line ~173). When a
  unit "acts" without consuming its move, it re-decides the same thing every pass and spins
  to the cap. Multiply by ~60+ `generatePath` call sites across `CvUnitAI` decision helpers.
- **Net:** late game = many units × (cascade depth) × (1–5 paths each). This is the most
  likely place where "more units" turns into super-linear wall-clock.

Known spin patterns (some fixed, all worth re-checking with the profiler):

- **Heal spin** (`CvUnitAI.cpp:~12721`) — mitigated by a `canHeal()` guard; can still spin if
  a unit claims heal while `canHeal()` is false mid-cascade. See memory
  `hunter-move-reinvocation`.
- **Property-control RESERVE↔PROPERTY_CONTROL flip** — documented fixed
  (`property-control-oscillation` memory); was ~750 flips/turn.
- **Explore/defensive two-tile oscillation** (`CvUnitAI.cpp:~16519`) — prevention filter in place.
- **City-attack retry** `AI_pickTargetCity` (`CvUnitAI.cpp:~17075`) — up to 8 adjacent × 2
  passes × `generatePath` per attack decision.
- **`AI_refreshExploreRange`** (`CvUnitAI.cpp:16441`) — builds a `CvReachablePlotSet`
  (O(range²)) then pathfinds many candidate plots; called per exploring/hunting unit when
  range is exhausted. Linked to the still-open sea-AI spin (memory `sea-ai-rework`).

### 4. Found values — clear is cheap, recompute is lazy — `CvPlayerAI.cpp:1189`

**Correction to first-pass notes:** the per-turn call `AI_updateFoundValues(true)`
(`AI_doTurnUnitsPre:551`) takes the `bClear` branch, which only walks plots calling
`clearFoundValue` and **early-returns at line 1209**. The expensive part — per-plot
`AI_foundValue()` — is the `bClear=false` path (lines 1213–1245) and is **gated by
`bNeedsCalculating`** (only areas lacking a cached best-found value), i.e. lazy/on-demand,
not every plot every turn.

- **Per-turn cost:** O(revealed plots) memory clears per AI player. Real and plot-scaling,
  but far cheaper than "recompute every plot every turn." Don't over-prioritize it.

### 5. `AI_doDiplo` — `CvPlayerAI.cpp:17394`

Tech-source precompute over players × techs, a 2-pass players loop scanning all active deals
and all bonuses for trade offers.

- **Cost:** ~O(players² × max(techs, bonuses, deals)). Player count is fixed, but tech/bonus/
  deal counts climb through the game.

### 6. `recalculateAllResourceConsumption` — `CvPlayer.cpp:27065`

Loops every bonus type and, per bonus, every city and that city's buildings.

- **Cost:** O(bonuses × cities × buildings-per-city), every turn per player. Grows on three
  axes that all increase over a game.

### 7. `clearCanConstructCache(NO_BUILDING)` — `CvPlayer.cpp:27773`

Per-turn cache flush over all building types; with `bIncludeCities` it also flushes per city.

- **Cost:** O(buildingTypes [× cities]). Cheap per element but unconditional; worth checking
  whether the whole cache must be dropped every turn vs. invalidated on actual state change.

### 8. Property solver — single-pass, not iterative — `CvPropertySolver.cpp:447`

Good news: **no convergence loop.** `gatherAndSolve` (`CvPropertySolver.cpp:420`) runs a fixed
3-phase predict→correct→apply pipeline (propagators, interactions, sources). Manipulator
gathering (`gatherActiveManipulators`, line 312) iterates the 7 game-object types; the CITY/
UNIT/PLOT iterators are O(cities)/O(units)/O(plots).

- **Cost:** O(objects-with-manipulators) per map per turn — linear, no hidden iteration blow-up.
  Lower priority unless the profiler says otherwise. Reference: `docs/reference/CvPropertySolver.md`.

### 9. `updateTradeRoutes` — `CvPlayer.cpp:4257`

Clears then re-establishes routes by comparing cities pairwise.

- **Cost:** ~O(cities²) per player.

### 10. Plot-group flood-fill — `CvPlayer.cpp:4095`

When plot groups are dirty, re-colors regions via recursive flood-fill across all plots.
Dirty-driven (not literally every turn) but O(plots) when it fires, and can chain into
`updateTradeRoutes`.

---

## Measuring: `[PERF]` stopwatches (primary) — IMPLEMENTED

The headline phases are now instrumented with wall-clock stopwatches that log to
`Performance.log`, gated by their own knob — **works with any DLL (Assert/Release), no
special Profile build.** This is the primary measurement path; the FProfiler below is the
fallback for drilling deeper.

- **Enable:** BUG options → Autolog → **"Performance / turn-timing log level"**
  (`Autolog__LogLevelPerf` → `gPerfLogLevel`). `0` = off, `1` = on.
- **Mechanism:** `PERF_SCOPE("label", ownerIdOr-1)` (RAII `ScopedPerfTimer` +
  `win32::Stopwatch`, in `BetterBTSAI.h`/`.cpp`); one `[PERF/phase] turn= owner= phase= ms=`
  line per phase on scope exit. Off-cost = one integer compare, so it ships in normal DLLs.
- **Instrumented (headline pass):** `CvGame::doTurn`, **`doTurn.visibilityRebuild`** (the
  stickytape, suspect #1), `CvPlayer::doTurn` (per-player via `owner=`),
  `CvPlayer::doTurnUnits`, `AI_doDiplo`, `CvPropertySolver::doTurn`,
  `recalculateAllResourceConsumption`, `updateTradeRoutes`.
- **Analyze:** use the `[PERF]` grep/awk recipes (total ms
  per phase, stickytape cost/turn, slowest empire). Sum across late-game turns to rank the
  suspects in real ms before optimizing.
- **Next:** add `PERF_SCOPE` around the unit/pathfinding cascade (suspect #3) if the headline
  pass points there.

### Intra-session growth diagnostic (turn times climb the longer you play, reset on reload)

> **CLOSED — episodic, not a leak.** Resolved with `[PERF/cabvset]` + `Tools/turn-perf-trend.awk`:
> creep does **not** steadily reproduce (one stretch R²=0.94 steep, the next R²=0.03 flat), and
> per-city constructible `setSize` is flat turn-over-turn. The growth was a heavy game-state stretch
> (war/build spree), not an in-memory accumulator. **No leak to chase** — optimize the absolute
> hotspot (CABV PreLoop) instead. The hypotheses below are kept as the investigation trail; all were
> either disproven (contract broker EXONERATED, see next block) or rendered moot by the cabvset data.
> Re-run the trend check if a future session feels like steady creep:
> `awk -f Tools/turn-perf-trend.awk -v phase=total "$LOGS/Performance.log"`.

Owner observation: turn time grows turn-over-turn within a session and **resets on game reload**.
That is a sharp signal — reload reconstructs everything *from the save*, so unit/city **counts
are preserved** across a reload. Therefore a phase that grows-and-resets is driven by
**in-memory accumulated state**, NOT by unit/city count (count-driven cost would survive the
reload). Decoder ring for `Performance.log` (`awk -F'phase=| ms=' '/PERF/{print $3"\t"$2}'`
grouped by turn, watch each phase's ms trend):

- **All phases inflate ~uniformly** → allocator/heap fragmentation from per-turn churn
  (CvReachablePlotSet/path objects). Reload defrags. Hard to fix in-code.
- **`doTurn.visibilityRebuild` climbs alone** → the visibility-count **drift** the stickytape
  papers over is accumulating within the session (load rebuilds clean from the save). *Leading
  hypothesis* — it's the most direct "in-memory state that resets on reload" candidate here.
- **`CvPropertySolver::doTurn` climbs** → property manipulators/objects accumulating.
- **`CvPlayer::doTurnUnits` climbs but unit count is flat** → a per-unit cache or re-decide
  spin growing (not raw count).
- **`AI_doDiplo` climbs** → deals/known-tech/bonus lists growing.
- **`CvGame::doTurn` climbs more than the sum of sub-phases** → growth in an *un-instrumented*
  span; add a `PERF_SCOPE` to bisect.

**Accumulator hunt — results (verified).** Searched for in-memory state that grows per turn and
resets on load:

- **FALSE POSITIVES (verified):** the static "current-X" caches do NOT accumulate — they self-
  clear during normal iteration. `resultsCache` (`CvUnitAI.cpp:1078`) clears whenever the owning
  player changes (many times/turn); `g_bestDefenderCache` (`CvPlot.cpp:3840`) clears whenever a
  different plot is queried (constantly); `cachedTargets` is the same player-keyed pattern. Don't
  chase these.
- **GENUINE CANDIDATE:** `CvContractBroker::m_workRequests`. Per-turn `cleanup()`
  (`CvContractBroker.cpp:54`) erases only **fulfilled** requests (`:77-88`); unfulfilled ones
  persist, and the broker is only fully wiped by `reset()` (`:41`) on load. Chronically-unfulfilled
  - re-posted requests ⇒ grows per turn, resets on reload. **Already self-instrumented:**
  `[CTB/turn] workRequestsRemaining=N` (`:91`) — enable `[CTB]`/player logging and watch that
  number across turns; a monotonic climb confirms the leak. Would surface in PERF as
  `CvPlayer::doTurnUnits` rising.
- **FALLBACK HYPOTHESIS:** if no single phase climbs but all inflate ~uniformly → heap
  fragmentation from per-turn allocation churn (CvReachablePlotSet / FAStar nodes / path vectors);
  resets on EXE restart. Different fix class (pooling), not a logic leak.

Two cheap data checks disambiguate: (1) `[PERF]` per-phase trend (one phase vs uniform);
(2) `[CTB] workRequestsRemaining` trend (climbing = the broker leak).

### First measured `[PERF]` data — turn ~1256 save (5 turns)

| Phase | result |
|---|---|
| `CvPlayer::doTurn` (per player) | **DOMINANT** — big AI empires (owners 1/4/9/10) ~10 s each, **max 15.7 s** |
| `CvPlayer::doTurnUnits` | 405 ms max — **unit AI/pathfinding is NOT the bottleneck** |
| `doTurn.visibilityRebuild` (stickytape) | ~35 ms — **not** the cost here (kills the #1 hypothesis for this save) |
| `CvPropertySolver::doTurn` | ~166 ms |
| `recalculateAllResourceConsumption` | ≤326 ms |
| `AI_doDiplo` / `updateTradeRoutes` | ≤234 ms / 0.4 ms |

**Contract broker EXONERATED (measured):** every vector bounded and per-turn-cleared —
`workRequests` max 142, `advertisingUnits` max 807, `advertisingTenders` max 140,
`contractedUnits` max 14; none grow across turns. The 26 MB `ContractBroker.log` is just
level-3 log volume (84k `[CTB/tender/cand]` lines), not a leak. The earlier "genuine candidate"
is wrong.

**Caveat — timings contaminated:** this run had Autolog at level ≥3 (hence the 26 MB of AI
logs), so megabytes of synchronous log I/O happened *inside* the timed turns. The 10–15 s is
partly disk I/O. **Re-measure clean: `gPerfLogLevel`=1, Autolog log level = 0.**

**Conclusion + next step:** the sink is the **economic/per-city** half of `CvPlayer::doTurn`,
NOT units, visibility, property, or the broker — consistent with the CvCityAI building/unit
valuation loops above. Added three bisection scopes inside `CvPlayer::doTurn`
(`doTurn.cities` = the `DoCityTurn` loop, `doTurn.AI_doTurnPre`, `doTurn.AI_doTurnPost`); a clean
re-measure will show whether the 10 s is the per-city production valuation (expected) or the
player-level AI economy.

### Full `[PERF]` scope inventory (comprehensive pass)

Instrumented top-to-bottom through the per-turn hot path (prune later if noisy). Labels:

- **Game/player:** `CvGame::doTurn`, `CvPlayer::doTurn`, `CvPlayer::doTurnUnits`,
  `doTurn.visibilityRebuild`, `CvPropertySolver::doTurn`, `recalculateAllResourceConsumption`,
  `updateTradeRoutes`, `AI_doDiplo`.
- **`CvPlayer::doTurn` split:** `doTurn.cities`, `doTurn.AI_doTurnPre`, `doTurn.AI_doTurnPost`.
- **`AI_doTurnPre` split (`pre.*`):** `pre.AI_updateBonusValue`, `pre.AI_doEnemyUnitData`,
  `pre.AI_doCommerce`, `pre.AI_doMilitary`, `pre.AI_doCivics`, `pre.AI_doReligion`,
  `pre.AI_doMilitaryProductionCity`.
- **Per-city (`city.*`, owner = city owner):** `city.doTurn`, `city.cacheFlush`,
  `city.recalcPopGrowth`, `city.AI_doTurn`, `city.AI_updateBestBuild`,
  `city.AI_updateWorkersNeededHere`, `city.doCheckProduction`, `city.doCulture`,
  `city.doProduction`, `city.AI_chooseProduction`, `city.CalculateAllBuildingValues`,
  `city.AI_bestUnitAI`.
- **`CvGame::doTurn` tail split (`game.*`, owner=-1):** every call in `doTurn` is now wrapped so
  the parent total has ~no unaccounted gap — `game.doDeals`, `game.teamDoTurn`, `game.mapDoTurn`,
  `game.barbarians`, `game.doSpawns`, `game.doGlobalWarming`, `game.doHeadquarters`,
  `game.doDiploVote`, `game.writePlotSnapshot`, `game.doUpdateCacheOnTurn`, `game.updateScore`,
  `game.difficulty`, `game.doFoundCorporations`, `game.testVictory`, `game.engineDoTurn`,
  **`game.autoSave`** (the periodic spike — engine save inside the timer), and the three Python
  event hooks `game.py.beginGameTurn` / `game.py.preEndGameTurn` / `game.py.endGameTurn` (confirmed
  light: the Revolution stability hook + `onBeginGameTurn`; the Zizkov map-jam is one-shot on
  build, NOT periodic).
- **CABV internals (`[PERF/cabv]`, accumulating `PERF_ACCUM`):** 13 sub-dimensions per call —
  `preloop` (the hotspot), `building`, `defense`, `happy`, `health`, `exp`, `notdev`, `sea`,
  `maint`, `spec`, `commerceYields`, `commerceVal`, `food` — plus `flags` (focus) and `owner`.
- **CABV set composition (`[PERF/cabvset]`):** `turn= owner= city= numBuildings= constructible=
  enablers= setSize=` logged once per cache-build — the leak-vs-growth discriminator (setSize
  flat ⇒ no set growth).

Per-city scopes log once per city (and `AI_bestUnitAI` several times per production choice), so
**aggregate by phase label**. Total ms per phase across the whole log:

```
awk -F'phase=| ms=' '/PERF\/phase/{sum[$2]+=$3; n[$2]++} END{for(p in sum) printf "%12.1f  %6d  %s\n", sum[p], n[p], p}' Performance.log | sort -rn
```

Per phase **per owner** (which empire dominates each phase):

```
awk -F'owner=| phase=| ms=' '/PERF\/phase/{k=$3"|owner="$2; sum[k]+=$4} END{for(k in sum) printf "%12.1f  %s\n", sum[k], k}' Performance.log | sort -rn | head -40
```

Read it as a tree: `CvPlayer::doTurn` ≈ `doTurn.cities` + `doTurn.AI_doTurnPre` +
`doTurn.AI_doTurnPost` + misc; `doTurn.cities` ≈ Σ `city.doTurn`; `city.doTurn` ≈ `city.AI_doTurn`
(+ `AI_updateBestBuild`/`WorkersNeededHere`) + `city.doProduction` (→ `AI_chooseProduction` →
`CalculateAllBuildingValues` + `AI_bestUnitAI`) + the rest. Whichever leaf carries the mass is
the optimization target.

### Creep check ("does it go slower and slower?")

`Tools/turn-perf-trend.awk` sums a phase per game-turn, least-squares-fits `ms = a + b*turn`
across the session, and prints the slope (ms added per turn), %/turn of the mean, R²
(trustworthiness of the trend) and a `CREEP` / `weak` / `flat` verdict. It drops a partial
trailing turn automatically and needs ≥3 complete turns.

```
LOGS="$USERPROFILE/Documents/My Games/Beyond The Sword/Logs"
awk -f Tools/turn-perf-trend.awk                       "$LOGS/Performance.log"  # CvPlayer::doTurn
awk -f Tools/turn-perf-trend.awk -v phase=total        "$LOGS/Performance.log"  # whole-turn wall clock
awk -f Tools/turn-perf-trend.awk -v phase=doTurn.cities -v lo=1300 -v hi=1360 "$LOGS/Performance.log"
```

**Leak vs growth:** a positive slope alone doesn't prove a per-session leak — teching up grows
the CABV constructible set, so `doTurn.cities` heavier late-game is *expected*. To separate them,
play a window, save+reload mid-session, and re-run the script on a fresh-session log: if the
fresh session starts lower and re-climbs → session leak; if it starts at the same mean → genuine
model growth. (NOTE: `Performance.log` is rewritten per session — copy it off before reloading if
you want to diff the same turn numbers before/after.) A 19-turn late-game sample measured slope
≈ +447 ms/turn (+1.6%/turn, R²≈0.43) — creep is real but modest, and concentrated in
`doTurn.cities` (CABV), consistent with constructible-set growth rather than a leak.

## FProfiler: how to measure (deeper, fallback)

The repo also ships a complete sampling profiler.

- **Macros:** `PROFILE_FUNC()` / `PROFILE(name)` / `PROFILE_BEGIN/END` —
  `Sources/FProfiler.h:94`. ~1100 call sites already exist; `CvCityAI` and `CvPlayerAI` are
  densely instrumented.
- **Build configs:** `Profile` and `ProfileExtra` in `Sources/fbuild.bff:159`. Build via
  `Tools/MakeDLLProfile.bat` (or `MakeDLLProfileExtra.bat` for the denser `PROFILE_EXTRA_FUNC`
  sites). These rebuild + deploy.
- **Enable/dump:** `startProfilingDLL(false)` / `stopProfilingDLL(false)`
  (`CvGameCoreDLL.cpp:477`). Output is tab-delimited `IFP_log.txt` in the BTS `Logs/` dir,
  with columns: total ms, main-thread ms, avg, #calls, child time, **self time**, parent.
- **Suggested method:** wrap `CvGame::doTurn` body in start/stop (or trigger at a known
  late-game autosave), play ~5 turns, sort `IFP_log.txt` by **self time**. Confirm whether
  `updateSight`, `AI_doEnemyUnitData`, `generatePath`, `AI_doDiplo`, and
  `recalculateAllResourceConsumption` actually dominate before touching them.
- **Gap:** there is no per-player or per-turn ms breakdown today — only per-function aggregate.
  A cheap add would be a per-player wall-clock log around `CvPlayer::doTurn` to see which
  players (size/army) cost most.

---

## Unit AI loop hot-paths (structural; confirm with `[PERF]`/FProfiler)

From a focused read of `CvUnitAI` (structural claims; the *magnitudes* are unproven until
measured — earlier agent "savings %" estimates were discarded as fabricated):

- The attack cascade (`AI_attackMove`, `CvUnitAI.cpp:2401`) can construct **many**
  `CvReachablePlotSet` objects in one call (each `AI_anyAttack`/`AI_cityAttack`/`AI_pillageRange`
  builds its own). Candidate: build once per attack phase and share.
- Expensive work before cheap gates: `AI_anyAttack` (`~17634`) builds the reachable set before
  any `canAttack()` check; `AI_safety` (`~15752`) builds the set twice (two passes) with no
  "already safe?" early-out. Candidate: cheap capability/danger gates first.
- This is exactly the "more granular gating earlier in the UnitAI process" lever — and the
  `CvHunterAI` → hunter+explorer vs dedicated army-module split is the structural home for it.

## Remaining live lever: building-production thrashing

Owner reports AI cities flip-flop half-finished buildings (wasted hammers) — measurable via
`[CIT/cancel] progressLost` in CityAI.log. Unrelated to the CABV history above (a production-choice
quality issue, not a recompute-cost issue); own fix, and improves AI quality too.

## FPS during the HUMAN turn — the render-path DllExport audit (distinct from AI turn time)

Everything above is **AI turn time** (the `doTurn`/CABV compute chain). A SEPARATE axis is **per-frame FPS
while the human plays** — the EXE polls the DLL through the ~1,214 `DllExport` functions every frame during
render, so any `DllExport` doing **uncached live compute** is a per-frame tax. Diagnostic tell (owner): the drop
appears **only after end-turn, never on load** — which EXONERATES EXE-intrinsic drawing (the scene draws
identically from the first loaded frame) and points at DLL compute the EXE re-polls. Bare-map removing the
stutter is consistent: it strips the drawn instances (unit models / city billboards / fog) the DLL is polled about.

**Audit result (all 1,214 DllExports classified).** The static/config/info surface (~577: CvGlobals proxies,
CvInitCore, all Infos, CvDeal/Fractal/Random) is trivial member returns — clean. **Fog visibility is CACHED**
(`CvPlot::isActiveVisible` → `isVisible` → `m_aiVisibilityCount[team] > 0`, two array reads) — the fog cost is
the EXE REPAINT, not a DLL recompute. The city banner's heavy reads are already CvDerivedCache O(1) (#430:
`getYieldRate100`→`yieldRate100`, wellbeing→`CascadeAccumulator::wellbeing`). The genuine uncached per-frame
compute concentrated in three spots, all scaling with on-screen units/cities:

1. **★ City-billboard debug log write (FIXED).** `CvGameTextMgr::buildCityBillboardIconString` wrote to
   `CvGameTextMgr_buildCityBillboardString.log` **unconditionally** (3 lines, incl. per holy-city religion) on
   every billboard rebuild — leftover debug logging doing disk I/O. Cadence is per-DIRTY (billboards rebuild on
   a dirty flag, not per-frame — the log settled at ~3 MB, not GB, and does not grow on a static loaded map),
   so it was a per-turn burst + wasteful I/O, not the sustained per-frame drop. Removed.
2. **★ Combat-strength walk, cache-bypassed for the human's own units (FIXED for the common case).**
   `maxCombatStrFloat`/`currCombatStrFloat` (the unit health-bar) dispatch into `maxCombatStr` — a ~700-line
   modifier walk — and the `CombatStrCache` was deliberately skipped for HUMAN-owned units. Reason (owner):
   the cache can't keep the live **Surround-and-Destroy** surrounded-modifier fresh, and the AI never
   reads/acts on that term (it doesn't understand S&D) so a stale-surround cache is harmless for AI — but the
   human tooltip must be correct. `surroundedDefenseModifier` hard-returns 0 when
   `GAMEOPTION_COMBAT_SURROUND_DESTROY` is OFF, so the cache is EXACT for humans in that (common) case → the
   bypass now only triggers for a human with S&D ON. Kills the per-frame ~700-line walk on the player's own
   on-screen units for every game without S&D. **Follow-up (S&D-ON case):** cache the pre-surround intermediate
   (base + non-surround modifiers) for everyone and fold the live `surroundedDefenseModifier` delta on top — the
   "cache the stable core, add the volatile term live" seam (the same shape as unit-happiness-on-top,
   [unit-carried modifiers apply on top, live, never cached](../../cascade.md#2b-the-wellbeing-channels--health--happiness-signed-split-the-2a-sibling)). NOTE S&D is still
   LIVE (removal is only PARKED — [surround-destroy-removal-map.md](surround-destroy-removal-map.md)); if it is
   removed, the bypass + the whole `surroundedDefenseModifier` term delete and the cache serves everyone.
3. **Unit-stack walks per plot (OPEN — separate pass).** `CvPlot::isFighting`/`isVisibleEnemyUnit`/
   `getNumVisibleUnits` (EXE-polled every frame) + `CvGame::getPlotUnits` (O(n²) over one
   stack) walk a plot's unit list uncached — cheap per plot, real when summed over stacked plots × visible plots
   × frames. Fix: reinstate the **event-driven-on-plot maintained counts** (increment/decrement on unit
   enter/leave/battle-start/end) so the query is a stored read. ⚠ Carries the same drift risk as the fog
   "stickytape": a maintained count that misses ANY mutation site goes stale and corrupts combat/visibility —
   needs a complete emit-site trace (the event-completeness discipline), NOT a rushed add.

Second-tier per-frame minor (uncollapsed redundant calls in the billboard bar fns — `foodDifference()` ×3-4,
`getCurrentProductionDifference()` ×2; the CvSelectionGroup interface cluster `isBusy`/`readyToSelect`
bypassing their own cache; `canBeSelected`'s foreign-city espionage loop) are cheap-but-tidy, not the driver.

**Measured outcome:** #1 + #2 shipped → owner reports FPS "significantly better," with a smaller residual left
for a separate pass (candidates: #3, and the S&D-ON combat-str follow-up). The fog compute (per-turn full
`clearVisibilityCounts` + `updateSight` rebuild, `CvGame::doTurn`) is CORRECT to run per-turn (owner: "the
nature of the beast") — the concern was only MID-turn per-frame compute, which is the three items above.

## Cross-references

- Memory: `ai-unit-movement-to-player-level`, `hunter-move-reinvocation`,
  `property-control-oscillation`, `sea-ai-rework`, `worker-escort-stall-mechanism`.
- Reference docs: `CvPathGenerator.md`, `pathfinding.md`, `CvPropertySolver.md`,
  `CvPlayerAI.md`, `CvSelectionGroupAI.md`.
