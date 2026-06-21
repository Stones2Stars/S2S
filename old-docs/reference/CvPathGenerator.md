# CvPathGenerator / CvPath

**Files:** `CvPathGenerator.h` / `CvPathGenerator.cpp`

## Overview

A custom A\*-based pathfinder with fully pluggable cost and validity callbacks. It
replaces the engine's default `FAStar` with a more flexible implementation that
supports per-unit-type customisation, multi-turn path planning, and AI-always pathing
mode — all without subclassing.

## `CvPath`

An immutable, forward-iterable path computed by `CvPathGenerator`.

### Iteration

```cpp
for (CvPath::const_iterator it = path.begin(); it != path.end(); ++it)
{
    CvPlot* pPlot = it->plot();
    int     iTurn = it->turn();
}
```

### Query Methods

| Method | Description |
|---|---|
| `length()` | Total number of nodes in the path. |
| `lastPlot()` | Returns the terminal `CvPlot*`. |
| `containsEdge(pFrom, pTo)` | Returns `true` if the path passes through the given directed edge. |
| `containsNode(pPlot)` | Returns `true` if a plot appears anywhere in the path. |
| `trimBefore(pPlot)` | Discards all nodes before the given plot; path starts at that plot afterward. |
| `movementRemaining()` | Movement points left in the final turn of the path. |
| `cost()` | Total computed path cost. |

## `CvPathGeneratorBase`

Abstract base providing the `useAIPathingAlways()` flag and the `getTerminalPlot()`
contract.

| Method | Description |
|---|---|
| `useAIPathingAlways()` | Returns `true` when AI pathing rules should always apply, even for human units. |
| `getTerminalPlot()` | Returns the destination plot (pure virtual). |

## Callback Typedefs

All path behaviour is customised through function-pointer callbacks:

| Typedef | Signature excerpt | Purpose |
|---|---|---|
| `HeuristicCost` | `(pGroup, fromX, fromY, toX, toY, &limitCost) → int` | Heuristic estimate of remaining cost; sets a search limit. |
| `EdgeCost` | `(generator, pGroup, fromX, fromY, toX, toY, flags, &movRemaining, pathTurns, &toNodeCost, bIsTerminal) → int` | Actual cost of traversing an edge including movement consumption. |
| `EdgeValidity` | `(pGroup, fromX, fromY, toX, toY, flags, isTerminus, bAssertTerminatesMove, pathTurns, &bToNodeInvalidity) → bool` | Returns `true` if an edge may be traversed; sets invalidity output parameter. |
| `TerminusValidity` | `(pGroup, toX, toY, flags, &bRequiresWar) → bool` | Returns `true` if the destination plot is a valid terminal; sets war-required output. |
| `TurnEndValidityCheckRequired` | `(pGroup, flags) → bool` | Returns `true` if turn-end validity must be re-checked for this group. |

## `CvPathGeneratorBase` Implementors

Any class that drives pathfinding (e.g. selection-group movement, air-strike range
checks) implements `CvPathGeneratorBase` and supplies the callbacks above.

## Node Pool

`CvPathGenerator` manages a pool of `CvPathNode` objects (stored in `PoolBucket`
vectors) to avoid per-node heap allocation during each search.

## Related

- [`CvSelectionGroup`](CvSelectionGroup.md) — primary consumer of generated paths  
- [`CvMap`](CvMap.md) — provides terrain data consumed by callbacks  
- [`CvPlot`](CvPlot.md) — individual nodes in every path  
- [pathfinding](pathfinding.md) — the engine `FAStar` finders this replaces for unit movement, still used for step distance / area / plot-group / route connectivity queries  
