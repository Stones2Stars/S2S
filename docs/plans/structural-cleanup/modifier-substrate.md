# The modifier scope ACCUMULATOR — the §1 substrate (the machine as specced)

> **Status:** in-build (2026-07-02) · **Authority:** [modifier.md](../../specs/modifier.md) §1/§2a/§3 (the machine
> this implements), [event-spine.md](../../specs/event-spine.md) (its build order names this — *"spine + the
> modifier scope accumulator"*; DOMAIN events serve *"cache-invalidation (the future modifier/enabler dirty
> triggers)"*, [tally.md](../../specs/tally.md) §4), [cutover.md](cutover.md) ruling 5 (the flip this unblocks).
>
> **Why it exists (the retro finding, owner 2026-07-02):** the top-down deposit design *"has clearly been lost
> during drycalc and implementation"* — StoneBase HAD to full-calc (an offline drycalc cannot hold continuous
> game state), the C++ port targeted StoneBase parity, and the calculator quietly became the de-facto
> implementation. Flip attempt #1 (25+-minute turn) proved a cache over a calculator cannot substitute for the
> machine. **This build IS the modifier machine as specced; the calculator is demoted to its true role — the
> verification ORACLE.**

## The shape — component slots OVER the calculator packages

modifier.md §2a decomposes the city rate into components with **different volatility**. The accumulator stores
each component as standing state and recomputes ONLY a component whose inputs changed — the calculator packages
(`PercentStack::percentStack`, `YieldBasePackages::basePlot/specialist/freeCity/goldenAge`,
`BuildingPackage::buildingFlat`, `CommerceCalc`) **are the per-component recompute functions**; the substrate
adds what was missing: the standing state + event-driven freshness. A clean read is pure arithmetic on stored
components (§2a combine) — **no source re-walk on any read** (§1 rule (a)).

Per **city** (channels ×100 fixed-point):
| component | §2a term | recompute fn | dirtied by |
|---|---|---|---|
| `PCT[7]` | the ONE additive percent stack (city+area+empire buildings, civics, traits, projects, civic building-keyed) | `PercentStack::percentStack` | building events in this city; the global epoch (tech/civic/trait/GA/project); turn roll |
| `PLOTS[3]` | Σ worked plots' isolated base packages | `YieldBasePackages::basePlot` | `setWorkingPlot`; turn roll (plot-substrate changes self-heal) |
| `SPEC[3]` | specialist totals (own sub-stack) | `YieldBasePackages::specialist` | specialist count changes; building events; the global epoch; turn roll |
| `EXTRA[3]` | building flats + perPopulation | `BuildingPackage::buildingFlat` | building events; `setPopulation`; turn roll |
| `C_BASEEXTRA[4]` (incr. B) | the §2 commerce baseExtra (religion/corp/GA/state-religion pool/player-extra/building-commerce block incl. doubleTime) | `CommerceCalc` sub-terms | building/religion/corp events; the global epoch; **turn roll (doubleTime is time-keyed)** |

Per **player**: `FREECITY[3]` + `GA[3]` (trait flats; golden-age member) — dirtied by trait/GA events + the epoch.

**Inputs read live at combine (never stored):** the trade-route yield (`/state` input, O(1) engine read), the
commerce **slider** (`getCommercePercent` — multiplies at combine, so slider moves need NO invalidation),
`isDisorder`, population (for the perPop term inside EXTRA's recompute).

**Dirty machinery:** per-city bitmask + a GLOBAL EPOCH (player/team-level events: tech, civic, trait,
golden-age, project) + a **turn stamp** — every component recomputes at least once per turn (the §3
"re-evaluated every recompute" dormancy cadence AND the self-heal for any unhooked mutation). Conditioned
deposits are thereby re-checked whenever their component recomputes — §3 holds by construction.

**Facts ride along:** `PCT`/`EXTRA` recomputes consume `EnablerKernel::computeCityBuildingFacts` (memoized;
evicted ONLY on building events — juggling/specialist churn never evicts the fixpoint).

## Event → dirty mapping (the DOMAIN hooks)

| hook site | effect |
|---|---|
| `CvCity::processBuilding` | city: PCT+SPEC+EXTRA(+C_*) dirty + facts evict |
| `CvCity::setPopulation` | city: EXTRA dirty (perPop) |
| `CvCity::setSpecialistCount` | city: SPEC dirty |
| `CvCity::setWorkingPlot` | city: PLOTS dirty |
| `CvTeam::setHasTech` / `CvPlayer::setCivics` / trait changes / `changeGoldenAgeTurns` (GA flip) / project completion | global epoch bump |
| *(slider)* | none — read live at combine |

Coarse-but-honest v1: anything missed self-heals at the turn roll, and the shadows MEASURE the residual as
attributable diff lines (never silent).

## Verification — two nets

1. **`[SLOT]` shadow** (increment A, pre-flip): per turn, sampled cities × channels — the accumulator's combine
   vs the CALCULATOR's fresh full compute. Divergence = a dirty-mapping hole (a named, attributable miss).
2. **The `[GETTER]` in-body net** (at flip time): accumulator vs the legacy in-body expression — the standing
   accepted-class residue plus anything new.

## Increments

- **A — yield plane** (food/production/commerce): the slot store + dirty hooks + `[SLOT]` shadow. Getters stay
  legacy (unflipped) — the substrate proves its freshness event-driven before it is load-bearing.
- **B — commerce plane** (gold/research/culture/espionage): C_PCT rides the same PCT machinery; C_BASEEXTRA +
  the building-commerce block as components; slider live at combine.
- **C — the flip**: `getYieldRate100`/`getCommerceRateTimes100` bodies return the accumulator (legacy stays
  in-body as the net oracle, per cutover.md ruling 5 / flip attempt #1's binding lessons). Measure the proof
  turn; the perf follow-up (compiling the deposit addresses string→int at load, per-plot package caches) lands
  only if the measured numbers demand it.

## Explicitly NOT this build

- The tally stays a read-only accessor (NOT a spine consumer) — unchanged.
- The enabler gates don't flip here (their own increment).
- The leaf math is NOT rewritten — the calculator packages remain the single source of the arithmetic
  (patterns.md single-source law); this layer only decides WHEN they run and stores their results.
