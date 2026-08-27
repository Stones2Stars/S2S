# 2. The entity lifecycle

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

| Call | When | Guard | Effect | Cite |
|---|---|---|---|---|
| ctor: `createUnitEntity(this)` / `setEntity(g_dummyEntity)` | every `CvUnit` construction, BEFORE `reset()` — the ctor pre-assigns `m_iX/m_iY = INVALID_PLOT_COORD` first, so the EXE receives a unit whose `plot()` is deterministically NULL rather than garbage that can alias an in-bounds plot | none | non-dummy mode: a real node per unit; dummy mode: the FIRST unit's real node becomes `g_dummyEntity`, every later unit attaches the shared dummy. `bGraphicsSetup=false`. | `Engine/CvUnit.cpp:48-52, 182-240` |
| `reloadEntity(bForceLoad)` | see caller census below | keeps an entity already of the wanted kind; destroys a mismatched one unless `bHoldsRealEntity && IsSelected()` (364); creates real (`g_numEntities++`, latch cleared) or attaches dummy when NULL | the ONE decision of real vs dummy; calls `setupGraphical()` iff `!bGraphicsSetup && bNeedsRealEntity && plot()` (417-420) | `Engine/CvUnit.cpp:317-422` |
| `setupGraphical()` | from `reloadEntity` (372) and `CvMap::afterSwitch` (1538) ONLY | returns WITHOUT latching if `!IsGraphicsInitialized \|\| !isInViewport()` (1061-1069); else latches `bGraphicsSetup=true` (1070), then `CvDLLEntity::setup()` if `!isUsingDummyEntities()` (1072-1075; the wrapper itself now guards `isRealEntity`) | then, unless `ACTIVITY_INTERCEPT` (→ `airCircle(true)`): `SetPosition(plot())` and NOTHING else — setup is placement, not movement. The EXE spawns a fresh node at the scene origin, and the move-family calls carry SHOWN-MOVEMENT semantics (`groupMove`'s own usage is the contract: `QueueMove` stages the stepped plots, `ExecuteMove` shows the movement), so either one issued here manufactures a visible run from mid-map to the plot on a unit that never moved. The 2019 billw `ExecuteMove(0,false)` multi-unit refresh hack is removed for that reason; if stacks regress to late-appearing figures, the refresh needs a non-movement replacement | `Engine/CvUnit.cpp:1057-1100`; `Engine/CvMap.cpp:1538` |
| `init(...)`: `setXY(bInit=true)` → `setupGraphical()` → `updateCenterUnit()` → `setFlagDirty(true)` | unit birth | the baseline birth order (`setXY` at 416, then `setupGraphical`/`updateCenterUnit`/`setFlagDirty` at 525/529/531); no `SetPosition` on the unit | ⚠ `reloadEntity` already fires INSIDE `setXY` (416), before `init`'s own tail: `joinGroup(NULL,true)` (13624, reached under `!bGroup && (!getGroup() \|\| getGroup()->getNumUnits() > 1)`, 13616 — which `init`'s call satisfies, passing `bGroup=false`) → `CvSelectionGroup::addUnit` head-swap `reloadEntity()` (4826-4833), and `setXY`'s own `updateCenterUnit` (14031, 14036) | `Engine/CvUnit.cpp:416, 525-531, 13616-13624, 14031-14039`; `Engine/CvSelectionGroup.cpp:4826-4833` |
| `setXY` graphics block: `QueueMove(pNewPlot)` else `SetPosition(pNewPlot)` | every coordinate change | `IsGraphicsInitialized && isInViewport()`; `QueueMove` iff `bShow \|\| bCheckPlotVisible && pNewPlot->isVisibleToWatchingHuman()` | the ordinary move; both arms reduce to "DESTINATION visible" — the origin plot is never consulted (`CvUnit::move` passes `bShow && pPlot->isVisibleToWatchingHuman()`, 5158-5171). Both no-op on the dummy. | `Engine/CvUnit.cpp:14146-14171, 5158-5171` |
| `setXY` tail `reloadEntity()` | after the move | iff viewport membership of old/new plot differs, OR (dummy mode) `isActiveVisible(false)` of old/new differs | the fog-edge real↔dummy transition; a fresh real node is then set up and placed by `setupGraphical`'s `SetPosition(plot())` (the earlier `QueueMove` no-op'd on the dummy) | `Engine/CvUnit.cpp:14054-14061` |
| `updateCenterUnit` → `newCenterUnit->reloadEntity(true)` | on a CHANGED centre verdict | the ONLY `bForceLoad=true` caller | forces a real node for the new centre unit; the OLD centre unit is never touched | `Engine/CvPlot.cpp:9993-9997` |
| `CvSelectionGroup::addUnit`/`removeUnit` head swap → `reloadEntity()` on old+new head | group membership change | `ENABLE_DYNAMIC_UNIT_ENTITIES` | the only PER-UNIT path that DOWNGRADES a real node during ordinary play besides `setXY`'s tail; the bulk `reloadEntity()` sweeps below (`CvPlayer::setupGraphical`, `changeCiv`, `setActivePlayer`) and `rebuildEntityArt` downgrade too | `Engine/CvSelectionGroup.cpp:4826-4833, 4866-4877` |
| `CvGame::setActivePlayer` graphics block | active player change, after the (hotseat-only) unit sweep | `IsGraphicsInitialized` | map `updateFog`, `updateVisibility`, `updateSymbols`, `updateMinimapColor`, then `updateUnitEnemyGlow()` (every player's units filtered `!isUsingDummyEntities`, raw `updateEnemyGlow`) | `Engine/CvGame.cpp:4849-4862, 4884-4896`; `Engine/CvGame.h:597` |
| `CvGame::setActivePlayer` unit sweep `reloadEntity()` | active player change | ONLY when `isHumanPlayer() && (isHotSeat() \|\| isPbem() \|\| bForceHotSeat)`; `CvGame::read` calls with default `false` (8723) | not a single-player load sweep | `Engine/CvGame.cpp:4808-4846, 8716-8723` |
| `CvPlayer::setupGraphical` (`DllExport`) | `baseInit` (329), `changeCiv` (1737), `setColor` (24519), and the EXE | `IsGraphicsInitialized` | cities `setupGraphical`; units `reloadEntity()` (the unit `setupGraphical` line is commented out) | `Engine/CvPlayer.cpp:1817-1826` |
| `CvPlayer::changeCiv` sweep | civ change | none | `reloadEntity()` every unit, then `setupGraphical()` again | `Engine/CvPlayer.cpp:1666-1668, 1737` |
| `rebuildEntityArt()` | warlord attach (10101), `setLeaderUnitType` (16045) | `destroyCurrentEntity()` only if `!IsSelected()` | a SELECTED unit whose leader model changes keeps its old node | `Engine/CvUnit.cpp:306-315` |
| `CvMap::beforeSwitch` / `afterSwitch` | `switchMap` (parallel maps, `eMap != CURRENT_MAP`), viewport `MILITARY_ADVISOR_LAUNCHING`, viewport `BRING_INTO_VIEW` when `m_mode == INITIALIZED` | unit filter `!isUsingDummyEntities()` — passes a NULL entity, excludes dummy-holders | before: `RemoveUnitFromBattle`+`removeEntity`+`destroyEntity` (no `setEntity(NULL)`), then EVERY plot `destroyGraphics()` = `hideGraphics(ALL)` + `m_bPlotLayoutDirty=false` (`Engine/CvPlot.cpp:12836-12855`; this is `destroyGraphics`'s ONLY caller, 1456); after: on the FIRST switch (`!plotsInitialized()`) `init`+`generateRandomMap`+`addGameElements` (or the WB map, 1464-1481), `ClearMinimap`+`InitGraphics` (1485-1486), every plot `setLayoutDirty`+`setFlagDirty` (1518-1519), `RebuildAllPlots` (1523), `createUnitEntity`+`setupGraphical` per non-dummy unit (1537-1538), `setRevealedPlots(activeTeam)` iff first switch and the map `startRevealed()` (1543-1544), then map `setupGraphical`/`updateFog`/`updateSymbols`/`updateFlagSymbols`/`updateMinimapColor` (1548-1552), and `setActionState(AFTER_SWITCH)` (1565). Both halves wrap `CvGame::processGreatWall` under `THE_GREAT_WALL` (1421, 1556-1561) — a deliberate off-switch, [special-systems.md](../special-systems.md#the-great-wall-render--compiled-out-on-purpose) | `Engine/CvMap.cpp:1412-1457, 1459-1568`; `Engine/CvPlot.cpp:12836-12855`; `Defines/CvGlobals.cpp:3051-3071`; `UI/CvViewport.cpp:337-341, 348-361` |
| `destroyCurrentEntity()` | `reloadEntity` (366), `rebuildEntityArt` (312) | real: `RemoveUnitFromBattle`+`removeEntity` under `!GetDone && IsGraphicsInitialized`, then `destroyEntity`, `g_numEntities--`; dummy: `g_dummyUsage--`; always `setEntity(NULL)` | | `Engine/CvUnit.cpp:273-304` |
| `~CvUnit` | death | `!isUsingDummyEntities()` (admits NULL), then `!GetDone && IsGraphicsInitialized` | `RemoveUnitFromBattle`+`removeEntity`, `destroyEntity` (wrappers guard NULL) | `Engine/CvUnit.cpp:247-262` |

**Which calls move or refresh a node.** `setupGraphical` places a freshly built node once, with
`SetPosition(plot())`; thereafter a scene node follows the unit only through the ordinary `setXY` graphics block
(below) and the `ExecuteMove` animations. The sites that touch a node's position/animation OUTSIDE a `setXY`
coordinate change: `setupGraphical`'s `SetPosition` (`Engine/CvUnit.cpp:1089-1098`); `groupMove`'s
timed `ExecuteMove` on a member that could NOT move after `joinGroup(NULL,true)` splits it off
(`Engine/CvSelectionGroup.cpp:3638-3639`) and on EVERY member at the end of the move (3677); and `updateCombat`'s
`ExecuteMove(0.5f,true)` on the attacker (`Engine/CvUnit.cpp:2784`). A unit holding an already-set-up real node
that does not move is never repositioned: `reloadEntity` → `kept` skips `setupGraphical` while the latch is up
(`Engine/CvUnit.cpp:370-373`).

**Raw EXE calls that BYPASS the `CvDLLEntity` guards** (no wrapper exists), with the guard each site carries:

- `AddMission` — `Engine/CvUnit.cpp:22541-22547`, gated on `CvMissionDefinition::isValid()` ONLY, which is what
  makes it dummy-safe: the plot must be `isActiveVisible(false)`, the attacker must be
  `!isUsingDummyEntities() && isInViewport()`, and so must the defender if one is set
  (`Engine/CvStructs.cpp:508-523`).
- `RemoveUnitFromBattle` — `~CvUnit` under `!isUsingDummyEntities()` (`Engine/CvUnit.cpp:247-254`);
  `destroyCurrentEntity` under `isRealEntity` (282-287); `airCircle`/`updateCombat`/`setCombatUnit` under
  `!isUsingDummyEntities() && isInViewport()` (1713, 3017, 3021, 6158); `CvMap::beforeSwitch` under the
  `!isUsingDummyEntities` filter (`Engine/CvMap.cpp:1435`).
- `updateEnemyGlow` — `setXY` tail (`Engine/CvUnit.cpp:14275`); `CvGame::updateUnitEnemyGlow` (`DllExport`,
  `Engine/CvGame.h:597`; every player's units filtered `!isUsingDummyEntities`, `Engine/CvGame.cpp:4884-4896`),
  called from `setActivePlayer` inside its `IsGraphicsInitialized` block (4849, 4862) and by the EXE;
  `CvPlayer::doWarnings` per unit under `!isUsingDummyEntities()` (`Engine/CvPlayer.cpp:16135-16140`).
- `updateGraphicEra` — the `CvPlayer::setCurrentEra` unit sweep under `!isUsingDummyEntities()`
  (`Engine/CvPlayer.cpp:11076-11081`).
- `showPromotionGlow` — `CvUnit::setPromotionReady` (`Engine/CvUnit.cpp:15825-15828`); `updatePromotionLayers` —
  `setHasUnitCombat` (17643-17646) and `setHasPromotion` (18215-18218); all three under
  `!isUsingDummyEntities() && isInViewport()`.
- `CvUnit::NotifyEntity` — `DllExport`, hides the wrapper, `!isUsingDummyEntities() && isInViewport()`
  (`Engine/CvUnit.cpp:1821-1827`).
- The PLOT symbols: `updatePosition` on the feature/route/river symbols inside `updateFeatureSymbol` (9686),
  `updateRouteSymbol` (9731) and `updateRiverSymbol` (9792, 9801) — so under those functions' own
  `isGraphicsVisible` gates (9655, 9702, 9746); `setupFloodPlains` in `updateRiverSymbolArt` (9820, 9829) under
  `IsGraphicsInitialized` (9812) and `!GC.viewportsEnabled() || isRevealed(activeTeam, true)` per symbol
  (`Engine/CvPlot.cpp`).

Wherever `isUsingDummyEntities()` is the guard it admits a NULL entity. `addEntity` is declared
(`Infrastructure/CvDLLEntityIFaceBase.h:23`) and has neither wrapper nor caller. `PlayAnimation`/`StopAnimation`/
`MoveTo`/unit `setVisible` have zero callers (the former commented billw test block has been removed).

**Guarded-wrapper callers outside the lifecycle rows.** `SetSiegeTower(true/false)` from `CvUnit::setCombatUnit`
(`Engine/CvUnit.cpp:16071-16073, 16128-16130`), each site under `!isUsingDummyEntities() && isInViewport()` on top
of the wrapper's `isRealEntity` (`Infrastructure/CvDLLEntity.cpp:143-148`). `NotifyEntity` from 29 live `CvUnit`
sites — `updateCombat` (3027-3028), `setDamage` (14485), `setFacingDirection` (15643), and the mission verbs
`sabotage` … `hurryFood` (7946-21697) — from
`CvSelectionGroup::startMission` (1461, 1796), `setActivityType` (4380, `MISSION_IDLE`) and `addUnit` (4823,
`MISSION_MULTI_SELECT`) through the group forwarder (`Engine/CvSelectionGroup.cpp:3289-3291`), and from Python via
`CyUnit::NotifyEntity` (`Python/CyUnit.cpp:28-30`). `airCircle` from `CvSelectionGroup::setActivityType` on
leaving/entering `ACTIVITY_INTERCEPT` (`Engine/CvSelectionGroup.cpp:4359-4370`) and from `setupGraphical` (1194).

**Other `isUsingDummyEntities`/`isInViewport` gate sites** — the animation entry points, each reducing to the
`isValid` shape above: combat focus in `updateCombat` requires `plot()->isInViewport()` AND
`pDefender->isInViewport()` (`Engine/CvUnit.cpp:2867-2871`); `updateAirStrike` gates its `addMission` on
`pPlot->isVisibleToWatchingHuman()` (2009-2014); `nuke` on `isActiveVisible(false) && !isUsingDummyEntities() &&
isInViewport()` (6909-6913); `doActiveDefense` on the same trio for the DEFENDER (20628); `fighterEngage` on
`kAirMission.isValid()` (20731-20735); and `plotExternal` (`DllExport`) ASSERTS `isInViewport()` and
`!isUsingDummyEntities()` on every EXE read of a unit's plot (14302-14309).

