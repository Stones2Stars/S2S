# CvPlayerAI

**File:** `CvPlayerAI.h` / `CvPlayerAI.cpp`  
**Inherits:** [`CvPlayer`](CvPlayer.md)

## Overview

The full AI decision stack for a single civilisation. Every non-human player has
its per-turn logic driven by this class. Key responsibilities:

- Turn-based strategic evaluation (research, diplomacy, production planning, military).
- Flavour-weighted preference system for guiding choices.
- Plot-danger caching for performance (`plotDangerCache`, 24 entries).
- Strategy and victory-strategy bitmasks (see [`AI_Defines.h`](../AI_Defines.h)).

Static helpers `getPlayer(PlayerTypes)` and `areStaticsInitialized()` expose the
global player array; `initStatics()` / `freeStatics()` manage its lifetime.

## Turn Entry Points

| Method | Description |
|---|---|
| `AI_doTurnPre()` | First phase: flavour evaluation, research, diplomacy posture, strategy flags. |
| `AI_doTurnPost()` | Second phase: finalise production queues, issue unit-build requests through `CvContractBroker`. |
| `AI_doTurnUnitsPre()` | Pre-pass over all units before the main unit loop. |
| `AI_doTurnUnitsPost()` | Post-pass cleanup after all units have acted. |
| `AI_unitUpdate()` | Drives the per-turn AI update for each selection group owned by this player. |

## Strategy & Research

| Method | Description |
|---|---|
| `AI_getFlavorValue(FlavorTypes)` | Returns the player's weighted preference for a strategic flavour. |
| `AI_bestTech(...)` | Finds the highest-value tech to research, up to `iMaxPathLength` steps away. |
| `AI_techValue(TechTypes, ...)` | Scores a single tech for research priority. |
| `AI_chooseResearch()` | Selects the next tech to queue. |
| `AI_chooseFreeTech()` | Selects a free tech (e.g., from a wonder or civic). |
| `AI_yieldWeight(YieldTypes)` | Returns the player's weight for a yield type. |
| `AI_commerceWeight(CommerceTypes, ...)` | Returns the player's weight for a commerce type. |

## Military & Danger

| Method | Description |
|---|---|
| `AI_getPlotDanger(pPlot, iRange, bTestMoves)` | Returns a cached danger value for a plot; uses `plotDangerCache`. |
| `AI_getAnyPlotDanger(pPlot, iRange, bTestMoves)` | Returns true if any danger exists in range. |
| `AI_getVisiblePlotDanger(...)` | Checks visible enemy units within range, optionally filtering by animal-only or acceptable odds. |
| `AI_militaryWeight(CvArea*)` | Sums the military weight of all living enemies in an area. |
| `AI_targetCityValue(pCity, ...)` | Scores an enemy city as a military target. |
| `AI_findTargetCity(CvArea*)` | Finds the best city to attack in an area. |
| `AI_movementPriority(pGroup)` | Returns a movement-priority score for a selection group. |

## Economy / Finance

| Method | Description |
|---|---|
| `AI_isFinancialTrouble()` | Returns true if the player is running dangerously low on gold. |
| `AI_hasCriticalGold()` | Returns true if gold reserves are at a critical minimum. |
| `AI_avoidScience()` | Returns true if the player should deprioritise science (economic crisis). |
| `AI_safeFunding()` | Returns the safe funding level (short). |
| `AI_fundingHealth(...)` | Returns the overall financial health score. |
| `AI_goldTarget()` | Returns the desired gold reserve target. |

## City & Settlement

| Method | Description |
|---|---|
| `AI_foundValue(iX, iY, ...)` | Scores a plot as a city founding location. |
| `AI_updateFoundValues(...)` | Refreshes founding-value cache for an area or the whole map. |
| `AI_updateAreaTargets()` | Refreshes the list of area-level military targets. |
| `AI_conquerCity(...)` | Handles AI logic when a city is conquered (razing, culture conversion). |
| `AI_assignWorkingPlots()` | Propagates a fresh citizen-assignment request to all owned cities. |
| `AI_makeAssignWorkDirty()` | Marks all city assignments as stale. |
| `AI_makeProductionDirty()` | Forces all cities to re-evaluate their production queues. |
| `AI_doCentralizedProduction()` | Coordinates production across cities for global strategic goals. |

## Diplomacy

| Method | Description |
|---|---|
| `AI_doPeace()` | Evaluates unsolicited peace opportunities with enemies. |
| `AI_isWillingToTalk(PlayerTypes)` | Returns true if the player will open a diplomacy channel. |
| `AI_demandRebukedSneak(PlayerTypes)` | Returns true if a rebuffed demand justifies a sneak attack. |
| `AI_demandRebukedWar(PlayerTypes)` | Returns true if a rebuffed demand justifies a formal war declaration. |
| `AI_hasTradedWithTeam(TeamTypes)` | Returns true if the player has an active trade deal with a team. |
| `AI_getGreeting(PlayerTypes)` | Returns the appropriate `DiploCommentTypes` greeting string. |

## Area / Territory

| Method | Description |
|---|---|
| `AI_isPrimaryArea(CvArea*)` | Returns true if this area contains the player's capital or most cities. |
| `AI_isAreaAlone(CvArea*)` | Returns true if no rivals are present in the area. |
| `AI_isCapitalAreaAlone()` | Returns true if the capital's area has no rivals. |
| `AI_isCommercePlot(CvPlot*)` | Returns true if a plot is commercially valuable for this player. |

## Caching Structures

| Structure | Description |
|---|---|
| `plotDangerCache` | 24-entry fixed-size cache keyed on `(x, y, range, bTestMoves)`. Cleared at the start of each turn via `clear()`. |
| `plotDangerCacheEntry` | Single cache record: coordinates, range, move-test flag, result, last-use counter. |
| `MissionTargetInfo` | Summarises unit mission targeting: count, closest distance, total volume. |

## Related

- [`CvPlayer`](CvPlayer.md) — base class (non-AI state)  
- [`CvCityAI`](CvCityAI.md) — city AI driven by player AI  
- [`CvUnitAI`](CvUnitAI.md) — unit AI driven by player AI  
- [`CvTeamAI`](CvTeamAI.md) — team-level wrapper also drives player AI  
- [`CvContractBroker`](CvContractBroker.md) — unit request/dispatch system  
- [`AI_Defines.h`](../AI_Defines.h) — strategy and victory flag constants  
