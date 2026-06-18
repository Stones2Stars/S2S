# PlotSnapshot

**File:** `Utils/PlotSnapshot.h` / `Utils/PlotSnapshot.cpp`

## Overview

`PlotSnapshot` dumps a CSV row per plot describing static plot state.
It exists as a **reference companion** to `BuildEvaluation.log`: when the
worker AI logs a decision keyed by `(x, y)` or `plotIdx`, the snapshot file
lets an analyst recover the full plot context (terrain, feature, owner,
working city, etc.) that produced that decision.

The same files are also the input data for a future test harness that drives
`CvWorkerAI::improveBonus` against fixed plot configurations and asserts
outcomes (Phase 2 — not yet built).

A snapshot fires at four moments:

| Trigger | Tag | Site |
|---|---|---|
| Start of a new game | `start` | `CvGame::onFinalInitialized(bNewGame=true)` |
| Loading a save | `load` | `CvGame::onFinalInitialized(bNewGame=false)` |
| In-game map regeneration | `regen` | end of `CvGame::regenerateMap` |
| Every game turn | `turn` | top of `CvGame::doTurn`, before any per-turn processing |

The `turn` tag fires once per game turn, before AI decisions are made for that
turn — meaning `BuildEvaluation.log` entries logged during turn N can be
cross-referenced against `PlotSnapshot_turn_t<N>.csv` to recover the exact
plot state the AI saw. This is the file you usually want to grep against.

## Output location

Each snapshot is written to a fresh file at:

```
Documents/My Games/Beyond The Sword/Logs/PlotSnapshot_<tag>_t<turn>.csv
```

`<turn>` is `CvGame::getGameTurn()` at capture time. Examples:

- `PlotSnapshot_start_t0.csv` — new game
- `PlotSnapshot_load_t142.csv` — save loaded at turn 142
- `PlotSnapshot_regen_t0.csv` — map regenerated
- `PlotSnapshot_turn_t201.csv` — start of turn 201

**Rotation policy:** the `turn` snapshots are rotated automatically — only
the last 3 (`PlotSnapshot_turn_t<N>.csv`, `..._t<N-1>.csv`, `..._t<N-2>.csv`)
are kept. When turn N is written, turn N-3 is deleted. This caps the
on-disk per-turn footprint at ~3 × file-size regardless of session length.

The `start`, `load`, and `regen` snapshots are **never** auto-deleted —
they're one-offs and useful as long-term references against later log
entries. If you want a permanent record of a particular turn, copy the file
out of the Logs directory before it rotates away.

Deletion resolves the log path via `%USERPROFILE%\Documents\My Games\Beyond
The Sword\Logs\`. This works for default Windows setups but does NOT handle
Documents-folder redirection (e.g. when Documents is moved to OneDrive). In
that case the deletion silently fails and old snapshots accumulate — they're
cosmetic overhead, never read by the planner. The shell-API path resolver
(`SHGetFolderPath`) is avoided because including `shlobj.h` clashes with
C2C's `CATEGORY_INFO` macro.

Files use `gDLL->logMsg(..., bWriteToConsole=false, bTimeStamp=false)`. The
timestamp suppression is deliberate — each row is parseable CSV.

## Schema

The first line is a `#`-prefixed comment carrying the schema version and
capture metadata; the second line is the column header; remaining lines are
one row per plot.

```
# PlotSnapshot schema=2 tag=start turn=0 mapW=80 mapH=52 numPlots=4160
plotIdx,x,y,terrain,feature,improvement,route,bonus,isWater,isHills,isPeak,isCity,isCityRadius,owner,workingCityId,workingCityName,area,improvementCurrentValue,numUnits,animals
```

| Column | Source | Notes |
|---|---|---|
| `plotIdx` | loop counter `i` | matches `GC.getMap().plotByIndex(i)` and the `iI` used in `CvWorkerAI`'s bonus-evaluation cache key |
| `x`, `y` | `pPlot->getX()`, `getY()` | matches the `at=(x,y)` field in `[WAI/*]` log lines |
| `terrain` | `GC.getTerrainInfo(getTerrainType()).getType()` | XML type-string or `NONE` if unset |
| `feature` | `GC.getFeatureInfo(getFeatureType()).getType()` | `NONE` when `getFeatureType() == NO_FEATURE` |
| `improvement` | `GC.getImprovementInfo(getImprovementType()).getType()` | `NONE` when no improvement built |
| `route` | `GC.getRouteInfo(getRouteType()).getType()` | `NONE` when no route built |
| `bonus` | `GC.getBonusInfo(getBonusType()).getType()` | **ground truth** — raw bonus on the plot regardless of per-team obsolescence. The worker AI uses `getNonObsoleteBonusType(team)` which may differ. |
| `isWater` | `pPlot->isWater()` | `0` / `1` |
| `isHills` | `pPlot->isHills()` | `0` / `1` |
| `isPeak` | `pPlot->isAsPeak()` | `0` / `1` |
| `isCity` | `pPlot->isCity()` | `0` / `1` |
| `isCityRadius` | `pPlot->isCityRadius()` | `0` / `1` — matches the `bCityRadius` local in `improveBonus` |
| `owner` | `pPlot->getOwner()` | player ID, or `-1` for unowned |
| `workingCityId` | `pPlot->getWorkingCity()->getID()` | `-1` if no working city |
| `workingCityName` | `pPlot->getWorkingCity()->getName()` | sanitised: commas, quotes, newlines, and non-ASCII replaced with `_` or `?` |
| `area` | `pPlot->area()->getID()` | useful for filtering same-landmass plots |
| `improvementCurrentValue` | `pPlot->getImprovementCurrentValue()` | **read-only access** — the snapshot deliberately does NOT call the lazy-init mutator `setImprovementCurrentValue()`; `0` here means "field not yet initialised" |
| `numUnits` | `pPlot->getNumUnits()` | total units on the plot (all owners) |
| `animals` | per-animal token list | pipe-separated, one token per`isAnimal()` unit: `<UnitType>@o<owner>c<combat>a<aggression>e<enemyOfActiveTeam>`.`c` is XML `iCombat` (in-game strength = `c/100`);`a` is `iAggression`(`0` = passive prey); `e` is `1`/`0`/`-1` for at-war-with-active-team / not / no-active-team. Empty when no animals. Companion to `HunterAI.log` `[HAI/*]` lines — join on `(x,y)`to see which animal a hunter engaged or walked past, and whether it was a recognised target (`e`). |

## Cross-referencing with BuildEvaluation.log

Worker AI log lines reference plots by `at=(x,y)`. To recover the full plot
state for a logged decision:

1. Find the relevant line in `BuildEvaluation.log`:

   ```
   [WAI/best] at=(48,32) bonus=BONUS_WHEAT build=BUILD_FARM value=30000 (improve)
   ```

2. Find the matching snapshot (look at the `t<turn>` suffix in filenames —
   pick the most recent one before the BuildEvaluation entry's turn).
3. Grep the snapshot:

   ```
   grep ",48,32," PlotSnapshot_start_t0.csv
   ```

   Output:

   ```
   2588,48,32,TERRAIN_GRASS,NONE,NONE,NONE,BONUS_WHEAT,0,0,0,0,1,0,-1,,3,0
   ```

The row tells you the plot was unowned grassland with Wheat, in city radius,
no improvement yet — matching the worker AI's decision to build a Farm.

## Reading by plotIdx

When a `[WAI/build/cand]` line references `plotIdx` (none currently do —
they reference `(x,y)` — but the cache uses plotIdx internally), the row
with that `plotIdx` is the first column. Direct lookup:

```
awk -F, '$1 == 2588' PlotSnapshot_start_t0.csv
```

`plotIdx == y * mapW + x` (standard Civ4 indexing). The header line
includes `mapW` so the formula is self-contained.

## What's not captured (yet)

- **Per-team / per-player visibility.** A plot's `getNonObsoleteBonusType(team)`
  varies by team (tech obsolescence) and fog-of-war reveal state; the snapshot
  captures the raw `getBonusType()` only. Add `bonus_team0`, `bonus_team1`, etc.
  columns later if needed — non-breaking schema extension.
- **Yields.** `pPlot->calculateNatureYield(...)` and `calculateImprovementYieldChange(...)`
  are computed in the worker AI scoring path; capturing them in the snapshot
  would let a test harness verify scoring without running the full game.
- **Adjacency.** Neighbour plot indices; useful for irrigation/road planning analyses.
- **Path reachability.** Per-(worker, plot) `generatePath` results — needed to
  fully reproduce `improveBonus` decisions in a test harness.

The schema version (`schema=1` in the header comment) is bumped any time a
column is added or semantics change. Consumers can branch on the version.

## Related

- [`CvWorkerAI.md`](CvWorkerAI.md) — the consumer that emits `BuildEvaluation.log`
  entries the snapshot is designed to support
- Memory `worker-ai-log-taxonomy` — full `[WAI/*]` tag reference for joining
  log entries back to code sections
