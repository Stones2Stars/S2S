# 2. The combine arithmetic

> Part of the **[cascade](../cascade.md)** spec.

Per `(family, member, unit, target)`, the slot composes the three value units ([json](../specs/json.md) §3.6):

> **`effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)`**

`flat`s sum into the base; `percent`s (additive deltas) sum then apply once; `multiplier`s compose by product.
`Σflat`, `Σpercent`, and `Πmultiplier` (flats + multipliers stored ×100, identity 100; a PERCENT is NOT
scaled — [the ×100 fixed-point model](../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)) are each their own accumulated number —
**the `unit` is part of the slot KEY (per `(family, member, unit, target)`), so a flat sum and a percent sum
are SEPARATE slots, never fields of one mixed struct** — the separation is what lets invalidation split
percent-vs-flat (§1). One `deposit(unit, value)` folds a value into its unit's slot; `effective(base)`
combines them at read.

All integer, ×100 fixed-point throughout ([the ×100 fixed-point model](../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)):
the single human→×100 conversion happened once in `readJson` ([json](../specs/json.md) §3.6); the slot does pure integer
math and never sees the human boundary.

> **⛔ PLOT SCALING CAN ONLY AFFECT ITSELF — A HARD RULE.** No scenario exists where a plot gives 1 hammer per 5 commerce, so it is codified as a hard rule, that the plot scaling can only
> effect itself."* A per-plot scaling of a channel reads that channel's own value on that plot and grants THAT
> CHANNEL. There is no cross-channel plot scaling, and none may be authored: a threshold on commerce cannot pay
> out in production.
> ⚑ **It is a structural simplification, not a restriction to police.** With the input and the output on one
> channel, the whole mechanic is plot-local — it needs no cross-scope reach at resolve time, no ordering between
> channels, and no fan-out when one channel moves another. That is what lets it live in the package.
>
> **⚖ THE MECHANIC IS TWO SEPARATE NUMBERS, BOTH FED IN: a THRESHOLD and an AMOUNT.** Maintain the per-yield threshold and the amount granted per yield as TWO separate numbers, both fed in. The interval is "per how much" of the plot's own value; the amount is what each whole interval grants.
> ⚖ **The AMOUNT comes from the `EXTRA_YIELD` global define, and that is fine** — the `EXTRA_YIELD` define stands and needs no change. ⛔ So a define read here is NOT a
> gap to close and NOT a missing authoring surface; do not "fix" it into curated data. What the ruling requires is
> that it stays a SEPARATE number the plane carries per channel — which it is — so that authoring it later is a
> data change and never a reshape.
> ⚠ **The THRESHOLD does not combine additively, and this is the trap:** the engine selects **the SMALLEST
> POSITIVE threshold held** (`CvPlayer::updateExtraYieldThreshold`), so two sources at 7 and 5 yield 5, never 12.
> A plain flat channel SUMS, so reading one through the ordinary roll-up is wrong by construction — it needs the
> non-additive family metadata this section already defines for `defense`'s floor kind. The AMOUNT is an ordinary
> additive number; only the threshold is a min.
> ⚖ **AND IT HAS TWO LEGS, ONE RAISING AND ONE LOWERING — `extraYieldThreshold` and `lessYieldThreshold`.** They
> are ONE mechanic with opposite signs: each selects the smallest positive threshold its owner holds, and each
> moves the SAME `EXTRA_YIELD` amount, one adding it and one subtracting it. Both are real authored data — the
> agricultural line raises, the lazy / gluttonous / excessive / nomad lines lower — so a plane carrying only the
> raising leg silently drops every downside a negative trait is meant to impose.
> ⛔ They are **two pairs, not one signed pair**: an owner can hold both at once at different thresholds, so one
> `(interval, amount)` slot per channel cannot express them. The lowering leg resolves on the value the raising
> one produced, which is the engine's own order (its second branch tests the already-raised running yield).
>
> **A plot's yield is ONE base package, resolved in isolation BEFORE the city modifiers.**
> All output from a single plot is computed in **complete isolation** as one base-yield package — `CvPlot::calculateYield`
> per plot (nature = terrain+feature+river+hills/peak + bonus; + improvement, floored at `-nature`; + route + the
> keyed/plots flats, `max(0,·)`) — and that result is passed **up the chain**: the city SUMS its worked-plot
> packages into the §1 `base`.
> **The plot yields ARE "the base the rest is calculated from."** So anything that scales a *specific improvement or
> plot component* resolves **inside** this per-plot package, **before** the city-level `(100+Σpercent)` stack ever runs.
> ⚖ **The CITY-CENTRE constant is the legacy `calculateYield` city block, inside this same isolated resolve,
> reading the plot's OWN city-ness LIVE (the flooring should be on the plot itself, not on the
> cascade)** — three terms on a city plot's yield channels: the YieldInfo `CityChange` constant (food −1 /
> production +1 / commerce +1) **plus** `population / PopulationChangeDivisor` (food /5, production /2,
> commerce /4 — integer division), both added BEFORE the plot scaling so the threshold plane tests the total
> legacy tested; and the `MinCity` floor (3/1/1) applied LAST. City-ness is the plot's own state, so none of it
> is mirrored onto the package as a fed operand; the `SEVT_PLOT_CITY` pair and the city's `SEVT_CITY_POPULATION`
> facts are RE-RESOLVE routes only (the refresh-an-operand shape), each folding the exact delta into the working
> city's worked-plot Σ. *(A founded city physically clears its plot's improvement, so the legacy city-block
> improvement exclusion needs no resolve leg; route flats stay in the base per this row.)*
> ⚖ **THE PLOT PACKAGE STORES FOUR SEGMENTS, AND THE FOURTH EXISTS FOR ONE OPERAND: nature · improvement ·
> ROUTE · rest.** Route and the owner's plot flats sum and floor identically, so the split buys nothing on the
> TOTAL — what it buys is the engine's per-plot GOLDEN-AGE threshold, which tests the **pre-improvement,
> pre-route** running yield (`nature + the city block + the owner's plot flats`,
> [golden-age.md](../reference/golden-age.md)). That operand is inexpressible while route and the owner's flats
> share a sum, and at the authored threshold of 1 the difference is very nearly every improved tile.
> ⛔ The SCALING is the opposite case and stays on the FULL total (the row above: *terrain + feature +
> improvement + route*) — the two thresholds deliberately take different operands, so do not "unify" them.
> ⚠ For REPORTING, `plotRest` keeps its meaning — the owner's flats **plus** route — so the segments still sum
> against `plotBase`; `plotRoute` is a breakdown of it, never a fourth term beside it
> ([http-endpoints.md](../specs/http-endpoints.md)).
>
> Today every component-specific buff is **flat** (so the package is a pure sum); should a per-improvement *percentage*
> ever be needed, it applies **here, inside the isolated plot calc** — **never** in the city `(100+Σpercent)` stack,
> which only ever scales the already-summed base. Consequence: a `basePlotYield` divergence is *necessarily* a per-plot
> **flat** miscount (missing or double-counted), because no city-level percentage exists that could move a single plot.
>
> **Completeness is the bar ([represent, don't fit](../specs/validation.md#the-observation-surface)).**
> Multiplier deposits are treated as identity on the yield/commerce channels — no source authors one, so the cascade
> is additive, exactly matching legacy. Live acceptance is done-is-observable
> ([done = observable in the running game](../specs/validation.md)) + turn time
> ([turn time is king](16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)): [validation](../specs/validation.md).

**Non-additive combine, declared as FAMILY metadata (never per-deposit):** a `min` member that floors the
combined total (e.g. `defense`'s floor kind). Authors write signed values; the mode wires the combiner.
⛔ **`naturalDefense` is NOT one of these and never was a kind.** There is no natural-defense channel: BUILDINGS
and CULTURE LEVELS author the SAME `defense.city.amount`, so the cascade holds one additive stack and the legacy
`max(buildingDefense, naturalDefense)` has no counterpart — a data-led behaviour change, not a combiner to
build. ⚠ A worst/best-across-sources combiner is **not part of the model** — do not read this paragraph as
licence to add one speculatively; mint it only if and when a family's data actually needs it.

> **⚖ THE FREE-AMOUNT SIGN CONVENTION — one convention per kind, never a per-source flip.** The
> `upkeep.freeMilitary` / `upkeep.freeCivilian` kinds carry **free-amount semantics throughout**: a POSITIVE entry
> GRANTS free upkeep, a NEGATIVE entry SHRINKS the free allowance. Entries sum like any other channel, and the
> **group total floors at zero as family-combine metadata** (the `min` mechanism above) — distinct from, and
> applied before, the engine's own `net = max(0, upkeep − Σfree)` floor. **Two floors, deliberately: one on the
> group, one at the consumption site.** A pop-scaled source authors `{P, per: {POPULATION, each: 100}}` keeping
> its own sign. ⚠ This is an owner-ruled INTENTIONAL divergence from the legacy asymmetric rounding helper
> (whose `mod<0` branch computed `v×100/(100−mod)`): the ruled shape is **additive linear**, attributed and never
> bit-chased ([validation.md](../specs/validation.md) intentional class).

> **⛔ There is NO `polarity` mode — wellbeing is FOUR ORDINARY CHANNELS:** `happiness`, `anger`,
> `health`, `unhealth`. Happiness sums against anger, health against unhealth, at the verdict (§2b). A negative
> deposit is routed to the opposing channel **at fill**, so the split is a routing rule, never a storage shape —
> no good/bad plane, no duplicated positions, no per-family combiner. **The routing granularity is PER ENTRY**
> (a deposit IS an entry, [json.md §3.9](../specs/json.md)) — a mixed-sign author SPLITS across the pair rather than
> netting, and per-entry is the only delta-able form, so the apply, the valuation and every other fill aggregate
> identically by construction. It reaches the FLAT side only: a negative PERCENT scales its own channel down and
> is never re-homed to the twin. This is what keeps
> [every modifiable number is a yield](01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1) literal: wellbeing is four yields like
> any other, on the one uniform package
> ([every derived cache is one shape](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta)).

---

