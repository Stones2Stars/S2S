# The modifier — "how much?"

The cascade machine that computes **per-turn magnitudes** — a city's food, a unit's strength, a property's
level. Sources **deposit** values; a target reads the **combined total**. It reads the modifier families
authored per the [json spec](json.md) §6; this doc is the **machine** that consumes them — the deposit flow, the
combine arithmetic, the conditioning, and the ownership rule that decides *where* a cross-entity modifier is
authored.

**At heart, a modifier is a [`requires`](enabler.md) gate plus an output.** It uses the *exact same* condition
vocabulary as the enabler — `all`/`any`/`noneOf`, atoms, predicates, scopes — so once `requires` is nailed the
modifier follows for free; the only thing it adds is a **magnitude** to deposit when the gate holds. So this spec
leans on that shared vocabulary (defined in [enabler](enabler.md) / [json](json.md)) and spends its effort on the
**output half**: how a magnitude deposits, accumulates, combines, and where it is owned.

---

## 1. One step: deposit DOWN, accumulate, read O(1)

Where the [enabler](enabler.md) is two passes, the modifier is **one step**: each source drops its deposit onto
a target, the deposits **accumulate**, and the target reads an **O(1) summed total**. No source needs the whole
picture; order doesn't matter (sums are commutative).

Magnitudes flow **DOWN** the scope spine (`world → … → city → plot | unit`). An empire-scope deposit on a civic
rolls down to each of the player's cities; a city-scope deposit lands locally; a `plots`-target deposit lands on
each matching worked plot (§5). The target holds one **accumulator slot** per `(family, member, unit)` and reads
its combined value — it never re-walks the sources.

This is purely top-down: a condition *inside* a deposit (`enabled`/`per`) is a forward **read** of state, never
an upward cascade-walk. The reverse view ("who modifies me") is derived once at load for the pedia, never on the
hot path.

**Three governing rules:** (a) **purely top-down** — sources deposit DOWN, targets read an O(1) accumulator; the
reverse index is cold-path only. (b) **tech-inflation is a downward DEPOSIT, not an upward gate.** (c) **info DATA
vs engine MACHINERY is a hard boundary** — JSON carries values/relationships only; producers, evaluators, and the
tally are engine-side.

---

## 2. The combine arithmetic

Per `(family, member, unit, target)`, the slot composes the three value units ([json](json.md) §3.6):

> **`effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)`**

`flat`s sum into the base; `percent`s (additive deltas) sum then apply once; `multiplier`s compose by product.
The slot carries `Σflat`, `Σpercent`, and `Πmultiplier` (stored ×100, identity 100) — one `deposit(unit, value)`
folds a value in, `effective(base)` reads it out.

**All integer, ×100 fixed-point, no float** — Civ4 multiplayer is deterministic lockstep, and CPU-dependent
float math desyncs. The single human→×100 conversion happened once in `readJson` ([json](json.md) §3.6); the
slot does pure integer math and never sees the human boundary.

> **Parity is the bar (owner ruling 2026-06-23).** The cascade reproduces the legacy engine **exactly** — parity is
> the only goal, and it is achievable: no bug has surfaced in any actual *calculation*, so the math matches. Any
> mismatch is a **data-collection gap** (a source the cascade didn't gather), never a formula difference — so there
> is no "close / same ballpark," no tolerance, no agent grading. While a channel is shadow-proven, multiplier
> deposits are treated as identity so the cascade is additive — exactly matching legacy — and the work is completing
> the gathered data until the diff is **0**. See [validation](validation.md).

**Three non-additive combine modes, declared as FAMILY metadata (never per-deposit):** a `min` member that floors
the combined total (e.g. `defense`); `combine: max|min` for worst/best-across-sources (anarchy turns,
`naturalDefense`); `polarity: signed-split` for good/bad accumulators (health/happiness — positive→good,
negative→bad). Authors write signed values; the mode wires the combiner.

---

## 3. Conditioning — re-evaluated every recompute (the dormancy model)

A deposit may carry `enabled` / `disabled` / `per` ([json](json.md) §3.7, §3.9). A deposit's condition uses the
**same vocabulary** as the enabler's `requires` — the same `all`/`any`/`noneOf` tree over the same atoms and
predicates — so a conditioned deposit is, in essence, **a `requires`-shaped gate with an output attached**: the
enabler resolves that shape to *availability* ("can I?"), the modifier resolves the *same* shape to a *magnitude*
("how much?").

**But they are SEPARATE FIELDS, not one condition** — because a thing can **require one condition yet gate its
effect (a buff *or* a nerf) on another**: a Forge `requires` connected iron to *operate*, but its +1 happiness is
`enabled` by *power*, not iron — and the magnitude can equally be negative (e.g. −production while polluted). So
the entity carries its `requires` once (whole-entity availability — the [enabler](enabler.md)'s job), and each
deposit carries its **own** `enabled`/`disabled` (does *this effect* apply). Same condition language, two
independent fields.

These conditions are **re-checked on every recompute**, and that re-check *is* the dormancy model: a deposit
whose `enabled` no longer holds (or whose `disabled` now holds) simply stops contributing — the source goes quiet
without being removed.

- **`enabled` then `disabled`** — `enabled` is read first, `disabled` second; a `disabled` that holds overrides
  ([json](json.md) §3.9).
- **`per`** scales the deposit by a count — local at `city`/`plot`, via the [tally](tally.md) at cross-city scopes.
- Whole-entity availability (is this building active at all?) is the [enabler](enabler.md)'s `requires`, not a
  per-deposit condition: a dormant entity deposits nothing, so the modifier machine never special-cases it.
- **Age-gated deposits** — legacy `CommerceChangeDoubleTimes` ("double after N turns") is **not** a timer/stage
  but a SECOND deposit on the same slot with `enabled:{existedFor:{min:N}}` (no post-sum multiply).

---

## 4. Ownership — the deliveryguy rule

> **This doc is the home of the deliveryguy ruling.**

A cross-entity modifier (X-keyed-by-Y) — does it live on X or fold onto Y? The test is **semantic sense: who
BRINGS this modifier to the table?** That deliverer **owns** it; the other entity is referenced as a
**condition** (`enabled` / `requires`), never the home. Two shapes, chosen per case by what reads cleanly:

- **own-output** — an entity's *own* produced output (a specialist's yield, an improvement's tile yield, a
  unit's strength) lives on **that entity**, with tech/civic/building as an `enabled` condition. *A civic
  boosting a Merchant's commerce → on the **specialist**, `enabled:{civic}` — NOT on the civic.*
- **governing-deliverer** — an entity that *delivers/governs* an effect on others lives on **the actor**, keyed
  by the target. *A route upgrading improvements → on the **route**, keyed by improvement.*

Plot-substrate entities (terrain / feature / improvement / route) each own their own `plot`-scope output. The
rule has **no special cases** — every cross-entity modifier lands by it.

**Conditioner axis:** a **tech** conditions on the **enabling** axis (`enabled:{tech}`, monotonic — once you
have it, you keep it); a **religion / resource** conditions on the **requiring** axis (`requires.operate`,
reversible — it can be lost).

**Data ≠ runtime.** The JSON is organised for a human (one home per relationship); `readJson` builds the links
both ways at parse so the machine reads top-down. Any "land it on the target" is a **parse transform**, never an
authored shape.

**`production` vs `buildRate`.** `production` = `getYieldRate100(PRODUCTION)` (total city output — scales every
build every turn; a flat ADD or city-wide percent). `buildRate` = `getProductionModifier(eItem)` (shrinks the
COST of a SPECIFIC item, never a per-turn yield), sub-shapes `buildRate.self` /
`.<scope>.{units|buildings|domains|unitCombats}.{TARGET}` (keyed) / `.<scope>.{military|space|worldWonder|teamWonder|nationalWonder}`
(category). `militaryProduction`/`spaceProduction` fold into the `buildRate` categories. (The "Versailles bug" =
filing an item discount under `production.city`.)

---

## 5. Targets — scope-wide, object-plural, or keyed

A deposit lands in one of three ways ([json](json.md) §6.1):

- **scope-wide** — no target: the scope object itself (the city is the common case).
- **plural object-target** (`plots` / `units` / …, predicate-filtered) — realized by evaluating the predicate
  against **every object of that kind in scope** and depositing onto each match. One uniform mechanism: an
  empire-wide sea-tile buff is `production.empire.plots {IS_WATER}`, applied to every worked water plot. This
  retires all the legacy per-plot-type / per-tile accumulators.
- **named-entity key** (`improvements.{FARM}`, `terrains.{…}`, `buildings.{…}`) — a deposit onto a specific
  named target, kept on the source (the deliveryguy, §4).

---

## 6. The unit plane — a self-accumulator

A `unit`-scope deposit is a **self-accumulator**: source == target. A unit's promotions and unit-combat class
deposit their stat changes onto the unit itself (the existing additive promotion stack), summed for O(1)
concatenation as each promotion is added — not a downward cascade.

**Host-from-occupants** effects — what a city gets *per unit stationed in it* (military happiness/anger) — are
**not** a bespoke host-family: they're an ordinary deposit on the source (the civic/trait), scaled by a
predicate-filtered unit count and targeting `cities`: `happiness.empire.cities.{unit: IS_MILITARY, flat: N}`
([json](json.md) §3.7). The **carrier↔cargo** behaviour splits across the two systems. The carry *ability* is a unit **skill** — whether
the unit may use the **load/unload** action is `is_cargo_vessel`, and the attack restriction it brings is
`defend_only` (both skills, [json](json.md) §8). The *amounts* live in the **`cargo`** modifier family (a unit
self-accumulator, set on the unit or a promotion), with two complementary members:
- **`cargo.space`** — how much the unit **carries** *and what*: `cargo.space.{unit: IS_<domain>, flat: N}` — a
  carrier is `cargo.space.{unit: IS_AIR, flat: N}` (*you can't transport a plane on a landing craft*); an
  unrestricted hold is just `cargo.space.flat`. (From legacy `iCargo` + `DomainCargo`.)
- **`cargo.size`** — the unit's cargo **footprint** (room it occupies when loaded), **defaulting to 1** if unset.
  (SizeMatters extends cargo via `smSpace`/`volume`/`volumeModifier` — a separate rework.)

No bespoke host↔cargo family is needed. The full unit-stat family vocabulary
(`strength`/`withdrawal`/`firstStrike`/… ) is [json](json.md) §6; this is the largest surface and lands last.

> **Movement & range** are their own resolver subsystem, not ordinary downward families: `moveCost` is computed
> **per `(unit, edge)`** with a route `min`-override, double-move divisors, and a floor — it doesn't fit the
> "deposit DOWN → O(1) summed read" shape. The plot-side base cost stays intrinsic on terrain/feature/route; only
> the cascading *deltas* (tech route changes, promotion move bonuses) are real modifier families. (Detail: the
> movement subsystem doc, pending.)

### Specialist counts

- **`freeSpecialists:{<scope>:{any:N, SPECIALIST_X:M, …}}`** — granted specialists; `any` = an assignable-slot
  bucket, a typed entry is auto-assigned. Leaf is a count (a list when conditioned).
- **`allowedSpecialists:{<scope>:{SPECIALIST_X:N}}`** — the manual-assign cap, per-type only (no `any`).
- `free` lives ON TOP of `allowed` (independent). The leaf being a count-by-type is the one sanctioned departure
  from the `.flat` leaf.

---

## See also
- [json.md](json.md) — the data this machine reads: the modifier-family address, `flat`/`percent`/`multiplier`
  units, `enabled`/`disabled`/`per` conditioning, `plots`/`units` targets, and the `buildRate` vs `production`
  split (§3, §6).
- [enabler.md](enabler.md) — the "can I?" machine. Availability is upstream of magnitude: an unavailable or
  dormant entity deposits nothing.
- [tally.md](tally.md) — the count machine a `per` scaler reads at cross-city scopes.
- [naming.md](naming.md) — the `INFOTYPE_NAME` ids used as deposit keys and condition atoms.
