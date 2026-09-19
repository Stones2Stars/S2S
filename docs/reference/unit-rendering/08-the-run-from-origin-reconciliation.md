# 7b. The run-from-origin reconciliation — MEASURED engine behaviour

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

**The EXE spawns a unit's scene node at the WORLD ORIGIN — the map centre (`plotXToPointX`'s `-fWidth/2` term) —
and a node keeps believing it is there until it is TOLD a location.** Two things therefore decide whether the
player sees a walk in from mid-map, and the defect has been found in each of them separately:

- **WHETHER the node was ever given a location.** `SetPosition` states where the node IS and animates nothing;
  it is the correct call at every placement moment, and a node that has had one is immune to everything below.
- **WHAT the DLL says about a node that has NOT had one.** ⛔ **ANY statement is reconciled against the origin
  the node still believes in — the move family holds no special status here.** `QueueMove`/`ExecuteMove` carry
  shown-movement semantics and are the loudest case, but a bare `NotifyEntity` — which passes no plot at all —
  produces the same walk, because the engine animates the unit into the state it was told about FROM where it
  thinks the unit stands.

⛔ **So the defect class is A NODE SPOKEN TO BEFORE IT WAS PLACED**, and it has two halves, both DLL-side and
both preventable:

1. **A MOVE call standing in for a PLACE call.** Prevented by the census in
   [§9](10-the-firaxis-reference-contract.md): `SetPosition` at the two placement moments;
   `QueueMove`/`ExecuteMove` only for real movement (`CvSelectionGroup::groupMove`, `CvUnit::updateCombat`) and
   nowhere else.
2. **A node that NOTHING ever placed.** `setupGraphical` is the only site that places a node during play, and it
   is reached only from `reloadEntity` and `CvMap::afterSwitch` ([§2](02-the-entity-lifecycle.md)) — so any path
   that speaks to the engine WITHOUT passing through one of those two is speaking to whatever state the node was
   left in. `CvUnit::ensureGraphicalPlacement` (`Engine/CvUnit.cpp:362`) is the guarantee: it places a node that
   has never been placed and does nothing to one that has, and it runs before the notification in
   `CvUnit::NotifyEntity` (1675) as well as at the end of `reloadEntity` (359).

⛔ **THE SELECTED UNIT WAS THE HOLE, AND IT IS THE ONE TO KEEP IN MIND, BECAUSE THE PLAYER ONLY EVER OPERATES ON
A SELECTED UNIT.** `reloadEntity` excludes a selected unit — correctly, because its node must never be destroyed
and rebuilt underneath it ([AGENTS.md](../../../AGENTS.md)) — and that exclusion used to swallow the placement
call as well, which the reason never covered. A selected unit's node could then never be placed, and MERGING,
UPGRADING, FORTIFYING and AWAKENING are all performed on the selected unit. Placement therefore sits OUTSIDE the
exclusion (`Engine/CvUnit.cpp:359`); only destroy/create is inside it.

⚑ **The symptoms it produces, so they are recognised rather than re-diagnosed:** a NEWLY CREATED unit walks in
from mid-map (its node is built and set up in one go at birth); a unit that held the shared dummy walks in the
first time it needs a real node; and a unit whose node was never placed walks in on a STANCE CHANGE — fortify and
awaken both reach `CvSelectionGroup::setActivityType`, which notifies every unit in the group and passes no plot.
A unit already holding a placed node never repeats any of it, because the `bGraphicsSetup` latch is the record
that a location was given ([§2](02-the-entity-lifecycle.md)).

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
