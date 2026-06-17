# CvMap

**File:** `CvMap.h` / `CvMap.cpp`  
**Inherits:** `CvMapInterfaceBase`

## Overview
`CvMap` owns and manages the entire spatial state of the game world for one map slot.
It stores the flat array of all `CvPlot` objects, the list of `CvArea` regions, plot
groups, viewport state, and multi-map metadata.

Access is through `GC.getMap()` (returns the active map) or `GC.getMap(MapTypes)` for
the parallel-maps feature.

## Coordinate Utilities

### `coordRange(iCoord, iRange, bWrap)`
Free function (defined in `CvMap.h`) that normalises a coordinate with optional
wrapping:
- If `bWrap` is `true` and `iRange != 0`, the value is folded into `[0, iRange)`.
- Used throughout the engine whenever plot coordinates may cross map edges.

## Lifecycle
| Method | Description |
|---|---|
| `CvMap(MapTypes eMap)` | Constructor; sets the map type slot. |
| `~CvMap()` | Destructor; frees all plot and area objects. |
| `init(CvMapInitData*)` | Allocates the plot grid, sets dimensions, and initialises areas. |
| `reset(CvMapInitData*)` | Resets all plots and areas to initial state. |
| `setupGraphical()` | Initialises visual/graphical representation of the map. |
| `getUnderlyingMap()` | Returns `this` cast as `CvMapInterfaceBase*` (interface accessor). |

## Plot Access
| Method | Description |
|---|---|
| `plot(int iX, int iY)` | Returns the `CvPlot*` at grid coordinates; handles wrapping via `coordRange`. |
| `plotByIndex(int iIndex)` | Returns the `CvPlot*` at a flat array index. |
| `numPlots()` | Returns the total number of plots (`width × height`). |
| `getGridWidth()` / `getGridHeight()` | Grid dimensions. |
| `isWrapX()` / `isWrapY()` | Whether the map wraps on each axis. |

## Areas
| Method | Description |
|---|---|
| `getArea(int iID)` | Returns an `CvArea*` by identifier. |
| `getNumAreas()` | Total number of distinct landmass / ocean areas. |
| `recalculateAreas()` | Rebuilds area assignments for all plots (called after map changes). |

## Plot Groups & Connectivity
| Method | Description |
|---|---|
| `updatePlotGroups(bool reInitialize)` | Rebuilds trade / resource connectivity groups. |
| `getPlotGroup(PlayerTypes, CvPlot*)` | Returns the `CvPlotGroup` that contains a plot for a given player. |
| `updateBuildingCommerce()` | Recalculates building commerce bonuses after connectivity changes. |
| `updateTradeRoutes()` | Refreshes all trade route connections. |

## Multi-Map Support (Parallel Maps)
`CvMap` supports parallel map slots (e.g. an underground layer alongside the surface).
`MapTypes` differentiates slots; `getCurrentMap()` on `CvGame` indicates the active one.

## Viewport
`CvViewport` manages the visible sub-region of a large map. `CvMap` holds a pointer to
the active viewport and updates it when the camera scrolls.

## Related
- [`CvPlot`](CvPlot.md) — individual tile objects owned by this map  
- [`CvArea`](../CvArea.h) — landmass / water body regions  
- [`CvGlobals`](CvGlobals.md) — exposes `GC.getMap()` accessor  
- [`CvPathGenerator`](CvPathGenerator.md) — queries `CvMap` for terrain data during searches  
