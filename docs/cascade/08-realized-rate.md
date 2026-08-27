# 2a. The realized RATE — what is BASE, what is added AFTER the percentages

> Part of the **[cascade](../cascade.md)** spec.

The §2-combine above is the *generic* slot. A city's **per-channel yield/commerce RATE** — `InfoValuation::cityRate`
for yields and `InfoValuation::commerceSplit` for the commerce channels, the value a citizen's worked output
finally becomes — is that combine applied with a **sharp two-tier shape**. This is the model the rate computation
must reproduce, and the order is load-bearing (it decides what the percent stack scales and what it doesn't):

> **`rate100 = (BASE + specialists) × modifier⁄100  +  100 × ⌊EXTRA100 ⁄ 100⌋`**
>
> `modifier = max(0, 100 + Σpercent)` (so `×modifier⁄100` ≡ `×(1 + Σ%)`). Everything is ×100 fixed-point integer.

### TIER 1 — BASE (everything the percent stack MULTIPLIES)

| BASE source | origin | base vs computed |
|---|---|---|
| **worked-plot yields** (`basePlotYield`) | Σ over the city's worked plots of each plot's ONE isolated base package (§2 plot-as-base): `max(0, terrain+feature+bonus)` nature + improvement (floored at −nature) + route + keyed building/civic/trait `plot`-flats + `plots`-target + city-centre constant + threshold/golden-age per-plot | **computed** from the curated plot substrate + engine plot state |
| **trade-route yield** (`tradeYield`) | engine-generated (the trade network) — ⚖ **already carrying its OWN percent layer, see below** | **input** — out-of-scope: the cascade cannot re-derive the network, so the calc *folds the route yield in*, never derives it. **The ONE live-yield input** — a clean addition at the very end of the base, and the sole sanctioned exception to the pollution guardrail ([validation](../specs/validation.md), [the pollution guardrail](../specs/validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)). ⚠ **The route COUNT is the OPPOSITE case: `getMaxTradeRoutes` — game + player + coastal + `city.extra` slot deposits — is a modifier-influenced value the cascade COMPUTES, its own `tradeRoutes` channel.** Trade YIELD is read from the engine package; the trade-route COUNT is calculated here. Do not conflate them |
| **free-city yield** (`freeCityYield`) | Σ the player's active traits' `YieldChanges` (`{ch}.empire.flat`) | **computed** — derivable from the trait JSON, so it is COMPUTED, never read off the engine; consuming the live value would leave the trait→yield derivation unvalidated ([validation](../specs/validation.md) pollution guardrail). ⚠ NAMING: "free-city" here = the legacy trait accumulator (`CvPlayer::m_aiFreeCityYield`, free yield granted in every city) — **NOT** the WLTKD celebration ("We Love the King/Emperor Day"), whose sole gameplay effect is zero city maintenance ([economy.md](../reference/economy.md)) |
| **golden-age yield** | trait `goldenAge` member (`{ch}.empire.goldenAge.flat`) while in golden age | **computed** (`empire.goldenAge` member-mirror, §3 golden-age carve-out) |
| **specialist yields** (`specialist`) | per assigned specialist: `intrinsic × (100 + specialist-%)⁄100` + building-local (gated `city.flat`) + per-type (`empire.cities.flat` — the `cities` target lands it in the HOLDING city; a bare `empire.flat` on a specialist would roll down to EVERY city and cascade with city count) + perAll + trait governing-deliverer | **computed**. NOTE the specialist carries its **own** percent layer (its intrinsic ×`(100+specialist-%)`) *before* it joins BASE and takes the city `modifier` — two distinct percent stacks |

> **⚖ HOW `tradeYield` STAYS CURRENT — the ONE value the cascade FEEDS but does not HOLD, so it is REBUILT, not
> delta'd.** It is the engine's network OUTPUT (`CvCity::m_aiTradeYield`, ×100 like any amount), not a package
> slot, so the maintained sum does not reach it and no compiled deposit index can name what moves it. Its
> rebuild has four moments and they are the whole set: ONCE at the end of load (against the final cascade);
> TARGETED at the owner whenever a fact moves a `tradeRoutes` channel; on every plot-group / network change
> (which is what covers a city being FOUNDED or ACQUIRED — both reach `updatePlotGroups`); and once per player
> in `doTurn`.
> ⛔ **The per-turn rebuild is NOT the banned blanket** ([self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)):
> that rule bans papering over a MISSED invalidation, and this one exists because a genuine INPUT advances every
> turn — `getPeaceTradeModifier` scales with the at-peace counter, so a foreign route's profit legitimately
> differs turn to turn until it saturates. There is no fact to route it to; the turn IS the fact.
> ⛔ **AND THE CASCADE NEVER TRANSCRIBES THE PER-CHANNEL FORMULA EITHER — it folds `getTradeYield`, full stop.**
> The engine's `CvCity::calculateTradeYield` (profit × the player's per-yield trade modifier) is the ONE
> implementation, and it is engine-owned by the same KEEP ruling that puts the network there
> ([north-star.md](../architecture/north-star.md)). ⚠ A copy of that arithmetic on the CALC SURFACE reads like
> the canonical home — it sits beside the genuine `§2a` seams and looks like the one they all point at — and it
> is the opposite: a second implementation of a calculation this spec says the cascade must not own. One was
> built and never called; it is deleted rather than wired, because there was no consumer to wire it TO.
> ⚠ **City POPULATION deliberately gets NO route, and that is a cadence ruling rather than an omission.** It
> feeds the profit on both sides (`getBaseTradeProfit` reads the PARTNER's population, `getPopulationTradeModifier`
> the city's own), so a route would have to rebuild the owner AND every player trading with it — and it would
> fire once per city GROWTH, i.e. once per city per turn, each firing a full network walk. The mid-turn snapshot
> rule already answers it (§ EAGERLY BUILD ALL CACHES AT LOAD, below: *"getting a yield event in the middle of a
> turn is not retroactive; start of next turn is what is expected"*), and the next `doTurn`
> is that start.

### TIER 2 — EXTRA (flat, added AFTER the percentages, NEVER multiplied)

| EXTRA source | origin |
|---|---|
| **building flat yields** (`BuildingFlatYield100`) | Σ active (non-dormant) buildings' `{ch}.city.flat` + `{ch}.city.perPopulation` × population |

The EXTRA is held ×100; the `100 × ⌊EXTRA100⁄100⌋` **truncates it to whole units** before re-scaling (the engine's
`getExtraYield100` order — a documented integer-truncation gotcha, not a rounding choice).

> For **§2 commerce** the same two-tier shape holds with the channel's own pieces: BASE = the COMMERCE-yield
> (`InfoValuation::cityRate`'s COMMERCE channel) × the channel slider + the §2 baseExtra sub-terms (religion, corporation, golden-age,
> state-religion pool, player-extra, the building-commerce block); EXTRA (post-modifier) = `production × prodToCommerce`.
> The building-commerce block is itself a pure per-building sum over the building's OWN entries (own-flat + tech +
> bonus + perPop + shrine + corp-HQ + the `CommerceChangeDoubleTime` whole-doubling) — and the building-keyed boosts
> (a wonder/civic/tech granting a channel to a building TYPE, `{c}.<scope>.buildings.{B}`) are part of that sum as
> the TARGET building's own reverse-landed conditioned entries: authored deliverer-side (§4), landed at CITY scope
> by the readJson reverse pass, gated on the source's presence at the authored scope. Civil disorder forces the
> whole rate to 0 before any of this.
>
> ⛔ **THE SPLIT IS A CITY/EMPIRE CONCERN — THE PLOT AND THE BUILDING DO NOT CARE.** *"The plot itself does
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
>.** *"Moving a slider should not really need to reassign citizens; it does not change commerce
> outputs at all, and plots are not evaluated on the commerce yields themselves."* The slider re-divides a
> COMMERCE yield it does not change, and the plot valuation never reads the commerce channels, so every input
> to a citizen decision is exactly what it was. The realized rates pick the new split up at the COMBINE, which
> is the whole of the work a slider causes.
> ⚑ **The measured cost of getting this wrong, because it is the reason the rule is written down:** the setter
> flagged every one of the player's cities for re-assignment, so ONE slider tick re-ran the full citizen
> assignment across the empire and **stalled for fifteen seconds**, of which the entire observable result was a
> couple of dozen facts — a handful of cities moving a citizen, which is the churn of a re-decision that had no
> new input to decide on.
> ⚠ It is the [a staleness flag is the fossil of a missing emit](03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) shape on the AI plane: the
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

