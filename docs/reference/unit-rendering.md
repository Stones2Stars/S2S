# Unit rendering — the pipeline, and graphics paging ON vs OFF

> **⚠ TREE-STATE NOTE:** `Sources/` was reverted to HEAD after the run-from-origin hunt, and **HEAD is
> verified clean — units render in place**. The run-from-origin regression lived entirely in the uncommitted
> experimental tree, which is preserved in a git stash (`run-from-origin hunt: full session source
> experiments`); any piece reintroduced from it is validated against the render-in-place check and against
> the §9 contract before it stays. Sections 2–6 below carry line cites and a few behavioural claims from that
> experimental tree and are being re-aligned to HEAD. §7b (measured engine behaviour of out-of-contract node
> introduction) and §9 (the Firaxis reference contract) are tree-state-independent and authoritative.

> **Reference — how the DLL drives unit graphics today.** The renderer is the closed EXE; the DLL reaches it only
> through the 26 virtuals of `CvDLLEntityIFaceBase` (`Sources/Infrastructure/CvDLLEntityIFaceBase.h:20-48`) and
> the 71 of `CvDLLEngineIFaceBase`. Everything here is what the DLL CALLS and WHEN; what the EXE does with a call
> is stated only where it has been MEASURED on the `[GFX]` spine domain (`Graphics.log`,
> `Sources/UI/CvGraphicsTrace.cpp:141`), and is otherwise listed under §8. Every line number is a citation into
> `Sources/`; the tree outranks this page.

## 1. The model

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

## 2. The entity lifecycle

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
| `CvMap::beforeSwitch` / `afterSwitch` | `switchMap` (parallel maps, `eMap != CURRENT_MAP`), viewport `MILITARY_ADVISOR_LAUNCHING`, viewport `BRING_INTO_VIEW` when `m_mode == INITIALIZED` | unit filter `!isUsingDummyEntities()` — passes a NULL entity, excludes dummy-holders | before: `RemoveUnitFromBattle`+`removeEntity`+`destroyEntity` (no `setEntity(NULL)`), then EVERY plot `destroyGraphics()` = `hideGraphics(ALL)` + `m_bPlotLayoutDirty=false` (`Engine/CvPlot.cpp:12836-12855`; this is `destroyGraphics`'s ONLY caller, 1456); after: on the FIRST switch (`!plotsInitialized()`) `init`+`generateRandomMap`+`addGameElements` (or the WB map, 1464-1481), `ClearMinimap`+`InitGraphics` (1485-1486), every plot `setLayoutDirty`+`setFlagDirty` (1518-1519), `RebuildAllPlots` (1523), `createUnitEntity`+`setupGraphical` per non-dummy unit (1537-1538), `setRevealedPlots(activeTeam)` iff first switch and the map `startRevealed()` (1543-1544), then map `setupGraphical`/`updateFog`/`updateSymbols`/`updateFlagSymbols`/`updateMinimapColor` (1548-1552), and `setActionState(AFTER_SWITCH)` (1565). Both halves wrap `CvGame::processGreatWall` under `THE_GREAT_WALL` (1421, 1556-1561) — a deliberate off-switch, [special-systems.md](special-systems.md#the-great-wall-render--compiled-out-on-purpose) | `Engine/CvMap.cpp:1412-1457, 1459-1568`; `Engine/CvPlot.cpp:12836-12855`; `Defines/CvGlobals.cpp:3051-3071`; `UI/CvViewport.cpp:337-341, 348-361` |
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

## 3. The plot side — choosing the centre unit

`updateCenterUnit` (`Engine/CvPlot.cpp:9965-10014`):

1. Gate: `m_bInhibitCenterUnitCalculation || !isGraphicsVisible(UNIT)` → `m_pCenterUnit = NULL`, trace
   `inhibited`/`notGraphicsVisible`, return — no reload, no flag dirty (9971-9983).
   `isGraphicsVisible(g) = IsGraphicsInitialized() && (m_visibleGraphics & g) && isInViewport()`
   (460-472) — the mask binds in BOTH paging modes (paging off is on-with-infinite-radius; the live walk
   maintains the mask exactly as page-in does). ⚑ Paging enters unit rendering ONLY through which plots the
   walk has shown.
2. `newCenterUnit = isActiveVisible(true) ? getPreferredCenterUnit() : NULL`; WorldBuilder falls back to the head
   unit (9985-9991). `isActiveVisible(bDebug) = isVisible(activeTeam, bDebug)` — a FOG test
   (`visibilityCount>0 || stolenVisibilityCount>0`), never a screen test (4925-4944).
3. `getPreferredCenterUnit` (1573-1607): `getSelectedUnit()` (first unit on the plot with `IsSelected()`, 3747-3755 —
   which is `false` for any dummy-holder, `Infrastructure/CvDLLEntity.cpp:82-85`), then
   `getBestDefender(activePlayer, NO_PLAYER, NULL, false, false, bTestCanMove=true)`, `getBestDefender(activePlayer)`,
   then three `getBestDefender(NO_PLAYER, activePlayer, ...)` variants — which, via `owner != eAttackingPlayer`
   (3607-3672), pick only NON-active-player units. A score of 0 is never chosen (`iValue > iBestValue`, `iBestValue`
   starts 0); `getDefenderScore` rejects dead/0-HP units and foreign units `isInvisible(activeTeam)` (3497-3593).
4. On change: `newCenterUnit->reloadEntity(true)`, assign, `updateMinimapColor`, `setFlagDirty(true)`,
   `setInfoBarDirty` (9993-10008). The displaced unit gets no call.
5. `[GFX] centerUnit` is emitted on EVERY pass (10019-10023) — at its investigation tiers this line alone wrote
   77,245 lines per 8 MB of `Graphics.log` ([spine.md](../spine.md), the re-tier-to-4 rule).

**Every reader of `m_pCenterUnit`.** `getCenterUnit(bForced)` returns the head unit when `bForced` and the
centre is NULL (9957-9963); `getDebugCenterUnit()` (`DllExport`, `Engine/CvPlot.h:891`) does the same under
`isDebugMode()` (9946-9955). Render readers: **the FLAG is the centre unit's** — `updateFlagSymbolIfVisible` reads
`getCenterUnit(isDebugMode())` (9857); `plotMinimapColor` reads the same under `isActiveVisible(true)` (10594);
`CvGame::selectAll` (`Engine/CvGame.cpp:2858`); `bNeedsRealEntity` (`Engine/CvUnit.cpp:323`). ⚠ GAMEPLAY readers
of render state: `CvSelectionGroup::startMission` sets the shadow target to `pShadowPlot->getCenterUnit(false)`
when exactly one unit qualifies (`Engine/CvSelectionGroup.cpp:1912`), and `CvUnit::canShadowAt` defaults its
subject to `getCenterUnit(false)` (`Engine/CvUnit.cpp:22494`) — both read a value that is NULL on any plot whose
UNIT graphics are not visible (§4), so shadowing depends on paging state and on `IsGraphicsInitialized`.

**Every `updateCenterUnit` trigger:** `updateGraphics` UNIT bit (`Engine/CvPlot.cpp:518`); `hideGraphics` UNIT bit
(566, then `m_pCenterUnit = NULL` at 567 and both flag entities destroyed); `changeVisibilityCount` active team on
a fog flip (8871); `changeStolenVisibilityCount` (8924); `setSpotIntensity` under `GAMEOPTION_COMBAT_HIDE_SEEK`
(10109); `addUnit(bUpdate)` (10195); `removeUnit(bUpdate)` (10241); `CvPlot::read` end (11174);
`hasStealthDefender` reveal (12404); `enableCenterUnitRecalc(true)` (12914); `unitGameStateCorrections` (13033);
`CvUnit::init` (`Engine/CvUnit.cpp:576`); `CvUnit::setXY` old+new plot when `bUpdate` (14224, 14229);
`CvMap::updateCenterUnit` whole-map sweep (`Engine/CvMap.cpp:622-628`), whose ONLY callers are the EXE-facing
proxies `CvMapExternal::updateCenterUnit` (`UI/CvMapExternal.cpp:76-78`) and `CvViewport::updateCenterUnit`
(`UI/CvViewport.cpp:502`) — its cadence is EXE-owned (§8).

`enableCenterUnitRecalc(false/true)` is used ONLY by `CvSelectionGroup::groupMove` on the start and destination
plots (`Engine/CvSelectionGroup.cpp:3601-3602, 3668-3669`; `Engine/CvPlot.cpp:12908-12916`). Inside that window
`m_bIsMidMove` is true, every `updateCenterUnit` call NULLs `m_pCenterUnit` (9986), and lifting the
inhibit re-runs the selection — reaching `reloadEntity(true)` → `setupGraphical`'s `SetPosition(plot())`
BEFORE the group's own `ExecuteMove` loop (3677), with no `isMidMove`/`isBusy` guard in `setupGraphical`.

Flags: `updateFlagSymbol` is `DllExport` (`Engine/CvPlot.h:885`) — the EXE is a direct caller — and inside the DLL
is reached only from `updateGraphics` (519); `updateFlagSymbolIfVisible` gates on `isGraphicsVisible(UNIT)`
(9846-9853) and is driven by `setFlagDirty` through `CvMap::updateFlagSymbolsInternal(bForce)`
(`Engine/CvMap.cpp:507-527`: `bForce` repaints every plot regardless of the dirty bit; the bit is cleared only
when the repaint actually happened). Its callers: `toggleDebugMode` (`Engine/CvGame.cpp:4666`), `afterSwitch`
(1582), the `DllExport` `CvMapExternal::updateFlagSymbols` (`UI/CvMapExternal.cpp:52-54`), the virtual
`CvViewport::updateFlagSymbols` (`UI/CvViewport.cpp:482-484`, no DLL caller), and — the ONE forced call —
`CvViewport::processActionState` in state `BRING_INTO_VIEW_COMPLETE`, `updateFlagSymbolsInternal(true)` after
re-selecting the preserved head unit (367-380).

**`setFlagDirty(true)` callers** (each repainted on the next `updateFlagSymbolsInternal` pass):
`updateCenterUnit` on a changed centre (`Engine/CvPlot.cpp:10002`); `addUnit`/`removeUnit` with `bUpdate` (10197,
10242); `unitGameStateCorrections` (13034); `CvUnit::init` (`Engine/CvUnit.cpp:578`); `setXY` old+new plot when
`bUpdate` (14225, 14230); `joinGroup` (13568) and `setMoves` (14564), both only when the unit's team is the active
team; `CvSelectionGroup::setActivityType` under the same team test (`Engine/CvSelectionGroup.cpp:4401-4403`);
`CvMap::afterSwitch` every plot (`Engine/CvMap.cpp:1520`). The `changeCiv` per-plot call is inside a `/* */` block
(`Engine/CvPlayer.cpp:1669-1696`) — dead.

`setLayoutDirty` is a bonus/improvement (plot-builder) mechanism, gated `IsGraphicsInitialized && isInViewport`,
and resets itself to false on a plot with no visible bonus/improvement or with FEATURE graphics not visible
(11510-11550); nothing in `Sources/` reads `isLayoutDirty` except `setLayoutDirty` itself — the consumer is the
EXE. **`setLayoutDirty(true)` callers** beyond `updateGraphics`/`hideGraphics` (498, 585): `updateVisibility`
(1338); `setIrrigated` on the 3×3 around the plot (6298); `setPlotType` when rebuilding graphics (6991);
`setBonusType` (7333); `setImprovementType` in debug mode only (7555); `setRevealed` on adjacent unrevealed
water (9294); `setRevealedImprovementType` for the active team (9474); `updateRouteSymbol` on a new route
symbol (9727) and `updateRiverSymbol` on a new river symbol (9797); `CvPlayer::setCurrentEra` on every plot
with a revealed improvement the player owns or (for the active player) nobody owns (`Engine/CvPlayer.cpp:11064`);
`CvTeam::processTech` on every plot whose bonus the tech reveals (`Engine/CvTeam.cpp:5466`) and
`setForceRevealedBonus` (5763); `CvMap::afterSwitch` every plot (`Engine/CvMap.cpp:1519`).

The EXE's own reads: `getCenterUnit()` raw (9938); `getBestDefenderExternal` returns NULL when the found defender is
off-viewport or holds the dummy (3479-3494); `CvGame::selectAll` selects the centre unit's group only if
active-player-owned (`Engine/CvGame.cpp:2854-2864`); `CvGame::selectUnit` asserts `!isUsingDummyEntities()` on
every unit inserted (2771, 2798) — `selectGroup` with alt/ctrl inserts every active-player `canMove()` unit on the
plot with NO such check (2812-2851).

## 4. Graphics paging ON vs OFF

> **⚖ THE DESIGN INTENT OF "PAGING OFF" IS PER-TYPE (owner): UNITS stay dynamically managed — fog of war
> requires unit graphics to come and go (the fog gate is `isActiveVisible` in the centre-unit system, not the
> paging bit) — while every OTHER graphics type (terrain, features, rivers, routes, cities), once seen, is
> never paged out again.** "Off" bounds the WORLD graphics' residency, never the units' fog behaviour.

`GC.isGraphicalPaging()` is read in exactly THREE places: `CvPlot::isGraphicsVisible` (470), `CvPlot::updateGraphics`
(491, paging-table enrolment), `CvPlotPaging::UpdatePaging` (214). `m_bGraphicalPaging` is written ONLY in
`refreshOptionsBUG` (`Defines/CvGlobals.cpp:3327-3329`), called from `setIsBug` (3302) and on BUG options-screen
close; it is not in the `cvInternalGlobals` initializer list (`Defines/CvGlobals.cpp:104-135`), so it is
indeterminate until then. `UpdatePaging` runs once per `CvGame::update` (`Engine/CvGame.cpp:2487-2490`).

| Mechanism | Paging ON (per `CvGame::update`) | Paging OFF |
|---|---|---|
| `UpdatePaging` body (`UI/CvPlotPaging.cpp:210-325`) | build + sort a vector of EVERY plot by toroidal distance to the look-at plot (225-236); frame budget `10 + PAGING_FRAME_TIME_MS/(1+4·moveDist²)` ms (242); evict while `g_iNumPagedInPlots > PAGING_RESIDENT_SOFT_CAP \|\| NeedToFreeMemory()` (256-263); walk nearest-first within budget calling `setRequireGraphicsVisible(type, dist² < d²)` for all six types (276-285) | a LIVE per-frame loop over every plot (301-322): on the ON→OFF transition frame (`g_bWasGraphicsPagingEnabled` still true) `disableGraphicsPaging()` per plot — require ALL + drop the paging handle (309-314); every frame after, `setRequireGraphicsVisible(ALL, true)` per plot (315-321), whose `showRequiredGraphics` shows the not-yet-visible delta and re-runs the build for any plot that could not render on an earlier frame; `g_bWasGraphicsPagingEnabled` cleared after (323). NO latch — a plot down at graphics-init converges once graphics are up (`Engine/CvPlot.cpp:479-494`) |
| Required mask `m_requiredVisibleGraphics` | per plot per frame within budget (282) | `ALL`, re-asserted EVERY frame (`setRequireGraphicsVisible(ALL,true)`, `UI/CvPlotPaging.cpp:320`); the transition frame's `disableGraphicsPaging` also requires ALL (`Engine/CvPlot.cpp:607-617`) |
| Visible mask `m_visibleGraphics` | set by `showRequiredGraphics` on the DELTA `(required ^ visible) & required` (490-491); cleared by `hideGraphics` | set the SAME way as the walk shows each plot; `isGraphicsVisible` binds on it in BOTH modes (the historic off-mode bypass answered whole-map-visible the instant graphics initialized — centre units assigned for plots nothing had built, the last mode asymmetry of the run-from-origin hunt) |
| `updateGraphics(toShow)` → `setLayoutDirty` + symbols/feature/river/route + (UNIT) `updateCenterUnit`+`updateFlagSymbol` + (CITY) city `setLayoutDirty` (496-535) | runs on each page-IN transition of a bit; an already-visible plot gets `updateGraphics(NONE)` = nothing | runs each frame on the delta via `showRequiredGraphics` until the plot is fully shown (then `toShow=NONE`, a mask compare), plus `setRevealed` (9314) |
| Page-OUT | ONLY `EvictGraphics` → `hideNonRequiredGraphics` → `hideGraphics(mask)` (`UI/CvPlotPaging.cpp:135-146`; `Engine/CvPlot.cpp:530-534`) under the eviction condition. Un-requiring a plot HIDES NOTHING by itself (447-458). With XML `PAGING_RESIDENT_SOFT_CAP=3000000` against a PLOT count, only `NeedToFreeMemory()` can trigger it: working set > `min(MAX_DESIRED_MEMORY_USED × 1024 ≈ 3.58 GB, ullTotalPhys − OS_MEMORY_ALLOWANCE)` (`Assets/XML/GlobalDefines.xml:40-48`; `UI/CvPlotPaging.cpp:82-131`, cap clause 116-122). [memory-footprint.md](memory-footprint.md) records the same inert cap and reads the 3.58 GB as above a 32-bit process's reach — the threshold is a DLL fact, its reachability an EXE (address-space) property | never |
| `hideGraphics(UNIT)` (536-586) | `updateCenterUnit()` hits the NOT_VISIBLE gate (bit already cleared at 538), `m_pCenterUnit=NULL`, both flag entities destroyed, `setLayoutDirty(true)`. The unit's real node is NOT touched | reached only from `setRevealed`/`uninit`/`destroyGraphics`; the inner `updateCenterUnit` runs a FULL recalc (may `reloadEntity(true)` a unit) whose result line 567 then discards |
| `updateCenterUnit` gate `isGraphicsVisible(UNIT)` | requires the UNIT bit paged in: plots beyond `PAGE_IN_DIST_UNIT` (XML 20, `Assets/XML/GlobalDefines.xml:88-89`) from the camera have `m_pCenterUnit=NULL` regardless of fog | `IsGraphicsInitialized && isInViewport` = with viewports off, graphics-initialized alone |
| `setRevealed` active team (9284-9320): `hideGraphics(ALL)` then `updateGraphics(ALL)` | every updater early-returns (visible==NONE); rebuild happens on a LATER frame when `UpdatePaging` re-requires the bits | synchronous rebuild; `updateCenterUnit` runs twice (566, 518) on the same unit; `m_visibleGraphics` left NONE (dead state while OFF) |
| Fog flip on the active team (`changeVisibilityCount` 8867-8872) | `updateFog`, `updateMinimapColor`, `updateCenterUnit` — the last refused for a plot outside the UNIT radius until paged in | the same trio; centre unit computed immediately |
| `bNeedsRealEntity` (`Engine/CvUnit.cpp:319-326`) | no paging term; but `plot()->getCenterUnit(false)==this` is FALSE for every unit on a plot whose UNIT bit is not paged in (its `m_pCenterUnit` was nulled at 9979), so only the active-player clause yields real nodes there | the centre clause depends on graphics-init + viewport only |
| `CvPlot::setupGraphical` = `showRequiredGraphics` + `updateVisibility` (620-627) from `CvMap::setupGraphical` (`Engine/CvMap.cpp:322-337`; callers `regenerateMap` 954, `toggleDebugMode` 4663, `afterSwitch` 1579, `CvMapExternal::setupGraphical` `DllExport`) | shows the delta of already-required bits | after the sweep both masks are `ALL` → `toShow = NONE` → NO component updater runs; `updateVisibility` (1329-1349) gates on `shouldHaveGraphics()`, then `setLayoutDirty(true)`, the symbol/feature-visibility/route updaters and the non-NPC city's `updateVisibility` — never `updateCenterUnit` |
| Paging table | plots enrolled in `updateGraphics` (491-494) even when `toShow==NONE`; evicted by INSERTION age (`iSeq` assigned at `AddPlot` only, `UI/CvPlotPaging.cpp:165-175`) | no plot ever holds a handle |

### Code that is NOT gated on paging but only behaves with paging ON

Ranked by confidence. "Re-runs ON" = what re-executes it under paging; "Re-runs OFF" = what (if anything) does.

1. **`CvPlot::updateCenterUnit` gets a periodic driver only until a plot converges.** ON: re-run on every UNIT-bit
   page-in transition (`showRequiredGraphics` delta → `updateGraphics(UNIT)`, `Engine/CvPlot.cpp:490-491, 523-526`).
   OFF: the live paging loop re-requires ALL each frame, so a plot whose UNIT bit is not yet visible re-runs
   `updateGraphics(UNIT)` → `updateCenterUnit` EVERY frame until it renders; once the plot is fully shown the delta
   is NONE and there is NO periodic driver — only the event callers in §3
   (`setXY`/`addUnit`/`removeUnit`/`enableCenterUnitRecalc`/fog flips/`setRevealed`/`CvPlot::read`) and the
   EXE-cadence `CvMapExternal::updateCenterUnit`. ⚑ Confidence: HIGH (code).
2. **The pre-init hole is CLOSED — paging-off is no longer a latched one-shot.** `showRequiredGraphics` now gates on
   `isInViewport() && GC.IsGraphicsInitialized()` (488), so a pass that runs before `SetGraphicsInitialized` marks
   NOTHING visible and defers, instead of setting `m_visibleGraphics=ALL` while the updaters early-returned and
   leaving `(required ^ visible)=NONE` forever. The live per-frame loop (`UI/CvPlotPaging.cpp:301-322`) re-requires
   ALL every frame, so the build re-runs once graphics come up and the plot renders — no latch, no lost sweep.
   `SetGraphicsInitialized` still has no DLL caller (`Defines/CvGlobals.cpp:3240`; `Defines/CvGlobals.h:2049-2052`),
   so its exact timing is EXE ordering (§8), but it no longer decides whether the map ever renders.
   ⚑ Confidence: HIGH (code).
3. **`bNeedsRealEntity`'s centre clause reads `m_pCenterUnit`, which paging NULLs.** With paging ON, `updateCenterUnit`'s
   early return (9979) makes `getCenterUnit(false)==this` false for every unit on an out-of-radius plot, so
   `reloadEntity` from `setXY`/head swaps/`setActivePlayer` gives such units the dummy unless active-player-owned
   (`Engine/CvUnit.cpp:323-325`). OFF: the clause tracks graphics-init + viewport only. ⚑ Confidence: HIGH.
4. **`setRevealed`'s rebuild is deferred ON and synchronous OFF, and leaves `m_visibleGraphics=NONE` on a plot
   with live scene objects.** OFF→ON toggle hazard: `getNonRequiredGraphicsMask = (required ^ visible) & visible`
   is NONE for such a plot (588-591), so the evictor can never select it. ⚑ Confidence: HIGH (code-derived, not
   runtime-observed).
5. **`updateFeatureSymbol` / `updateRiverSymbol` have NO periodic caller in either mode.** Outside `updateGraphics`
   they run only from map-edit setters (`setPlotType` 6990-6993, `setFeatureType` 7162, `setNOfRiver` 6136,
   `setWOfRiver` 6175) and the viewport spoof timer (`UI/CvViewport.cpp:308`). OFF: once at the sweep + each
   active-team `setRevealed`. ⚑ Confidence: HIGH.
6. **Flag symbols repaint only when dirty AND `updateFlagSymbolIfVisible` returns true** (`Engine/CvMap.cpp:511-527`;
   `Engine/CvPlot.cpp:9853-9860`). ON: a paged-out plot keeps its dirty bit until page-in re-runs `updateFlagSymbol`
   via `updateGraphics` (526). OFF: while a plot's UNIT delta is non-empty the live loop re-runs `updateFlagSymbol`
   via `updateGraphics(UNIT)` each frame; once the plot has converged, repaint depends on `setFlagDirty` and the EXE
   calling `CvMapExternal::updateFlagSymbols`, plus the forced whole-map repaint at the end of a `BRING_INTO_VIEW`
   (`UI/CvViewport.cpp:375`, §3). ⚑ Confidence: HIGH that the DLL has no periodic OFF-mode driver AFTER convergence;
   EXE cadence unverified.
7. **`m_bGraphicalPaging` is indeterminate between `new cvInternalGlobals()` and the first `refreshOptionsBUG`**
   (`Defines/CvGlobals.cpp:104-135, 3295-3302, 3327-3329`). Any `isGraphicsVisible`/`updateGraphics`/`UpdatePaging`
   read before `BugInit.init` → `setIsBug` reads garbage. Whether such a read occurs is EXE/Python ordering.
   ⚑ Confidence: MEDIUM (window exists; reachability unverified).
8. **`setLayoutDirty` silently un-dirties under paging.** `updatePlotBuilder` gates on `isGraphicsVisible(FEATURE)`
   (11531-11540), so with paging ON a `setLayoutDirty(true)` on a plot whose FEATURE bit is out of
   `PAGE_IN_DIST_FEATURES` (XML 15) resets `m_bPlotLayoutDirty=false`. The reverse case of this list (works OFF,
   drops ON). ⚑ Confidence: HIGH.
9. **No path re-places an already-set-up node in either mode.** `updateCenterUnit` re-runs only on the page-in
   DELTA under paging (490-491) and each frame until convergence with paging off (items 1-2); either way
   `reloadEntity(true)` on a unit whose latch is set is a `kept` that positions nothing (`Engine/CvUnit.cpp:370-373`).
   A node's position is established once at setup (`SetPosition(plot())`) and thereafter by the ordinary
   move path — never re-placed by paging. ⚑ Confidence: HIGH.
10. **Define-name mismatch:** the code reads `PAGING_FRAME_TIME_MS` (`UI/CvPlotPaging.cpp:242`); the XML authors
    `MAX_PAGING_FRAME_TIME_MS` (`Assets/XML/GlobalDefines.xml:67-68`, itself commented "unused"). The code default
    (100) is what runs. ⚑ Confidence: HIGH.
11. **`CvPlot::updateSymbols` is paging-dependent the same way as item 5.** It gates on
    `isGraphicsVisible(SYMBOLS)` (`Engine/CvPlot.cpp:1418-1423`) and, outside `updateGraphics`, runs only from
    `setOwner` (6677), `setRevealedImprovementType` (9473), `setRevealedRouteType` (9519) and the whole-map
    `CvMap::updateSymbols` (`Engine/CvMap.cpp:573-581`; callers `toggleDebugMode` `Engine/CvGame.cpp:4665`,
    `setActivePlayer` 4859, `afterSwitch` `Engine/CvMap.cpp:1581`). `PAGE_IN_DIST_SYMBOLS=999`
    (`Assets/XML/GlobalDefines.xml:72-73`) makes the ON gate whole-map in practice. ⚑ Confidence: HIGH.
12. **`bForce` bypasses the FEATURE gate but not the RIVER gate — and nobody uses the bypass.**
    `updateFeatureSymbol(bForce)` returns unless `isGraphicsVisible(FEATURE) || bForce` (9655); its three callers
    (506, 6990, 7162) all pass the default `false`. `updateRiverSymbol(bForce=true)` is passed at `setNOfRiver`
    (6136), `setWOfRiver` (6175) and the viewport spoof timer (`UI/CvViewport.cpp:308`), but the gate at 9746 has no
    `bForce` term, so those calls are still refused on a paged-out / pre-init plot. ⚑ Confidence: HIGH.
13. **`CvCity::getVisibleBuildings` answers "no buildings" for a paged-out city.** It returns
    `iChosenNumGenerics=0` with an empty list unless `plot()->isGraphicsVisible(CITY)` (`Engine/CvCity.cpp:13246-13250`);
    ON that is the `PAGE_IN_DIST_CITY` radius, OFF it is graphics-init + viewport. ⚑ Confidence: HIGH.
14. **`pageGraphicsOut` is reached by two different conditions.** `hideGraphics` calls it only when the mask has
    dropped to NONE (`Engine/CvPlot.cpp:581-584`, the paged-out plot leaves the table); the OFF sweep's
    `disableGraphicsPaging` calls it unconditionally after requiring ALL (609) — so with paging OFF no plot holds a
    handle, and a later `hideGraphics` (from `setRevealed`/`uninit`/`destroyGraphics`) releases nothing.
    ⚑ Confidence: HIGH.

## 5. Timelines

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

## 6. The working model, and where the tree differs from it

⚠ **This is a MODEL, not a ruling.** It is the owner's stated expectation of how rendering works, offered as a
lens for reading the tree — not a design the tree is required to satisfy, and not licence to change the code
toward it. Nothing below is a defect by virtue of appearing here; each item is a place where the tree behaves
differently from the model, recorded so the difference is known rather than assumed away.

The model, paging OFF (owner, verbatim): *"every unit that is on top of its stack and not in fog of war should
have its graphics rendered on the plot it stands on; everything else should NOT be rendered until required, and
then rendered ON the plot, never by moving graphics from the centre plot to that plot."*

Where the tree differs, DLL-side (whether the EXE draws a non-centre real node is §8):

1. **Non-centre active-player units get REAL nodes.** The `|| getOwner() == activePlayer` clause in
   `bNeedsRealEntity` (`Engine/CvUnit.cpp:310-317`) builds a node for every active-player unit on a fog-visible
   plot, top of stack or not (on a populated load most units still hold the dummy — only centre + active-player
   units get real nodes, the split readable off `[GFX] entity`). The reason: the EXE dereferences a
   SELECTED unit's entity at offset `0x24`, and `selectGroup`(alt/ctrl)/`SELECT_HEALTHY`/`nextPlotUnit`/
   `cycleSelectionGroups`/`FLYOUT_SELECT_UNIT` insert non-centre units with no entity check
   (`Engine/CvGame.cpp:2812-2851`; `Engine/CvGameInterface.cpp:1000-1020, 1695-1727`); removing the clause was
   measured to crash select-all with `faultAddr=0x24`.
2. **Nodes are built against a laid-out plot, not flown in.** Paging-off now runs `updateGraphics` per plot each
   frame — the same path as paging-on (§4) — and `showRequiredGraphics` defers any pre-init pass (488), so a
   unit's scene node is created and set up against a plot the landscape has already built. `setupGraphical`
   places the node with `SetPosition(plot())` and nothing else (`Engine/CvUnit.cpp:1089-1098`) — never
   `QueueMove`/`ExecuteMove`, whose shown-movement semantics manufacture the run-from-mid-map-to-plot the model
   forbids — and ordinary movement is the `setXY` graphics block. `groupMove` additionally issues a timed
   `ExecuteMove` to a member that could not move (`Engine/CvSelectionGroup.cpp:3638-3639`) and to every member at
   the end of the move (3677).
3. **A unit that stops being centre / drops into fog is never un-rendered by the DLL.** `updateCenterUnit`
   reloads only the NEW centre unit (`Engine/CvPlot.cpp:10000-10015`); a fog-out only NULLs `m_pCenterUnit`; the
   DLL never calls `setVisible` on a unit entity. A real node is downgraded only by `setXY`'s fog-edge test
   (14054-14061), a group head swap (`Engine/CvSelectionGroup.cpp:4826-4833, 4866-4877`), or a bulk
   `reloadEntity()` sweep.
4. **A required unit that already holds a node is never (re)positioned.** `reloadEntity` → `kept` skips
   `setupGraphical` when the latch is set (`Engine/CvUnit.cpp:370-373`); a fog→visible flip of such a
   unit issues no positioning call at all.
5. **With `ENABLE_DYNAMIC_UNIT_ENTITIES=0` every unit holds a real node unconditionally** — fog, viewport and
   stack position ignored (`Engine/CvUnit.cpp:208-210, 321`).
6. **The ctor builds a node before the unit has a plot** — every unit in non-dummy mode, and the two bootstrap
   entities (`g_dummyUnit`'s own node and the shared `g_dummyEntity`) in dynamic mode. The ctor pre-assigns
   `m_iX/m_iY = INVALID_PLOT_COORD` before `createUnitEntity`, so the window is deterministic (`plot()` is NULL)
   rather than garbage — but the node is still created positionless and depends on the later
   `setupGraphical` `SetPosition` for placement (`Engine/CvUnit.cpp:182-240`).
7. **`BRING_INTO_VIEW` performs a whole-map rebuild even with viewports off.** `m_mode` is INITIALIZED from
   `CvMap::reset` (`Engine/CvMap.cpp:293-298`), so alt-numpad pans, ctrl-alt-C and the `LOADING` state
   (`UI/CvViewport.cpp:126-210, 421-437`) reach `beforeSwitch`/`afterSwitch` (348-361): every real unit node
   destroyed and recreated, `RebuildAllPlots`, `setLayoutDirty` on every plot, `createUnitEntity`+`setupGraphical`
   for every non-dummy unit regardless of stack-top or fog (`Engine/CvMap.cpp:1413-1596`).
8. **A centre-unit change mid-`groupMove` runs `setupGraphical` inside the walk window.**
   `enableCenterUnitRecalc(true)` (3668-3669) runs `updateCenterUnit` → `reloadEntity(true)` → `setupGraphical`'s
   `SetPosition(plot())` before the group's `ExecuteMove` loop, and `setupGraphical` has no `isMidMove()`
   guard (`Engine/CvUnit.cpp:1089-1098`).
9. **Rendering-when-required is event-driven once a plot has converged** (§4 item 1): after the live paging-off
   loop has fully shown a plot, a stack change without a `setXY`/`addUnit`/`removeUnit`/fog flip on it — e.g. the
   centre unit dies via a path that does not `removeUnit(bUpdate)`, or a `toggleDebugMode` that changes
   `isActiveVisible(true)` without an `updateCenterUnit` sweep (`Engine/CvGame.cpp:4657-4667`) — is re-presented
   only when the EXE calls `CvMapExternal::updateCenterUnit`.

## 7. Doc contradictions (fix-the-doc items)

| Doc cite | Claim | Code cite | Truth |
|---|---|---|---|
| `docs/reference/memory-footprint.md:125-127` | a real entity only for ON-SCREEN units; counted under `[PERF/entity]` | `Engine/CvUnit.cpp:310-317`; `Engine/CvPlot.cpp:4948-4952`; `UI/CvGraphicsTrace.cpp:169` | The criterion is `isActiveVisible(false)` (fog), not screen; the count is the `[GFX] entity` line at level 4; no `[PERF/entity]` tag exists. |
| `Assets/XML/GlobalDefines.xml:62-69` (comment) | `MAX_PAGING_FRAME_TIME_MS` is unused | `UI/CvPlotPaging.cpp:242` | The mechanism is live under `PAGING_FRAME_TIME_MS`, which is not authored; the XML key is inert. |
| `docs/plans/parked/multimap-zone-rework.md:14, 49` | proactive eviction shipped via `PAGING_RESIDENT_SOFT_CAP`; zone paging can "reuse the shipped proactive eviction" | `UI/CvPlotPaging.cpp:256-263`; `Assets/XML/GlobalDefines.xml:47-48` | The branch exists but the authored cap (3,000,000) is compared to a PLOT count; it can never trigger (`docs/reference/memory-footprint.md` states the same). Eviction is `NeedToFreeMemory`-only. |
| `docs/plans/parked/turn-time-optimization.md:612` | `isActiveVisible` is a single count read | `Engine/CvPlot.cpp:4932-4944` | It also ORs `getStolenVisibilityCount`. |
| `docs/specs/json.md:1122-1125` | `getArtInfo(iIndex, eEra, eStyle)` resolves a civilization art-style override first | `Infos/CvUnitInfo.cpp:207-227` | `eStyle` is unused; the function walks era bands then falls back to the unit's art tag. |
| `Engine/CvGame.cpp:4741, 2439-2441` (comments) | `setFinalInitialized` fires for a NEW GAME ONLY and marks final init | `Engine/CvGame.cpp:4741-4746, 472, 2452-2454` | Its body only prints; `m_bFinalInitialized` is set by `onFinalInitialized` from the first `CvGame::update` on BOTH paths. |
| `Tools/CvHttpServer.cpp:8-9`; `AI/BetterBTSAI.cpp:52` (comments) | `/computed/perf` serves the memory gauge | `Tools/CvHttpServer.cpp:409-419` | No such route exists in the route table; `docs/spine.md:949-952` is the correct side. |

## 7b. The run-from-origin reconciliation — MEASURED engine behaviour (owner mechanism, runtime-verified)

**The engine renders a graphics assignment it must reconcile as a VISIBLE MOVE from the world origin (= the
map centre: `plotXToPointX`'s `-fWidth/2` term) to the unit's plot, at the node's first presentation — and NO
DLL input prevents it.** Each statement below is runtime-verified on the `[GFX]` scene trace, not inferred:

- The reconciliation fires at the plot's centre-unit assignment: every `notifyEntity` from a stack
  manipulation is followed within ~30-80ms by a `centerUnit` flip and by engine reads of the new centre's
  state; a queued walk survives ~23 minutes un-drained until such a flip presents it.
- The engine resolves `CvUnit::canMove` / `hasMoved` / `isWaiting` by mangled name (present in the EXE image)
  and reads them at setup and at every centre flip — but the walk is NOT gated on them: a full session
  answering `canMove=false` for every unit still jogged every fresh centre node in.
- Position inputs are accepted and ignored for a never-presented node: `SetPosition` / `updatePosition` /
  `QueueMove`+`ExecuteMove(0)` in every ordering (before and after `setup()`), with coordinates and the plot
  transform verified live at call time, leave the first presentation walking from origin. A unit's own MOVE
  path (`setXY` → `QueueMove`/timed `ExecuteMove`) is the one sequence that leaves a node position-synced.
- Node creation timing does not matter (pre-init ctor births under `ENABLE_DYNAMIC_UNIT_ENTITIES=0`, post-init
  sweeps, paced or flooded schedules — all jog), churn does not help (C2C's destroy/recreate-per-reload,
  restored verbatim, MULTIPLIED runners in both paging modes: every fresh node's first presentation walks),
  and both paging modes exhibit it (`paging=1` sessions traced).
- Load-time entity creation is a single-frame burst from the `CvPlayer::setupGraphical` sweep (active-player
  clause), not from the paging walk; `beforeSwitch`/`afterSwitch` does NOT run on a load (zero `destroyEntity`
  lines across load traces — closes the former open question).

The structural invariant the ctor now honours (no `createUnitEntity` before the unit stands on a plot; lazy
dummy bootstrap in `reloadEntity`) removes the plotless first assignment but does not remove the jog. The
un-eliminated remainder is engine-internal: the node's spawn position and the animate-the-delta behaviour.

## 8. Open questions (not decidable from the DLL)

- **Whether `CvMap::afterSwitch` runs on a load.** The `LOADING` → `bringIntoView(force)` → `BRING_INTO_VIEW` →
  `afterSwitch` chain exists with `m_mode` INITIALIZED (`UI/CvViewport.cpp:421-437, 348-361`; `Engine/CvMap.cpp:293-298`),
  but whether it fires on a load is not decidable from the tree. Candidates for it NOT firing: `CvMapExternal::read`
  (virtual, not `DllExport`, no DLL caller) not invoked; no selectable unit at that frame.
- **Whether the EXE draws a REAL node that is not the plot's `getCenterUnit()`** — the active-player clause builds
  them; the renderer's use of `getCenterUnit()` is inside the EXE.
- **Whether the EXE keeps drawing a real node whose plot fell into fog**, and what it does with `isLayoutDirty` /
  `isLayoutStateDifferent` / `setLayoutStateToCurrent` (all `DllExport`, no DLL reader).
- **What the EXE reads from a `CvUnit` at `createUnitEntity` time** (the ctor now guarantees
  `m_iX/m_iY = INVALID_PLOT_COORD` there, so a position read gets a deterministic no-plot, but what the EXE does
  with the rest of the not-yet-`reset` unit is unknown), and whether `createUnitEntity` re-binds a dangling
  `m_pEntity` after `beforeSwitch`'s `destroyEntity` (which does not null it).
- **Cadence of the EXE's calls to `CvMapExternal::updateCenterUnit` / `updateFlagSymbols` / `setupGraphical`** — the
  only possible periodic re-run sources for centre-unit and flag repaint with paging OFF.
- **What the EXE dereferences at entity offset `0x24` for a selected unit**, and whether at insert time or later;
  the only DLL evidence is `CvGame::selectUnit`'s asserts (`Engine/CvGame.cpp:2771, 2798`) and the measured crash.
- **Whether `getGroup()` can be NULL when `setupGraphical` runs** (`Engine/CvUnit.cpp:1077`, unguarded deref).
- **The numeric `gDLL->getMillisecsPerTurn()`** (EXE virtual), hence the real duration of `groupMove`'s
  `ExecuteMove` (`MISSION_MOVE_TO iTime=4`, `Assets/XML/Units/CIV4MissionInfos.xml:7-11`).
- **Whether the billw multi-unit refresh concern is still real.** `setupGraphical` no longer issues
  `ExecuteMove(0,false)` (its shown-movement semantics manufactured the run-from-origin; §2, §6 item 2). billw's
  2019 observation — without it, only one figure of a stack showed and the rest appeared ~10s later — was made
  against graphics paging ON (verified in the C2C archive: commit `d9e623416`, "fix(Paging V2): multi-unit
  graphics work correctly"; the pre-billw body was `setup()` only, so under the dummy system no placement call
  ever existed in this path before the `SetPosition` above), whose page-in churn retries setup continually, and
  may itself have been a symptom of the then-unplaced nodes. His test block shows `SetPosition(plot())` among
  the candidates that did NOT cure his refresh symptom under paging ON — refresh and placement are different
  questions. A late-figures
  symptom seen now, under paging OFF and `SetPosition` placement, is a NEW observation to diagnose fresh — if a
  refresh is genuinely needed it must be a non-movement mechanism, never the `ExecuteMove` back. The one
  remaining possibly-empty-queue caller is `groupMove`'s could-not-move member
  (`Engine/CvSelectionGroup.cpp:3638-3639`).

## 9. The Firaxis reference contract (vanilla BTS source, `<BTS install>/CvGameCoreDLL/`)

The original Firaxis DLL — the code the closed EXE ships with and renders correctly against — defines the
contract. Traced in full (call census + verbatim bodies); the invariants, cited to vanilla source:

- **The DLL positions a unit node exactly twice in its life**: once via the `SetPosition` arm of the `setXY`
  called from `init` (the entity already exists — ctor-created — and `setup()` runs AFTER, `CvUnit.cpp:107-111`),
  and thereafter only via `QueueMove`+`ExecuteMove` pairs during actual movement. On a LOAD the DLL issues **no
  position at all**: the EXE calls the DllExport `CvPlayer::setupGraphical` → per-unit `setupGraphical()` →
  bare `setup()`, and **the engine's `setup()` reads the unit's coordinates itself and places the node**
  (`CvPlayer.cpp:738-759`; `CvUnit::read` is pure deserialization, `CvUnit.cpp:11254`).
- **`setupGraphical` is `setup()` + the intercept `airCircle` and NOTHING else** (`CvUnit.cpp:409-422`). No
  `ExecuteMove`, no position verb. `ExecuteMove` exists at exactly two sites in the whole vanilla tree:
  `groupMove` (flushing queued moves, `CvSelectionGroup.cpp:3092,3105`) and `updateCombat`
  (`ExecuteMove(0.5f,true)` before `AddMission`, `CvUnit.cpp:1375`). An empty-queue `ExecuteMove` at setup —
  the billw C2C refresh — is a call shape the reference does not contain.
- **A centre-unit change touches no entity, ever**: `setCenterUnit` = pointer + `updateMinimapColor` +
  `setFlagDirty` + `setInfoBarDirty` (`CvPlot.cpp:7773-7791`). The flag is the centre unit's derivative,
  rebuilt by destroy/`create(player)`/`setPlot(flag, plot)`/`updateUnitInfo` under the EXE-driven dirty sweep
  (`CvPlot.cpp:7650-7742`; `CvMap::updateFlagSymbols` has no in-DLL caller).
- **Never called by vanilla on a unit entity**: `addEntity` (declared, zero callers), `updatePosition` (used
  ONLY on feature/route/river symbols), `MoveTo`, `PlayAnimation`, `StopAnimation`, `setVisible` (one call,
  on a city). A fix reaching for any of these is outside the contract.
- **Selection makes zero entity calls** except `addUnit`'s `NotifyEntity(MISSION_MULTI_SELECT)` fan on the
  ownerless (NO_PLAYER) UI selection group (`CvSelectionGroup.cpp:3792-3847`); `selectUnit`/`selectGroup`/
  `cycleSelectionGroups` only mutate the selection list.
- **Vanilla `reloadEntity` exists** (destroy → create → `setupGraphical`, `CvUnit.cpp:68-82`) and is called
  ONLY for art rebuilds (warlord attach, `setLeaderUnitType`) — never from centre-unit changes; position
  survives a rebuild because the engine's `setup()` re-reads the unit's plot.
- ⚠ **The dummy-entity system (C2C) is structurally outside this contract**: it creates real entities after
  the two placement moments the contract provides (birth `setXY`; load-time pre-existing entities placed by
  the engine's init/setup). Every S2S "run in from world origin" observation traces to presentations of nodes
  introduced outside those two moments — see §7b for the measured behaviour.

## 10. See also

- [superseded-ideas.md #42](../architecture/superseded-ideas.md) — why nothing can be shared or reused across
  the boundary: the 26 + 71 virtuals carry no instancing primitive, so residency (paging/viewports) and art
  payload are the only two dials.
- [memory-footprint.md](memory-footprint.md) — `shouldHaveGraphics() = IsGraphicsInitialized() && isInViewport()`
  with the `isRevealed` clause commented out (`Engine/CvPlot.cpp:612-615`), the deliberate `PAGE_IN_DIST_* = 999`
  for the Low-cost components, and the inert soft cap (§4).
- [special-systems.md](special-systems.md#the-great-wall-render--compiled-out-on-purpose) — the `THE_GREAT_WALL`
  off-switch on `beforeSwitch`/`afterSwitch` (§2).
- [spine.md](../spine.md) — the `[GFX]` domain's tiering and the measured volume of the per-pass `centerUnit`
  line (§3).
- [multimap-zone-rework.md](../plans/parked/multimap-zone-rework.md) — the parked zone plan keeps `CvViewport` +
  `CvPlotPaging` and would replace `UpdatePaging`'s distance marking with zone membership; its eviction premise is
  §7.
