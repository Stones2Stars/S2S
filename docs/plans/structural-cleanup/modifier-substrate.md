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
   **Component-decomposed since 2026-07-03:** every `[MODIFIER/slot]` diff line carries per-component
   slot-vs-calc pairs — yield leg `plotS/C` (live CvPlot-cache pull vs the basePlot package — the one term the
   two sides source differently by design) + `empS/C` + `specS/C` + `extraS/C` + `pctS/C`; commerce leg
   `ycS/C` (the shared commerce-YIELD input — a diff here means the divergence lives in the yield slots, not
   the commerce plugins) + `cspecS/C` + `cbaseS/C` + `cpctS/C` — so the diverging component names itself
   (slider/disorder are live at combine on both sides and can never be the diverging term).
   **Plus the per-plot probe:** a diverging `plotS/C` pair triggers `[MODIFIER/plotdiff]` lines — per worked
   plot, the engine's CvPlot value vs the SAME per-plot package basePlot sums (`basePlotOne`, single-source),
   with the plot's substrate string (terrain/feature/improvement/route/bonus/centre) AND the engine's three
   serialized improvement-yield accumulators (`accPlayer`/`accTeam`/`accCity`) for the plot's improvement.
   **✅ THE ATTRIBUTION CLOSED (2026-07-03, live) — the standing `[SLOT]` residue decomposes into exactly two
   named classes, both reconciling bit-exact through the combine arithmetic:**
   - **The plot-pair class (the yield bulk + the systematic commerce-channel residue, e.g. the Seoul research
     case = a +2 plot-commerce delta × slider × modifier):** the engine's SERIALIZED improvement-yield
     accumulators hold values with **no live data source** — a full writer census (civic/trait/building
     ImprovementYieldChanges in XML *and* JSON, improvement TechYieldChanges, Python events) accounts for
     every data-backed contribution, which the cascade mirrors and cancels; the un-backed remainder (chiefly
     `accCity` — the state-repositories.md broken city cache — plus occasional `accPlayer` residue) IS the
     divergence, per-plot, exactly. This is the accepted **stale-engine-state class (cutover.md note f2)**
     riding the accumulator's live `getPlotYield` pull: the ENGINE side is wrong (phantom yields from
     unreachable history), the calculator's derivation is right, and the cutover repairs it. NOT a slot
     dirty-mapping hole — no substrate action needed.
   - **The `cbase` class (small commerce residues):** the standing C_BASEEXTRA slot lagging a fresh mid-turn
     mutation the hooks don't carry (the watched bonus-network / religion condition-flip class) — self-heals
     at the turn roll, surfaces as named `cbaseS/C` pairs when it fires.
2. **The `[GETTER]` in-body net** (at flip time): accumulator vs the legacy in-body expression — the standing
   accepted-class residue plus anything new.

## Increments

- **A — yield plane** (food/production/commerce): the slot store + dirty hooks + `[SLOT]` shadow. Getters stay
  legacy (unflipped) — the substrate proves its freshness event-driven before it is load-bearing.
  **✅ VERIFIED (2026-07-03, live):** with the turn-roll self-heal deliberately OFF (an initial version had it
  on — that made the sweep recompute everything next to its own oracle, a tautological 0, caught and fixed),
  the slots carried purely hook-and-epoch-maintained state through a full real turn and read
  `[MODIFIER/slot] checked=66 diverging=0` against the fresh calculator. The coarse hook map covered every
  mutation that moved a sampled value; the known unhooked classes (bonus-network / religion condition flips)
  stay watched — they surface as named diffs when they fire.
- **B — commerce plane** (gold/research/culture/espionage): C_PCT rides the same PCT machinery; C_BASEEXTRA +
  the building-commerce block as components; slider live at combine.
- **C — the flip**: `getYieldRate100`/`getCommerceRateTimes100` bodies return the accumulator (legacy stays
  in-body as the net oracle, per cutover.md ruling 5 / flip attempt #1's binding lessons). Measure the proof
  turn; the perf follow-up (compiling the deposit addresses string→int at load, per-plot package caches) lands
  only if the measured numbers demand it.
  **✅ LANDED (2026-07-03, live through many measured turns).** The road there, each step measured and ruled:
  (1) the first flip ran ~2x slow — the census convicted the single assembled C_RATE chaining every juggle
  move into the whole commerce assembler (pctStack 45.9k + commerceRate 42.9k calls ≈ 9.3min) → the
  **plugin-number decomposition** (owner ruling: every package a standing number; channels independent; the
  slider LIVE at combine, zero invalidation); (2) worker-AI plot evals → the **plots PULL** (the stored PLOTS
  component deleted; `getPlotYield` — the CvPlot cache, per state-repositories' pull model — read live at
  combine; worker churn costs nothing); (3) `CvDerivedCache` **BUILT** (both forms; plot cache migrated;
  spec holes plugged); (4) the **eager load-end warm-up** (owner: hide the cost in load; every plot cache +
  every city's slots); (5) **per-player epochs** for tech/civic/GA (the cross-player invalidation storm
  killed) — but the `changeBuildingCount` bump REVERTED on measurement (5x regression: completions are the
  highest-frequency player event; sibling-empire freshness belongs to the ruled turn-end unified rebuild).
  **Steady state:** pctStack ~4.6k calls, accRefresh ~10k, turn feel at baseline. The `[SLOT]` residue
  is ✅ **FULLY ATTRIBUTED** via the component-decomposed diff + the per-plot probe (see Verification §1):
  the stale-serialized-accumulator class (engine-side phantom, cutover repairs) + the watched unhooked
  `cbase` class (turn-roll self-heals). `[GETTER]` = the legacy repair map, standing.
  **✅ THE COMPILED DEPOSIT INDEX LANDED (2026-07-03)** — the named perf follow-up, built as
  `Cascade/CvCascadeDepositIndex.{h,cpp}`: every deposit's address/unit is interned to ints at readJson
  push-time (append-only interner; the dotted segments + a FK-resolved target id ride each deposit), the
  MMKernel matchers compare ints (a never-authored query address answers 0 without touching a deposit), the
  percent stack walks a compiled per-channel candidate list instead of all ~5202 building infos, and the two
  buildings-keyed walks (civic percent + empire commerce) invert onto targetFk ledgers. Verified live on the
  same-turn reload: every call/divergence counter BIT-IDENTICAL to the pre-index turn (matching changed, math
  provably didn't); measured modifier calc ~25s → **well under 1s/turn** (pctStack ~70x). The compiled
  segments (family × scope × target per source) are the ready generator of the data-derived event→cache
  routing (state-repositories.md end-state — the hand-wired hook masks' replacement, a follow-up).

- **D — the WELLBEING channel (§2b) — ✅ PORTED calculator-first (2026-07-03).** `Cascade/CvCascadeWellbeing.{h,cpp}`:
  the four realized health/happiness verdicts from the deposits + raw-state inputs, transcribed from the
  StoneBase assembler that reached attributed parity (the classified fold, the member walks, the extra-part
  subtraction, the four engine bodies term-substituted). Verification: the `[MODIFIER/wellbeing]` shadow inside
  the modifier harness's city loop + `cascHappy/cascUnhappy/cascGoodHealth/cascBadHealth` on
  `/computed/cities/wellbeing` (on-demand diff, no turn-play). Measured on the reference save: unhappy
  exact-to-±1, happy +1..+3, goodHealth 0..−5, badHealth −9..−28 — the documented accepted residue (the
  improvement BALANCE-CUT + the stored-accumulator DRIFT, modifier.md §2b). **The `ACCD_WB` slot LANDED in the
  same pass:** `aWb[4]` standing verdicts on the rate slots, all five existing CvCity dirty sites gained
  `ACCD_WB` (building/population/specialist/religion/corporation; epoch + turn roll cover the rest), and the
  shadow nets the slots against its fresh compute (the `slotDiverging` summary field). **Queued:** the
  freshness proof over real played turns (slotDiverging must read 0), THEN the getter flip
  (happyLevel/unhappyLevel/goodHealth/badHealth return the slots, legacy in-body as the net); the residue's
  per-term drift attribution rides the oracle's `*Recomputed` twins.

## Explicitly NOT this build

- The tally stays a read-only accessor (NOT a spine consumer) — unchanged.
- The enabler gates don't flip here (their own increment).
- The leaf math is NOT rewritten — the calculator packages remain the single source of the arithmetic
  (patterns.md single-source law); this layer only decides WHEN they run and stores their results.
