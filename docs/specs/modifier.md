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

> **⚖ STORAGE SEMANTICS — the SCOPE PRINCIPLE.** Deposits **ACCUMULATE** in a package **AT THEIR OWN SCOPE** — one
> uniform package format (Σflat / Σpercent per channel, §2) held on each scope object (world / team / empire /
> city / plot), each package **event-MAINTAINED** at its own scope only: the fact names the source, the compiled
> index names that source's deposits, and applying them keeps the slot current with nothing marked or deferred
> ([DEC-maintained-sum](../architecture/decisions.md#dec-maintained-sum)). The downward "roll" is
> realized **AT READ TIME**: the realized value is the trivial sum of the ~5 scope packages, with per-city gates
> (state-religion-in-city, coastal, connected, area membership) applied live at the combine. **A lower
> scope never STORES an upper scope's sums** — that would force downward fan-out and "break
> the principle of the cascade in the first place." LOAD is not a rebuild either: the reseed's in-read facts
> apply through the same path play uses.
> (Mechanics: [state-repositories.md](../architecture/state-repositories.md) — the maintained-sum model + the
> two planes, only one of which is ever evaluated.)
>
> **⛔ THE ORIGIN RULE — THE PURE CASCADE DESIGN (owner).** Which half of a package a scope ever fills is not
> incidental, it IS the model:
> - **YIELDS come from exactly three sources — PLOT, SPECIALISTS, and BUILDINGS (city).** Nothing else produces a
>   yield, so the flat/yield side exists at **plot** and **city** only.
> - **MODIFIERS come from everything BUT plot** — city, empire, team, world. The percent side exists at
>   every scope except plot.
>
> Plot and the upper scopes are mirror images (yield-only vs percent-only); **CITY is the one scope carrying
> both**. This is why every scope can hold the SAME package type while many stay half-empty — emptiness is a
> property of the origin rule, never a reason to omit a scope's package or to hand-shape a bespoke struct for it.
>
> **⚖ WHAT THE RULE GOVERNS — ONLY THE CHANNELS THAT ACTUALLY PRODUCE OUTPUT (owner).** *"Only commerce yields
> and base yields actually produce output."* So the rule binds exactly those: the **base yields**
> (food / production / commerce) and the **commerce yields** (gold / research / culture / espionage). Their flats
> are authored at plot and city only, and none authors a percent at plot — that is the origin rule, in full.
>
> ⛔ **Every other family is NOT output, so it is not bound by it — and this is a CATEGORY difference, not a list
> of exceptions.** **Happiness is the worked case: it is a TRANSIENT STATE, not a yield that produces anything**
> — a condition the city is *in*, which changes how other things behave (growth, anger, food consumption) while
> producing no output of its own. Nothing is *made* by happiness. So "where output originates" simply has no
> claim on it, and wellbeing authoring **flats at EMPIRE and AREA** (the civic/tech/trait grants that roll down,
> §2b) is the model working, not an exception to it. The same holds for **plot-scope PERCENTS** — health's
> feature-fallout class (§2b), defense, the property plane.
>
> ⚖ **PROPERTIES are the honest IN-BETWEEN, and the test does not need to resolve them (owner).** You *could*
> argue a property produces output — a value genuinely accumulates and propagates — but what it ultimately does
> is **affect a transient state**, so it sits between the two. That ambiguity costs nothing here: either way it
> is not an output-producing YIELD, so the origin rule does not bind it (the property plane authors plot-scope
> percents), and the property engine is **self-contained by design** — what happens inside it stays inside it
> ([engine.md](../reference/engine.md)), so no classification of it needs to leak outward. ⛔ Do not force
> properties to one side to make the taxonomy tidy; the in-between is the accurate answer.
>
> ⚠ **The word "yield" carries TWO senses, and conflating them is what makes this look like a contradiction.**
> [DEC-universal-yield](../architecture/decisions.md#dec-universal-yield)'s *"every modifiable number is a
> yield"* means **every such number is a CHANNEL in the one machine** — a statement about carriage. The origin
> rule's "yields" means **output-producing yields** — a statement about where output comes from. A family is
> classified by asking *"does this produce output?"*, never by which list it appears on.
>
> **How a non-output family's sides are enforced: BY THE DATA.** Each scope's channel set is **minted from the
> compiled deposits** (KEYS ONLY WHERE NEEDED, [state-repositories.md](../architecture/state-repositories.md)),
> so which sides a scope fills is answered at load, and a read of a side no source authored answers 0 with no
> storage existing anywhere. ⛔ **A read-side roll-up therefore never hand-gates a scope out of its chain** — the
> channel set is the gate; a hand-written one silently deletes an authored family's contribution, and with no
> runtime to catch it (the empire wellbeing flats are the case that bites: 558 authorings).
> **Consequence (owner requirement): every modifier/yield cache consolidates to ONE shape** — the per-family
> hand-named scalar members (`scGpBaseBld`, `scDefense`, `scMaintModCity`, …) collapse into the same
> Σflat/Σpercent-per-channel form the yields and commerce already use, so a new scope or channel is DATA rather
> than a new struct.

This is purely top-down: a condition *inside* a deposit (`enabled`/`per`) is a forward **read** of state, never
an upward cascade-walk. **The reverse view ("who references/modifies me") is derived once at load, never on a
hot path** — realized as reverse edge FAMILIES on the referenced info object itself, populated by the readJson
reverse pass (`EDGEF_RELATED` = the display/pedia candidate lists the tooltips iterate; `EDGEF_REQUIRED_BY` =
the enabler's requires-reverse-index). After load every info ALREADY CARRIES its reverse lookups; no consumer
builds its own scan or side index ([DEC-one-reverse-view](../architecture/decisions.md#dec-one-reverse-view)).

**Three governing rules:** (a) **purely top-down** — sources deposit DOWN, targets read an O(1) accumulator; the
reverse index is cold-path only. (b) **tech-inflation is a downward DEPOSIT, not an upward gate** — a researched
tech deposits down onto everything below it (cheaper/better); the lower thing never reaches UP with a `hasTech`
gate. (c) **info DATA vs engine MACHINERY is a hard boundary** — the JSON carries only values + relationships;
the producers, evaluators, and tally that consume them are engine-side, so authoring stays declarative.

**Every modifiable number is a yield.** ANY number game mechanics modify — base yields, commerce, free XP, free
specialists, property magnitudes, combat percents, heal rates — is a channel in this ONE machine, carried in the ONE
uniform package format (Σflat / Σpercent per channel per scope; the unit is part of the slot key). A number still
computed by a legacy ad-hoc path outside the machine is a shortcut to fold in
([DEC-universal-yield](../architecture/decisions.md#dec-universal-yield)).

> ⚠ **Two shapes get mistaken for exemptions from that last sentence; neither is one.** A **PARTIAL leg** (a
> pre-improvement / nature-only yield) is still yield compute — it is a SEGMENT of the scope's own package
> ([contexts.md](../architecture/contexts.md)), never a per-call walk kept outside the machine because it
> answers a narrower question. A **WHAT-IF** (*"what would this improvement yield here"*) is yield compute too:
> the what-if plane is a READ of this machine (the `expected*` valuations,
> [patterns.md](../architecture/patterns.md)), not licence for a consumer to keep its own yield arithmetic.
> ⇒ The test is the QUESTION, never the caller or the name — a `calculate*` on a game object that sums info
> getters per read is by construction the legacy path this replaces.

**The output-seam.** Where the engine performs placement/application, the machine owns the two ends and the engine
the middle: (1) authored INPUTS are source-centric deposits (a package); (2) placement/application is engine
infrastructure (free-specialist assignment; the golden-age plot-base-yield-threshold "+1"), not modeled; (3) the
OUTPUT yields flow back as a package, consumed exactly like plot yields. Free specialists (amount + forced type →
deposits; engine places; output yields = package) and golden age (length + grant = JSON inputs; plot-threshold
effect = engine middle; extra plot yield = output package) are the exemplars.

---

## 2. The combine arithmetic

Per `(family, member, unit, target)`, the slot composes the three value units ([json](json.md) §3.6):

> **`effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)`**

`flat`s sum into the base; `percent`s (additive deltas) sum then apply once; `multiplier`s compose by product.
`Σflat`, `Σpercent`, and `Πmultiplier` (flats + multipliers stored ×100, identity 100; a PERCENT is NOT
scaled — [DEC-fixedpoint-x100]) are each their own accumulated number —
**the `unit` is part of the slot KEY (per `(family, member, unit, target)`), so a flat sum and a percent sum
are SEPARATE slots, never fields of one mixed struct** — the separation is what lets invalidation split
percent-vs-flat (§1). One `deposit(unit, value)` folds a value into its unit's slot; `effective(base)`
combines them at read.

**All integer, ×100 fixed-point, no float** — Civ4 multiplayer is deterministic lockstep, and CPU-dependent
float math desyncs. The single human→×100 conversion happened once in `readJson` ([json](json.md) §3.6); the
slot does pure integer math and never sees the human boundary.

> **⛔ PLOT SCALING CAN ONLY AFFECT ITSELF — A HARD RULE (owner).** *"I do not think there is any scenario where a
> plot gives 1 hammer per 5 commerce, and as such we codify that as a hard rule, that the plot scaling can only
> effect itself."* A per-plot scaling of a channel reads that channel's own value on that plot and grants THAT
> CHANNEL. There is no cross-channel plot scaling, and none may be authored: a threshold on commerce cannot pay
> out in production.
> ⚑ **It is a structural simplification, not a restriction to police.** With the input and the output on one
> channel, the whole mechanic is plot-local — it needs no cross-scope reach at resolve time, no ordering between
> channels, and no fan-out when one channel moves another. That is what lets it live in the package.
>
> **⚖ THE MECHANIC IS TWO SEPARATE NUMBERS, BOTH FED IN (owner): a THRESHOLD and an AMOUNT.** *"You maintain the
> per-yield threshold, and the amount you get on the per-yield — treat them as 2 separate numbers that get fed
> in."* The interval is "per how much" of the plot's own value; the amount is what each whole interval grants.
> ⚖ **The AMOUNT comes from the `EXTRA_YIELD` global define, and that is fine (owner):** *"we can live with the
> EXTRA_YIELD define for now — we don't need to change that at this point."* ⛔ So a define read here is NOT a
> gap to close and NOT a missing authoring surface; do not "fix" it into curated data. What the ruling requires is
> that it stays a SEPARATE number the plane carries per channel — which it is — so that authoring it later is a
> data change and never a reshape.
> ⚠ **The THRESHOLD does not combine additively, and this is the trap:** the engine selects **the SMALLEST
> POSITIVE threshold held** (`CvPlayer::updateExtraYieldThreshold`), so two sources at 7 and 5 yield 5, never 12.
> A plain flat channel SUMS, so reading one through the ordinary roll-up is wrong by construction — it needs the
> non-additive family metadata this section already defines for `defense`'s floor kind. The AMOUNT is an ordinary
> additive number; only the threshold is a min.
>
> **A plot's yield is ONE base package, resolved in isolation BEFORE the city modifiers.**
> All output from a single plot is computed in **complete isolation** as one base-yield package — `CvPlot::calculateYield`
> per plot ([calc-map](../reference/legacy-value-calc-map.md) §10.1: `calculateNatureYield`(`getBaseYield`=
> terrain+feature+river+hills/peak + bonus) + improvement (floored at `-nature`) + route + the keyed/plots flats,
> `max(0,·)`) — and that result is passed **up the chain**: the city SUMS its worked-plot packages into the §1 `base`.
> **The plot yields ARE "the base the rest is calculated from."** So anything that scales a *specific improvement or
> plot component* resolves **inside** this per-plot package, **before** the city-level `(100+Σpercent)` stack ever runs.
> Today every component-specific buff is **flat** (so the package is a pure sum); should a per-improvement *percentage*
> ever be needed, it applies **here, inside the isolated plot calc** — **never** in the city `(100+Σpercent)` stack,
> which only ever scales the already-summed base. Consequence: a `basePlotYield` divergence is *necessarily* a per-plot
> **flat** miscount (missing or double-counted), because no city-level percentage exists that could move a single plot.
>
> **Completeness is the bar ([DEC-represent-dont-fit](../architecture/decisions.md#dec-represent-dont-fit)).**
> Multiplier deposits are treated as identity on the yield/commerce channels — no source authors one, so the cascade
> is additive, exactly matching legacy. Live acceptance is done-is-observable
> ([DEC-done-is-observable](../architecture/decisions.md#dec-done-is-observable)) + turn time
> ([DEC-turn-time-is-king](../architecture/decisions.md#dec-turn-time-is-king)): [validation](validation.md).

**Non-additive combine, declared as FAMILY metadata (never per-deposit):** a `min` member that floors the
combined total (e.g. `defense`'s floor kind). Authors write signed values; the mode wires the combiner.
⛔ **`naturalDefense` is NOT one of these and never was a kind.** There is no natural-defense channel: BUILDINGS
and CULTURE LEVELS author the SAME `defense.city.amount`, so the cascade holds one additive stack and the legacy
`max(buildingDefense, naturalDefense)` has no counterpart — a data-led behaviour change, not a combiner to
build. ⚠ A worst/best-across-sources mode is therefore **unimplemented and currently unneeded**; do not read
this paragraph as licence to add one speculatively — mint it if and when a family's data actually needs it.

> **⚖ THE FREE-AMOUNT SIGN CONVENTION (owner) — one convention per kind, never a per-source flip.** The
> `upkeep.freeMilitary` / `upkeep.freeCivilian` kinds carry **free-amount semantics throughout**: a POSITIVE entry
> GRANTS free upkeep, a NEGATIVE entry SHRINKS the free allowance. Entries sum like any other channel, and the
> **group total floors at zero as family-combine metadata** (the `min` mechanism above) — distinct from, and
> applied before, the engine's own `net = max(0, upkeep − Σfree)` floor. **Two floors, deliberately: one on the
> group, one at the consumption site.** A pop-scaled source authors `{P, per: {POPULATION, each: 100}}` keeping
> its own sign. ⚠ This is an owner-ruled INTENTIONAL divergence from the legacy asymmetric rounding helper
> (whose `mod<0` branch computed `v×100/(100−mod)`): the ruled shape is **additive linear**, attributed and never
> bit-chased ([validation.md](validation.md) intentional class).

> **⛔ There is NO `polarity` mode — wellbeing is FOUR ORDINARY CHANNELS (owner):** `happiness`, `anger`,
> `health`, `unhealth`. Happiness sums against anger, health against unhealth, at the verdict (§2b). A negative
> deposit is routed to the opposing channel **at fill**, so the split is a routing rule, never a storage shape —
> no good/bad plane, no duplicated positions, no per-family combiner. This is what keeps
> [DEC-universal-yield](../architecture/decisions.md#dec-universal-yield) literal: wellbeing is four yields like
> any other, on the one uniform package
> ([DEC-uniform-cache-shape](../architecture/decisions.md#dec-uniform-cache-shape)).

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
| **worked-plot yields** (`basePlotYield`) | Σ over the city's worked plots of each plot's ONE isolated base package (§2 plot-as-base): `max(0, terrain+feature+bonus)` nature + improvement (floored at −nature) + route + keyed building/civic/trait `plot`-flats + `plots`-target + city-centre constant + threshold/golden-age per-plot | **computed** from the curated plot substrate + engine plot state |
| **trade-route yield** (`tradeYield`) | engine-generated (the trade network) — ⚖ **already carrying its OWN percent layer, see below** | **input** — out-of-scope: the cascade cannot re-derive the network, so the calc *folds the route yield in*, never derives it. **The ONE live-yield input** — a clean addition at the very end of the base, and the sole sanctioned exception to the pollution guardrail ([validation](validation.md), [DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in)). ⚠ **The route COUNT is the OPPOSITE case (owner): `getMaxTradeRoutes` — game + player + coastal + `city.extra` slot deposits — is a modifier-influenced value the cascade COMPUTES, its own `tradeRoutes` channel.** Trade YIELD is read from the engine package; the trade-route COUNT is calculated here. Do not conflate them |
| **free-city yield** (`freeCityYield`) | Σ the player's active traits' `YieldChanges` (`{ch}.empire.flat`) | **computed** — derivable from the trait JSON, so it is COMPUTED, never read off the engine; consuming the live value would leave the trait→yield derivation unvalidated ([validation](validation.md) pollution guardrail). ⚠ NAMING: "free-city" here = the legacy trait accumulator (`CvPlayer::m_aiFreeCityYield`, free yield granted in every city) — **NOT** the WLTKD celebration ("We Love the King/Emperor Day"), whose sole gameplay effect is zero city maintenance ([economy.md](../reference/economy.md)) |
| **golden-age yield** | trait `goldenAge` member (`{ch}.empire.goldenAge.flat`) while in golden age | **computed** (`empire.goldenAge` member-mirror, §3 golden-age carve-out) |
| **specialist yields** (`specialist`) | per assigned specialist: `intrinsic × (100 + specialist-%)⁄100` + building-local (gated `city.flat`) + per-type (`empire.cities.flat` — the `cities` target lands it in the HOLDING city; a bare `empire.flat` on a specialist would roll down to EVERY city and cascade with city count) + perAll + trait governing-deliverer | **computed**. NOTE the specialist carries its **own** percent layer (its intrinsic ×`(100+specialist-%)`) *before* it joins BASE and takes the city `modifier` — two distinct percent stacks |

> **⚖ HOW `tradeYield` STAYS CURRENT — the ONE value the cascade FEEDS but does not HOLD, so it is REBUILT, not
> delta'd.** It is the engine's network OUTPUT (`CvCity::m_aiTradeYield`, ×100 like any amount), not a package
> slot, so the maintained sum does not reach it and no compiled deposit index can name what moves it. Its
> rebuild has four moments and they are the whole set: ONCE at the end of load (against the final cascade);
> TARGETED at the owner whenever a fact moves a `tradeRoutes` channel; on every plot-group / network change
> (which is what covers a city being FOUNDED or ACQUIRED — both reach `updatePlotGroups`); and once per player
> in `doTurn`.
> ⛔ **The per-turn rebuild is NOT the banned blanket** ([DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal)):
> that rule bans papering over a MISSED invalidation, and this one exists because a genuine INPUT advances every
> turn — `getPeaceTradeModifier` scales with the at-peace counter, so a foreign route's profit legitimately
> differs turn to turn until it saturates. There is no fact to route it to; the turn IS the fact.
> ⚠ **City POPULATION deliberately gets NO route, and that is a cadence ruling rather than an omission.** It
> feeds the profit on both sides (`getBaseTradeProfit` reads the PARTNER's population, `getPopulationTradeModifier`
> the city's own), so a route would have to rebuild the owner AND every player trading with it — and it would
> fire once per city GROWTH, i.e. once per city per turn, each firing a full network walk. The mid-turn snapshot
> rule already answers it ([state-repositories.md](../architecture/state-repositories.md): *"getting a yield event
> in the middle of a turn is not retroactive; start of next turn is what is expected"*), and the next `doTurn`
> is that start.

### TIER 2 — EXTRA (flat, added AFTER the percentages, NEVER multiplied)

| EXTRA source | origin |
|---|---|
| **building flat yields** (`BuildingFlatYield100`) | Σ active (non-dormant) buildings' `{ch}.city.flat` + `{ch}.city.perPopulation` × population |

The EXTRA is held ×100; the `100 × ⌊EXTRA100⁄100⌋` **truncates it to whole units** before re-scaling (the engine's
`getExtraYield100` order — a documented integer-truncation gotcha, not a rounding choice).

> For **§2 commerce** the same two-tier shape holds with the channel's own pieces: BASE = the COMMERCE-yield
> (`getYieldRate100(COMMERCE)`) × the channel slider + the §2 baseExtra sub-terms (religion, corporation, golden-age,
> state-religion pool, player-extra, the building-commerce block); EXTRA (post-modifier) = `production × prodToCommerce`.
> The building-commerce block is itself a pure per-building sum over the building's OWN entries (own-flat + tech +
> bonus + perPop + shrine + corp-HQ + the `CommerceChangeDoubleTime` whole-doubling) — and the building-keyed boosts
> (a wonder/civic/tech granting a channel to a building TYPE, `{c}.<scope>.buildings.{B}`) are part of that sum as
> the TARGET building's own reverse-landed conditioned entries: authored deliverer-side (§4), landed at CITY scope
> by the readJson reverse pass, gated on the source's presence at the authored scope. Civil disorder forces the
> whole rate to 0 before any of this.
>
> ⛔ **THE SPLIT IS A CITY/EMPIRE CONCERN — THE PLOT AND THE BUILDING DO NOT CARE (owner).** *"The plot itself does
> not need to care about the commerce split, nor the building, beyond what is written in the tooltip."* A plot
> produces its isolated base package; a building deposits into its channels. **Neither knows or needs to know**
> that the city's COMMERCE yield is later divided into gold / research / culture / espionage by the player's
> sliders — that division happens where the sliders live, at CITY and EMPIRE. So the split never propagates
> downward into a plot or building read, no plot/building surface grows a per-commerce-channel shape for it, and
> the dependency it creates is bounded to **(city commerce yield + slider + active process) → the empire's
> commerce receivers**. The ONE place a lower scope's contribution meets the split is **DISPLAY** — a tooltip
> saying what this building is worth — and that is the [valuation](../architecture/patterns.md) answering a
> resolved delta, not the plot or building carrying split knowledge of its own.
>
> ⛔ **AND THEREFORE A SLIDER MOVE RE-EVALUATES NOTHING — no citizen re-assignment, no plot re-scoring
> (owner).** *"Moving a slider should not really need to reassign citizens; it does not change commerce
> outputs at all, and plots are not evaluated on the commerce yields themselves."* The slider re-divides a
> COMMERCE yield it does not change, and the plot valuation never reads the commerce channels, so every input
> to a citizen decision is exactly what it was. The realized rates pick the new split up at the COMBINE, which
> is the whole of the work a slider causes.
> ⚑ **The measured cost of getting this wrong, because it is the reason the rule is written down:** the setter
> flagged every one of the player's cities for re-assignment, so ONE slider tick re-ran the full citizen
> assignment across the empire and **stalled for fifteen seconds**, of which the entire observable result was a
> couple of dozen facts — a handful of cities moving a citizen, which is the churn of a re-decision that had no
> new input to decide on.
> ⚠ It is the [DEC-flag-is-fossil](../architecture/decisions.md#dec-flag-is-fossil) shape on the AI plane: the
> flag asserted that something a citizen cares about had moved, and nothing had.

### How the percentages "smash together" — ONE additive stack

`modifier` is **a single additive sum** — every active source's `{channel}.<scope>.percent`, added together, then
`max(0,·)`:

- **active buildings** (this city, non-dormant): `city.percent`
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

The city's **health** and **happiness** levels are the §2 combine over **FOUR ORDINARY CHANNELS (owner)** —
`happiness`, `anger`, `health`, `unhealth` — summed in **opposing pairs** at the verdict: happiness against anger,
health against unhealth. They are four yields like any other, carried on the one uniform package with no special
storage: a source depositing a negative value is routed to the opposing channel **at fill**, so nothing about the
combine or the cache is wellbeing-specific.

**⛔ A CHANNEL *IS* THE LEVEL — there is no separate verdict getter, and the distinction that remains is
DEPOSITS vs REALIZED.** Two reads, and conflating them double-counts:

| read | answers | composes with |
|---|---|---|
| the GROUP read (`getWellbeing`) | the DEPOSITS only — the cascade's roll-up over the scope chain | a CANDIDATE's `expectedWellbeing`, which answers in the same vocabulary ([patterns.md](../architecture/patterns.md) § THE TWO READ ROLES) |
| the REALIZED read (`realizedWellbeing`) | deposits **+** the raw-state inputs below | nothing — it is this city's own level |

The raw-state inputs are folded at the REALIZED read, exactly where the engine folds them, so a consumer never
re-derives one. A consumer wanting one side of a pair indexes the array; there is **no per-side getter**.

The **opposing-pair NETS** (`InfoValuation::netHappiness` / `netHealth`) live once on the calc surface, are fed
the four channels rather than an object — which is what lets the same implementation net a city's realized set
and a candidate's expected delta — and are **signed** (a surplus is as meaningful as a deficit). The realized
end-state values are the clamps over them, and are a final-state CALCULATION, never a channel or a getter
([patterns.md](../architecture/patterns.md) rule 6): `healthRate = min(0, health − unhealth)`;
`angryPopulation = clamp(anger − happiness, 0, pop)`.

⚠ The wellbeing channel oracle went with the route-table purge ([http-endpoints](http-endpoints.md)); when the
route table is rebuilt it wants one field per named engine term, so a divergence localises to a single source.

**The TARGET/INPUT split (the tradeYield precedent, [validation](validation.md) input rules):**

- **DEPOSIT-COMPUTED (the cascade's targets)** — everything a live source's `health`/`happiness` family deposits
  produce: **buildings** (city `flat`/`perPopulation` + the empire-scope rollups + conditioned entries incl.
  `HAS_STATE_RELIGION`-gated and the reverse-landed source-keyed boosts — a wonder/civic/tech `buildings.{B}`
  wellbeing deposit is authored deliverer-side (§4) but the readJson reverse pass lands it on the TARGET building
  as a CITY-scope conditioned entry gated on the source's presence at the authored scope, so it reads
  building-side under this term), **civics** (empire flats + the keyed/heterogeneous members read civic-side:
  `features.{F}`, `nonStateReligion`, the `cities.{unit: IS_MILITARY}` per-unit scaler, the ranked `cities`
  scaler — the civic's `buildings.{B}` member lands building-side per the above), **traits** (same member
  vocabulary), **features** (`health.plot.percent` — summed over radius plots, ÷100 — the fallout class),
  **bonuses** (`empire.cities` flats, presence-gated — ⛔ NEVER a bare `empire` flat: that lands in the empire
  package and rolls DOWN to every city, while the engine applies it on the per-city presence fact, so one
  connected luxury is counted once per holding city and the product handed back to every city. The `cities`
  target lands it in the HOLDING city's package, which is what a luxury means — the cities that HAVE it are
  happier. The specialist precedent one entity over,
  [legacy-value-calc-map.md §1.5](../reference/legacy-value-calc-map.md)), **specialists** (city flats; the fractional values are the
  curator's ÷100 de-scale of the legacy latent-×100 — the engine `…/100` at use), **corporations**
  (`HAS_CORPORATION`-conditioned city flats), **techs**/**projects** (empire — projects also the lone `world`
  scope)/**handicaps** (empire flats), and **military units** (`happiness.empire.cities.{unit: IS_MILITARY}`
  §3.7). **Religion happiness has NO religion-side data** (verified: legacy religion info carries none) — the
  state/non-state religion terms derive from CIVIC/TRAIT/BUILDING configs × religion presence.
  ⚖ **Improvement health is a BALANCE-CUT (curator ruling, `curate_improvement.py`):** legacy `iHealthPercent`
  is deliberately dropped from the data, so the engine's `improvementGood/Bad` term is an **intentional
  divergence** — attributed via the oracle's `improvementGood100/Bad100` fields, shown, never chased
  ([validation](validation.md) intentional-model-change class); the term dies at the channel's legacy cut.
  ⚖ **Improvement HAPPINESS, by contrast, IS represented** (owner ruling — no gaps): the intrinsic per-radius
  improvement happiness (`happiness.plot.flat` on the improvement) and the civic per-improvement happiness
  (`happiness.empire.improvements.{I}.flat`) are **folded into the feature happiness terms** (`featSubstrate` +
  `featMember`) — because the legacy `getFeatureGoodHappiness` bundles feature + improvement happiness into ONE
  number. Structurally live end-to-end; **zero data carries it today** (schema-only civic field, no improvement
  authors `iHappiness`), so the verdict is unchanged — the path is future-proof for any modder data.
  **Celebrity happiness** is currently an INPUT; the `skills.celebrity` unit-scan port (the CvCity scan,
  todo.md) is PENDING migration work to finish it.
- **RAW-STATE INPUTS (folded, never derived)** — the runtime timers/counters no deposit produces: the **anger
  percents** (overcrowding = f(pop), noMilitary, foreign-culture, enemy-religion, hurry/conscript/defy/
  revRequest timers, war-weariness, revIndex, civic anger%), the **espionage counters**, **event anger**
  (one-shot event state), **tax-rate unhappiness**, **foreign-culture anger**, **landmark anger** (option-gated —
  ⚖ KEEP through the migration: the existing engine implementation stays, *"straight up state derived from the
  plot in question"*; the landmark data pass is a sanctioned separate data pass (#448); the engine impl KEEPS),
  **city-over-limit**, and **vassal** terms. These are saved/derived-from-saved state — legitimate inputs, since
  no deposit produces them and nothing about them is a cascade output ([validation](validation.md) pollution
  guardrail) — and the calc folds them at the level combine exactly where the engine does.
  ⚖ **The `extraHappiness`/`extraHealth` accumulators are EVENT-GRANTED persisted state (owner ruling), a
  SANCTIONED read, not a ride-in:** the CITY `getExtraHappiness`/`getExtraHealth` are written ONLY by `applyEvent`
  (an event granting extra happiness/health) — genuine one-shot non-derivable state (the event-store class,
  [state-repositories.md](../architecture/state-repositories.md)); the PLAYER accumulator additionally bundles the
  DERIVABLE trait+tech, which the calc NETS OUT (− engine trait/tech + the cascade nets), keeping only the
  event/unattributed residual. Wiring these as proper cascade event grants is **event-rework scope** (#425 events
  stay Python / the F3 grants apply-loop), NOT a modifier-cut ride-in to fix here.
- **GATE FLAGS** — `isNoUnhappiness` / `isNoCapitalUnhappiness` / `isNoUnhealthyPopulation` /
  `isBuildingOnlyHealthy` zero their side wholesale. They are **HARD OFF-SWITCHES, never modifiers (owner)**:
  while such a building is present *"unhappiness does not exist in the city"* — the side ceases to exist rather
  than being reduced. The building half lives in `attributes` (json §8) as a held city-scope intrinsic, read by
  `CvBuildingInfo::isNoUnhappiness()` and its siblings and counted city-side (`changeNoUnhappinessCount`).
  ⛔ **ZERO buildings author one, and that is DELIBERATE, not a data gap — the mechanic is "wildly overpowered"
  (owner).** So the chain is wired and every read answers false: it is live-but-inert HEADROOM (the
  corporation-obsolete class, [culture-religion-research.md](../reference/culture-religion-research.md)).
  Finding a wired chain with no authorings is therefore never licence to author one, and equally never a reason
  to purge the flag as unused.
- **`unhealthyPopulation`** (= `max(0, pop − angryPop)` unless flagged) enters the BAD side as the engine's
  population term — a state-derived input (it reads the happiness verdict; the calc computes it from its own
  happiness result, never reads the engine's).

⚠ Two engine quirks the calc reproduces verbatim — named here so the reproduction is DELIBERATE and visible rather
than accidental. Whether they survive is a SPEC decision (the spec leads), never a silent "fix" at a call site:
`badHealth` adds `min(0, extraBuildingBadHealth)` **twice** (once inside `totalBadBuildingHealth`, once
directly); and the anger percents scale by `pop/PERCENT_ANGER_DIVISOR` with truncating integer division.

**⛔ TRAVELING UNIT MODIFIERS RIDE ON TOP (GENERAL — all channels).** A modifier that
TRAVELS with a unit (unit-sourced happiness, anger, property emission, and any future unit-carried channel
value) is **never part of a cached cascade computation**: it is computed LIVE at read and **added on top as a
FLAT term, after and outside every percentage modification**. Two structural consequences: (1) unit movement
never dirties any cache — the cached sums are unit-free by construction; (2) the traveling value is a plain
flat addition to the realized number, never an input to a percent stack. The implementation shape: the cache
stores the unit-free number (+ any epoch-stable per-unit multiplier, e.g. a civic's per-military-unit VALUE);
the read folds `perUnit × liveCount` / the live unit walk on top (an O(1)-ish live engine read).
**The AUTHORING BAN that keeps this coherent: no unit gives — or can ever be
ALLOWED to give — PERCENTAGES to yields of any kind.** A unit-carried value is always a raw flat number on
top; a unit-authored percent would force units back inside the cached percent stacks and break the whole
on-top model. Enforceable at the curator/validation layer: a `units/**` JSON authoring a yield/commerce
`percent` deposit is a data error.
Ledgered as [DEC-unit-modifiers-on-top](../architecture/decisions.md#dec-unit-modifiers-on-top).

> **⚖ THE COMMANDER RIDES ON TOP OF A UNIT EXACTLY AS A UNIT RIDES ON TOP OF A CITY (owner).** *"Whatever a
> commander does is on top, it is not part of the unit itself — it is literally the combat calc's job to check
> if the commander has points left to add to the attack."* So this is the SAME rule one scope down, not a new
> one: the commander→unit relationship is the unit→city relationship, and everything above applies unchanged.
> - The unit's RESOLVED values ([state-repositories.md](../architecture/state-repositories.md) UNIT plane) are
>   **COMMANDER-FREE by construction**: they gather the unit's own info ∪ its promotions ∪ its unit-combat
>   classes, and nothing else. A commander attaching, detaching or moving is neither a promotion nor a
>   combat-class change, so it must never be a cache input — there is no fact that would move it, and baking it
>   in yields a plausible, permanently stale number the moment the commander moves.
> - The commander's contribution is added **LIVE, ON TOP, at the COMBAT CALC**, which is also the only place
>   that can ask the question the mechanic actually turns on: **has this commander got control points left to
>   spend on this attack?** A stat read cannot answer that, which is why the fold does not belong in one.
> ⛔ So a per-unit stat getter that reaches through `getCommander()` and adds the commander's own accumulator is
> the wrong shape twice over: it puts a traveling modifier inside the unit's own number, and it spends the
> commander's points without ever checking whether any remain.

**UNIT-driven wellbeing is END-TURN cadence.** The military/unit-count happiness
term recomputes **once per turn** (the substrate's turn-roll), NEVER per unit move — a per-move mark hook made
every post-move rate read pay the wellbeing walk (a measured unit-automation collapse) and is banned. The
within-turn lag this leaves on the wellbeing slots (a handful of cities whose garrison changed mid-turn) is the
RULED cadence, not a freshness hole; the getter flip proceeds with it.

**The STORED-ACCUMULATOR DRIFT class.** The legacy wellbeing terms are
INCREMENTAL SERIALIZED accumulators (`m_iBonusGood/BadHappiness`, `m_iBuildingGood/BadHappiness`,
`m_paiStateReligionHappiness`, `m_iExtraBuilding*FromTech`, …) — event-sourced numbers that carry decades of
save history. **The old cache model folded event-type grants DIRECTLY into these caches** (there is no separate
event-yield data — the per-building `m_aBuildingHappy/HealthChange` ledgers carry nothing on real saves), so a
stored value that disagrees with its current-state recompute is **DRIFT (history pollution), never
event state to preserve**. The oracle emits a `*Recomputed` twin beside each incremental accumulator
(bonus/building/stateReligion, happiness + health; the `extraBuilding`/`feature`/`religion` city accumulators
self-heal via the engine's `update*()` rebuilders and need no twin). Parity discipline: a verdict diff equal to
`Σ(stored − recomputed)` is **engine-wrong / cascade-right** — attributed-accepted (the same class as the
improvement-yield phantoms), repaired wholesale when the slots recompute from data.

---

## 3. Conditioning — re-applied when its own dependency moves (the dormancy model)

A deposit may carry `enabled` / `disabled` / `per` ([json](json.md) §3.7, §3.9). A deposit's condition uses the
**same vocabulary** as the enabler's `requires` — the same `all`/`any`/`noneOf` tree over the same atoms and
predicates — so a conditioned deposit is, in essence, **a `requires`-shaped gate with an output attached**: the
enabler resolves that shape to *availability* ("can I?"), the modifier resolves the *same* shape to a *magnitude*
("how much?").

> **⛔ A condition is a PREDICATE, never a bespoke sub-scope MEMBER ([DEC-conditions-are-predicates]).**
> A deposit that applies only under some game state — only in the capital, only during a golden age,
> per military unit — carries that state as a **predicate** in its `enabled`/`disabled` (or a `per`/`unit:` scaler,
> §[json](json.md) §3.7), at the deposit's normal scope: `{family}.empire.percent` + `enabled:"IS_CAPITAL"`, NOT a
> bespoke `{family}.empire.capital.percent` member. **The predicate registry is EXTENSIBLE** — if the condition has
> no predicate named verbatim yet ([json](json.md) §3.5), **define a new one** (spec + evaluator + the state fact
> it reads); that *extends* the model. Encoding the condition as a new **member** instead *changes the core
> structure* — the kraken way, and the exact shape (`byEra`, `empire.capital`, `perMilitaryUnit`) agents keep
> re-inventing. Retire any such member to a predicate-gated deposit.
>
> **Exception — golden age.** Golden-age yield/commerce is applied by the **core
> engine** and is **not defined as data anywhere** — modelling it via `IS_GOLDEN_AGE` would mean authoring it
> virtually everywhere it fires. So `empire.goldenAge` is a **PERMANENT engine member-mirror (effect-only)** (NOT a
> retire-now invention); golden-age **LENGTH + grant ARE curated JSON** (`goldenAge.empire.percent`,
> `grants.goldenAge`). The `IS_GOLDEN_AGE` predicate exists ([json](json.md) §3.5) ([golden-age](../reference/golden-age.md)).

**But they are SEPARATE FIELDS, not one condition** — because a thing can **require one condition yet gate its
effect (a buff *or* a nerf) on another**: a Forge `requires` connected iron to *operate*, but its +1 happiness is
`enabled` by *power*, not iron — and the magnitude can equally be negative (e.g. −production while polluted). So
the entity carries its `requires` once (whole-entity availability — the [enabler](enabler.md)'s job), and each
deposit carries its **own** `enabled`/`disabled` (does *this effect* apply). Same condition language, two
independent fields.

**⛔ NOTHING IS RE-CHECKED ON A RECOMPUTE, BECAUSE THERE IS NO RECOMPUTE.** A conditioned deposit is applied
`±value` by **the ATOM's own verdict crossing**, and a `per`-scaled one by `±value × Δcount` from **the COUNT's
own fact** — the two routed planes of the maintained sum
([state-repositories.md](../architecture/state-repositories.md) § THE MAINTAINED SUM;
[DEC-maintained-sum](../architecture/decisions.md#dec-maintained-sum)). That re-application *is* the dormancy
model: a deposit whose `enabled` stops holding (or whose `disabled` starts) is withdrawn from the slot at that
instant — the source goes quiet without being removed.
⚑ **Both routes are reverse indices derived from the compiled deposit index** (atom → the deposits it gates,
count-key → the deposits it scales), so the cost is the deposits that atom or count actually touches — never a
walk of the scope's deposits asking each whether it cares, and never a sweep of the entity database.

> **⛔ THE TWO INDICES ARE KEYED THE SAME WAY AND ARE NOT INTERCHANGEABLE — asking the wrong one answers EMPTY,
> which is indistinguishable from "nothing is conditioned on this".** A condition atom's `type` interns into the
> **ATOM** index (`gatedByType`); a `per` scaler's token interns into the **COUNT** index (`gatedByToken`). Both
> are keyed by a plain string, so `"ERA"` is a legal key in either — and a route that reaches for the wrong one
> compiles, runs, reports nothing, and moves nothing.
> ⚑ **The tell is that a bare TOKEN can appear on both sides.** Most atoms are `INFOTYPE_NAME` ids and most
> count-keys are tokens, so the two key spaces look disjoint until a family uses a token as a THRESHOLD:
> `{type: "ERA", max: 1}` is a condition (atom index), while `per: {type: "ERA"}` would be a scaler (count index).
> ⇒ **When wiring a route, decide which QUESTION the deposits ask — "is this gate true?" or "how many?" — and
> take the matching index. Where a family is authored both ways, route BOTH.**
> ⚠ An empty list is silent by design (the route census reports nothing when the list size is zero), so a
> mis-keyed route leaves no trace at all. **Report the real list size, never a placeholder** — that count is the
> only thing that distinguishes a route with nothing to do from a route asking the wrong question.
>
> **⛔ AND A THRESHOLD IS NOT A PRESENCE CROSSING, so it cannot ride the ±1 atom route.** An `ERA`/`POPULATION`
> threshold has no held/not-held verdict for the as-if-held hypothetical to pin: when the counter moves, some
> deposits turn OFF and others turn ON in the same step. Such a gate is **RE-RESOLVED against the new state and
> moved by the DIFFERENCE** from what the slot already holds — which handles both directions in one pass and is
> idempotent if the fact is seen twice. The `±value` crossing form is only ever correct for a genuine presence
> atom.

- **`enabled` then `disabled`** — `enabled` is read first, `disabled` second; a `disabled` that holds overrides
  ([json](json.md) §3.9).
- **`per`** scales the deposit by a count — local at `city`/`plot`, via the [tally](tally.md) at cross-city scopes.
- Whole-entity availability (is this building active at all?) is the [enabler](enabler.md)'s `requires`, not a
  per-deposit condition: a dormant entity deposits nothing, so the modifier machine never special-cases it.
- **Age-gated deposits** — legacy `CommerceChangeDoubleTimes` ("double after N YEARS") is **not** a timer/stage
  but a SECOND deposit on the same slot with `enabled:{existedFor:{min:N}}` (no post-sum multiply). ⚠ The unit is GAME
  YEARS, not turns — the age is measured against the stored build YEAR, and that is what the tooltip has
  always promised ([json.md §3.5](json.md)).

  > **⚖ THE TURN BOUNDARY IS THE AGE GATE'S FACT, AND IT CARRIES EVERYTHING THE GATE NEEDS (owner).** *"Start
  > turn should be an event, like anything else, that has turn number, which should give cascade what it needs
  > to figure it out."*
  > ⚑ **This is the one condition class whose dependency is ELAPSED TIME.** No source moves, no count moves and
  > no atom crosses when a build becomes due — so there is nothing else in the engine that could announce it,
  > and the age gate is the only member of the family that needs a cadence fact at all. The turn number is the
  > whole of the input; the deposit's own stored build year supplies the rest.
  > ⇒ **It rides the PLAYER-scoped turn-started fact**, whose cities are the ones whose builds can come due, and
  > it is a RE-BOOK by value difference rather than a `±1` crossing (an age gate has no held/not-held verdict to
  > pin, exactly as a threshold has none).
  > ⛔ **It is NOT the banned per-turn blanket** ([DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal)):
  > the worklist is exactly the deposits the `existedFor` reverse index names, and a turn on which nothing came
  > due moves nothing. It satisfies the sanctioned event-triggered recalc test
  > ([contexts.md](../architecture/contexts.md)) — a genuine DOMAIN fact, a NON-LOCAL consequence the fact cannot
  > name, and no finer route to derive.
  >
  > **⛔ THE APPLY PATH MUST SET THE CARRIER SLOT, OR THE GATE ANSWERS FALSE EVERYWHERE.** `existedFor` asks about
  > the DEPOSITING entity, so it reads `sourceBuilding` off the eval ctx and answers FALSE when nothing set it
  > ([contexts.md](../architecture/contexts.md) § THE SOURCE SLOTS — deliberately, since resolving it against
  > whichever entity a walk reached last is worse than declining). Every walk that resolves a building's entries
  > therefore sets it: the plane-A city apply, the re-book routes, and the gather alike. ⚠ Setting it in the
  > GATHER alone puts the oracle and the stored plane on different answers for this whole class — the two sides
  > then disagree by construction, which is a divergence no missed emit explains.

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

> **⛔ THE TWO TRAIT SETS ARE COMPLETELY SEPARATE — SEPARATED BY ID, NOT ONLY BY FOLDER (owner).**
> A leader's traits resolve to ONE `CvTraitInfo` table from *either* its simple set (`traits/simple/`, the
> `DefaultTraits`) *or* its complex/Thunderbrd set (`traits/complex/`, the `DefaultComplexTraits`), chosen at runtime
> by **`GAMEOPTION_LEADER_COMPLEX_TRAITS`**. The curator emits both as **two cleanly-separated, self-complete folders**
> (`traits/simple/` + `traits/complex/`); a consumer **loads the one active folder** by the live game option — this is
> NOT an entity-level option gate and NOT a mid-game swap (any WorldBuilder mid-game trait swap is a post-migration
> concern).
> **A complex trait KEEPS ITS OWN `TRAIT_COMPLEX_` IDENTITY** ([naming.md](naming.md): `TRAIT_` is a simple trait,
> `TRAIT_COMPLEX_` a complex one). ⛔ It is NEVER re-keyed onto the base trait's id: that re-key is what
> manufactured the colliding-id problem — two genuinely different entities answering to one name — which then
> forced every reader to disambiguate by game option and made a wrong read silently return wrong magnitudes.
> Distinct ids remove the ambiguity by construction rather than by discipline.
> ⚖ **A COMPLEX-ONLY RUNG OF A SPLIT LINE TAKES THE PREFIX TOO (owner).** A developing line's upper rungs exist
> only in the complex set (the simple ladder tops out early), so they are not `CvInfoReplacements` variants — and
> keeping their authored id left a chain reading `TRAIT_COMPLEX_SEAFARING` → `TRAIT_COMPLEX_SEAFARING1` →
> `TRAIT_SEAFARING2`. **The LINE is the complex variant, so every rung of it is**, whether or not that particular
> rung has a simple twin. The test is the rung's LINE, never the rung's own id.
> **⚖ IT IS A TYPE RENAME, AND THE SAVELOAD MECHANISM TRANSLATES IT (owner).** A renamed Type is NOT a removed
> one: the record still exists under a new id, so resolving the old name to `-1` and letting the allow-missing
> class read drop the slot throws away a rung the player still holds. ⛔ The earlier ruling here — that the loss
> was "accepted deliberately" — is SUPERSEDED: the old id is mapped to the new one in `Assets/savemigration.txt`
> (a bare `INFOTYPE_NAME` key, which cannot collide with a `Class::field` rename) and applied at the ONE
> stored-Type resolution point the class reads share.
> ⚠ The distinction generalizes beyond traits, and [save.md §7](save.md)'s three removal classes do not cover it:
> that decision procedure asks what to do when a Type is GONE. Ask first whether it is gone or merely RENAMED —
> only the first is a removal.
> ⛔ **The re-key has ONE definition, on the STORE (`Store::trait_rekey`), applied where the inverted edges are
> handed out** — because a trait id is named from several curators, above all the TECH edge that GATES a rung
> ([enabler.md](enabler.md): without it every upper rung is permanently unreachable, and silently so). A
> per-curator copy would drift and emit an id no record defines.
> **⛔ EVERY RECORD IN THE COMPLEX SET CARRIES `TRAIT_COMPLEX_`, WITH NO EXCEPTIONS (owner).** *"If it was built
> as complex, it's complex, no matter what."* The prefix STATES THE SET — it is not a marker for "is a variant of
> a simple trait" — so a complex-ONLY line with no simple counterpart is `TRAIT_COMPLEX_` like every other record
> in the folder. Folder and prefix agree by construction.
> ⚑ **The reason is that the alternative makes a WRONG SET UNDETECTABLE (owner): *"otherwise it will be
> impossible to truly distinguish between the simple and complex set."*** If a plain `TRAIT_` id were legal
> inside the complex set, a held `TRAIT_EFFICIENT1` would be indistinguishable from a simple-set leak — no
> consumer, log line or check could tell the two apart. With the prefix stating the set, any plain `TRAIT_` id in
> a complex game is unambiguously wrong, and that is the property the ids exist to have.
> ⚠ A record that does not obey this is a CURATOR defect, and fixing it rides the curator + regen in the same
> work item ([DEC-recurate-on-decision](../architecture/decisions.md#dec-recurate-on-decision)); the id change is
> a TYPE RENAME the save layer translates via `Assets/savemigration.txt` (the rename rule below), never a removal.
> ⚑ The one remaining shared id is **`TRAIT_BARBARIAN`**, the NPC trait base-filled into `complex/` so that set
> stays self-complete (below) — the only simple trait with no complex variant.
> (The enabler is unaffected either way: it reads trait *presence*; only the modifier cascade reads trait
> *family values*.)
>
> **⛔ Inverted-onto-a-SHARED-entity boosts stay on the TRAIT, per set — the own-output carve-out.**
> The [deliveryguy rule](#4-ownership--the-deliveryguy-rule) normally puts a trait's boost of *another* entity's output
> ON that entity as **own-output** (a trait boosting a Merchant's commerce → on the **specialist**, `enabled:{trait}`).
> But a **specialist is ONE shared file**, while a split trait's `SpecialistYield/CommerceChange` has **different values
> in the simple vs complex set** — so inverting it onto the specialist would force a single value across both systems and
> break the clean separation. Therefore, for a TRAIT keyed to a specialist (or any shared sub-city target with a per-set
> value), the deposit takes the **governing-deliverer** shape instead: it lives **on the trait, keyed by the target** —
> `yield.empire.specialists.{SPECIALIST_X}.flat` (and `commerce.…`) — authored in **each set's folder** (simple = the
> base value; complex = the **replacement's** value — a **whole-Info swap, NO base-fill**, per the legacy
> replacement semantics: a field the replacement
> omits is **0/absent** in the complex, never inherited from base). The cascade reads it from the **active** trait
> set and applies it × the city's count of that specialist. *(Building/civic specialist boosts have no
> simple/complex split, so they keep the ordinary own-output inversion onto the specialist.)*
>
> **⛔ Trait option resolution — the curator translates the CRAZY → sensible; the cascade applies only CLEAN gates
> (this is the volcano every agent rollerskates into — read it before touching trait values).**
> Several `GAMEOPTION_LEADER_*` options can be live at once (complex, developing, pure, no-negative, …) and each
> mutates a trait's *effective* values. The TB implementation was a runtime hack — **deleted from this tree**, and
> described here only so it is never rebuilt: a base trait carried an inline replacement id + a `BoolExpr` condition,
> and a global re-run swapped the WHOLE `CvTraitInfo` in place for the first replacement whose condition held. **We do
> NOT emulate that hack anywhere in the cascade.** The split of responsibility is absolute:
>
> - **CRAZY → curator (`curate_trait`), offline, once.** The replacement/promotion-line machinery is dissolved into
>   sensible JSON:
>   - **Simple/complex split** by `COMPLEX_TRAITS` — the two `DefaultTraits`/`DefaultComplexTraits` sets become
>     `traits/simple/` + `traits/complex/`. **`complex/` is SELF-COMPLETE = a SUPERSET of `simple/` (owner ruling
>     2026-07-21):** every trait is present, so the option-gated active-set read (`getTraitInfo`/`MMKernel::traitData`)
>     NEVER falls back to a simple record under `COMPLEX_TRAITS` — the read can be made fail-loud with nothing to fall
>     back to. Each id's complex def is its `Has(COMPLEX_TRAITS)`-gated **replacement** where one exists (WHOLE-SWAP, no
>     base-fill — a field the replacement omits is absent, mirroring the legacy replacement semantics); a simple trait
>     with **no** replacement is base-filled into `complex/` whole (its base IS the complex version — e.g.
>     `TRAIT_BARBARIAN`, the NPC-civ trait). **Folder classification** keys on the `OnGameOptions: COMPLEX` gate /
>     replacement-variant; a developing-line (`PromotionLine`) member that UNIQUELY lacks the gate its siblings carry is
>     a SOURCE-data bug to fix (restore the tag), not a classifier change (the `TRAIT_TIMID1` case). The active set is
>     chosen by the live option (callout above).
>     **⛔ TRAITS ARE NOT CONTENT-LOCKED — THE CURATOR IS THE AUTHORITY AND THE FOLDERS ARE REGENERATED (owner).**
>     The lock made the two folders hand-maintained, which is precisely how they drifted: a hand edit could put an
>     edge in one set pointing at an entity only the other set has, and nothing regenerable existed to correct it.
>     So `curate_trait` reads the legacy XML like every other curator and `--write` rewrites both folders.
>     ⚑ **Its input is the ARCHIVED XML** (`SourceArchive/Assets/**`, searched by `store.py` alongside the live
>     roots): the trait XML was removed from `Assets/` once its JSON landed, and it is curator INPUT there and
>     nowhere else — never a game load path ([DEC-no-xml-into-game](../architecture/decisions.md#dec-no-xml-into-game)),
>     and unrelated to the red-ratchet ban on reviving a `CvXInfo` from `SourceArchive/Infos/`.
>     ⚠ Community-owned trait CONTENT still lands through `_additions/` like any other post-curation authoring
>     ([curators/README.md](curators/README.md)), which is what the lock was reaching for — a regenerable base with
>     an overlay, not a frozen folder.
>   - **⛔ THE LADDER EDGE IS RESOLVED FROM LINE MEMBERSHIP, NEVER FROM THE ID SPELLING.** A rung `enables` the rung
>     above it ([json.md §9](json.md): a ladder is an `enables` edge, not a section), and which rung that IS comes
>     from the line's members ordered by `iLinePriority` — restricted to the FOLDER being emitted, so a chain simply
>     ends where that set ends (`simple/` tops out at rung 1) and never reaches into the other set.
>     ⚠ Deriving the successor by string arithmetic on the id (`TRAIT_X1` → `TRAIT_X2`) is WRONG and fails silently
>     in three ways the data actually contains: a line that RENAMES mid-chain (`TRAIT_NOMAD1` → `TRAIT_NOMADIC2`)
>     gets an edge to a fabricated id no record defines; a line whose ranks SKIP loses the link entirely; and a top
>     rung gets an edge to a rung above it that does not exist. The base rung is `iLinePriority` 0/absent, and the
>     two arms (`+1,+2,+3` and `-1,-2,-3`) each chain outward from it, so a line carrying both FORKS at the base.
>   - **Developing line — do NOT auto-develop (engine-verified).** A `PromotionLine` is a chain of trait *levels*
>     (`TRAIT_NOMAD1`→`TRAIT_NOMADIC2`→`…`, ordered by `iLinePriority`, each with a `PrereqTech`+`TraitPrereq`), but
>     **researching a level's `PrereqTech` does NOT advance the held trait**. The **held trait the engine reports IS
>     the authoritative level**; the cascade uses its payload as-is. ⚠️ A tech-gated "collapse" that folds higher
>     levels into the entry is the WRONG model (it re-levels traits the engine leaves alone). Levels advance by some
>     other gameplay progression, not by tech alone; until that's mapped, trust the engine's own reading.
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

> **⚖ AN EMPIRE→CITIES DEPOSIT HAS TWO LEGS, FOR THE SAME REASON THE AMENITY FOLD DOES**
> ([contexts.md](../architecture/contexts.md) § THE FOLD HAS TWO LEGS). A source above city scope delivers its
> CITY-scope deposits by fanning over the owner's cities — which reaches exactly the cities standing **at that
> moment**, and that is not all of them:
> - **at LOAD the emit order is not uniform**, and nothing makes it so: some empire-level facts are announced
>   before the cities deserialize and some after, so one grantor's fan lands and the next one's iterates an empty
>   list. A fan alone therefore delivers a subset decided by where a member happens to sit in a read.
> - **at PLAY a city that starts existing later** — founded, or acquired — receives nothing from what its owner
>   already holds, permanently.
>
> ⇒ **The second leg is the CITY's: when a city starts existing it folds the city-scope deposits of every source
> its owner already holds.** The trigger is the city's own OWNERSHIP fact, which is the one announcement common
> to founding, conquest and the save read alike — so there is no separate load pass and no city-founded special
> case beside it.
> ⛔ **It must be IDEMPOTENT rather than guarded.** The package already records which sources have deposited into
> it (the same liveness key planes B and C test), so the fold SKIPS what the fan already delivered. Suppressing
> the fan during load instead would work only while a hand-written guard stays in step with an emit order nobody
> controls; the package's own record cannot disagree with what was applied.
> ⚠ This is not a rebuild and not a recompute — the worklist is the owner's HELD sources, each resolved through
> the one per-entry evaluator ([DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)).

- **scope-wide** — no target: the scope object itself (the city is the common case).
- **plural object-target** (`plots` / `units` / …, predicate-filtered) — realized by evaluating the predicate
  against **every object of that kind in scope** and depositing onto each match. One uniform mechanism: an
  empire-wide sea-tile buff is `production.empire.plots {IS_WATER}`, applied to every worked water plot. This
  retires all the legacy per-plot-type / per-tile accumulators.
- **named-entity key** (`improvements.{FARM}`, `terrains.{…}`, `buildings.{…}`) — a deposit onto a specific
  named target, kept on the source (the deliveryguy, §4).

> **⛔ A KEYED DEPOSIT IS READ AS AN ENTRY-LIST READ OVER THE LIVE SOURCES — never off a scope package.** Outside
> PLOT scope a keyed entry deliberately does **not** fold into the scope's Σflat/Σpercent slots (only the plot's own
> substrate keys resolve there, §2 plot-as-base; the `empires` fan is the one target whose fold IS the deposit). So
> a consumer answering *"how much does this source give THIS target"* asks each live source what IT deposits onto
> that key — the city's OPERATING buildings, its assigned specialists × count, the empire's held traits — and sums.
> ⚑ **Why it is a rule and not a detail: folding a keyed entry into the scope slot is silently, plausibly WRONG.**
> A building's `experience.city.unitCombats.{UNITCOMBAT_MELEE}` folded scope-wide would hand EVERY unit trained
> there the melee-only experience — a number that looks reasonable, breaks no invariant, and nothing catches. The
> package answers the scope-wide leg; the keyed axes are read beside it.
> ⚠ The read is per-source and cheap because it iterates the handful an entity AUTHORED
> ([DEC-materialize-at-mapfrom](../architecture/decisions.md#dec-materialize-at-mapfrom)); it is never a walk of a
> keyed container the info no longer holds, which is the own-data inversion
> ([pedia-read-map](../reference/pedia-read-map.md) finding 2).
>
> ⛔ **A KEYED READ SERVES THE UNCONDITIONED ENTRIES ONLY — the conditioned tail is the VALUATION's, exactly as
> it is on the point plane** ([patterns.md § THE GETTER SETUP](../architecture/patterns.md): the compiled sum,
> the conditioned list and the `expected*` what-if are three distinct reads, and a keyed deposit needs all three
> just as a scope-wide one does). A keyed walk that sums the tail applies every tech-gated and age-gated deposit
> from turn 0 — silently, because the number stays plausible. ⚑ The keyed twin of `expected*` is what serves
> that tail (through the ONE evaluator against the contexts); until it exists a keyed+conditioned deposit is
> honestly UNSERVED, which is the correct exposed state rather than a gap to paper over with an unconditional
> sum ([DEC-no-legacy-masking](../architecture/decisions.md#dec-no-legacy-masking)).
>
> ⚠ **THE DIRECT-KEYED ADDRESS IS A REAL SHAPE, AND ITS SENTINEL MUST NOT COLLIDE WITH "NOT AUTHORED".** A
> named-entity key may sit straight under the scope with no plural container token
> (`allowedSpecialists.city.{SPECIALIST_X}`, `religion.city.{RELIGION_X}`), so the compiled entry carries NO
> target-segment. A read that treats "no segment" as a failure answers 0 for every such address while the caller
> passes the right family, kind and target — invisible, because nothing errors. The two meanings are opposite
> intents and each needs its own value: *this address carries no container token* vs *that token was never
> authored anywhere*.

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
  > **⚖ THE RESTRICTION IS THE CARRIER'S AND GOVERNS ITS WHOLE HOLD — including capacity a PROMOTION grants
  > (owner).** WHAT a carrier may take is a property of the carrier; HOW MUCH sums from every source. So a
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
  > ([validation.md](validation.md) intentional-model-change class; the spec leads, legacy behaviour is not
  > preserved for its own sake).
  > ⚠ Consequence: a carrier whose base capacity is 0 still has a restriction to state, and the §3.9 entry
  > grammar has no payload-less form for it — the open item in [todo.md](../plans/structural-cleanup/todo.md).
  ⚖ **The "what" is ALWAYS a TAG predicate — that is what tags are for (owner).** The legacy restriction by
  `SPECIALUNIT_*` group (`SpecialCargo` / `SMNotSpecialCargo`) brings no new qualifier form with it: it authors as
  the same `{unit: IS_<TAG>}` shape as the domain case. ⚠ It does require the tag to exist AND to be
  DISCRIMINATING — several legacy groups are indistinguishable on the current tag set (people and troops are both
  merely `landUnit`; fighters and seaplanes both merely air/military), so converting one before its tag is minted
  silently WIDENS what the carrier accepts. Mint the tag first; that is ordinary open-registry authoring
  ([tags.md](tags.md)).
  ⚖ **Capacity has ONE home, and Size Matters DERIVES from it (owner):** `smSpace` follows from how many units
  the carrier can hold, so it is never a second authored number ([json.md §9](json.md)).
- **`cargo.size`** — the unit's cargo **footprint** (room it occupies when loaded), **defaulting to 1** if unset.
  (SizeMatters extends cargo via `smSpace`/`volume`/`volumeModifier` — a separate rework.)

No bespoke host↔cargo family is needed. The full unit-stat family vocabulary
(`strength`/`withdrawal`/`firstStrike`/… ) is [json](json.md) §6; this is the largest surface and lands last.

> **Movement & range** are their own resolver subsystem, not ordinary downward families: `moveCost` is computed
> **per `(unit, edge)`** with a route `min`-override, double-move divisors, and a floor — it doesn't fit the
> "deposit DOWN → O(1) summed read" shape. **But the RESOLVER being bespoke does not make its INPUTS intrinsic
> (owner): a plot substrate's base movement cost IS the `movement` family** — `movement.plot.flat` on the
> terrain / feature / route — and it composes with the cascading deltas (tech route changes, promotion move
> bonuses) in the ordinary way, as the §3.9 entry list. The route case shows it directly: the base cost is the
> bare number and a tech-gated change is a conditioned entry beside it, in one slot.
> ⚑ The distinction to hold: **the resolver reads the family and applies its own arithmetic** (min-override,
> divisors, floor). What was wrong was parking the base value in `identity`, which carries no effects
> ([json.md §7](json.md)) — a movement cost is plainly one. (Detail: the movement subsystem doc, pending.)

### Specialist counts

- **`freeSpecialists:{<scope>:{any:N, SPECIALIST_X:M, …}}`** — granted specialists; `any` = an assignable-slot
  bucket, a typed entry is auto-assigned. Leaf is a count (a list when conditioned). ⚠ Here `any` is a **count key**
  (an untyped specialist slot), **NOT** the [json](json.md) §3.4 condition combinator.
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
  family; none belongs to the grants machine (`specialists` is not in the json.md §5 grants vocabulary; *if
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

## See also

- [json.md](json.md) — the data this machine reads: the modifier-family address, `flat`/`percent`/`multiplier`
  units, `enabled`/`disabled`/`per` conditioning, `plots`/`units` targets, and the `buildRate` vs `production`
  split (§3, §6).
- [enabler.md](enabler.md) — the "can I?" machine. Availability is upstream of magnitude: an unavailable or
  dormant entity deposits nothing.
- [tally.md](tally.md) — the count machine a `per` scaler reads at cross-city scopes.
- [naming.md](naming.md) — the `INFOTYPE_NAME` ids used as deposit keys and condition atoms.
