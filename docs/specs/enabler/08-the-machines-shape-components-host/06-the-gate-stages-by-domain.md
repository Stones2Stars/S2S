# The gate stages, by domain

> Part of the **[08-the-machines-shape-components-host](../08-the-machines-shape-components-host.md)** spec.

The gate verdict is a per-id flag (`setGateFailed`): a failed gate flips a tree member LISTED → GREYED, membership
untouched. **A domain whose gate stage has not landed never sets the flag, so its members stay LISTED** — the
enable-side over-offer, which is a VISIBLE defect to fix, never a reason to fall back to legacy.

Every domain carries all three stages — membership, the `requires` gate, and an `allowed` cap — with the cap
taking its domain's own shape:

| domain | what its `allowed` cap bounds |
|---|---|
| techs | world-unique founder techs |
| buildings | world/team/empire self-caps + the per-city wonder-CATEGORY cap (§4) |
| units | world lifetime-created; empire era-scaled national cap |
| projects · civics · processes · builds | the plain per-scope cap |
| promotions | none — and the gate is on demand, not a maintained flag (§7.1 carve-out) |

**Promotions are the exception to the over-offer:** they set no gate flag, but `requires` + the unit-state
applicability leg (unitcombat QUALIFIED/DISQUALIFIED, game options, promotion-line prereq tech, and the runtime
spy/pillage/commander/commodore/blend + intercept/evasion/XP caps) are enforced ON DEMAND at level-up, so the
promotion offer is not over-inclusive.

