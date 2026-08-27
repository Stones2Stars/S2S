# 6. The unit plane — a self-accumulator

> Part of the **[cascade](../cascade.md)** spec.

A `unit`-scope deposit is a **self-accumulator**: source == target. A unit's promotions and unit-combat class
deposit their stat changes onto the unit itself (the existing additive promotion stack), summed for O(1)
concatenation as each promotion is added — not a downward cascade.

**Host-from-occupants** effects — what a city gets *per unit stationed in it* (military happiness/anger) — are
**not** a bespoke host-family: they're an ordinary deposit on the source (the civic/trait), scaled by a
predicate-filtered unit count and targeting `cities`: `happiness.empire.cities.{unit: IS_MILITARY, flat: N}`
([json](../specs/json.md) §3.7). The **carrier↔cargo** behaviour splits across the two systems. The carry *ability* is a unit **skill** — whether
the unit may use the **load/unload** action is `is_cargo_vessel`, and the attack restriction it brings is
`defend_only` (both skills, [json](../specs/json.md) §8). The *amounts* live in the **`cargo`** modifier family (a unit
self-accumulator, set on the unit or a promotion), with two complementary members:

- **`cargo.space`** — how much the unit **carries** *and what*: `cargo.space.{unit: IS_<domain>, flat: N}` — a
  carrier is `cargo.space.{unit: IS_AIR, flat: N}` (*you can't transport a plane on a landing craft*); an
  unrestricted hold is just `cargo.space.flat`. (From legacy `iCargo` + `DomainCargo`.)
  > **⚖ THE RESTRICTION IS THE CARRIER'S AND GOVERNS ITS WHOLE HOLD — including capacity a PROMOTION grants
  >.** WHAT a carrier may take is a property of the carrier; HOW MUCH sums from every source. So a
  > restriction never binds only the entry it is written on: an ancient galley that carries civilians carries
  > civilians in the space `PROMOTION_TRANSPORT1` adds too, never a warrior in the promoted slot.
  > ⚑ **This is a real mechanic, not an edge case:** the whole ancient-navy transport line has **zero base
  > `iCargo`** and earns its hold by promotion (TRANSPORT1/2/3 on `UNITCOMBAT_WOODEN_SHIPS`), so the carrier
  > declaring WHAT and the promotion supplying HOW MUCH is the normal shape there, not an anomaly.
  >
  > ⚖ **A PROMOTION ADDS SPACE, NEVER PERMISSION — an INTENTIONAL divergence from legacy (owner: "we go with
  > yours, it's cleaner").** In the legacy game a transport promotion WIDENS the class carried: an unpromoted
  > galley takes a settler, a promoted one takes military. The ruled model does not reproduce that — WHAT is the
  > carrier's, fixed, and a promotion only ever changes HOW MUCH. ⛔ So do not "repair" this back by letting a
  > promotion author a wider qualifier: the behaviour change is chosen, and the reason is that a permission that
  > moves with promotions puts WHAT in two places and makes a carrier's rule unreadable from the carrier
  > ([validation.md](../specs/validation.md) intentional-model-change class; the spec leads, legacy behaviour is not
  > preserved for its own sake).
  > ⚠ Consequence: a carrier whose base capacity is 0 still has a restriction to state, and the §3.9 entry
  > grammar has no payload-less form for it — an open item for the json spec.
  ⚖ **The "what" is ALWAYS a TAG predicate — that is what tags are for.** The legacy restriction by
  `SPECIALUNIT_*` group (`SpecialCargo` / `SMNotSpecialCargo`) brings no new qualifier form with it: it authors as
  the same `{unit: IS_<TAG>}` shape as the domain case. ⚠ It does require the tag to exist AND to be
  DISCRIMINATING — several legacy groups are indistinguishable on the current tag set (people and troops are both
  merely `landUnit`; fighters and seaplanes both merely air/military), so converting one before its tag is minted
  silently WIDENS what the carrier accepts. Mint the tag first; that is ordinary open-registry authoring
  ([tags.md](../specs/tags.md)).
  ⚖ **Capacity has ONE home, and Size Matters DERIVES from it:** `smSpace` follows from how many units
  the carrier can hold, so it is never a second authored number ([json.md §9](../specs/json.md)).
- **`cargo.size`** — the unit's cargo **footprint** (room it occupies when loaded), **defaulting to 1** if unset.
  (SizeMatters extends cargo via `smSpace`/`volume`/`volumeModifier` — a separate rework.)

No bespoke host↔cargo family is needed. The full unit-stat family vocabulary
(`strength`/`withdrawal`/`firstStrike`/… ) is [json](../specs/json.md) §6; this is the largest surface and lands last.

> **Movement & range** are their own resolver subsystem, not ordinary downward families: `moveCost` is computed
> **per `(unit, edge)`** with a route `min`-override, double-move divisors, and a floor — it doesn't fit the
> "deposit DOWN → O(1) summed read" shape. **But the RESOLVER being bespoke does not make its INPUTS intrinsic
>: a plot substrate's base movement cost IS the `movement` family** — `movement.plot.flat` on the
> terrain / feature / route — and it composes with the cascading deltas (tech route changes, promotion move
> bonuses) in the ordinary way, as the §3.9 entry list. The route case shows it directly: the base cost is the
> bare number and a tech-gated change is a conditioned entry beside it, in one slot.
> ⚑ The distinction to hold: **the resolver reads the family and applies its own arithmetic** (min-override,
> divisors, floor). What was wrong was parking the base value in `identity`, which carries no effects
> ([json.md §7](../specs/json.md)) — a movement cost is plainly one.

### Specialist counts

- **`freeSpecialists:{<scope>:{any:N, SPECIALIST_X:M, …}}`** — granted specialists; `any` = an assignable-slot
  bucket, a typed entry is auto-assigned. Leaf is a count (a list when conditioned). ⚠ Here `any` is a **count key**
  (an untyped specialist slot), **NOT** the [json](../specs/json.md) §3.4 condition combinator.
  > **⛔ `any` IS AN AMOUNT, NOT A TARGET — and that decides whether the family works at all.** The untyped
  > bucket is N slots whose specialist type the ENGINE picks at placement (the two-part seam below), so it
  > carries no target: it decodes as the **memberless scope-wide amount**, exactly like any other magnitude.
  > ⚑ The consequence is structural rather than cosmetic. A deposit carrying a TARGET segment is excluded from
  > its scope's package by construction (only point-foldable entries fold), so registering `any` as a target
  > token strands the amount outside the package plane — no scope roll-up can answer it, and the only read left
  > is a per-call walk of every authoring source. A TYPED `SPECIALIST_X` entry is genuinely keyed and correctly
  > stays an entry-list read (§5); `any` is not, and must never be given the same treatment.
- **`allowedSpecialists:{<scope>:{SPECIALIST_X:N}}`** — the manual-assign cap, per-type only (no `any`).
- `free` lives ON TOP of `allowed` (independent). Normally a modifier leaf is `<scope>.<unit>` (e.g. a bare
  number or `.flat`); specialist counts instead use a **count-by-type** leaf (the `SPECIALIST_*` type — or `any`
  — IS the key, its value the count) — the one sanctioned exception, chosen for legibility.
- **freeSpecialists are MODIFIERS, never grants.** A free specialist is alive **only as
  long as its source is** — building present / civic adopted / trait active — the continuous-deposit shape, not a
  handed-out provision. Every legacy `changeFreeSpecialistCount` apply (civic/trait/building) classifies to THIS
  family; none belongs to the grants machine (`specialists` is not in the [json.md §5](../specs/json.md) grants vocabulary; *if
  anything is ever found that genuinely grants PERMANENT free specialists — surviving source destruction — we deal
  with it then*; no hypothetical machinery).
- **⚖ THE TWO-PART SEAM (the promotion-SPA seam pattern applied to specialists).**
  Free specialists split cascade-vs-engine in two parts: **(1) the AMOUNT** of free specialists is the
  CASCADE's — the summed `freeSpecialists` deposits (per type + the `any` bucket) from live sources;
  **(2) the PLACEMENT** — the engine decides how to place them within the parameters it has (typed entries
  auto-assign; the `any` bucket + citizen assignment ride the existing, reliable engine infrastructure);
  **(3)** consumers then *"simply deal with the OUTPUT of that"* — the realized per-type counts
  (`getSpecialistCount + getFreeSpecialistCount`) are a **sanctioned output-seam read**, never a
  self-containment ride-in. Demolition consequence: the cut replaces WHO MAINTAINS THE AMOUNTS (the cascade's
  summed deposits replace the `changeFreeSpecialistCount` process-applies feeding the placement); the
  placement machinery and its output reads stay.

---

