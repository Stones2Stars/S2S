# CvPathGenerator / CvPath — the in-DLL unit pathfinder

> **Status:** reference   ·   **Verified against:** 44720ee1f (2026-06-20)
> **Grounding:** `Sources/Infrastructure/CvPathGenerator.h` (`CvPath`, `CvPathGeneratorBase`, the callback typedefs, `CvPathGenerator` API), `Sources/Engine/CvSelectionGroup.h` (`getPath`/`getPathGenerator` consumer accessors). Line numbers drift — confirm the named class/function, not the integer.
> A custom A\*-based unit pathfinder with fully pluggable cost and validity callbacks. It **supersedes the engine's legacy `FAStar` unit finder** ([`pathfinding`](pathfinding.md) §8) for actual unit movement, adding per-unit-type customisation, multi-turn planning, and an AI-always pathing mode — without subclassing. This is the path generator that ships (the legacy engine unit finder is compiled out under `USE_OLD_PATH_GENERATOR`).

`CvPathGenerator` lives in `Sources/Infrastructure/`, not `Sources/Engine/`. The shared instance is reached through `CvSelectionGroup::getPathGenerator()` (static); a selection group's last computed path through `CvSelectionGroup::getPath()`.

---

## 1. `CvPath` — the computed path

An immutable, forward-iterable path produced by `CvPathGenerator` (`Sources/Infrastructure/CvPathGenerator.h`).

### Iteration

```cpp
for (CvPath::const_iterator it = path.begin(); it != path.end(); ++it)
{
    CvPlot* pPlot = it->plot();
    int     iTurn = it->turn();
}
```

### Query methods

| Method | Description |
|---|---|
| `length()` | Total number of nodes in the path. |
| `lastPlot()` | The terminal `CvPlot*`. |
| `containsEdge(pFrom, pTo)` | `true` if the path passes through the given directed edge. |
| `containsNode(pPlot)` | `true` if a plot appears anywhere in the path. |
| `trimBefore(pPlot)` | Discards all nodes before the given plot; the path then starts at that plot. |
| `movementRemaining()` | Movement points left in the final turn of the path. |
| `cost()` | Total computed path cost. |

---

## 2. `CvPathGeneratorBase` — the contract

Abstract base providing the `useAIPathingAlways()` flag and the `getTerminalPlot()` contract. Any class that drives pathfinding (selection-group movement, air-strike range checks) is a `CvPathGeneratorBase`.

| Method | Description |
|---|---|
| `useAIPathingAlways()` | `true` when AI pathing rules apply even to human units (returns the `m_useAIPathingAlways` member). |
| `getTerminalPlot()` | The destination plot (pure virtual). |

---

## 3. Callback typedefs

All path behaviour is customised through function-pointer callbacks, supplied via `CvPathGenerator::Initialize(HeuristicFunc, CostFunc, ValidFunc, TerminusValidFunc, TurnEndValidCheckNeeded)`:

| Typedef | Signature excerpt | Purpose |
|---|---|---|
| `HeuristicCost` | `(pGroup, fromX, fromY, toX, toY, &iLimitCost) → int` | Heuristic estimate of remaining cost; sets a search limit. |
| `EdgeCost` | `(generator, pGroup, fromX, fromY, toX, toY, flags, &iMovementRemaining, iPathTurns, &iToNodeCost, bIsTerminalNode) → int` | Actual cost of traversing an edge, including movement consumption. |
| `EdgeValidity` | `(pGroup, fromX, fromY, toX, toY, flags, isTerminus, bAssertTerminatesMove, iPathTurns, &bToNodeInvalidity) → bool` | `true` if an edge may be traversed; sets the invalidity output. |
| `TerminusValidity` | `(pGroup, toX, toY, flags, &bRequiresWar) → bool` | `true` if the destination plot is a valid terminal; sets the war-required output. |
| `TurnEndValidityCheckRequired` | `(pGroup, flags) → bool` | `true` if turn-end validity must be re-checked for this group. |

The callbacks take a `const CvSelectionGroup*` so cost/validity vary per unit type — this is the per-unit-type customisation the legacy engine finder lacked.

---

## 4. `CvPathGenerator` — the generator

`class CvPathGenerator : public CvPathGeneratorBase`, constructed with a `CvMap*`. Key public surface:

| Method | Description |
|---|---|
| `Initialize(...)` | Install the five callbacks (§3). |
| `generatePath(pFrom, pTo, pGroup, iFlags, iMaxTurns, iOptimizationLimit = -1)` | Run a search for `pGroup` from `pFrom` to `pTo`; returns whether a path was found. |
| `newgeneratePath(...)` | Same signature; the reworked search path. |
| `generatePathForHypotheticalUnit(pFrom, pTo, ePlayer, eUnit, iFlags, iMaxTurns)` | Path for a *hypothetical* unit type (no live `CvUnit`) — used by the contract broker / Python (`CyMap`) for distance probes. |
| `haveRouteLength(pTo, pGroup, iFlags, &iRouteLen)` | Cheap "is there a route and how long" probe. |
| `getLastPath()` | The `CvPath&` from the most recent search on this generator. |
| `getTerminalPlot()` | The destination plot of the current search. |
| `reset()` | Clear generator state. |

### Node pool and edge-cost caching

`CvPathGenerator` manages a pool of `CvPathNode` objects (`CvAllocationPool<CvPathNode>`, stored in `PoolBucket` vectors) to avoid per-node heap allocation during each search. Per-plot edge costs are cached through `CvPathPlotInfoStore` / `CvPathGeneratorPlotInfo` so repeated searches over the same terrain don't re-cost every edge. Searches are driven by a `std::priority_queue` ordered by a `CvPathNodeComparer`.

---

## 5. Consumers

The generator is reached statically; selection groups own their last path:

| Accessor | What it returns | Where |
|---|---|---|
| `CvSelectionGroup::getPathGenerator()` | The shared `CvPathGenerator*` | `Sources/Engine/CvSelectionGroup.h` |
| `CvSelectionGroup::getPath()` | The group's last `CvPath&` | `Sources/Engine/CvSelectionGroup.h` |

Verified call sites: `CvUnitAI` (`haveRouteLength` for range checks), `CvContractBroker` (`generatePathForHypotheticalUnit` + `getLastPath().length()` for work-request distance), `CyMap` (Python hypothetical-unit pathing), `CvEventReporter` (`SelfTest`). Movement entry points (`CvSelectionGroup::generatePath`, `getPathFirstPlot`, `getPathEndTurnPlot`, …) route to this generator via the `#else` branch of their `USE_OLD_PATH_GENERATOR` split — see [`pathfinding`](pathfinding.md) §8.

## See also

- [`pathfinding`](pathfinding.md) — the engine `FAStar` finders. §8 there is the old-vs-new pair to this doc: the legacy engine unit finder this generator replaces (compiled out under `USE_OLD_PATH_GENERATOR`), and the step/area/plot-group/route/border finders that remain live for non-movement queries.
- `Sources/Engine/CvSelectionGroup.h` / `.cpp` — primary consumer; owns `getPath()` and the static `getPathGenerator()`.
- `Sources/Infrastructure/CvPathGenerator.h` / `.cpp` — the class itself (`CvPath`, `CvPathGeneratorBase`, callbacks, node pool, plot-info cache).
