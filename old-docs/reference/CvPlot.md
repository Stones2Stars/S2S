# CvPlot

**File:** `CvPlot.h` / `CvPlot.cpp`  
**Uses:** `CvGameObject`, `CvProperties`, `CvPlotPaging`

## Overview

`CvPlot` represents a single tile on the game map. It is the most queried object in
the DLL — almost every AI evaluation, movement check, combat calculation, and
city-founding score reads from `CvPlot`.

Each plot carries:

- Terrain, feature, bonus resource, improvement, and route identifiers.
- Owner, city pointer, unit stack, and river/coast edge flags.
- A [`CvProperties`](CvProperties.md) instance for simulation variables (crime, pollution, …).
- Path-planner cache (`CvPathGeneratorPlotInfo`) for `CvPathGenerator`.

## Coordinate Access

| Method | Description |
|---|---|
| `getX()` / `getY()` | Grid coordinates. |
| `getViewportX()` / `getViewportY()` | Screen-space coordinates (via `CvViewport`). |

## Terrain & Features

| Method | Description |
|---|---|
| `getTerrainType()` | Returns the `TerrainTypes` enum value. |
| `getFeatureType()` | Returns the `FeatureTypes` enum value (forest, jungle, …). |
| `getBonusType(TeamTypes)` | Returns the `BonusTypes` visible to a given team. |
| `getImprovementType()` | Returns the current `ImprovementTypes`. |
| `getRouteType()` | Returns the `RouteTypes` (road, railroad, …). |
| `isWater()` / `isCoastalLand()` / `isLake()` | Quick terrain category tests. |
| `isHills()` / `isPeak()` / `isFlatlands()` | Elevation tests. |

## Ownership & Vision

| Method | Description |
|---|---|
| `getOwner()` | Returns the `PlayerTypes` who owns this plot. |
| `isVisible(TeamTypes, bool)` | Returns `true` if a team can currently see this plot. |
| `isRevealed(TeamTypes, bool)` | Returns `true` if a team has ever seen this plot. |

## Units & Cities

| Method | Description |
|---|---|
| `getPlotCity()` | Returns the `CvCity*` on this plot, or `NULL`. |
| `getNumUnits()` | Number of units currently on the plot. |
| `getUnitByIndex(int)` | Returns a unit on the plot by stacking index. |
| `getBestDefender(eOwner, eAttacker, pAttacker, ...)` | Returns the best defending unit; parameterised by `EDefenderScore::flags`. |
| `getNumDefenders(PlayerTypes)` | Counts visible defenders for a player. |
| `getNumVisibleEnemyDefenders(pUnit)` | Counts how many enemy defenders a given unit can see. |

## Strength Evaluation (Flag Sets)

### `EDefenderScore::flags`

Controls which considerations apply when selecting a best defender:

| Flag | Effect |
|---|---|
| `TestAtWar` | Only include units belonging to teams at war with the querying team. |
| `TestPotentialEnemy` | Include potential (not yet declared) enemies. |
| `TestCanMove` | Only include units that have movement remaining. |
| `Assassinate` | Favour lower-strength units (assassination target selection). |
| `ClearCache` | Bypass cached result and recompute. |

### `StrengthFlags::flags`

Controls strength summation for the same plot:

| Flag | Effect |
|---|---|
| `DefensiveBonuses` | Apply terrain and fortification bonuses to each unit's strength. |
| `TestAtWar` | Only sum units at war with the querying team. |
| `TestPotentialEnemy` | Include potential enemies. |
| `CollateralDamage` | Account for collateral damage splash when summing attack strength. |

## Properties

`CvPlot` embeds a `CvProperties` instance for tile-level simulation variables.

| Method | Description |
|---|---|
| `getProperties()` | Returns a `const CvProperties*` for read access. |
| `getPropertiesWritable()` | Returns a mutable `CvProperties*` for modification. |

## Path Cache

`CvPlot` also caches pathfinding metadata (`CvPathGeneratorPlotInfo`) so that
`CvPathGenerator` can avoid redundant edge-validity lookups during a single search.

## Related

- [`CvMap`](CvMap.md) — owns and indexes all `CvPlot` instances  
- [`CvProperties`](CvProperties.md) — embedded property system  
- [`CvPathGenerator`](CvPathGenerator.md) — consumes per-plot path cache  
- [`CvCityAI`](CvCityAI.md) — `AI_plotValue()` scores plots for citizen assignment  
- [`CvPlayerAI`](CvPlayerAI.md) — `AI_foundValue()` scores plots for city founding  
