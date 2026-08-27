# 6. The working model, and where the tree differs from it

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

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

