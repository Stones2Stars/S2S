# CvArmy

**File:** `CvArmy.h` / `CvArmy.cpp`  
**Compile guard:** `CVARMY_BREAKSAVE` (disabled by default)

## Overview

`CvArmy` groups multiple `CvSelectionGroup` objects under a single strategic command,
allowing coordinated multi-stack operations (e.g. a combined land assault). It tracks a
designated leader group, a list of member groups, a mission type, and a target plot.

> **Note:** `CvArmy` is wrapped in the `CVARMY_BREAKSAVE` preprocessor guard to
> prevent unintentional save-game format changes. It is not active in production builds
> unless that symbol is defined.

## Lifecycle

| Method | Description |
|---|---|
| `CvArmy()` | Constructor; zero-initialises all members. |
| `~CvArmy()` | Destructor; does not free groups (they are owned by the player). |
| `init(iID, eOwner, eMissionType)` | Sets the army identifier, owning player, and initial mission. |
| `uninit()` | Releases internal state. |
| `reset(iID, eOwner, bConstructorCall)` | Full reset; used at construction and on game reset. |

## Identity

| Method | Description |
|---|---|
| `getID()` | Returns the army's unique integer identifier. |
| `setID(int)` | Sets the army's identifier. |
| `getOwner()` | Returns the `PlayerTypes` of the owning civilisation. |

## Leader & Groups

| Method | Description |
|---|---|
| `getLeader()` | Returns the `CvSelectionGroup*` designated as the army's leader. |
| `setLeader(CvSelectionGroup*)` | Sets the leader group by pointer. |
| `setLeader(int iGroupID)` | Sets the leader group by ID. |
| `addGroup(CvSelectionGroup*)` | Enrolls an additional group into the army. |
| `removeGroup(CvSelectionGroup*)` | Removes a group (e.g. after it is destroyed or detached). |
| `getGroup(int iGroupID)` | Returns a member group by its ID. |
| `getGroupByIndex(int iIndex)` | Returns a member group by list position. |
| `findNewLeader()` | Selects a replacement leader from remaining member groups. |

## Mission & Target

| Method | Description |
|---|---|
| `getMission()` | Returns the current mission type (`int` / army-mission enum). |
| `setMission(int)` | Updates the mission type. |
| `getTargetPlot()` | Returns the `CvPlot*` the army is marching toward. |
| `setTargetPlot(CvPlot*)` | Updates the target objective. |

## Turn Logic

| Method | Description |
|---|---|
| `doTurn()` | Per-turn update: checks whether group missions should be refreshed, evaluates target validity, and coordinates movement. |
| `CheckTargetCity()` | Returns `true` if the target plot still contains a valid enemy city. |
| `CheckTargetDefendPlot()` | Returns `true` if the target plot still needs defending. |
| `disband()` | Dissolves the army; member groups are released back to independent operation. |

## Private Members

| Member | Type | Description |
|---|---|---|
| `m_iID` | `int` | Unique army ID. |
| `m_eOwner` | `PlayerTypes` | Owning player. |
| `m_eMission` | `int` | Current mission type (e.g. `ARMY_MISSION_ATTACK_CITY`). |
| `m_pTargetPlot` | `CvPlot*` | Objective plot. |
| `m_iLeaderGroupID` | `int` | ID of the designated leader `CvSelectionGroup`. |
| `m_groupIDs` | `std::vector<int>` | IDs of all member groups. |

## Related

- [`CvSelectionGroupAI`](CvSelectionGroupAI.md) — member groups of the army  
- [`CvPlayerAI`](CvPlayerAI.md) — creates and disbands armies  
- [`CvPlot`](CvPlot.md) — army target coordinate  
- [`CvContractBroker`](CvContractBroker.md) — included transitively via `CvArmy.h`  
