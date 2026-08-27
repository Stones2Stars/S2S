# ⚖ THE SPATIAL CARVE-OUT — a PATH is not a maintained sum, so it is a LEGITIMATE cache

> Part of the **[cascade](../cascade.md)** spec.

> A pathfinding cache is warranted, because it is the most expensive and at the same time unmaintai

Everything above says derived state is a MAINTAINED SUM and a cache is a defect. **SPATIAL results are the one
class that rule does not reach, and the reason is structural rather than an exemption granted to them:**

- **A path is not a Σ over sources, so there is no delta to apply.** It moves NON-LOCALLY — one terrain change
  or one new route re-routes paths that do not touch the changed plot at all — so no fact can name the set of
  results it invalidated. [the maintained sum](05-three-planes.md#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed) needs `Δ(v × c) = v × Δc`; a
  shortest path has no such identity.
- **And it is the most expensive thing the engine does**, because computing one *requires* scanning plots. That
  is the definition of the operation, not an implementation that could be improved into a fetch.

⇒ **So a pathfinding cache is WANTED, and deleting one is a regression.** `PATHFINDING_CACHE` /
`PATHFINDING_VALIDITY_CACHE` are legitimate; so is `CvPlot`'s path-validity memo and the culture-distance
cache — `cultureDistance`, culture spread and the property propagators are all SPATIAL permanent carve-outs,
for exactly this reason.

⛔ **What the carve-out does NOT license.** It is scoped to results that are genuinely spatial:
- **not** an ordinary derived value that merely feels expensive — if a fact can name what moved it, it is a
  maintained sum and the cache is the defect ([a staleness flag is the fossil of a missing emit](03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up));
- **not** a read-side `ensure()` — a spatial cache is filled at its own INVALIDATION point, never lazily on a
  read that is specified as a bare fetch (the tombstoned protocol, [superseded-ideas](../architecture/superseded-ideas.md) #14);
- **not** freedom from invalidation. Being unmaintainable-by-delta means it is CLEARED, wholesale, by the
  events that can move it (terrain, route, ownership) — a spatial cache still has to be wrong for nobody.

## ⛔ `CvDerivedCache` IS REPLACED BY `ContextDict` — VIRTUALLY EVERYWHERE

> *"`CvDerivedCache` should be replaced by `ContextDict` virtually everywhere needed, and we just need to start
> taking one cluster at a time with event wiring."*

**`CvDerivedCache` (`Sources/Infrastructure/CvDerivedCache.h`) no longer exists.** It was a templated
mark→recompute value-holder — a `markDirty` that triggered a recompute over the owner's current state, exactly
the calculation a fact was supposed to make unnecessary. Every tenant converted, one cluster (an entity's facts
plus the store they feed) at a time: its events re-cut to name their happenings
([spine.md](../spine.md) § A FACT NAMES THE HAPPENING), its store re-expressed as a keyed
accumulator ([every derived store is a keyed accumulator](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta)) or a channel slot in
`CvCascadePackage` (§ THE MAINTAINED SUM, above), and its recompute deleted in the same change. The legacy
`CvCity` hand-rolled staleness caches (`m_aiCommerceRate`, `m_aiBuildingCommerce100`, squirrelBanana) went the
same way.

⛔ **It is not reintroduced, and not reached for "just this once."** A recompute is only ever necessary when
inputs arrive UNANNOUNCED, which a saturated emit surface makes impossible
([a staleness flag is the fossil of a missing emit](03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up)) — a surviving tenant would be a MISSING EMIT wearing a
component, the same shape a staleness flag wears one level out. The boundary between the two replacements —
keyed count vs summed magnitude — is § EVERY DERIVED STORE IS ONE SHAPE, above.

---

