# 6. The unit plane — a self-accumulator

> Part of the **[cascade](../cascade.md)** spec.

A `unit`-scope deposit is a **self-accumulator**: source == target. A unit's promotions and unit-combat class
deposit their stat changes onto the unit itself (the existing additive promotion stack), summed for O(1)
concatenation as each promotion is added — not a downward cascade.

**Host-from-occupants** effects — what a city gets *per unit stationed in it* (military happiness/anger) — are
**not** a bespoke host-family: they're an ordinary deposit on the source (the civic/trait), scaled by a
predicate-filtered unit count and targeting `cities`: `happiness.empire.cities.{unit: IS_MILITARY, flat: N}`
([json](../specs/json.md) §3.7). The **carrier↔cargo** behaviour lives entirely in the **`cargo`** modifier family (a unit
self-accumulator, set on the unit or a promotion), with two complementary members:

> ⛔ **HAVING A HOLD *IS* THE CARRY ABILITY — there is no separate carrier flag, skill or info scalar.** A unit
> is a carrier iff `cargo.space` gives it capacity, so `CvUnit::isCarrier` asks exactly that and nothing else.
> ⚠ The legacy `SpecialCargo`/`DomainCargo` scalars survive ONLY as the promotion-set overrides
> (`PROMOTION_TRANSPORT_PEOPLE` and its five siblings). **No unit authors them**, so a carrier test written on
> either one answers false for every transport in the game — which is precisely how the whole transport system
> came to be dead while reading as implemented.

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
  > ⚖ **A PROMOTION ADDS SPACE, NEVER PERMISSION — an INTENTIONAL divergence from legacy (we go with
  > yours, it's cleaner).** In the legacy game a transport promotion WIDENS the class carried: an unpromoted
  > galley takes a settler, a promoted one takes military. The ruled model does not reproduce that — WHAT is the
  > carrier's, fixed, and a promotion only ever changes HOW MUCH. ⛔ So do not "repair" this back by letting a
  > promotion author a wider qualifier: the behaviour change is chosen, and the reason is that a permission that
  > moves with promotions puts WHAT in two places and makes a carrier's rule unreadable from the carrier
  > ([validation.md](../specs/validation.md) intentional-model-change class; the spec leads, legacy behaviour is not
  > preserved for its own sake).
  > ⚠ Consequence: a carrier whose base capacity is 0 still has a restriction to state, and the §3.9 entry
  > grammar has no payload-less form for it — an open item for the json spec. Until it exists such a hold is
  > **unrestricted**: with no qualified entry there is no restriction to read, so the promotion-granted space
  > takes anything (`CvUnitInfo::admitsCargo` answers true when the carrier authored no qualifier at all).
  ⚖ **THE CAPACITY AND THE RESTRICTION ARE READ BY TWO DIFFERENT CALLS, AND NEITHER IS `getCargo(CARGO_SPACE)`.**
  The point sum folds only UNCONDITIONED entries, so a qualified hold — which is every authored carrier — is
  invisible to it. `CvUnitInfo::getCargoSpaceTotal()` is the capacity (qualified entries included, because a
  restriction says what a hold takes and never how much); `CvUnitInfo::admitsCargo(candidate)` is the
  restriction, evaluated against the candidate's own info — domain for `IS_LAND`/`IS_AIR`/`IS_WATER`, tag
  bitset for the rest. ⛔ A new consumer asking "how much cargo does this unit type carry?" uses the former;
  reaching for the point read reintroduces the silent zero.
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

## ⛔ A CACHE IS INVALIDATED BY EVERY INPUT THAT FEEDS IT, NEVER BY A SUBSET

`URS_STRENGTH_FLAT` gathers over the unit's held set, and `baseCombatStrPreCheck()` adds it to the serialized
base. But with `GAMEOPTION_COMBAT_SIZE_MATTERS` on, `baseCombatStr()` returns the **`m_iSMStrength` cache
INSTEAD** of that sum, and the cache is only rebuilt when `processPromotion` sets its recalc rider.

⛔ **That rider was gated on ONE family.** It fired for `combat` (`COMBAT_AMOUNT`) and not for `strength`
(`SCALAR_STRENGTH` / `CASC_UNIT_FLAT`), while the gather reads BOTH — so a `strength` promotion was gathered
correctly into the resolved plane and then never read, because the cache still held the pre-promotion value.
⚑ **Measured: all thirteen `PROMOTION_MIGHT*` were inert under Size Matters**, up to `MIGHT13`'s **+30
strength**, while every `combat` promotion applied normally. The signature to recognise is exactly that split —
*one family of promotion works and another silently does nothing* — and it points at an invalidation rider, never
at the gather or the authored data.
⇒ **When a value has TWO source families, its invalidation names both.** The slot table
(`CvUnitResolved.cpp`, `g_aSlotAddress`) is the list of what feeds a slot; a rider that does not cover every row
feeding the cached value is the defect.
⚠ `processUnitCombat` is the correct shape beside it: it recalcs on `bSM && bByPromo` with no family filter at
all, so it cannot fall out of step with the gather.

---
