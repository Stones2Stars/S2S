# Pathfinding — the engine `FAStar` finders

> **Status:** reference   ·   **Verified against:** 44720ee1f (2026-06-20)
> **Grounding:** `Sources/Engine/CvMap.cpp` (`setup`, finder wiring), `Sources/Engine/CvGameCoreUtils.cpp` (`step*`/`path*`/`route*`/`border*`/`area*`/`plotGroup*` callbacks), `Sources/Engine/CvPlot.cpp` (`calculatePathDistanceToPlot`). Line numbers drift — confirm the named function, not the integer.
> This is the low-level A\* layer: the engine `FAStar` finder object, the callback contract that drives it, and the helpers built on top (step distance, area / plot-group flood fills, route/border connectivity). It is **not** the unit-movement pathfinder — that is [`CvPathGenerator`](path-generator.md), the in-DLL A\* that supersedes the legacy engine unit finder (see §8).

There are **two** pathfinding systems in the codebase. This doc covers the **engine `FAStar` layer** (`gDLL->getFAStarIFace()` + the global finders on `GC`). The legacy engine *unit* finder is compiled out (`USE_OLD_PATH_GENERATOR` off — §8); the other engine finders here are always live and serve every non-movement query (step distance, area labelling, plot-group/route/border connectivity).

---

## 1. The `FAStar` finder object

`gDLL->getFAStarIFace()` returns the engine's A\* interface. It lives across the EXE/DLL boundary — the DLL cannot construct a finder's internals directly, only drive it through this interface.

A **finder is a stateful object.** It owns:

- a node grid sized to the map,
- the open/closed lists for the current search,
- a "last node" pointer to the destination node of the **most recent** search run on it,
- an integer "info" value (`SetInfo`/`GetInfo`) and a user data pointer (`SetData`/`GetData`).

### Lifecycle / interface

| Call | Purpose |
|---|---|
| `create()` | Allocate a new, independent finder. |
| `Initialize(finder, w, h, wrapX, wrapY, DestValid, Heuristic, Cost, Valid, NotifyChild, NotifyList, NotifyTerminal)` | Configure grid + the callbacks (see §2). |
| `SetData(finder, ptr)` | Attach a user data pointer, delivered to every callback as `const void* pointer`. |
| `GeneratePath(finder, fromX, fromY, toX, toY, bReverse, iInfo, bRebuild)` | **Run** the search on `finder`. `iInfo` becomes `GetInfo(finder)` inside the callbacks. Returns whether a path was found. |
| `GetLastNode(finder)` | Return the destination `FAStarNode*` of the **last search that ran on `finder`** (or the last reached node). The answer lives here. |
| `ForceReset(finder)` | Clear finder state before a fresh search. |
| `destroy(finder)` | Free a finder created with `create()`. |

> **Critical:** `GetLastNode` must be called on the **same finder** that `GeneratePath` ran on. Reading a different finder returns stale data from whatever last used it — this is exactly bug #73 (see §7).

The global finders are created and `Initialize`d once per map in `CvMap::setup` (`Sources/Engine/CvMap.cpp`, ~lines 306–312). Ad-hoc searches `create()` a local finder (see §6).

---

## 2. The callback contract

`Initialize` takes the grid parameters then a fixed sequence of function pointers. Any of them may be `NULL` if that finder does not need it. In `Initialize` order:

| Slot | Signature | Role |
|---|---|---|
| **DestValid** | `int(int iToX, int iToY, const void* p, FAStar* f)` | Is the destination acceptable? Lets the search terminate (e.g. allow a water→land transition only at a specific plot). |
| **Heuristic** | `int(int iFromX, int iFromY, int iToX, int iToY)` | Admissible cost estimate to the goal. |
| **Cost** | `int(FAStarNode* parent, FAStarNode* node, int data, const void* p, FAStar* f)` | Cost of stepping from `parent` to `node`. |
| **Valid** | `int(parent, node, data, p, f)` | **Edge gate** — may the search step from `parent` onto `node`? Returns `TRUE`/`FALSE`. This is where most game rules live. |
| **NotifyChild** ("Add") | `int(parent, node, data, p, f)` | Called when `node` is (re)parented; used to **propagate payload** down the tree (e.g. accumulate distance into `node->m_iData1`). |
| **NotifyList** | `int(parent, node, data, p, f)` | Called for each node added to the result set; used by flood-fill counters (area/plot-group). |
| **NotifyTerminal** | — | Unused in this codebase (always `NULL`). |

The `int data` parameter carries an `ASNC_*` phase flag (e.g. `ASNC_INITIALADD` for the start node) so a callback can special-case the first node.

### `FAStarNode` fields you will use

- `m_iX`, `m_iY` — the plot coordinate of this node.
- `m_iData1`, `m_iData2` — **finder-specific payload**, written by the NotifyChild/Add callback. For the **step finder**, `m_iData1` accumulates the **tile count** from the start (set by `stepAdd`). For the legacy **unit path finder**, `m_iData1`/`m_iData2` carry movement bookkeeping (moves remaining / turn count) — see `pathAdd` in `Sources/Engine/CvGameCoreUtils.cpp`. That legacy unit path is compiled out by default (`USE_OLD_PATH_GENERATOR` off; see §8).
- parent linkage — walk it to reconstruct the path.

---

## 3. The global finders

Each map (multimap-aware: indexed by `MapTypes`) owns these finders, accessed via `GC.get*Finder(eMap = NO_MAP)`. Wiring is in `CvMap::setup`; the callbacks are all defined in `Sources/Engine/CvGameCoreUtils.cpp`.

| Finder | Accessor | DestValid / Heuristic / Cost | Valid | Notify | Purpose |
|---|---|---|---|---|---|
| Unit path (AI) | `getPathFinder` | `pathDestValid` / `pathHeuristic` / `pathCost` | `pathValid` | `pathAdd` | **Legacy** BtS unit-movement A\*, gated by `USE_OLD_PATH_GENERATOR` — **not defined** (off in fbuild + IntelliSense). Dormant: the finder is still `Initialize`d, but the DLL code that drives it is `#ifdef`'d out. Unit movement uses [`CvPathGenerator`](path-generator.md) instead. |
| Unit path (UI) | `getInterfacePathFinder` | same as above | `pathValid` | `pathAdd` | Same: legacy/dormant under the off-by-default macro. |
| **Step** | `getStepFinder` | `stepDestValid` / `stepHeuristic` / `stepCost` | `stepValid` | `stepAdd` | Tile-count distance / reachability (terrain only, no diplomacy). |
| Route | `getRouteFinder` | — | `routeValid` | — | Connectivity along existing routes (roads). |
| Border | `getBorderFinder` | — | `borderValid` | — | Border/culture-adjacency traversal. |
| Area | `getAreaFinder` | — | `areaValid` | `joinArea` (NotifyList) | Continent/ocean-body labelling (flood fill). |
| Plot group | `getPlotGroupFinder` | — | `plotGroupValid` | `countPlotGroup` (NotifyList) | Trade-network / plot-group connectivity (flood fill). |

Finders with only a `Valid` (+ NotifyList) callback are used for **flood fills**, not A\* to a target — see §5.

---

## 4. The step callbacks in detail

The step finder answers "how many tiles is the shortest path?" Its callbacks live in `Sources/Engine/CvGameCoreUtils.cpp`:

**`stepCost` → always `1`.**

```cpp
int stepCost(...) { return 1; }
```

Every legal move costs one. So step "distance" is a **tile hop count**, independent of terrain, roads, or movement points — *not* a turn count. With `stepHeuristic = stepDistance` (the wrap-aware Chebyshev / king-move tile distance) the search is effectively a uniform-cost BFS for the fewest-tiles path.

**`stepAdd` → accumulates the distance into `m_iData1`.**

```cpp
node->m_iData1 = (data == ASNC_INITIALADD) ? 0 : (parent->m_iData1 + 1);
```

This is why `GetLastNode(finder)->m_iData1` is the path length.

**`stepValid` → terrain-only edge gate (the *global* step finder uses this).** Rejects: impassable plots (peaks/ice), the Super-Forts choke plot (via `GetInfo`), cross-area moves, and diagonal water "corner cuts" across a land isthmus.

**`stepDestValid` → destination acceptance.** Normally requires same area, but if the finder's info int names a land plot it permits a single water↔land transition (used to find transport assault landing spots).

### `teamStepValid` — the team-restricted variant

`teamStepValid` is **not** wired to the global step finder. It is supplied to a *locally created* finder when callers want "where could this team's units actually go on the ground" (e.g. `CvPlot::calculatePathDistanceToPlot`, `Sources/Engine/CvPlot.cpp`). Differences from `stepValid`:

- It performs the same area + diagonal-isthmus structural checks, **but does not check `isImpassable()`** — a team path can route over impassable tiles that the generic step finder rejects (a known quirk).
- It adds **diplomacy filtering** on the destination tile's owner (`ePlotTeam`): passable if the tile is unowned, owned by the explicitly-allowed second team (`teamVec[1]`), friendly territory, a team you have a war plan against / are at war with, or covered by open borders; otherwise **blocked** (neutral closed borders).

The team data arrives via `SetData`:

```cpp
std::vector<TeamTypes> teamVec;
teamVec.push_back(eTeam);     // teamVec[0] = the team being pathed for
teamVec.push_back(NO_TEAM);   // teamVec[1] = optional "also allowed" team (none)
gDLL->getFAStarIFace()->SetData(finder, &teamVec);
```

Inside the callback, `pointer` is that `teamVec`; `teamVec[0]` drives all the diplomacy checks and `teamVec[1]` is an extra "treat this team's tiles as passable" exception.

---

## 5. Flood-fill usage (area / plot group)

Area and plot-group "finders" don't path to a target — they flood the connected region from a seed and tag/count every reachable node. The idiom is `GeneratePath` with a **destination of `(-1, -1)`** and the region id in the info int (from `CvMap::calculateAreas`, `Sources/Engine/CvMap.cpp`):

```cpp
gDLL->getFAStarIFace()->GeneratePath(&GC.getAreaFinder(), pPlot->getX(), pPlot->getY(),
                                     -1, -1, pPlot->isWater(), iArea);
```

The NotifyList callback (`joinArea`, `countPlotGroup`) runs for every node visited, stamping/counting membership.

---

## 6. Worked example: `CvPlot::calculatePathDistanceToPlot`

This returns the team-legal **tile distance** from this plot to a target (`Sources/Engine/CvPlot.cpp`, `calculatePathDistanceToPlot`). It cannot reuse the global step finder (which uses terrain-only `stepValid`), so it builds a local one configured with `teamStepValid`:

```cpp
std::vector<TeamTypes> teamVec;
teamVec.push_back(eTeam);
teamVec.push_back(NO_TEAM);
FAStar* pTeamStepFinder = gDLL->getFAStarIFace()->create();
gDLL->getFAStarIFace()->Initialize(pTeamStepFinder, w, h, wrapX, wrapY,
    stepDestValid, stepHeuristic, stepCost, teamStepValid, stepAdd, NULL, NULL);
gDLL->getFAStarIFace()->SetData(pTeamStepFinder, &teamVec);
gDLL->getFAStarIFace()->GeneratePath(pTeamStepFinder, getX(), getY(),
    pTargetPlot->getX(), pTargetPlot->getY(), false, 0, true);

const FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pTeamStepFinder); // read the SAME finder
const int iPathDistance = pNode ? pNode->m_iData1 : -1;                          // m_iData1 = tile count
gDLL->getFAStarIFace()->destroy(pTeamStepFinder);
return iPathDistance;
```

The pattern: `create` → `Initialize` (with the desired callbacks) → `SetData` → `GeneratePath` → `GetLastNode` (same finder) → read `m_iData1` → `destroy`.

---

## 7. Pitfalls

- **Finders are stateful and shared.** The `GC.get*Finder()` finders are reused all over the engine. `GetLastNode` must read the finder that just ran `GeneratePath`; reading a *different* finder yields leftover data from an unrelated search. This was bug **#73**: `calculatePathDistanceToPlot` generated on a local `pTeamStepFinder` but read `GetLastNode(&GC.getStepFinder())` (the global one) — silently returning a stale, wrong distance. Fix: `GetLastNode(pTeamStepFinder)`.
- **Not re-entrant.** Don't run a global finder while another search using the same finder is in flight, and don't hold a `GetLastNode` pointer across another search on that finder.
- **`stepValid` vs `teamStepValid` have different rules.** `stepValid` checks `isImpassable()` and choke plots but no diplomacy; `teamStepValid` checks diplomacy but **not** `isImpassable()`. Pick the one whose policy you actually want.
- **`SetInfo`/`GetInfo`** is the channel for the per-search int (the `iInfo` arg to `GeneratePath`): a choke/invalid plot index for the step finder, a permitted transition plot for `stepDestValid`, or the region id for area/plot-group fills.
- **Multimap:** finders are per-`MapTypes`; pass the right map to `GC.get*Finder(eMap)` when not operating on the active map.

---

## 8. Relationship to `CvPathGenerator` (old vs new unit pathfinder)

Unit movement has **two** implementations, selected at compile time by `USE_OLD_PATH_GENERATOR`. That macro is **not defined** anywhere (verified 2026-06-20: it appears only in `#ifdef`/`#else` guards in `CvUnit`/`CvSelectionGroup`/`CvUnitAI`/`CvGameCoreUtils`, never in a `#define`), so the **old** path is compiled out and the **new** one ships.

- **Old (`USE_OLD_PATH_GENERATOR` defined — OFF here):** the engine `FAStar` unit finders (`getPathFinder` / `getInterfacePathFinder`) driven by the `path*` callbacks. Paths are `FAStarNode*` walked via `m_pParent`, with `m_iData1`/`m_iData2` carrying moves/turns (`m_iData2 == 1` = end-of-turn node), retrieved through `CvSelectionGroup::getPathLastNode()`. Pulls in `FAStarNode.h` (hence the `#ifdef USE_OLD_PATH_GENERATOR #include "FAStarNode.h"` guards in `CvUnit`/`CvSelectionGroup`).
- **New (default):** [`CvPathGenerator`](path-generator.md), an in-DLL pluggable A\* with per-unit cost/validity callbacks, a `CvPathNode` pool, multi-turn planning, an AI-always pathing mode, and optimization limits. Paths are `CvPath` objects iterated via `.plot()` / `.turn()`, retrieved through `getPath()` / `getPathGenerator()`.

In the code the split is direct: `generatePath`, `getPathFirstPlot`, `getPathEndTurnPlot`, etc. each have an `#ifdef USE_OLD_PATH_GENERATOR` (old, `FAStarNode`-based) / `#else` (new, `CvPath`-based) — the `#else` is what compiles. The richer `generatePath(pFrom, pTo, iFlags, bReuse, piPathTurns, iMaxPathLen, iOptimizationLimit)` signature is the new generator's. (The old engine finder also had a caching bug — it reused end-of-turn costs in non-end-of-turn inter-tile moves — which the new generator avoids; see the comment in `CvSelectionGroup::generatePath`.)

**Independent of the macro**, the other engine `FAStar` finders documented here are always active and handle the non-movement queries: **step distance / reachability** (step finder), **route connectivity** (route finder), **area labelling** and **plot-group / trade-network** connectivity (area & plot-group finders), and **borders** (border finder). The unit `getPathFinder`/`getInterfacePathFinder` are still `Initialize`d in `CvMap::setup` but, with the macro off, are not driven for in-DLL unit movement.

## See also

- [`CvPathGenerator`](path-generator.md) — the in-DLL unit-movement pathfinder that supersedes the legacy engine unit finder described in §8 (the old-vs-new pair).
- `Sources/Engine/CvMap.cpp` (`CvMap::setup`) — owns and wires every global finder.
- `Sources/Engine/CvPlot.cpp` (`calculatePathDistanceToPlot`) — the canonical local-finder + `teamStepValid` consumer (§6).
- `Sources/Engine/CvGameCoreUtils.cpp` — all callbacks + helpers (`step*`, `path*`, `route*`, `border*`, `area*`, `plotGroup*`, `stepDistance`, `plotDistance`).
