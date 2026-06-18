# Multi-map → Zone rework

Status: **PAUSED after Phase A-soft** (shipped — PR #327, game-load verified). Field study; parked while other priorities take precedence. **Resume at Phase A-hard** (the single-map collapse + save-salvage shim + win measurement), or jump to Phase B (zone paging). Owner-driven design; this doc is the execution reference.

## Goal & scope

Replace the inherited C2C **multi-map** (17 separate `CvMap` instances reached by `switchMap`) with **one map divided into mapscript-defined zones**, paged at **zone granularity**. Gameplay scope is "The Expanse" — **within the solar system only**.

- **Keep:** Earth, Subterrain, Cislunar, Moon, Mars, Venus, Inner Solar System, Outer Solar System, Titan, Transneptunian.
- **Purge (later, gameplay pass):** Nearby Stars, Orion Arm, Milky Way, Local Group, Virgo Supercluster, Universe, Distant Cosmos.

## Decisions (settled)

1. **The graphics-paging "leak" is design, not an EXE leak.** Eviction (`hideNonRequiredGraphics`) only ran under `NeedToFreeMemory()` (~1.9 GB); `setRequireGraphicsVisible(false)` only *marks*. Fixed-direction: proactive eviction (shipped via `PAGING_RESIDENT_SOFT_CAP`). Measured footprint is game-DATA-dominated (~1.65 GB commit / ~3.3 GB virtual, ~800 MB headroom on the LAA ~4 GB EXE); plot graphics are a ~100 MB slice.
2. **The scroll-lag is paging *granularity*, not paging.** Per-tile distance paging churns continuously. Fix = **zone paging**: load a whole zone as a unit (smooth within), background-preload adjacent zones, only churn at zone crossings.
3. **One map with zones, not separate CvMaps.** Background-preloading the next zone requires it to be in the same map. Drops `switchMap` entirely.
4. **Keep `MapCategory`, repurpose as zones.** `MapCategoryTypes` (16 values in `CIV4MapCategoryInfos.xml`) is independent of `MapTypes`/`switchMap` and is exactly a "which region can X exist in" system.
5. **Keep `CvViewport` + `CvPlotPaging`** (paging is reworked to zones, not removed).
6. **Save salvage is in-DLL, not c4lib.** S2S saves are a self-describing **tagged** stream with a **class-remapping table** (`CvTaggedSaveFormatWrapper`), built to survive enum add/remove. `c4lib` parses *stock* BTS positionally (`BTS.schema`) and cannot read this format. The DLL migrates its own save.

## Phase A — Remove old multimap

### A-soft (now; save-preserving, no `NUM_MAPS` change)

Keeps the owner's load-only test fixture working; declutters reachable surface.

- **Functional lock:** guard `cvInternalGlobals::switchMap` (`CvGlobals.cpp:2801`) to no-op for any map not already `plotsInitialized()` — no new map ever loads into memory; already-loaded maps stay switchable (no fixture trap).
- **UI removal (save-safe, Python/XML):** delete `ParallelMapsInput.py`, `ParallelMapsScreen.py`; remove `init.xml:106` event; `CvMainInterface.py` `ParallelMapsBtn` + `showParallelMapsScreenButton` + `getNumMapsInitialized` checks + click handler; `CvScreensInterface.py` hooks; `CvScreenEnums.PARALLEL_MAPS_SCREEN`.
- **Defer** XML mission/outcome removal: `MissionTypes`/`OutcomeTypes` are XML-order enums; deleting mid-file shifts indices and can corrupt the fixture's queued data. Left dormant (neutralised by the lock) until A-hard.
- Verify: clean `XmlLoad.log`, fixture loads, single-map game plays.

### A-hard (later; the real wins; breaks/ salvages the save)

Collapse to a single `CvMap`. The wins are simplicity/correctness + a modest turn-time/memory gain (removes hot-path `[CURRENT_MAP]` indirection in `CvPlayer.units()/cities()`, `GC.getMap()`, pathfinders).

- Trim `MapTypes` (`CvEnums.h:677`) → `NUM_MAPS = 1`; trim Python enum exposure (`CyEnumsInterface.cpp` **and** `CvPythonEnumLoader.cpp`).
- Remove dormant machinery: `switchMap`, `beforeSwitch`/`afterSwitch`, `moveUnitToMap`/`updateIncomingUnits`/`deleteOffMapUnits`/`TravelingUnit`/`m_IncomingUnits`, `getMapByIndex`/`getMaps`, the `switchMap` Python binding; `CURRENT_MAP` → constant `MAP_EARTH`.
- Collapse `CvPlayer` per-map containers (`m_cities/m_units/m_selectionGroups/m_plotGroups/m_groupCycles`, `m_startingCoords`) and the per-map pathfinder arrays to single.
- Remove XML missions/outcomes/`CIV4MapInfo` (keep Earth)/WB saves; remove `P2K_Multimaps_Test`.
- **Save-salvage shim** (so the fixture still loads): route the map *container* reads through the saved count + remap, read-or-discard:
  - `CvMapExternal::read` (`CvMapExternal.cpp:182`) loops `GC.getMaps()` (current count) → change to loop the **saved** map count (from the read class-mapping table for `REMAPPED_CLASS_TYPE_MAPS`); for each old index `getNewClassEnumValue(..., allowMissing)` → read into Earth if it maps, else read into a throwaway `CvMap` and discard. Per-map blocks are self-consistent so discard consumes exactly the right bytes.
  - Same pattern for the `CvPlayer` per-map serialization loops.
- **Win measurement:** fresh fixed-seed autoplay game (deterministic, loads on both soft and hard builds — the hand fixture only loads on soft), compare `doTurn` ms (`[PERF]`), peak memory, save size, LOC/indirection-sites removed.

## Phase B — Zone multimap (one map, zone paging)

- **B1 Zone model:** per-plot zone id (serialized) + zone registry (id, bounds, adjacency); repurpose `MapCategoryTypes` as the unit/content zone restriction.
- **B2 Mapscript:** lay out solar-system zones in one map's coords with impassable buffers; assign each plot its zone id + category; declare adjacency.
- **B3 Zone paging:** in `CvPlotPaging::UpdatePaging`, swap distance-marking (`dist2 < pageInDist²`) for zone membership (current ∪ adjacent required); reuse the shipped proactive eviction for the rest; background-preload adjacent zones over the frame budget. → smooth within a zone.
- **B4 Per-zone view + minimap:** viewport-scope-to-zone or UI minimap crop (decide).
- **B5 Inter-zone travel:** the launch mechanic — move a unit's coords across a buffer, gated by MapCategory-as-zone (replaces `moveUnitToMap`/`GO_TO_MAP`).
- **B6 Navigation UI:** Alt-keys/buttons → `bringIntoView` to zone center + zone-scoped minimap.
- **B7 (deferred):** AI awareness + the space-concept "linear continuity" rework (make space extend Earth's gradients so existing AI carries over).

## Removal inventory (from audit)

**C++:** `MapTypes`/`NUM_MAPS` (`CvEnums.h:677-700`); `m_maps`/`getMaps`/`getMapByIndex`/`switchMap` (`CvGlobals.h:183-218,848`, `.cpp:412-428,2801-2957`); per-map pathfinder arrays (`CvGlobals.h:854-860`); `m_eCurrentMap`/`getCurrentMap`/`setCurrentMap`/`CURRENT_MAP` (`CvGame.h:80-81,375,888`, 53 refs, 30 in CvPlayer.cpp); `beforeSwitch`/`afterSwitch`/`moveUnitToMap`/`updateIncomingUnits`/`deleteOffMapUnits`/`TravelingUnit`/`m_IncomingUnits`/`m_eType` (`CvMap.{h,cpp}`); `CvMapExternal.cpp` proxy + serialization; Python enum (`CyEnumsInterface.cpp`, `CvPythonEnumLoader.cpp`); `CvPlayer` per-map containers (`CvPlayer.h:1773,2015-2019`, init/reset/serialize loops).
**KEEP:** `MapCategory` (CvEnums + `CvBonusInfo/CvBuildingInfo/CvBuildInfo/CvFeatureInfo/CvPromotionInfo/CvImprovementInfo` + `CvPlot::getMapCategories/isMapCategoryType`), `CvViewport`, `CvPlotPaging`.
**XML/Python/data:** `CIV4MapInfo.xml` (17 maps); `CIV4MissionInfos.xml` `MISSION_GO_TO_MAP_*` (1795-1928); `CIV4OutcomeInfos.xml` `GO_TO_MAP`/`COLONIZE_MAP` (1428-1747); `CvOutcomeInterface.py` can*/go* (800-919); ParallelMaps UI + hooks; `P2K_Multimaps_Test` (disabled); Pepper2000 `EXPLORE_*` space content (separate gameplay pass); `PrivateMaps/Multimaps/*.WBSave` (15).

## Not used: c4lib

`hankinsohl/c4lib` (C++ lib + `c4edit` CLI) reads/edits/writes **stock** BTS saves via `BTS.schema` (positional). S2S saves are tagged + class-remapped, which it has no schema for. Salvage is done in-DLL (it owns the format) — see A-hard shim.

## Filed

GH Stones2Stars/S2S #324 — minidump filename month off-by-one (`tm_mon` not +1, `CvGlobals.cpp:224`).
