# CvCityAI

**File:** `CvCityAI.h` / `CvCityAI.cpp`  
**Inherits:** [`CvCity`](CvCity.md)

## Overview
Provides full automated management of a city. The class is the most method-rich in the
DLL; it covers citizen placement, production selection, building evaluation, worker
coordination, defence contracts, and property control.

Caching is central to its design: two dedicated caches prevent redundant calculations
during a single turn.

## Turn Entry Point
| Method | Description |
|---|---|
| `AI_doTurn()` | Main per-turn entry. Runs in order: update assignments → floating-defender contracts → choose production → update worker needs → property/threat update → governor/emphasis handling. |

## Citizen Assignment
| Method | Description |
|---|---|
| `AI_assignWorkingPlots()` | Optimises which tiles are worked and which specialist slots are filled. Ensures the city-centre tile is always worked; removes excess specialists; fills remaining population optimally. Updates UI when city is selected. |
| `AI_updateAssignWork()` | Calls `AI_assignWorkingPlots()` only when dirty and the human player is not in the city screen. |
| `AI_avoidGrowth()` | Returns true when growth would worsen happiness or health, or emphasise flags forbid it. |
| `AI_ignoreGrowth()` | Returns true when neither food nor great-people output is emphasised and food is not abundant. |
| `AI_specialistValue(eSpecialist, bAvoidGrowth, bRemove)` | Scores a specialist slot: yields, commerce, GPP (scaled by civics/era), experience, health, happiness, property effects, and investigation values for crime mechanics. |

## Production Selection
| Method | Description |
|---|---|
| `AI_chooseProduction()` | Top-level production decision. Checks for anarchy/occupation/danger; sets economic, military, and cultural priority flags; handles emergencies (defence, happiness, health, property control); scores and queues units, buildings, projects, or processes. |
| `AI_bestUnit(...)` | Scores all trainable unit types against AI-type weights and current needs; returns the best `UnitTypes`. |
| `AI_bestUnitAI(...)` | Returns the best `UnitAITypes` for what the city should train. |
| `AI_trained(UnitTypes, UnitAITypes)` | Called after a unit is produced; updates internal state. |

## Building Evaluation

### Focus Flags (`BUILDINGFOCUS_*`)
Buildings are scored against bitmasks of focus flags:

| Flag | Meaning |
|---|---|
| `BUILDINGFOCUS_FOOD` | Food output |
| `BUILDINGFOCUS_PRODUCTION` | Hammer output |
| `BUILDINGFOCUS_GOLD` | Commerce / gold |
| `BUILDINGFOCUS_RESEARCH` | Science |
| `BUILDINGFOCUS_CULTURE` | Culture |
| `BUILDINGFOCUS_DEFENSE` | City defence |
| `BUILDINGFOCUS_HAPPY` | Happiness |
| `BUILDINGFOCUS_HEALTHY` | Health |
| `BUILDINGFOCUS_EXPERIENCE` | Unit experience |
| `BUILDINGFOCUS_MAINTENANCE` | Maintenance reduction |
| `BUILDINGFOCUS_SPECIALIST` | Specialist slots |
| `BUILDINGFOCUS_ESPIONAGE` | Espionage points |
| `BUILDINGFOCUS_BIGCULTURE` | Large culture bonus |
| `BUILDINGFOCUS_WORLDWONDER` | World wonders |
| `BUILDINGFOCUS_DOMAINSEA` | Naval domain |
| `BUILDINGFOCUS_PROPERTY` | Property control (crime, pollution, …) |
| `BUILDINGFOCUS_INVESTIGATION` | Investigator coverage |

### Building scoring methods
| Method | Description |
|---|---|
| `AI_buildingValue(BuildingTypes, iFocusFlags, ...)` | Full building score against a focus-flag set. |
| `AI_buildingValueThreshold(...)` | As above but returns early if a threshold is met (performance). |
| `AI_bestBuildingsThreshold(...)` | Finds the highest-value building the city can construct. |
| `AI_scoreBuildingsFromListThreshold(...)` | Scores a pre-filtered list of buildings. |
| `AI_getBuildingYieldValue(...)` | Yield-only component of a building score. |
| `AI_getBuildingCommerceValue(...)` | Commerce-only component. |
| `AI_getBuildingProductionValue(...)` | Production-only component. |
| `buildingMayHaveAnyValue(...)` | Quick pre-check: returns false if no focus flag could ever match. |
| `CalculateAllBuildingValues()` | Populates `BuildingValueCache` for all focus combinations in one pass. |
| `AI_FlushBuildingValueCache()` | Invalidates `BuildingValueCache` (called on city state changes). |

## Plot & Improvement
| Method | Description |
|---|---|
| `AI_plotValue(pPlot, ...)` | Scores a plot for citizen assignment. |
| `AI_findBestImprovementForPlot(pPlot, ...)` | Selects the optimal improvement for a given tile. |
| `AI_yieldValue(aiYields, ...)` | Converts a yield array into a single score; uses `yieldCache`. |
| `AI_yieldMultiplier(YieldTypes)` | Per-yield multiplier for this city's context. |
| `AI_getPlotMagicValue(pPlot, ...)` | Comprehensive tile-quality score used in founding/expansion evaluation. |
| `AI_updateBestBuild()` | Refreshes the best build action for each unimproved plot. |
| `AI_updateWorkersNeededHere()` | Recomputes how many workers the city needs. |
| `AI_getBestBuild(int)` | Returns the cached best build for the nth plot slot. |

## City Importance & Strategy
| Method | Description |
|---|---|
| `AI_getCityImportance(...)` | Returns a strategic weight for the city used in production prioritisation. |
| `AI_getTargetSize()` | Target population the AI wants the city to reach. |
| `AI_getGoodTileCount()` | Number of high-value tiles in range. |
| `AI_countGoodTiles(...)` | Count of tiles meeting a quality threshold. |
| `AI_countWorkedPoorTiles()` | Count of below-average tiles that are currently being worked. |
| `AI_calculateTargetCulturePerTurn()` | Desired culture-per-turn to win or maintain a cultural victory. |
| `AI_stealPlots()` | Instructs the AI to spend culture to flip contested border tiles. |
| `AI_cachePlayerCloseness(...)` | Caches proximity to rival players for threat evaluation. |

## Defence
| Method | Description |
|---|---|
| `AI_doContractFloatingDefenders()` | Requests defenders through `CvContractBroker` based on threat level. |
| `AI_establishSeeInvisibleCoverage()` | Ensures the city has units that reveal invisible units. |
| `AI_establishInvestigatorCoverage()` | Ensures investigator units are present for crime detection. |

## Property Control
| Method | Description |
|---|---|
| `AI_isNegativePropertyUnit(pUnit)` | Returns true if a unit would worsen a property (e.g. increase crime). |
| `AI_meetsUnitSelectionCriteria(pUnit, criteria)` | Checks if a unit meets the given `CvUnitSelectionCriteria` for property management. |
| `getPropertySourceValue(...)` | Value contribution of a property source building or unit. |
| `getPropertyDecay(...)` | Decay rate for a property in this city. |
| `getPropertyNonBuildingSource(...)` | Non-building property source value. |
| `buildingPropertiesValue(...)` | Total property-related score of a building. |
| `happynessValue(...)` | Happiness component of a building score. |
| `healthValue(...)` | Health component of a building score. |

## Caching Structures
| Structure | Description |
|---|---|
| `yieldCache` | 16-entry cache keyed on `(yields[], commerce[], flags)`. Prevents redundant `AI_yieldValue()` calls within a turn. |
| `BuildingValueCache` | Per-focus-bitmask pre-calculated building scores. Flushed by `AI_FlushBuildingValueCache()`. |

## Related
- [`CvCity`](CvCity.md) — base class  
- [`CvPlayerAI`](CvPlayerAI.md) — drives `AI_doTurn()` and production decisions  
- [`CvContractBroker`](CvContractBroker.md) — defender and worker unit requests  
- [`CvProperties`](CvProperties.md) — property system (crime, pollution, …)  
- [`AI_Defines.h`](../AI_Defines.h) — city role flag constants  
