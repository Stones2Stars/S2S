# 7b. The run-from-origin reconciliation — MEASURED engine behaviour

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

**The EXE spawns a unit's scene node at the WORLD ORIGIN — the map centre (`plotXToPointX`'s `-fWidth/2` term) —
and a node keeps believing it is there until something tells it otherwise.** What decides whether the player sees
a walk in from mid-map is therefore WHICH call the DLL issues on a node that has never been presented:

- **A move-family call RECONCILES the delta as a shown walk.** `QueueMove` stages stepped plots and `ExecuteMove`
  shows the movement — `groupMove`'s own use is the contract — so either one on a not-yet-presented node
  animates origin → plot on a unit that never moved.
- **`SetPosition` states where the node IS and animates nothing.** It is the correct call at every placement
  moment, and it cures the run-in.

⛔ **So the defect class is a MOVE call standing in for a PLACE call**, and it is DLL-side and preventable. The
census that keeps it prevented is in [§9](10-the-firaxis-reference-contract.md): `SetPosition` at the two
placement moments; `QueueMove`/`ExecuteMove` only for real movement (`CvSelectionGroup::groupMove`,
`CvUnit::updateCombat`) and nowhere else.

⚑ **The two symptoms it produces, so they are recognised rather than re-diagnosed:** every NEWLY CREATED unit
walks in from mid-map (its node is built and set up in one go at birth), and a unit that held the shared dummy
walks in the first time it needs a real node — classically a fortified stack member becoming the centre/selected
unit as it starts to move. A unit already holding a set-up real node never repeats it, because the
`bGraphicsSetup` latch skips `setupGraphical` entirely ([§2](02-the-entity-lifecycle.md)).

⚠ **Under the dummy-entity system real nodes are created LATE, which is what makes this reachable at all.**
Vanilla creates a node in the ctor and places it at birth `setXY`, so it has no un-presented-node window to get
wrong ([§9](10-the-firaxis-reference-contract.md)). Every S2S run-in traces to a node introduced outside those two
moments.

Other engine behaviour measured on the `[GFX]` scene trace, which stands independently of the above:

- The first presentation fires at the plot's centre-unit assignment: every `notifyEntity` from a stack
  manipulation is followed within ~30-80ms by a `centerUnit` flip and by engine reads of the new centre's state.
  A queued walk survives un-drained — ~23 minutes observed — until such a flip presents it.
- The engine resolves `CvUnit::canMove` / `hasMoved` / `isWaiting` by mangled name (present in the EXE image) and
  reads them at setup and at every centre flip, but the walk is not gated on them: a session answering
  `canMove=false` for every unit still jogged every fresh centre node in.
- Node creation TIMING does not matter (pre-init ctor births under `ENABLE_DYNAMIC_UNIT_ENTITIES=0`, post-init
  sweeps, paced or flooded schedules), and churn makes it worse rather than better: C2C's
  destroy/recreate-per-reload multiplied the runners in both paging modes, because each fresh node gets its own
  first presentation. Both paging modes exhibit it.
- Load-time entity creation is a single-frame burst from the `CvPlayer::setupGraphical` sweep (active-player
  clause), not from the paging walk; `beforeSwitch`/`afterSwitch` does NOT run on a load (zero `destroyEntity`
  lines across load traces).
