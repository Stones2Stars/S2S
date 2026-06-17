# CvSelectionGroupAI

**File:** `CvSelectionGroupAI.h` / `CvSelectionGroupAI.cpp`  
**Inherits:** [`CvSelectionGroup`](CvSelectionGroup.md)

## Overview
Adds AI decision logic to a group of one or more units that move and act together.
The class bridges `CvUnitAI` (individual unit decisions) and `CvPlayerAI` (player-level
strategy) by coordinating attacks, garrison assignment, and mission state for the whole
group at once.

## Lifecycle & State
| Method | Description |
|---|---|
| `AI_reset()` | Resets all AI members (mission type, plot, unit reference, force-separate flag, group-attack flag). |
| `read(FDataStreamBase*)` | Loads AI state from a save stream. |
| `write(FDataStreamBase*)` | Saves AI state to a save stream. |

## Separation Utilities
| Method | Description |
|---|---|
| `AI_separate()` | Splits the group into single-unit groups. |
| `AI_separateNonAI(UnitAITypes)` | Splits out all units whose AI type does *not* match. |
| `AI_separateAI(UnitAITypes)` | Splits out all units whose AI type *does* match. |
| `AI_separateImpassable()` | Splits out units that cannot enter the current terrain. |
| `AI_separateEmptyTransports()` | Splits out transports that are carrying no units. |

## Turn Update
| Method | Description |
|---|---|
| `AI_update()` | Drives all units in the group through their `CvUnitAI::AI_update()` cycle; returns `true` if any unit is still active. |

## Combat Evaluation
| Method | Description |
|---|---|
| `AI_attackOdds(pPlot, bPotentialEnemy, bForce, bWin, iThreshold)` | Returns combined attack odds for the whole group against a target plot. |
| `AI_getBestGroupAttacker(pPlot, bPotentialEnemy, iUnitOdds, ...)` | Selects the single best attacking unit from the group for a given plot; optionally fills `pDefender` and checks assassination/surprise flags. |
| `AI_getBestGroupSacrifice(pPlot, bForce, bNoBlitz)` | Selects the most expendable unit for a sacrificial attack. |
| `AI_compareStacks(pPlot, flags, iRange)` | Compares this group's combat power against the defending stack at a plot (returns positive = attackers stronger). |
| `AI_sumStrength(pAttackedPlot, eDomainType, flags)` | Sums total combat strength of all group members, filtered by domain and strength flags. |

## Group Attack Queue
| Method | Description |
|---|---|
| `AI_queueGroupAttack(iX, iY)` | Schedules a combined group attack on the given coordinates next turn. |
| `AI_cancelGroupAttack()` | Cancels a queued group attack. |
| `AI_isGroupAttack()` | Returns `true` if a group attack is currently queued. |

## Mission AI State
| Method | Description |
|---|---|
| `AI_getMissionAIType()` | Returns the current `MissionAITypes` enum value. |
| `AI_setMissionAI(eNewMissionAI, pNewPlot, pNewUnit)` | Sets the mission type, target plot, and target unit simultaneously. |
| `AI_getMissionAIPlot()` | Returns the mission target plot. |
| `AI_getMissionAIUnit()` | Returns the mission target unit. |
| `AI_noteSizeChange(iChange, iVolume)` | Updates internal size and volume counters when units join/leave. |

## Control / Status
| Method | Description |
|---|---|
| `AI_isControlled()` | Returns `true` if the group is under AI control (not human-moved this turn). |
| `AI_isDeclareWar(pPlot)` | Returns `true` if moving the group to the given plot would constitute a war declaration. |
| `AI_isForceSeparate()` | Returns `true` if the force-separate flag is set. |
| `AI_makeForceSeparate()` | Sets the force-separate flag so the group splits on next update. |

## Garrison
| Method | Description |
|---|---|
| `AI_isCityGarrison(pCity)` | Returns `true` if this group is tasked as the garrison of a specific city. |
| `AI_setAsGarrison(pCity)` | Marks this group as the garrison for a city. |

Both delegate to the **head unit's** membership state (`CvUnitAI::m_iGarrisonCity`).
Garrison membership is the *auxiliary* defense tier — members keep their own UNITAI and
count toward city defense strength, not the primary-defender quota. See the garrison-tiers
section in [`CvUnitAI.md`](CvUnitAI.md) (#384).

## Defender Selection
| Method | Description |
|---|---|
| `AI_findBestDefender(pTargetPlot, allowAllDefenders, bConsiderPropertyValues)` | Returns the best defending unit in the group for a target plot, optionally considering property effects. |
| `AI_ejectBestDefender(pTargetPlot, allowAllDefenders)` | Removes and returns the best defender (used to detach a unit for static defence). |
| `AI_hasBeneficialPropertyEffectForCity(pCity, pProperty)` | Returns `true` if any unit in the group provides a positive property effect to the city. |
| `AI_ejectBestPropertyManipulator(pTargetCity)` | Removes and returns the best property-management unit for a city. |
| `AI_getGenericValueTimes100(eFlags)` | Returns a composite value score for the group (×100 for precision). |
| `AI_isFull()` | Returns `true` if all transports in the group are at cargo capacity. |

## Protected Members
| Member | Type | Description |
|---|---|---|
| `m_iMissionAIX` / `m_iMissionAIY` | `int` | Coordinates of the mission target plot. |
| `m_eMissionAIType` | `MissionAITypes` | Current mission type. |
| `m_missionAIUnit` | `IDInfo` | Handle to the mission target unit. |
| `m_bForceSeparate` | `bool` | Flag to force group dissolution on next update. |
| `m_bGroupAttack` | `bool` | Flag indicating a queued group attack. |
| `m_iGroupAttackX` / `m_iGroupAttackY` | `int` | Coordinates of the queued group attack. |

## Related
- [`CvSelectionGroup`](CvSelectionGroup.md) — base class  
- [`CvUnitAI`](CvUnitAI.md) — individual unit decisions within the group  
- [`CvPlayerAI`](CvPlayerAI.md) — player AI that coordinates groups  
- [`CvContractBroker`](CvContractBroker.md) — contract dispatch referenced by `AI_isAwaitingContract()` (protected)  
