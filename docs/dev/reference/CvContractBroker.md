# CvContractBroker

**File:** `CvContractBroker.h` / `CvContractBroker.cpp`

## Overview
`CvContractBroker` is a publish/subscribe coordination layer that decouples the
"I need a unit" signal from the "I am available to fulfil that need" signal. This removes
the need for cities and units to search for each other directly, which improves
performance and reduces AI coupling.

Each `CvPlayer` owns one `CvContractBroker` instance.

## Key Concepts

### Work Requests (`workRequest`)
Posted by cities or player AI logic when a capability is needed.

| Field | Type | Description |
|---|---|---|
| `iPriority` | `int` | Scheduling priority (see constants below). |
| `eUnitFlags` | `unitCapabilities` | Required capability flags OR'd together. |
| `eAIType` | `UnitAITypes` | Required unit AI role. |
| `eUnitType` | `UnitTypes` | Specific unit type required, or `NO_UNIT`. |
| `iAtX` / `iAtY` | `int` | Location where capability is needed. |
| `iMaxPath` | `int` | Maximum movement path allowed. |
| `iUnitId` | `int` | If set, a specific unit is requested. |
| `iWorkRequestId` | `int` | Unique identifier for this request. |
| `iRequiredStrengthTimes100` | `int` | Minimum combat strength required (×100). |
| `bFulfilled` | `bool` | Set to `true` once a unit is matched. |
| `criteria` | `CvUnitSelectionCriteria` | Additional filter criteria. |

### Advertising Units (`advertisingUnit`)
Posted by idle units broadcasting their current capabilities and location.

| Field | Type | Description |
|---|---|---|
| `iUnitId` | `int` | Unit identifier. |
| `eUnitType` | `UnitTypes` | Unit type. |
| `iDefensiveValue` | `int` | Defensive combat score. |
| `iOffensiveValue` | `int` | Offensive combat score. |
| `bIsWorker` / `bIsHealer` | `bool` | Capability flags. |
| `iAtX` / `iAtY` | `int` | Current location. |
| `iContractedWorkRequest` | `int` | Request ID this unit is currently contracted to (`-1` if free). |
| `iMatchedToRequestSeqThisPlot` | `int` | Sequence number of last match at this plot. |
| `iMatchedToRequestSeqAnyPlot` | `int` | Sequence number of last match anywhere. |
| `iMinPriority` | `int` | Minimum request priority this unit will accept. |

### City Tenders (`cityTender`)
Cities broadcast willingness to build a unit for the highest-priority outstanding request.

| Field | Type | Description |
|---|---|---|
| `iCityId` | `int` | City identifier. |
| `iMinPriority` | `int` | Minimum priority the city will satisfy. |

### Capability Flags (`unitCapabilities`)
| Flag | Value | Description |
|---|---|---|
| `NO_UNITCAPABILITIES` | `0` | No capability. |
| `DEFENSIVE_UNITCAPABILITIES` | `1 << 0` | Can serve as a defender. |
| `OFFENSIVE_UNITCAPABILITIES` | `1 << 1` | Can serve as an attacker. |
| `WORKER_UNITCAPABILITIES` | `1 << 2` | Can perform worker tasks. |
| `HEALER_UNITCAPABILITIES` | `1 << 3` | Can heal other units. |

## Priority Constants

| Constant | Value | Description |
|---|---|---|
| `CITY_BUILD_PRIORITY_CEILING` | 900 | Highest city-build priority. |
| `MINIMUM_CAPITAL_DEFENDER_PRIORITY` | 800 | Capital must always have a defender. |
| `HIGHEST_PRIORITY_ESCORT_PRIORITY` | 600 | Urgent escort need. |
| `MINIMUM_CITY_DEFENDER_PRIORITY` | 500 | Any city needs a defender. |
| `HIGH_PRIORITY_ESCORT_PRIORITY` | 400 | Important escort request. |
| `CITY_NO_WORKERS_WORKER_PRIORITY` | 300 | City has no workers at all. |
| `FLOATING_DEFENDER_REQUEST_PRIORITY_MAX` | 200 | Maximum priority for floating-defender requests. |
| `LOW_PRIORITY_ESCORT_PRIORITY` | 100 | Low-urgency escort. |

## Compile Switch
`USE_UNIT_TENDERING` (defined in the header) enables the full city-tendering flow
where cities advertise build slots for outstanding requests. Disabling it falls back to
direct unit assignment only.

## Related
- [`CvPlayerAI`](CvPlayerAI.md) — owns and drives the broker  
- [`CvCityAI`](CvCityAI.md) — posts work requests and city tenders  
- [`CvUnitAI`](CvUnitAI.md) — advertises units and reads contracted work requests  
- [`CvUnitSelectionCriteria`](../CvUnitSelectionCriteria.h) — filter criteria attached to requests  
