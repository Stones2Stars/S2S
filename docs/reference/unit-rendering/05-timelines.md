# 5. Timelines

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

EXE-owned steps are marked `[EXE]`; their order relative to DLL entry points is not visible from the tree.

**Save load.** `[EXE]` `CvGame::read` → `reset` (`ResetPaging` ×2, `m_bFinalInitialized=false`;
`Engine/CvGame.cpp:8424-8429, 1057, 1288`) → … `[EXE]` `CvMap::read` → `CvPlot::read` per plot (`m_units.Read`
11048; ends `updateCenterUnit()` 11174 — refused while graphics are down) → `[EXE]` `CvPlayer::read` per player
(`CvUnit::read` writes `m_iX/m_iY` raw, `Engine/CvUnit.cpp:18359-18360`; NO `setXY`/`reloadEntity`; a corrupt-group
repair may `addUnit` → head-swap `reloadEntity`, `Engine/CvPlayer.cpp:16900-16926`) → `CvGame::read` tail
`setActivePlayer(firstHuman)` (8716-8723; no unit sweep in single player, §2) → `[EXE]` `SetGraphicsInitialized(true)`
at an EXE-chosen point → `[EXE]` `CvPlayer::setupGraphical` per player (`DllExport`; `reloadEntity()` every unit —
measured as ~9k contiguous `kept` lines grouped by owner) → `[EXE]` `CvMapExternal::read` sets viewport state
`LOADING` (`UI/CvMapExternal.cpp:193`) → `[EXE]` `CvGame::update` #1: `onFinalInitialized(false)` (2452-2454) →
`processActionState` (2473): `LOADING` selects a head unit and `bringIntoView(unit, bForceCenter=true)`
(`UI/CvViewport.cpp:421-437`) → `BRING_INTO_VIEW` runs `beforeSwitch`/`afterSwitch` iff `m_mode == INITIALIZED`
(348-361) — with viewports off `CvMap::reset` already set INITIALIZED via `setMapOffset(0,0)`
(`Engine/CvMap.cpp:293-298`; `UI/CvViewport.cpp:44-49`), yet whether `afterSwitch` actually runs on a load is not
established from the tree (§8) →
`UpdatePaging` (2489): ON: distance-sorted `setRequireGraphicsVisible` per frame; OFF: the live per-frame loop →
`showRequiredGraphics` → `updateGraphics(UNIT)` → `updateCenterUnit` → `reloadEntity(true)` on the chosen centre
unit — the only `bForceLoad=true` caller (§2) → `createUnitEntity` → `setupGraphical` → `setup` +
`SetPosition(plot())`. Every load-time real node is created through this `updateCenterUnit` path. `CvGame::onFinalInitialized` also clears every plot's
visibility and re-registers sight via `CvMap::updateSight(true,false)` (551), so every active-team plot that
becomes visible runs the `updateFog`/`updateMinimapColor`/`updateCenterUnit` trio then
(`Engine/CvPlot.cpp:8867-8872`).

**New game.** `[EXE]` `CvGame::init` → `reset` (`ResetPaging`) → NPC `addPlayer` → `AI_init` — creates NO units
(`Engine/CvGame.cpp:126-462`). `[EXE]` `CvPlayer::init` per player → `baseInit` → `setupGraphical` (no-op unless
graphics already up; `Engine/CvPlayer.cpp:305-329`). `[EXE or afterSwitch firstSwitch]` `CvMap::init` +
`generateRandomMap` + `addGameElements` (plot-graphics mutation guarded by `IsGraphicsInitialized`,
`Engine/CvPlot.cpp:6986`). `[EXE]` `CvGame::setInitialItems` (`DllExport`, `Engine/CvGame.cpp:827-884`) →
`initFreeUnits` → `CvPlayer::initFreeUnits` → `initUnit` → `CvUnit` ctor (dummy) → `CvUnit::init`: `setXY(bUpdate)`
[`addUnit` → `updateCenterUnit`, refused if graphics down] → `joinGroup` → head-swap `reloadEntity` →
`updateCenterUnit` → `reloadEntity()` [`bNeedsRealEntity` true via the active-player clause even with
`m_pCenterUnit` NULL, so `createUnitEntity` runs pre-graphics and `setupGraphical` early-returns without latching
while graphics are down]. `[EXE]` `SetGraphicsInitialized(true)`. `[EXE]` `CvMapExternal::setupGraphical` →
`CvViewport::setupGraphical` (mode INITIALIZED, `UI/CvViewport.cpp:218-222`) → `CvMap::setupGraphical` →
`CvPlot::setupGraphical` (shows nothing: both masks NONE). `[EXE]` `setFinalInitialized` — body only prints
(`Engine/CvGame.cpp:4741-4747`); the flag is set by `onFinalInitialized` from `CvGame::update` on both paths (472,
2452-2454). `[EXE]` `CvGame::update` #1 → `onFinalInitialized(true)` → `UpdatePaging` → … →
`updateCenterUnit` → `reloadEntity(true)` → real node + `setupGraphical` (`SetPosition(plot())`). The DLL-internal `regenerateMap` fixes the
order units-before-`CvMap::setupGraphical` for the regen case (`Engine/CvGame.cpp:943-957`).

