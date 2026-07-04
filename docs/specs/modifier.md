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
each matching worked plot (§5). The target reads a combined value — it never re-walks the sources.

> **⚖ STORAGE SEMANTICS — the SCOPE PRINCIPLE.** Deposits accumulate in a package **AT THEIR OWN SCOPE** — one uniform package
> format (Σflat / Σpercent per channel, §2) cached on each scope object (world / team / empire / area /
> city / plot), each package event-invalidated at its own scope only. The downward "roll" is realized **AT
> READ TIME**: the realized value is the trivial sum of the ~5 scope packages, with per-city gates
> (state-religion-in-city, coastal, connected, area membership) applied live at the combine. **A lower
> scope never STORES an upper scope's sums** — that would force downward invalidation fan-out and "break
> the principle of the cascade in the first place." The only full rebuild of every package is at LOAD.
> (Cache mechanics: [state-repositories.md](../architecture/state-repositories.md) — the per-scope package
> model + the CvDerivedCache component.)

This is purely top-down: a condition *inside* a deposit (`enabled`/`per`) is a forward **read** of state, never
an upward cascade-walk. The reverse view ("who modifies me") is derived once at load for the pedia, never on the
hot path.

**Three governing rules:** (a) **purely top-down** — sources deposit DOWN, targets read an O(1) accumulator; the
reverse index is cold-path only. (b) **tech-inflation is a downward DEPOSIT, not an upward gate** — a researched
tech deposits down onto everything below it (cheaper/better); the lower thing never reaches UP with a `hasTech`
gate. (c) **info DATA vs engine MACHINERY is a hard boundary** — the JSON carries only values + relationships;
the producers, evaluators, and tally that consume them are engine-side, so authoring stays declarative.

---

## 2. The combine arithmetic

Per `(family, member, unit, target)`, the slot composes the three value units ([json](json.md) §3.6):

> **`effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)`**

`flat`s sum into the base; `percent`s (additive deltas) sum then apply once; `multiplier`s compose by product.
`Σflat`, `Σpercent`, and `Πmultiplier` (stored ×100, identity 100) are each their own accumulated number —
**the `unit` is part of the slot KEY (per `(family, member, unit, target)`), so a flat sum and a percent sum
are SEPARATE slots, never fields of one mixed struct** — the separation is what lets invalidation split
percent-vs-flat (§1). One `deposit(unit, value)` folds a value into its unit's slot; `effective(base)`
combines them at read.

**All integer, ×100 fixed-point, no float** — Civ4 multiplayer is deterministic lockstep, and CPU-dependent
float math desyncs. The single human→×100 conversion happened once in `readJson` ([json](json.md) §3.6); the
slot does pure integer math and never sees the human boundary.

> **A plot's yield is ONE base package, resolved in isolation BEFORE the city modifiers (owner ruling 2026-06-27).**
> All output from a single plot is computed in **complete isolation** as one base-yield package — `CvPlot::calculateYield`
> per plot ([calc-map](../plans/structural-cleanup/legacy-value-calc-map.md) §10.1: `calculateNatureYield`(`getBaseYield`=
> terrain+feature+river+hills/peak + bonus) + improvement (floored at `-nature`) + route + the keyed/plots flats,
> `max(0,·)`) — and that result is passed **up the chain**: the city SUMS its worked-plot packages into the §1 `base`.
> **The plot yields ARE "the base the rest is calculated from."** So anything that scales a *specific improvement or
> plot component* resolves **inside** this per-plot package, **before** the city-level `(100+Σpercent)` stack ever runs.
> Today every component-specific buff is **flat** (so the package is a pure sum); should a per-improvement *percentage*
> ever be needed, it applies **here, inside the isolated plot calc** — **never** in the city `(100+Σpercent)` stack,
> which only ever scales the already-summed base. Consequence: a `basePlotYield` divergence is *necessarily* a per-plot
> **flat** miscount (missing or double-counted), because no city-level percentage exists that could move a single plot.

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

## 2a. The realized RATE — what is BASE, what is added AFTER the percentages

The §2-combine above is the *generic* slot. A city's **per-channel yield/commerce RATE** — the engine's
`getYieldRate100` (§1) and `getCommerceRateTimes100` (§2), the value a citizen's worked output finally becomes — is
that combine applied with a **sharp two-tier shape**. This is the model the rate computation must reproduce, and the
order is load-bearing (it decides what the percent stack scales and what it doesn't):

> **`rate100 = (BASE + specialists) × modifier⁄100  +  100 × ⌊EXTRA100 ⁄ 100⌋`**
>
> `modifier = max(0, 100 + Σpercent)` (so `×modifier⁄100` ≡ `×(1 + Σ%)`). Everything is ×100 fixed-point integer.

### TIER 1 — BASE (everything the percent stack MULTIPLIES)

| BASE source | origin | base vs computed |
|---|---|---|
| **worked-plot yields** (`basePlotYield`) | Σ over the city's worked plots of each plot's ONE isolated base package (§2 plot-as-base): `max(0, terrain+feature+bonus)` nature + improvement (floored at −nature) + route + keyed building/civic/trait `plot`-flats + `plots`-target + city-centre constant + threshold/golden-age per-plot | **computed** from the curated plot substrate + `/state/plots` |
| **trade-route yield** (`tradeYield`) | `/state` input | **input** — out-of-scope (the trade network); the calc *folds it in*, never derives it. The ONE live-yield input (see [http-endpoints](http-endpoints.md)) |
| **free-city yield** (`freeCityYield`) | Σ the player's active traits' `YieldChanges` (`{ch}.empire.flat`) | **computed** (was a `/state` read; now derived — [http-endpoints](http-endpoints.md)). ⚠ NAMING (owner clarification 2026-07-04): "free-city" here = the legacy trait accumulator (`CvPlayer::m_aiFreeCityYield`, free yield granted in every city) — **NOT** the WLTKD celebration ("We Love the King/Emperor Day"), whose sole gameplay effect is zero city maintenance ([economy.md](../reference/economy.md)) |
| **golden-age yield** | trait `goldenAge` member (`{ch}.empire.goldenAge.flat`) while in golden age | **computed** (`empire.goldenAge` member-mirror, §3 golden-age carve-out) |
| **specialist yields** (`specialist`) | per assigned specialist: `intrinsic × (100 + specialist-%)⁄100` + building-local (gated `city.flat`) + per-type (`empire.flat`) + perAll + trait governing-deliverer | **computed**. NOTE the specialist carries its **own** percent layer (its intrinsic ×`(100+specialist-%)`) *before* it joins BASE and takes the city `modifier` — two distinct percent stacks |

### TIER 2 — EXTRA (flat, added AFTER the percentages, NEVER multiplied)

| EXTRA source | origin |
|---|---|
| **building flat yields** (`BuildingFlatYield100`) | Σ active (non-dormant) buildings' `{ch}.city.flat` + `{ch}.city.perPopulation` × population |

The EXTRA is held ×100; the `100 × ⌊EXTRA100⁄100⌋` **truncates it to whole units** before re-scaling (the engine's
`getExtraYield100` order — a documented integer-truncation gotcha, not a rounding choice).

> For **§2 commerce** the same two-tier shape holds with the channel's own pieces: BASE = the COMMERCE-yield
> (`getYieldRate100(COMMERCE)`) × the channel slider + the §2 baseExtra sub-terms (religion, corporation, golden-age,
> state-religion pool, player-extra, the building-commerce block); EXTRA (post-modifier) = `production × prodToCommerce`.
> The building-commerce block is itself a pure per-building sum (own-flat + tech + bonus + perPop + shrine + corp-HQ +
> the `CommerceChangeDoubleTime` whole-doubling). Civil disorder forces the whole rate to 0 before any of this.

### How the percentages "smash together" — ONE additive stack

`modifier` is **a single additive sum** — every active source's `{channel}.<scope>.percent`, added together, then
`max(0,·)`:

- **active buildings** (this city, non-dormant): `city.percent` + `area.percent`
- **empire buildings** (every building the player owns anywhere — rolls DOWN to each city): `empire.percent`
- **adopted civics**: `empire.percent`
- **the player's active traits** (the option-selected set, pure-filtered §4): `empire.percent`
- **projects** (commerce channels only; yields find none): `empire.percent`

They are **purely additive** — `+30% +20% −10% = +40%`, applied **once** as `×140⁄100`. The engine keeps these in
*separate accumulators* (`modBuilding`, `modPlayer`, `modCapital`, `modBonus`, `modFromBuildings`, …); the cascade
**unifies them into this one sum** because addition is associative — the per-accumulator split changes nothing the
result can see. `multiplier` deposits (`Π(multiplier⁄100)`, §2) exist in the generic model but are **identity here**:
no yield/commerce source authors a multiplier, so the stack is additive-only and matches legacy exactly.

The two tiers + the single additive stack ARE the coherent shape: a BASE assembled from its sources, scaled **once**
by the unified percent total, with the building FLATs bolted on **after** — never inside — the percentages.

---

## 2b. The WELLBEING channels — health + happiness (signed-split, the §2a sibling)

The city's **health** and **happiness** levels are the §2 combine applied with the **`polarity: signed-split`**
family metadata (§2): every source deposits ONE signed value; the **good/happy side sums `max(0, source)`**, the
**bad/unhappy side sums `−min(0, source)`** — the same source feeds both accumulators, split by sign at combine
(the engine's exact shape: `happyLevel`/`unhappyLevel` `CvCity.cpp:5709/5626`, `goodHealth`/`badHealth`
`:5851/:5878`). The realized verdicts: `healthRate = min(0, good − bad)`; `angryPopulation =
clamp(unhappy − happy, 0, pop)`. The channel oracle is **`/computed/cities/wellbeing`**
([http-endpoints](http-endpoints.md)) — one field per named engine term.

**The TARGET/INPUT split (the tradeYield precedent, [validation](validation.md) input rules):**

- **DEPOSIT-COMPUTED (the cascade's targets)** — everything a live source's `health`/`happiness` family deposits
  produce: **buildings** (city `flat`/`perPopulation` + the area/empire-scope rollups + conditioned entries incl.
  `HAS_STATE_RELIGION`-gated), **civics** (empire flats + the keyed/heterogeneous members: `buildings.{B}`,
  `features.{F}`, `nonStateReligion`, `perMilitaryUnit`, the ranked `cities` scaler), **traits** (same member
  vocabulary), **features** (`health.plot.percent` — summed over radius plots, ÷100 — the fallout class),
  **bonuses** (empire flats, presence-gated), **specialists** (city flats; the fractional values are the
  curator's ÷100 de-scale of the legacy latent-×100 — the engine `…/100` at use), **corporations**
  (`HAS_CORPORATION`-conditioned city flats), **techs**/**projects** (empire — projects also the lone `world`
  scope)/**handicaps** (empire flats), and **military units** (`happiness.empire.cities.{unit: IS_MILITARY}`
  §3.7). **Religion happiness has NO religion-side data** (verified: legacy religion info carries none) — the
  state/non-state religion terms derive from CIVIC/TRAIT/BUILDING configs × religion presence.
  ⚖ **Improvement health is a BALANCE-CUT (curator ruling, `curate_improvement.py`):** legacy `iHealthPercent`
  is deliberately dropped from the data, so the engine's `improvementGood/Bad` term is an **intentional
  divergence** — attributed via the oracle's `improvementGood100/Bad100` fields, shown, never chased
  ([validation](validation.md) intentional-model-change class); the term dies at the channel's legacy cut.
  **Celebrity happiness** is an INPUT until the `skills.celebrity` unit-scan port (the ⏳ post-migration CvCity
  scan, data-migration-remaining.md).
- **RAW-STATE INPUTS (folded, never derived)** — the runtime timers/counters no deposit produces: the **anger
  percents** (overcrowding = f(pop), noMilitary, foreign-culture, enemy-religion, hurry/conscript/defy/
  revRequest timers, war-weariness, revIndex, civic anger%), the **espionage counters**, **event anger**
  (one-shot event state), **tax-rate unhappiness**, **foreign-culture anger**, **landmark anger** (option-gated),
  **city-over-limit**, and **vassal** terms. These are saved/derived-from-saved state (legitimate inputs per the
  [http-endpoints](http-endpoints.md) hard rule) — the calc folds them at the level combine exactly where the
  engine does.
- **GATE FLAGS** — `isNoUnhappiness` / `isNoCapitalUnhappiness` / `isNoUnhealthyPopulation` /
  `isBuildingOnlyHealthy` zero their side wholesale; building `attributes` (json §8) carry them
  post-classification-wiring; until then they are read as state.
- **`unhealthyPopulation`** (= `max(0, pop − angryPop)` unless flagged) enters the BAD side as the engine's
  population term — a state-derived input (it reads the happiness verdict; the calc computes it from its own
  happiness result, never reads the engine's).

⚠ Two engine quirks the calc mirrors verbatim (never "fixes" — [DEC-mirror-then-redesign]):
`badHealth` adds `min(0, extraBuildingBadHealth)` **twice** (once inside `totalBadBuildingHealth`, once
directly); and the anger percents scale by `pop/PERCENT_ANGER_DIVISOR` with truncating integer division.

**⛔ TRAVELING UNIT MODIFIERS RIDE ON TOP (owner ruling 2026-07-03, GENERAL — all channels).** A modifier that
TRAVELS with a unit (unit-sourced happiness, anger, property emission, and any future unit-carried channel
value) is **never part of a cached cascade computation**: it is computed LIVE at read and **added on top as a
FLAT term, after and outside every percentage modification**. Two structural consequences: (1) unit movement
never dirties any cache — the cached sums are unit-free by construction; (2) the traveling value is a plain
flat addition to the realized number, never an input to a percent stack. The implementation shape: the cache
stores the unit-free number (+ any epoch-stable per-unit multiplier, e.g. the civic perMilitaryUnit VALUE);
the read folds `perUnit × liveCount` / the live unit walk on top (an O(1)-ish live engine read).
**The AUTHORING BAN that keeps this coherent: no unit gives — or can ever be
ALLOWED to give — PERCENTAGES to yields of any kind.** A unit-carried value is always a raw flat number on
top; a unit-authored percent would force units back inside the cached percent stacks and break the whole
on-top model. Enforceable at the curator/validation layer: a `units/**` JSON authoring a yield/commerce
`percent` deposit is a data error.
Ledgered as [DEC-unit-modifiers-on-top](../architecture/decisions.md#dec-unit-modifiers-on-top).

**UNIT-driven wellbeing is END-TURN cadence (owner ruling 2026-07-03).** The military/unit-count happiness
term recomputes **once per turn** (the substrate's turn-roll), NEVER per unit move — a per-move dirty hook made
every post-move rate read pay the wellbeing walk (a measured unit-automation collapse) and is banned. The
within-turn lag this leaves on the wellbeing slots (a handful of cities whose garrison changed mid-turn) is the
RULED cadence, not a freshness hole; the getter flip proceeds with it.

**The STORED-ACCUMULATOR DRIFT class (owner ruling 2026-07-03 + measured).** The legacy wellbeing terms are
INCREMENTAL SERIALIZED accumulators (`m_iBonusGood/BadHappiness`, `m_iBuildingGood/BadHappiness`,
`m_paiStateReligionHappiness`, `m_iExtraBuilding*FromTech`, …) — event-sourced numbers that carry decades of
save history. **The old cache model folded event-type grants DIRECTLY into these caches** (there is no separate
event-yield data — the per-building `m_aBuildingHappy/HealthChange` ledgers measure ZERO on the reference
save), so a stored value that disagrees with its current-state recompute is **DRIFT (history pollution), never
event state to preserve**. The oracle emits a `*Recomputed` twin beside each incremental accumulator
(bonus/building/stateReligion, happiness + health; the `extraBuilding`/`feature`/`religion` city accumulators
self-heal via the engine's `update*()` rebuilders and need no twin). Parity discipline: a verdict diff equal to
`Σ(stored − recomputed)` is **engine-wrong / cascade-right** — attributed-accepted (the same class as the
improvement-yield phantoms), repaired wholesale at the cutover when the slots recompute from data. Measured on
the reference save: the stored `bonusGoodHappiness` carries +1..+7 phantom drift in 61 of 185 diverging cities
(the cascade matches the engine's own recompute in all 185); drift+balance-cut adjustment reconciles
`badHealth` to 104/182 exact zeros (median 0) and `goodHealth` to median 1.

---

## 3. Conditioning — re-evaluated every recompute (the dormancy model)

A deposit may carry `enabled` / `disabled` / `per` ([json](json.md) §3.7, §3.9). A deposit's condition uses the
**same vocabulary** as the enabler's `requires` — the same `all`/`any`/`noneOf` tree over the same atoms and
predicates — so a conditioned deposit is, in essence, **a `requires`-shaped gate with an output attached**: the
enabler resolves that shape to *availability* ("can I?"), the modifier resolves the *same* shape to a *magnitude*
("how much?").

> **⛔ A condition is a PREDICATE, never a bespoke sub-scope MEMBER ([DEC-conditions-are-predicates], owner ruling
> 2026-06-28).** A deposit that applies only under some game state — only in the capital, only during a golden age,
> per military unit — carries that state as a **predicate** in its `enabled`/`disabled` (or a `per`/`unit:` scaler,
> §[json](json.md) §3.7), at the deposit's normal scope: `{family}.empire.percent` + `enabled:"IS_CAPITAL"`, NOT a
> bespoke `{family}.empire.capital.percent` member. **The predicate registry is EXTENSIBLE** — if the condition has
> no predicate named verbatim yet ([json](json.md) §3.5), **define a new one** (spec + evaluator + the `/state` fact
> it reads); that *extends* the model. Encoding the condition as a new **member** instead *changes the core
> structure* — the kraken way, and the exact shape (`byEra`, `empire.capital`, `perMilitaryUnit`) agents keep
> re-inventing. Retire any such member to a predicate-gated deposit.
>
> **⏳ Exception — golden age (owner ruling 2026-06-28).** Golden-age yield/commerce is applied by the **core
> engine** and is **not defined as data anywhere** — modelling it via `IS_GOLDEN_AGE` would mean authoring it
> virtually everywhere it fires. So `empire.goldenAge` **stays a member-mirror for now**, a deferred special case
> (NOT a retire-now invention). The `IS_GOLDEN_AGE` predicate exists ([json](json.md) §3.5) and is **reserved for
> when golden age is extracted from the engine core and moved where it belongs — post-migration** ([golden-age](../reference/golden-age.md)).

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

> **⛔ Trait modifier sources — pick the active set by the OPTION, never the id (kraken-resilience, owner 2026-06-25).**
> A leader's traits resolve to ONE `CvTraitInfo` table from *either* its simple set (`traits/simple/`, the
> `DefaultTraits`) *or* its complex/Thunderbrd set (`traits/complex/`, the `DefaultComplexTraits`), chosen at runtime
> by **`GAMEOPTION_LEADER_COMPLEX_TRAITS`**. The curator emits both as **two cleanly-separated, self-complete folders**
> (`traits/simple/` + `traits/complex/`); a consumer **loads the one active folder** by the live game option — this is
> NOT a `loadPrune` and NOT a mid-game swap (loadPrune is a load-time entity-DROP under an option, [json](json.md) §9 —
> never reach for it to select a set's values; any WorldBuilder mid-game trait swap is a post-migration concern). The two
> sets share **~64 colliding type ids**, so a consumer reading a trait's modifier families MUST select the active set
> from the **live game option (asserted via `/state`)** — NEVER infer it from a trait id's spelling (a `…1`-suffixed id
> is not "simple") nor from file load-order. Using the wrong file silently yields wrong magnitudes — this was the exact
> `modPlayer` yield divergence that proved the rule. (The enabler is unaffected: it reads trait *presence*, which
> `/state` already resolves to the active set; only the modifier cascade reads trait *family values*.)

> **⛔ Inverted-onto-a-SHARED-entity boosts stay on the TRAIT, per set — the own-output carve-out (owner ruling 2026-06-25).**
> The [deliveryguy rule](#4-ownership--the-deliveryguy-rule) normally puts a trait's boost of *another* entity's output
> ON that entity as **own-output** (a trait boosting a Merchant's commerce → on the **specialist**, `enabled:{trait}`).
> But a **specialist is ONE shared file**, while a split trait's `SpecialistYield/CommerceChange` has **different values
> in the simple vs complex set** — so inverting it onto the specialist would force a single value across both systems and
> break the clean separation. Therefore, for a TRAIT keyed to a specialist (or any shared sub-city target with a per-set
> value), the deposit takes the **governing-deliverer** shape instead: it lives **on the trait, keyed by the target** —
> `yield.empire.specialists.{SPECIALIST_X}.flat` (and `commerce.…`) — authored in **each set's folder** (simple = the
> base value; complex = the **replacement's** value — a **whole-Info swap, NO base-fill** per the engine's
> `CvInfoReplacements`, owner ruling 2026-06-25 superseding the 2026-06-21 fill-from-base: a field the replacement
> omits is **0/absent** in the complex, never inherited from base — the base SPIRITUAL→PRIEST yield the complex drops
> proved it). The cascade reads it from the **active** trait set and applies it × the city's count of that specialist. *(Building/civic specialist boosts have no
> simple/complex split, so they keep the ordinary own-output inversion onto the specialist.)*

> **⛔ Trait option resolution — the curator translates the CRAZY → sensible; the cascade applies only CLEAN gates
> (owner ruling 2026-06-25; this is the volcano every agent rollerskates into — read it before touching trait values).**
> Several `GAMEOPTION_LEADER_*` options can be live at once (complex, developing, pure, no-negative, …) and each
> mutates a trait's *effective* values. The TB implementation is a runtime hack (`CvInfoReplacements`: a base trait
> carries an inline `ReplacementID` + `ReplacementCondition` `BoolExpr`; `GC.updateReplacements()` swaps the WHOLE
> `CvTraitInfo` in `aInfos[id]` for the first replacement whose condition holds — re-run on state changes). **We do
> NOT emulate that hack anywhere in the cascade.** The split of responsibility is absolute:
>
> - **CRAZY → curator (`curate_trait`), offline, once.** The replacement/promotion-line machinery is dissolved into
>   sensible JSON:
>   - **Simple/complex split** by `COMPLEX_TRAITS` — the two `DefaultTraits`/`DefaultComplexTraits` sets become
>     `traits/simple/` + `traits/complex/`, each self-complete (base overwritten by its `Has(COMPLEX_TRAITS)`-gated
>     replacement, blanks filled from base). The active set is chosen by the live option (callout above).
>   - **Developing line — do NOT auto-develop (verified 2026-06-25).** A `PromotionLine` is a chain of trait *levels*
>     (`TRAIT_NOMAD1`→`TRAIT_NOMADIC2`→`…`, ordered by `iLinePriority`, each with a `PrereqTech`+`TraitPrereq`), but
>     **researching a level's `PrereqTech` does NOT advance the held trait** — proven by dump: a leader holding
>     `TRAIT_INDUSTRIOUS1` reads `extraYieldThreshold=7` (level 1) despite holding `TECH_RENAISSANCE_LIFESTYLE`
>     (the level-2 prereq, which would give 6). The **held trait `/state` reports IS the authoritative level**; the
>     cascade uses its payload as-is. ⚠️ A tech-gated "collapse" that folds higher levels into the entry is the WRONG
>     model (it re-levels traits the engine leaves alone) — it was tried and reverted. Levels advance by some other
>     gameplay progression, not by tech alone; until that's mapped, trust `/state`.
>   - **Complete, not pre-filtered.** The JSON carries ALL values — positive AND negative — plus the `negativeTrait`
>     flag, so the runtime gates below have the full data to act on. The curator never bakes in a pure/no-negative pass.
> - **CLEAN gates → cascade, at eval (its ordinary condition-eval, NOT hack emulation).**
>   - **`PURE_TRAITS` gate (implemented)** — when `GAMEOPTION_LEADER_PURE_TRAITS` is live, drop each trait value whose
>     alignment opposes the trait's: a `negativeTrait`'s **upside** values drop and a positive trait's **downside**
>     values drop (engine `CvTraitInfo` getters keyed on `isNegativeTrait()` + sign). Concretely for thresholds: an
>     `extraYieldThreshold` is an UPSIDE → dropped from a negative trait; a `lessYieldThreshold` is a DOWNSIDE →
>     dropped from a positive trait (engine `getLessYieldThreshold` 2132-2147 sets it to −1). The cascade reads the
>     `negativeTrait` flag (`NegativeTraits*` in the repo) + the live option. This is how "parity comes to us": a
>     legacy behaviour we judge correct is reproduced by a clean gate, never re-implemented as the hack.

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
  bucket, a typed entry is auto-assigned. Leaf is a count (a list when conditioned). ⚠ Here `any` is a **count key**
  (an untyped specialist slot), **NOT** the [json](json.md) §3.4 condition combinator.
- **`allowedSpecialists:{<scope>:{SPECIALIST_X:N}}`** — the manual-assign cap, per-type only (no `any`).
- `free` lives ON TOP of `allowed` (independent). Normally a modifier leaf is `<scope>.<unit>` (e.g. a bare
  number or `.flat`); specialist counts instead use a **count-by-type** leaf (the `SPECIALIST_*` type — or `any`
  — IS the key, its value the count) — the one sanctioned exception, chosen for legibility.
- **freeSpecialists are MODIFIERS, never grants (owner ruling 2026-07-02).** A free specialist is alive **only as
  long as its source is** — building present / civic adopted / trait active — the continuous-deposit shape, not a
  handed-out provision. Every legacy `changeFreeSpecialistCount` apply (civic/trait/building) classifies to THIS
  family; none belongs to the grants machine.

---

## See also
- [json.md](json.md) — the data this machine reads: the modifier-family address, `flat`/`percent`/`multiplier`
  units, `enabled`/`disabled`/`per` conditioning, `plots`/`units` targets, and the `buildRate` vs `production`
  split (§3, §6).
- [enabler.md](enabler.md) — the "can I?" machine. Availability is upstream of magnitude: an unavailable or
  dormant entity deposits nothing.
- [tally.md](tally.md) — the count machine a `per` scaler reads at cross-city scopes.
- [naming.md](naming.md) — the `INFOTYPE_NAME` ids used as deposit keys and condition atoms.
