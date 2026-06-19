# Caveman2Cosmos DLL Documentation

## Overview

The **C2C GameCoreDLL** is the core logic library for the *Caveman2Cosmos* mod of
Civilization IV: Beyond the Sword. It is compiled as a Win32 DLL (`CvGameCoreDLL.dll`)
and loaded by the game engine at startup.

The DLL is written in **C++ (pre-C++11 / VS2003-compatible)** and relies on:

- **Boost 1.55** (smart pointers, bind, foreach, range adaptors, optional, format, …).
- **Python C-API** for the `Cy*` binding layer that exposes game objects to Python scripts.
- **Doxygen + Graphviz** for auto-generated API documentation.

The project solution is `S2S.sln` (builds with the VS2019 toolset while keeping
language-standard compatibility with the original BtS build chain).

---

## Architecture

The codebase is organised into the following broad layers, each described in detail below.

| Layer | Prefix | Purpose |
|---|---|---|
| Core game objects | `Cv*` | Game state: game, players, cities, units, map, teams |
| AI extensions | `CvPlayerAI`, `CvCityAI`, `CvUnitAI`, `CvTeamAI`, `CvGameAI`, `CvSelectionGroupAI` | Per-object AI decision making |
| Supporting systems | `CvProperties`, `CvPathGenerator`, `CvContractBroker`, `CvOutcome`, `CvArmy`, … | Cross-cutting gameplay modules |
| Python bindings | `Cy*` | Exposes C++ objects to Python mod scripts |
| Utility / infrastructure | `FAssert`, `FProfiler`, `logging`, `BetterBTSAI`, … | Debugging, profiling, logging |

---

## Core Game Objects

### `CvGame` / `CvGameAI`

- `CvGame` is the global game-state singleton, accessed via `GC.getGame()`.
  - Manages turn flow, score, diplomacy initialisation, map regeneration, trade routes,
    blockades, replay messages, and end-game conditions.
- `CvGameAI` extends `CvGame` with AI-wide helpers:
  - `AI_combatValue(UnitTypes)` — normalised combat score used when comparing units
    across the entire rule set.
  - `AI_makeAssignWorkDirty()` / `AI_updateAssignWork()` — invalidate and refresh
    citizen-assignment caches for all players.

### `CvPlayer` / `CvPlayerAI`

- `CvPlayer` represents a single civilisation (human or AI). It owns the city list,
  unit list, selection groups, build lists, contract broker, and player-level properties.
- `CvPlayerAI` extends `CvPlayer` with the full AI decision stack:
  - **Turn phases** — `AI_doTurnPre()` and `AI_doTurnPost()` are the two entry points
    called each turn for research, diplomacy, production planning, and military strategy.
  - **Flavour system** — `AI_getFlavorValue(FlavorTypes)` returns the player's weighted
    preference for a particular strategic flavour (military, science, culture, …).
  - **Caching** — `plotDangerCache` (24-entry fixed-size cache) avoids recomputing
    danger values for the same plot, range, and move-test flag combination.
  - **Strategy flags** — an integer bitmask (see [AI Strategy Flags](#ai-strategy-flags))
    controls the player's current military and domestic posture.
  - **Victory strategy flags** — a second bitmask drives long-term victory pursuit
    (space, conquest, culture, domination, diplomacy — each in four escalating stages).

### `CvCity` / `CvCityAI`

- `CvCity` stores all city state (population, buildings, worked plots, specialists, …).
- `CvCityAI` provides automated city management:
  - **Citizen assignment** — `AI_assignWorkingPlots()` optimises which plots citizens
    work and which specialist slots are filled.
  - **Production selection** — `AI_chooseProduction()` scores units, buildings,
    projects, and processes against the city's current focus and strategic role.
  - **Building evaluation** — `AI_buildingValue()` / `AI_buildingValueThreshold()`
    evaluate each building against a bitmask of `BUILDINGFOCUS_*` flags (food,
    production, gold, research, culture, defence, happiness, health, experience,
    maintenance, specialists, espionage, property control, and more).
  - **Yield value caching** — `yieldCache` (16-entry) prevents redundant
    `AI_yieldValue()` calculations for identical yield/commerce/flag combinations.
  - **Building value caching** — `BuildingValueCache` pre-calculates building scores
    for every focus combination and invalidates them on state change.
  - **Property control** — `AI_establishSeeInvisibleCoverage()`,
    `AI_establishInvestigatorCoverage()`, and related methods handle negative-property
    units and buildings (crime, pollution, tourism, etc.).
  - **City role** — each city is tagged with `AI_CITY_ROLE_*` flags that steer which
    buildings and units are preferred (e.g. `BIG_CULTURE`, `BIG_PRODUCTION`,
    `STAGING`, `LICHPIN`).

### `CvUnit` / `CvUnitAI`

- `CvUnit` stores all unit state (type, owner, plot, promotions, moves remaining, …).
- `CvUnitAI` wraps `CvUnit` with the full unit behaviour loop:
  - `AI_update()` — main per-turn entry point; handles automation, mission execution,
    contract dispatch, healing, and state finalisation.
  - `AI_follow()` — pursues a target: attacks, pillages, or moves toward an objective.
  - `AI_load()` — finds and boards the best available transport.
  - `AI_promote()` — evaluates the available promotion tree and picks the best option.
  - `AI_upgrade()` — walks the upgrade chain and performs upgrades when conditions allow.
  - **Contract state machine** — units can be in one of five `ContractualState` values
    (`NONE`, `AWAITING_ANSWER`, `AWAITING_WORK`, `FOUND_WORK`, `NO_WORK_FOUND`),
    coordinated through `CvContractBroker`.

### `CvTeam` / `CvTeamAI`

- `CvTeam` aggregates multiple players sharing vision, research, and diplomacy.
- `CvTeamAI` drives team-level strategy:
  - `AI_doTurnPre()` — early-turn setup.
  - `AI_doTurnPost()` — updates worst enemy, refreshes area strategies, then calls
    `AI_doWar()` for war-related decisions (skipped for human, NPC, or minor-civ teams).
  - `AI_calculateAreaAIType()` — classifies a land area as offensive, defensive,
    neutral, etc., weighing recent attacks, military strength, and strategic options.
  - `AI_calculateAdjacentLandPlots()` / `AI_calculateProximity()` — geographic
    measurements used for war planning.
  - `AI_getOurPlotStrength()` — sums friendly unit strength within a range of a plot,
    optionally including defensive bonuses and vassal forces.

### `CvSelectionGroup` / `CvSelectionGroupAI`

- `CvSelectionGroup` groups one or more units that move and act together.
- `CvSelectionGroupAI` adds cooperative decision logic:
  - Separation utilities (`AI_separate*`) split groups by AI type, impassable terrain,
    or empty transports.
  - `AI_update()` — drives all units in the group through their AI update cycle.
  - `AI_attackOdds()` / `AI_getBestGroupAttacker()` — evaluate and select the best
    attacker from the group against a target plot.
  - `AI_compareStacks()` / `AI_sumStrength()` — compare relative stack power.
  - `AI_findBestDefender()` / `AI_ejectBestDefender()` — choose the best defender for
    a target plot, optionally considering property values.
  - Mission AI state (`MissionAITypes`) and garrison assignment are stored per group.

---

## Supporting Systems

### Property System (`CvProperties`, `CvPropertySolver`)

The property system is a generic extensible attribute mechanism that can be attached to
any game object (game, team, player, city, unit, plot). Properties represent named
integer values (e.g. crime, pollution, tourism) with both a current value and a per-turn
change rate.

- `CvProperties` — stores a list of `(PropertyType, value, change)` tuples; provides
  add/subtract, predict, propagate, and checksum operations.
- `CvPropertySolver` — singleton that iterates all game objects each turn and applies
  `CvPropertySource`, `CvPropertyInteraction`, and `CvPropertyPropagator` rules to
  compute the next state.
- `CvPropertyManipulators` (on info objects) — declare which sources/interactions/
  propagators a building, improvement, or unit applies.

### Contract Broker (`CvContractBroker`)

Decouples the "need a unit" signal from the "I can provide a unit" signal:

- **Work requests** (`workRequest`) — posted by cities or the player AI with a priority,
  required capabilities (`DEFENSIVE`, `OFFENSIVE`, `WORKER`, `HEALER`), AI type, unit
  type, location, and path limit.
- **Advertising units** (`advertisingUnit`) — posted by idle units with their
  offensive/defensive/worker/healer values and current location.
- **City tenders** (`cityTender`) — cities broadcast willingness to build units for the
  highest-priority unfulfilled request.
- Priority constants range from `CITY_BUILD_PRIORITY_CEILING` (900) down to
  `LOW_PRIORITY_ESCORT_PRIORITY` (100).

### Pathfinding (`CvPathGenerator`, `CvPath`)

Custom A\*-based pathfinder with pluggable cost and validity callbacks:

- `CvPath` — an immutable linked list of `CvPathNode` objects iterable via
  `const_iterator`; exposes `length()`, `lastPlot()`, `containsEdge()`,
  `containsNode()`, `trimBefore()`, `movementRemaining()`, and `cost()`.
- `CvPathGenerator` — manages node pool allocation, drives the search, and exposes
  both AI-always and standard pathing modes.
- Callback typedefs: `HeuristicCost`, `EdgeCost`, `EdgeValidity`, `TerminusValidity`,
  `TurnEndValidityCheckRequired` — allow per-unit-type customisation without
  subclassing.

### Outcome System (`CvOutcome`, `CvOutcomeList`, `CvOutcomeMission`)

Encapsulates probabilistic results of unit missions:

- `CvOutcome` — a single possible result: yield/commerce grants, unit spawning,
  promotions, GP points, bonus resources, property changes, happiness timers,
  population boosts, anarchy reduction, or event triggers.  Has `getChance()`,
  `isPossible()`, `isPossibleInPlot()`, `execute()`, and `AI_getValueInPlot()`.
- `CvOutcomeList` — an ordered list of `CvOutcome` objects; rolls them in sequence.
- `CvOutcomeMission` — binds an outcome list to a specific unit mission type.

### Army System (`CvArmy`)

Groups multiple `CvSelectionGroup` objects under a unified command:

- Tracks a leader group, a list of member group IDs, a target plot, and a mission type.
- `doTurn()` drives coordinated movement; `CheckTargetCity()` and
  `CheckTargetDefendPlot()` evaluate objective validity each turn.
- Guarded by the `CVARMY_BREAKSAVE` compile switch (disabled by default to maintain
  save-game compatibility).

### Map and Plot (`CvMap`, `CvPlot`)

- `CvMap` — owns the flat array of all `CvPlot` objects; manages areas, plot groups,
  fog-of-war, viewports, and multi-map support. `coordRange()` handles wrapping.
- `CvPlot` — stores per-tile state (terrain, feature, bonus, improvement, route, owner,
  city pointer, unit stack, route, river, properties, path info). Exposes flag sets
  `EDefenderScore::flags` and `StrengthFlags::flags` for fine-grained defender and
  strength queries.

### Globals Singleton (`CvGlobals` / `GC`)

`CvGlobals` is the single giant singleton (accessed as `GC`) that holds every loaded
`CvInfo*` array (terrain, feature, bonus, building, unit, tech, civic, …), pointers to
the engine interfaces (`CvDLLEngineIFaceBase`, etc.), the map, the game object, and all
script/logging infrastructure. Nearly every source file includes `CvGlobals.h`.

---

## Python Bindings (`Cy*`)

All `Cv*` objects are mirrored by `Cy*` wrapper classes that register their methods with
the Python C-API. The binding files are grouped by domain:

| File(s) | Exposes |
|---|---|
| `CyGame` / `CyGameInterface.cpp` | `CvGame` methods |
| `CyPlayer` / `CyPlayerLoader*` | `CvPlayer` + AI helpers |
| `CyCity` / `CyCityOutputHistoryInterface.cpp` | `CvCity` + output history |
| `CyUnit` / Python unit loader | `CvUnit` |
| `CyMap` / `CyMapGenerator` | `CvMap`, map generation |
| `CyPlot` / `CyPropertiesInterface.cpp` | `CvPlot` + property system |
| `CyTeam` | `CvTeam` |
| `CySelectionGroup` | `CvSelectionGroup` |
| `CyInfoInterface1‒4.cpp` | All `CvInfo*` subclasses |
| `CyGameCoreUtils` | Free utility functions |
| `CyEnumsInterface.cpp` | Enum constants |
| `CyBoolExprInterface.cpp` / `CyIntExprInterface.cpp` | Expression trees |

---

## AI Strategy Flags

Defined in `AI_Defines.h`; stored as bitmasks on `CvPlayerAI`.

### Military / Domestic Strategy (`AI_STRATEGY_*`)

| Flag | Meaning |
|---|---|
| `AI_DEFAULT_STRATEGY` | Baseline balanced posture |
| `AI_STRATEGY_DAGGER` | Aggressive early rush |
| `AI_STRATEGY_CRUSH` | Convert production to city-attack units |
| `AI_STRATEGY_ALERT1` | Neighbouring threat detected |
| `AI_STRATEGY_ALERT2` | Imminent attack expected |
| `AI_STRATEGY_TURTLE` | Full defensive posture |
| `AI_STRATEGY_LAST_STAND` | Existential threat — maximise defence |
| `AI_STRATEGY_FINAL_WAR` | All-out offensive push |
| `AI_STRATEGY_GET_BETTER_UNITS` | Prioritise unit upgrades |
| `AI_STRATEGY_FASTMOVERS` | Favour high-mobility units |
| `AI_STRATEGY_LAND_BLITZ` | Mass armoured assault |
| `AI_STRATEGY_AIR_BLITZ` | Air dominance campaign |
| `AI_STRATEGY_PRODUCTION` | Domestic production focus |
| `AI_STRATEGY_MISSIONARY` | Religion spread priority |
| `AI_STRATEGY_BIG_ESPIONAGE` | Heavy espionage investment |
| `AI_STRATEGY_ECONOMY_FOCUS` | Catch-up tech at military expense |

### Victory Strategies (`AI_VICTORY_*`)

Each victory type has four escalating stages (e.g. `SPACE1`…`SPACE4`):

| Victory type | Description |
|---|---|
| `SPACE` | Space race — build ship components |
| `CONQUEST` | Military conquest of rivals |
| `CULTURE` | Culture victory via wonders, buildings, and culture slider |
| `DOMINATION` | Territory and population dominance |
| `DIPLOMACY` | UN / diplomatic victory |

### City Role Flags (`AI_CITY_ROLE_*`)

Set per city to bias production choices:
`VALID`, `BIG_CULTURE`, `BIG_PRODUCTION`, `BIG_MILITARY`, `SCIENCE`, `GOLD`,
`PRODUCTION`, `SPECIALIST`, `FISHING`, `STAGING`, `LICHPIN`.

---

## AI Turn Flow (Detailed)

```
Each game turn, per AI team/player:

CvTeamAI::AI_doTurnPre()
  └─ early-turn team setup

  For each city (CvCityAI):
    AI_doTurn()
      ├─ AI_assignWorkingPlots()   — optimise citizen placement
      └─ AI_chooseProduction()     — select next build item

  CvPlayerAI::AI_doTurnPre()
    ├─ evaluate flavour / strategy flags
    ├─ plan research path
    └─ assess diplomatic posture

  For each unit (CvUnitAI):
    AI_update()
      ├─ check contract broker for work assignments
      ├─ execute current mission (attack / move / build / heal / …)
      └─ request new mission if idle

  CvPlayerAI::AI_doTurnPost()
    ├─ finalise production queues
    └─ issue unit-build requests through CvContractBroker

CvTeamAI::AI_doTurnPost()
  ├─ AI_updateWorstEnemy()
  ├─ AI_updateAreaStragies()
  └─ AI_doWar()   [skipped for human / NPC / minor-civ teams]
```

---

## Logging and Debugging

`BetterBTSAI.h` defines a conditional logging system controllable by global level
variables:

| Variable | Controls |
|---|---|
| `gPlayerLogLevel` | `LOG_BBAI_PLAYER` / `LOG_EVALAI_PLAYER` |
| `gTeamLogLevel` | `LOG_BBAI_TEAM` / `LOG_EVALAI_TEAM` |
| `gCityLogLevel` | `LOG_BBAI_CITY` / `LOG_EVALAI_CITY` |
| `gUnitLogLevel` | `LOG_BBAI_UNIT` / `LOG_EVALAI_UNIT` |

Free functions: `logBBAI()` (printf-style), `logAIJson()` (structured JSON trace),
`logCB()` (contract broker events), `logAiEvaluations()` (levelled detail log),
`logToFile()` (arbitrary file sink).

When `ENABLE_AI_LOGS` is not defined all macros compile away to no-ops with zero
runtime cost.

The `FProfiler` / `PROFILE_EXTRA_FUNC()` macros integrate with the engine's built-in
profiler for performance measurement of hot paths.

---

## Build

| File | Purpose |
|---|---|
| `S2S.sln` | Visual Studio solution |
| `S2S.vcxproj` | Main project — compiles all `Cv*`, `Cy*`, and utility files |
| `fbuild.bff` | FASTBuild script for faster incremental builds |
| `_precompile.cpp` | Pre-compiled header translation unit |
| `CvGameCoreDLL.h` | Master PCH — Boost, STL, engine interface includes |
| `CvGameCoreDLL.def` | DLL export definitions |

---

## Navigation

Individual class reference pages (in [docs/dev/reference/](docs/reference/)):

### AI Classes

| Class | Description |
|---|---|
| [CvGameAI](docs/reference/CvGameAI.md) | Global AI helpers; normalised combat value; dirty-work propagation |
| [CvPlayerAI](docs/reference/CvPlayerAI.md) | Full per-player AI: research, diplomacy, military, danger caching |
| [CvCityAI](docs/reference/CvCityAI.md) | City management: citizen assignment, production, building scoring |
| [CvUnitAI](docs/reference/CvUnitAI.md) | Unit behaviour: missions, combat, contracts, promotion, upgrade |
| [CvTeamAI](docs/reference/CvTeamAI.md) | Team strategy: war planning, diplomacy, area classification |
| [CvSelectionGroupAI](docs/reference/CvSelectionGroupAI.md) | Group coordination: attacks, garrison, mission state |

### Supporting Systems

| Class | Description |
|---|---|
| [CvProperties](docs/reference/CvProperties.md) | Generic extensible property container (crime, pollution, …) |
| [CvPropertySolver](docs/reference/CvPropertySolver.md) | Per-turn solver for the property simulation system |
| [CvContractBroker](docs/reference/CvContractBroker.md) | Publish/subscribe unit-need dispatch between cities and units |
| [CvPathGenerator / CvPath](docs/reference/CvPathGenerator.md) | Pluggable A\* pathfinder with per-unit cost and validity callbacks |
| [CvOutcome / CvOutcomeList / CvOutcomeMission](docs/reference/CvOutcome.md) | Probabilistic mission results defined in XML |
| [CvArmy](docs/reference/CvArmy.md) | Multi-stack coordinated assault groups |
| [CvMap](docs/reference/CvMap.md) | Map grid, areas, plot groups, viewport, multi-map support |
| [CvPlot](docs/reference/CvPlot.md) | Individual tile: terrain, units, properties, path cache |
| [CvGlobals](docs/reference/CvGlobals.md) | Master singleton (`GC`): all info arrays, engine interfaces, services |

In Doxygen the full **Classes** tab lists every `Cv*` and `Cy*` class with inheritance
diagrams and collaboration graphs. Search for the `AI_` prefix to find all AI-specific
methods quickly.
