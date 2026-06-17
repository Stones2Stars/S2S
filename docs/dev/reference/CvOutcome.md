# CvOutcome / CvOutcomeList / CvOutcomeMission

**Files:** `CvOutcome.h/.cpp`, `CvOutcomeList.h/.cpp`, `CvOutcomeMission.h/.cpp`

## Overview
The outcome system encapsulates probabilistic results that can occur when a unit
executes a mission. Rather than hard-coding rewards in the mission logic, outcomes are
defined in XML and resolved at runtime, making them fully moddable.

---

## `CvOutcome`
A single possible result from one roll of the outcome system.

### Result Types
An outcome can produce any of the following (configured in XML):

| Result | Description |
|---|---|
| Yield grant | `getYield(eYield, kUnit)` — a yield quantity delivered to the owning city or player. |
| Commerce grant | `getCommerce(eCommerce, kUnit)` — gold, research, culture, or espionage points. |
| Unit spawn | `getUnitType()` + `getUnitToCity(kUnit)` — spawns a new unit, optionally in the nearest city. |
| Promotion | `getPromotionType()` — grants a specific promotion to the executing unit. |
| Great Person points | `getGPP()` + `getGPUnitType()` — adds GP points toward a specific GP type. |
| Bonus resource | `getBonusType()` — reveals or grants a bonus resource. |
| Property change | `getProperties()` — modifies a `CvProperties` set on the plot or city. |
| Happiness timer | `getHappinessTimer()` — grants temporary happiness. |
| Population boost | `getPopulationBoost()` — directly increases city population. |
| Anarchy reduction | `getReduceAnarchyLength(kUnit)` — shortens the current anarchy period. |
| Event trigger | `getEventTrigger()` — fires an `EventTriggerTypes` event. |

### Probability
| Method | Description |
|---|---|
| `getChance(kUnit)` | Base probability (0–100) of this outcome occurring, taking the unit into account. |
| `getChancePerPop()` | Additional probability per population point in the target city. |

### Kill Flag
| Method | Description |
|---|---|
| `isKill()` | Returns `true` if the executing unit is destroyed when this outcome fires. |

### Possibility Checks
| Method | Description |
|---|---|
| `isPossible(kUnit)` | Returns `true` if the outcome can fire for the unit considering its stats. |
| `isPossibleSomewhere(kUnit)` | Returns `true` if any plot exists where the outcome could fire. |
| `isPossibleInPlot(kUnit, kPlot, bForTrade)` | Returns `true` if the outcome can fire on a specific plot. |
| `isPossible(kPlayer)` | Returns `true` if the outcome can fire for a player (AI evaluation path). |

### Execution
| Method | Description |
|---|---|
| `execute(kUnit, eDefeatedPlayer, eDefeatedUnitType)` | Rolls the outcome and applies all effects if the chance check passes. Returns `true` if it fired. |

### AI Evaluation
| Method | Description |
|---|---|
| `AI_getValueInPlot(kUnit, kPlot, bForTrade)` | Returns a numeric value estimate for this outcome on a given plot; used by `CvUnitAI` to score mission targets. |

### Display
| Method | Description |
|---|---|
| `buildDisplayString(szBuffer, kUnit)` | Populates a wide string buffer with a human-readable description of the outcome. |

### Python Support
| Method | Description |
|---|---|
| `compilePython()` | Pre-compiles any Python expressions embedded in the outcome's XML definition. |

---

## `CvOutcomeList`
An ordered collection of `CvOutcome` objects. The list is rolled sequentially — each
outcome makes its own independent chance check. Multiple outcomes can fire from a
single mission roll.

---

## `CvOutcomeMission`
Binds a `CvOutcomeList` to a specific `MissionTypes` value. Stored on
`CvUnitInfo` objects so that every unit type can declare which outcomes apply to which
of its missions.

---

## Related
- [`CvUnitAI`](CvUnitAI.md) — calls `AI_getValueInPlot` when scoring mission targets  
- [`CvProperties`](CvProperties.md) — property-change outcomes write here  
- [`CvUnit`](CvUnit.md) — executes outcomes via `CvOutcomeMission`  
