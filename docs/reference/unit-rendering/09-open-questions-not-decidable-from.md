# 8. Open questions (not decidable from the DLL)

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

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

