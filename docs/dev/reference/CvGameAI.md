# CvGameAI

**File:** `CvGameAI.h` / `CvGameAI.cpp`  
**Inherits:** [`CvGame`](CvGame.md)

## Overview
Global AI layer on top of the game singleton. Holds no complex strategy state of its
own — its role is to provide AI-wide helpers and to propagate dirty-work signals to
all players and units.

Accessed through `GC.getGame()` (which returns the concrete `CvGameAI` subtype at runtime).

## Methods

### Lifecycle
| Method | Description |
|---|---|
| `AI_init()` | Calls `AI_reset()` to clear previous AI state. |
| `AI_uninit()` | Releases any AI-specific resources. |
| `AI_reset()` | Calls `AI_uninit()`, resets `m_iPad` and unit-AI caches. |

### Work assignment
| Method | Description |
|---|---|
| `AI_makeAssignWorkDirty()` | Invalidates citizen-assignment caches for every living player. |
| `AI_updateAssignWork()` | Forces a fresh `AI_updateAssignWork()` call on every living *human* player. |

### Combat evaluation
| Method | Description |
|---|---|
| `AI_combatValue(UnitTypes)` | Returns a normalised combat score (base 100) for a unit type, factoring in air/land strength, first-strikes, and best-unit normalisation. Used when comparing units across the rule set. |

### Serialisation
| Method | Description |
|---|---|
| `read(FDataStreamBase*)` | Calls `CvGame::read` then reads `m_iPad`. |
| `write(FDataStreamBase*)` | Calls `CvGame::write` then writes `m_iPad`. |

## Protected Members
| Member | Type | Description |
|---|---|---|
| `m_iPad` | `int` | Padding / versioning integer persisted in save games. |

## Related
- [`CvGame`](CvGame.md) — base class  
- [`CvPlayerAI`](CvPlayerAI.md) — per-player AI, driven from here  
- [`CvGlobals`](CvGlobals.md) — `GC.getGame()` returns this object  
