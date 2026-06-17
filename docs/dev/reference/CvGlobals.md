# CvGlobals

**File:** `CvGlobals.h` / `CvGlobals.cpp`  
**Singleton** — accessed as the global `GC` object

## Overview
`CvGlobals` (macro-aliased to `GC`) is the single entry point for virtually every
piece of global state in the DLL. It owns:

- All loaded `CvInfo*` arrays (terrain, feature, bonus, building, unit, tech, civic,
  promotion, great person, policy, …).
- Pointers to the engine interface singletons (`CvDLLEngineIFaceBase`, etc.).
- The active `CvMap` (and all parallel-map slots).
- The `CvGameAI` game instance.
- The `CvPropertySolver` singleton.
- Profiler, random number generator, and logging infrastructure.
- Build-option booleans and tuning constants read from the INI file.

Almost every `.cpp` file in the DLL includes `CvGlobals.h` and uses `GC.*`.

## Major Accessor Categories

### Game & Map
| Accessor | Returns | Description |
|---|---|---|
| `getGame()` | `CvGameAI&` | The global game-state and AI object. |
| `getMap()` | `CvMap&` | The currently active map. |
| `getMap(MapTypes)` | `CvMap&` | A specific parallel-map slot. |

### Player & Team
| Accessor | Returns | Description |
|---|---|---|
| `getPlayer(PlayerTypes)` | `CvPlayerAI&` | Forwarded to `CvPlayerAI::getPlayer()`. |
| `getTeam(TeamTypes)` | `CvTeamAI&` | Forwarded to `CvTeamAI::getTeam()`. |

### Info Arrays
All `CvInfo*` objects are bulk-loaded from XML at game start and stored in typed
arrays. Example accessors:

| Accessor | Info type | XML source |
|---|---|---|
| `getTerrainInfo(TerrainTypes)` | `CvTerrainInfo` | `Terrain_infos.xml` |
| `getFeatureInfo(FeatureTypes)` | `CvFeatureInfo` | `Feature_InfoTypes.xml` |
| `getBonusInfo(BonusTypes)` | `CvBonusInfo` | `Bonus_infos.xml` |
| `getBuildingInfo(BuildingTypes)` | `CvBuildingInfo` | `Building_infos.xml` |
| `getUnitInfo(UnitTypes)` | `CvUnitInfo` | `Unit_infos.xml` |
| `getTechInfo(TechTypes)` | `CvTechInfo` | `Tech_infos.xml` |
| `getCivicInfo(CivicTypes)` | `CvCivicInfo` | `Civic_infos.xml` |
| `getPromotionInfo(PromotionTypes)` | `CvPromotionInfo` | `Promotion_infos.xml` |
| `getPropertyInfo(PropertyTypes)` | `CvPropertyInfo` | Property XML |
| `getUnitCombatInfo(UnitCombatTypes)` | `CvUnitCombatInfo` | Combat-class XML |

_(Many more arrays exist; see `CvGlobals.h` for the complete list.)_

### Engine Interfaces
The DLL calls back into the engine through a set of pure-virtual interface objects.
These are stored on `CvGlobals` and allocated by the engine at DLL load time:

| Member | Interface | Purpose |
|---|---|---|
| `m_pEngineIFace` | `CvDLLEngineIFaceBase` | Rendering, entity management. |
| `m_pEventReporter` | `CvDLLEventReporterIFaceBase` | Game event callbacks. |
| `m_pFAStarIFace` | `CvDLLFAStarIFaceBase` | Engine-side pathfinder (legacy). |
| `m_pPythonIFace` | `CvDLLPythonIFaceBase` | Python script execution. |
| `m_pXMLIFace` | `CvDLLXMLIFaceBase` | XML loading pipeline. |
| `m_pInterfaceIFace` | `CvDLLInterfaceIFaceBase` | UI / HUD callbacks. |

### Supporting Services
| Accessor | Returns | Description |
|---|---|---|
| `getPropertySolver()` | `CvPropertySolver&` | The property simulation solver. |
| `getDefineINT(key)` | `int` | Integer tuning constant from `GlobalDefines.xml`. |
| `getDefineFLOAT(key)` | `float` | Float tuning constant. |
| `getDefineSTRING(key)` | `const char*` | String constant. |

## Compile-Time Feature Flags
Several important features are toggled by `#define` symbols that appear in the headers
included by `CvGameCoreDLL.h`:

| Symbol | Effect when defined |
|---|---|
| `ENABLE_FOGWAR_DECAY` | Enables gradual fog-of-war decay over time. |
| `PLOT_DANGER_CACHING` | Activates `plotDangerCache` on `CvPlayerAI`. |
| `YIELD_VALUE_CACHING` | Activates `yieldCache` on `CvCityAI`. |
| `CVARMY_BREAKSAVE` | Compiles in the `CvArmy` class (breaks old saves). |

## Related
- [`CvGameAI`](CvGameAI.md) — returned by `GC.getGame()`  
- [`CvMap`](CvMap.md) — returned by `GC.getMap()`  
- [`CvPropertySolver`](CvPropertySolver.md) — returned by `GC.getPropertySolver()`  
- [`CvPlayerAI`](CvPlayerAI.md) / [`CvTeamAI`](CvTeamAI.md) — returned by `GC.getPlayer()` / `GC.getTeam()`  
