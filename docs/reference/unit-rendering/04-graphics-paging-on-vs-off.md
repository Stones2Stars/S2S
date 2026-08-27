# 4. Graphics paging ON vs OFF

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

> **⚖ THE DESIGN INTENT OF "PAGING OFF" IS PER-TYPE: UNITS stay dynamically managed — fog of war
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
| Page-OUT | ONLY `EvictGraphics` → `hideNonRequiredGraphics` → `hideGraphics(mask)` (`UI/CvPlotPaging.cpp:135-146`; `Engine/CvPlot.cpp:530-534`) under the eviction condition. Un-requiring a plot HIDES NOTHING by itself (447-458). With XML `PAGING_RESIDENT_SOFT_CAP=3000000` against a PLOT count, only `NeedToFreeMemory()` can trigger it: working set > `min(MAX_DESIRED_MEMORY_USED × 1024 ≈ 3.58 GB, ullTotalPhys − OS_MEMORY_ALLOWANCE)` (`Assets/XML/GlobalDefines.xml:40-48`; `UI/CvPlotPaging.cpp:82-131`, cap clause 116-122). [memory-footprint.md](../memory-footprint.md) records the same inert cap and reads the 3.58 GB as above a 32-bit process's reach — the threshold is a DLL fact, its reachability an EXE (address-space) property | never |
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

