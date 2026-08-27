# 3. The plot side — choosing the centre unit

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

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
   77,245 lines per 8 MB of `Graphics.log` ([spine.md](../../spine.md), the re-tier-to-4 rule).

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

