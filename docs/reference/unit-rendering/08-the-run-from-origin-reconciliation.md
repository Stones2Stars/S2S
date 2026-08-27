# 7b. The run-from-origin reconciliation — MEASURED engine behaviour

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

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

