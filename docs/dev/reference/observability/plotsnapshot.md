# PlotSnapshot — the per-plot CSV map/city-reference snapshot

> **Status:** reference · **Verified against:** `Sources/Utils/PlotSnapshot.{h,cpp}` (2026-06-20)
> **Grounding:** every behavioural claim is cited to `Sources/Utils/PlotSnapshot.cpp` (the single emit function `writePlotSnapshot`) and the `CvGame` call sites; engine `Cv*` are under `Sources/Engine/`. Line numbers **drift** — confirm the named function, not the integer.
> PlotSnapshot is a **first-class map/city-reference logging surface**: a CSV dump of every plot's static state, captured at fixed game moments, so any later log line keyed by `(x,y)` or `plotIdx` can be joined back to the exact plot context that produced it. After reading you will know what it captures, when it fires, where the files land, how rotation works, and how to join against it.

**BLUF.** `writePlotSnapshot(tag)` writes one CSV file — one row per plot — to the BTS `Logs/` directory at four moments (new game, save load, map regen, every turn). Each row carries the plot's static state (terrain, feature, improvement, route, bonus, water/hills/peak/city flags, owner, working city, area, units, animals). It is a **reference companion** to any `(x,y)`/`plotIdx`-keyed log: grep the snapshot to recover the full plot context. It is not tied to one consumer — the worker/hunter AI was merely its origin (see [Origin & consumers](#origin--consumers)).

## When it fires

`writePlotSnapshot(const char* tag)` (`Sources/Utils/PlotSnapshot.cpp`, declared in `PlotSnapshot.h`) is called from `CvGame` (`Sources/Engine/CvGame.cpp`) at four points:

| Trigger | Tag | Site |
|---|---|---|
| Start of a new game | `start` | `CvGame::onFinalInitialized(bNewGame=true)` |
| Loading a save | `load` | `CvGame::onFinalInitialized(bNewGame=false)` |
| In-game map regeneration | `regen` | end of `CvGame::regenerateMap` |
| Every game turn | `turn` | top of `CvGame::doTurn`, before any per-turn processing |

The `turn` snapshot fires **before** AI decisions for that turn, so a log line emitted during turn N can be cross-referenced against `PlotSnapshot_turn_t<N>.csv` to recover the exact plot state the engine/AI saw that turn. This is the file you usually grep against.

## Output location & filenames

Each call writes a fresh, truncating (`fopen "w"`) file at:

```
%USERPROFILE%\Documents\My Games\Beyond The Sword\Logs\PlotSnapshot_<tag>_t<turn>.csv
```

`<turn>` is `GC.getGame().getGameTurn()` at capture time. Examples: `PlotSnapshot_start_t0.csv`, `PlotSnapshot_load_t142.csv`, `PlotSnapshot_regen_t0.csv`, `PlotSnapshot_turn_t201.csv`.

The file is written via raw `fopen`/`fprintf`/`fclose`, **not** `gDLL->logMsg`. This is deliberate (`PlotSnapshot.cpp` header comment): `gDLL` keeps log handles open for the process lifetime, which would make `remove()` of a prior snapshot fail with "file in use", defeating rotation. Owning open/close means the file is closed before the function returns, so the cleanup step below can delete old files — and one `fopen` replaces ~9600 `logMsg` calls per turn.

### Rotation & cleanup

Cleanup runs **after** `fclose`, and depends on the tag (`writePlotSnapshot`):

- **`turn` tag:** keep only the last 3 turn snapshots. When turn N is written (and `N >= 3`), `PlotSnapshot_turn_t<N-3>.csv` is deleted (`deleteTurnSnapshot`). On-disk per-turn footprint stays ~3× file-size regardless of session length.
- **`start` / `load` / `regen` tag:** `deleteAllPlotSnapshots(filename)` wipes **every other** `PlotSnapshot_*.csv` in the `Logs/` dir except the one just written — clearing both leaked turn snapshots from a crashed prior session and old `start`/`load`/`regen` one-offs. So a one-off survives turn rotation but does **not** survive the next start/load/regen. To keep a permanent record of a particular turn, copy the file out of `Logs/` before the next non-turn snapshot fires.

### Path-resolution caveat (carry this)

The path is resolved from `%USERPROFILE%\Documents\...\Logs\` (`buildLogsAbsolutePath`), **not** via the shell API `SHGetFolderPath` — `shlobj.h` is avoided because it clashes with C2C's `CATEGORY_INFO` macro. This works on default Windows layouts but does **not** handle Documents redirection (e.g. Documents moved to OneDrive): in that case writes fail entirely and any deletion silently fails, so old snapshots accumulate (cosmetic overhead, never read by anything).

## Schema

The first line is a `#`-prefixed comment carrying the schema version and capture metadata; the second is the column header; the rest are one row per plot. Timestamps are deliberately absent (each row is parseable CSV).

```
# PlotSnapshot schema=2 tag=start turn=0 mapW=80 mapH=52 numPlots=4160
plotIdx,x,y,terrain,feature,improvement,route,bonus,isWater,isHills,isPeak,isCity,isCityRadius,owner,workingCityId,workingCityName,area,improvementCurrentValue,numUnits,animals
```

The current schema version is **`schema=2`** (emitted literally in `writePlotSnapshot`). It is bumped whenever a column is added or semantics change; consumers can branch on it.

| Column | Source | Notes |
|---|---|---|
| `plotIdx` | loop counter `i` | matches `GC.getMap().plotByIndex(i)`; `plotIdx == y * mapW + x` (standard Civ4 indexing; `mapW` is in the header line, so the formula is self-contained) |
| `x`, `y` | `pPlot->getX()`, `getY()` | the plot's grid coordinates |
| `terrain` | `terrainName(getTerrainType())` | XML type-string, or `NONE` when `NO_TERRAIN` |
| `feature` | `featureName(getFeatureType())` | `NONE` when `NO_FEATURE` |
| `improvement` | `improvementName(getImprovementType())` | `NONE` when `NO_IMPROVEMENT` |
| `route` | `routeName(getRouteType())` | `NONE` when `NO_ROUTE` |
| `bonus` | `bonusName(getBonusType())` | **ground truth** — the raw bonus on the plot, regardless of per-team tech obsolescence. A consumer using `getNonObsoleteBonusType(team)` may see a different value (see [What's not captured](#whats-not-captured-yet)) |
| `isWater` | `pPlot->isWater()` | `0` / `1` |
| `isHills` | `pPlot->isHills()` | `0` / `1` |
| `isPeak` | `pPlot->isAsPeak()` | `0` / `1` |
| `isCity` | `pPlot->isCity()` | `0` / `1` |
| `isCityRadius` | `pPlot->isCityRadius()` | `0` / `1` |
| `owner` | `pPlot->getOwner()` | player ID, or `-1` for unowned |
| `workingCityId` | `pWorkingCity->getID()` | `-1` if no working city |
| `workingCityName` | `pWorkingCity->getName()`, sanitised | non-ASCII → `?`; commas/quotes/newlines → `_` (`copyCityNameSanitized`), so the row stays splittable on `,` without quoting |
| `area` | `pPlot->area()->getID()` | `-1` if no area; useful for filtering same-landmass plots |
| `improvementCurrentValue` | `pPlot->getImprovementCurrentValue()` | **read-only** — the snapshot deliberately does NOT call the lazy-init mutator `setImprovementCurrentValue()`; `0` here means "field not yet initialised", not "value is zero" |
| `numUnits` | `pPlot->getNumUnits()` | total units on the plot (all owners) |
| `animals` | per-animal token list | `|`-separated, one token per `isAnimal()` unit: `<UnitType>@o<owner>c<combat>a<aggression>e<enemyOfActiveTeam>`. `c` is XML `iCombat` (in-game strength = `c/100`); `a` is `iAggression` (`0` = passive prey); `e` is `1`/`0`/`-1` for at-war-with-active-team / not / no-active-team. Empty when no animals (`appendAnimalsField`) |

## Joining a log line back to plot state

Any log line that references a plot by `at=(x,y)` (or carries a `plotIdx`) can be resolved to full plot context:

1. Find the line, e.g. in `BuildEvaluation.log`:
   ```
   [WAI/best] at=(48,32) bonus=BONUS_WHEAT build=BUILD_FARM value=30000 (improve)
   ```
2. Pick the most recent snapshot at or before that turn (read the `t<turn>` suffix).
3. Grep by coordinate, or `awk` by `plotIdx`:
   ```
   grep ",48,32," PlotSnapshot_turn_t201.csv
   awk -F, '$1 == 2588' PlotSnapshot_turn_t201.csv
   ```
   ```
   2588,48,32,TERRAIN_GRASS,NONE,NONE,NONE,BONUS_WHEAT,0,0,0,0,1,0,-1,,3,0,1,
   ```
The row shows unowned grassland with Wheat, in city radius, no improvement yet — the context behind a "build Farm" decision.

## What's not captured (yet)

Honest gaps; all are non-breaking schema extensions if added (bump `schema=`):

- **Per-team / per-player view.** `getNonObsoleteBonusType(team)` varies by team (tech obsolescence) and fog-of-war reveal; the snapshot captures only raw `getBonusType()`. Per-team `bonus_team<N>` columns could be added later.
- **Yields.** `calculateNatureYield(...)` / `calculateImprovementYieldChange(...)` are not captured; capturing them would let an offline harness verify yield scoring without running the game.
- **Adjacency.** Neighbour plot indices (useful for irrigation/road analyses).
- **Path reachability.** Per-(unit, plot) `generatePath` results — needed to fully reproduce a pathing-dependent decision offline.

## Origin & consumers

PlotSnapshot's **origin** was supporting worker/hunter-AI build evaluation: `plotIdx` matches the key in the worker AI's bonus-evaluation cache, `isCityRadius` mirrors the `bCityRadius` local in its improve path, and the `animals` field is a companion to hunter-AI engage decisions. But that is **one consumer, not its purpose** — the snapshot is a general map/city-reference surface, useful for any analysis or shadow that needs to reconstruct static plot state at a known turn. The worker-AI emitter lives at `Sources/AI/CvWorkerAI.cpp` (`[WAI/*]` lines, `BuildEvaluation.log`); the hunter-AI emitter at `Sources/AI/CvHunterAI.cpp` (`[HAI/*]` lines). The full field taxonomy of those log lines is in the [logging field catalog](logging-field-catalog.md).

## See also
- [`README.md`](README.md) — the observability scale + the three canonical hook shapes; this snapshot is the *snapshot-file* form of the surveillance net ([DEC-obs-scale](../../architecture/decisions.md#dec-obs-scale)).
- [`http-server.md`](http-server.md) — the live HTTP/SSE surface and the live-read rules; note PlotSnapshot is a written `.log`-dir CSV, so the same "game holds logs open" caveat applies to reading it mid-session.
- [`logging-field-catalog.md`](logging-field-catalog.md) — the field-level catalog of the `[WAI/*]`/`[HAI/*]` (and all other) log lines whose `(x,y)`/`plotIdx` keys join against this snapshot.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing.
