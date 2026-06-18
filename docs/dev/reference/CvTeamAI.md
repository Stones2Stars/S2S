# CvTeamAI

**File:** `CvTeamAI.h` / `CvTeamAI.cpp`  
**Inherits:** [`CvTeam`](CvTeam.md)

## Overview

Manages team-level AI strategy — diplomacy, war planning, area strategies, and
inter-team attitude. `CvTeamAI` aggregates multiple `CvPlayerAI` instances that share
vision, research, and diplomatic standing.

The static `getTeam(TeamTypes)` accessor returns the global team array entry, similar
to `CvPlayerAI::getPlayer()`.

## Lifecycle

| Method | Description |
|---|---|
| `AI_init()` | Initialises team AI state. |
| `AI_uninit()` | Releases team AI resources. |
| `AI_reset(bool bConstructor)` | Full state reset; called at construction and on game reset. |

## Turn Entry Points

| Method | Description |
|---|---|
| `AI_doTurnPre()` | Early-turn team setup before player turns run. |
| `AI_doTurnPost()` | Post-turn: calls `AI_updateWorstEnemy()`, `AI_updateAreaStragies(false)`, then `AI_doWar()`. Skipped for human, NPC, and minor-civ teams. |
| `AI_makeAssignWorkDirty()` | Propagates a dirty-work signal to all team members. |

## War & Military

| Method | Description |
|---|---|
| `AI_doWar()` | Executes war-related decisions: attacks, declarations, peace evaluations. |
| `AI_getOurPlotStrength(pPlot, iRange, bDefensiveBonuses, bTestMoves, bIncludeVassals)` | Sums all friendly unit strength within a radius of a plot. |
| `AI_countMilitaryWeight(pArea)` | Returns the combined military weight of all living enemies in an area. |
| `AI_calculateAreaAIType(pArea, bPreparingTotal)` | Classifies an area as offensive, defensive, neutral, etc., based on attacks, targets, and relative military weight. |
| `AI_isLandTarget(eTeam, bNeighborsOnly)` | Returns `true` if the specified team is a viable land attack target. |
| `AI_isAllyLandTarget(eTeam)` | Returns `true` if the team or any ally is a viable land target. |
| `AI_calculatePlotWarValue(eTeam)` | Scores the value of plots owned by another team as war targets. |
| `AI_calculateBonusWarValue(eTeam)` | Scores bonus resources in another team's territory as a war motivation. |
| `AI_shareWar(eTeam)` | Returns `true` if both teams are at war with the same third party. |

## Geography & Proximity

| Method | Description |
|---|---|
| `AI_isPrimaryArea(pArea)` | Returns `true` if any team member considers this their primary (home) area. |
| `AI_isAnyCapitalAreaAlone()` | Returns `true` if any member's capital area has no rival presence. |
| `AI_hasCitiesInPrimaryArea(eTeam)` | Returns `true` if the specified team has cities in this team's primary area. |
| `AI_calculateAdjacentLandPlots(eTeam)` | Counts shared land-border plots with another team; used in proximity calculations. |
| `AI_calculateCapitalProximity(eTeam)` | Measures the distance between this team's capital and another team's capital. |

## Diplomacy & Attitude

| Method | Description |
|---|---|
| `AI_getAttitude(eTeam, bForced)` | Returns the enum `AttitudeTypes` toward another team. |
| `AI_getAttitudeVal(eTeam, bForced)` | Returns the raw numeric attitude value. |
| `AI_getMemoryCount(eTeam, eMemory)` | Returns how many times a specific diplomatic event has been recorded against a team. |
| `AI_chooseElection(kVoteSelectionData)` | Evaluates UN/election vote options and returns the best choice index. |
| `AI_updateWorstEnemy()` | Refreshes the cached worst-enemy team based on current game state. |
| `AI_updateAreaStragies(bTargets)` | Refreshes area-level strategy objects; optionally updates target lists. |
| `AI_updateAreaTargets()` | Rebuilds the list of area-level attack targets. |

## Related

- [`CvTeam`](CvTeam.md) — base class  
- [`CvPlayerAI`](CvPlayerAI.md) — member players driven by this team  
- [`AI_Defines.h`](../AI_Defines.h) — `AreaAITypes` and strategy constants  
