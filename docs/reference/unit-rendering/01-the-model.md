# 1. The model

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

A plot presents exactly ONE unit: `CvPlot::m_pCenterUnit`, chosen by `CvPlot::updateCenterUnit`
(`Engine/CvPlot.cpp:9965-10014`) and handed to the EXE raw through the `DllExport` `getCenterUnit()`
(`Engine/CvPlot.cpp:9938`). A unit's scene node is a `CvEntity` held by `CvDLLEntity`; with the XML define
`ENABLE_DYNAMIC_UNIT_ENTITIES=1` (`Assets/XML/A_New_Dawn_GlobalDefines.xml:264-265`, read once into
`g_bUseDummyEntities` at `Engine/CvUnit.cpp:189-196`) every unit shares ONE `g_dummyEntity`
(`Engine/CvUnit.cpp:48`) unless `reloadEntity` decides it needs a REAL node
(`bNeedsRealEntity`, `Engine/CvUnit.cpp:319-326`): dynamic entities off, or a forced load, or the plot is
fog-visible to the active team AND (the unit is the plot's centre unit OR belongs to the active player).
`isRealEntity(e) = e != NULL && e != g_dummyEntity` (`Engine/CvUnit.cpp:175-178`);
`isUsingDummyEntities() = entity && entity == g_dummyEntity`, i.e. **FALSE for NULL** (`Engine/CvUnit.cpp:266-271`).
Every `CvDLLEntity` wrapper that hands an entity to the EXE is gated on `isRealEntity`
(`Infrastructure/CvDLLEntity.cpp:20-161`); `ExecuteMove` additionally requires `isInViewport()`
(`Infrastructure/CvDLLEntity.cpp:119-124`); `createUnitEntity`/`createCityEntity` are unguarded (they create).
Graphics paging (`GC.isGraphicalPaging()`, BUG option `MainInterface__EnableGraphicalPaging`, default True —
`Defines/CvGlobals.cpp:3327-3329`, `Assets/Config/BUG Main Interface.xml:32`; re-read on every BUG options-screen
`close()`, `Assets/Python/BUG/BugOptionsScreen.py:67-73`) and viewports
(`ENABLE_VIEWPORTS`, XML 0 — `Assets/XML/ParallelMaps_GlobalDefines.xml:5-6`) are two separate mechanisms:
with viewports off `isInViewport()` is unconditionally true (`UI/CvViewport.h:325-330`).

