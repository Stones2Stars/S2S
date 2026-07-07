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

**Operating buildings ride along:** `PCT`/`EXTRA` recomputes consume `EnablerKernel::recomputeOperatingBuildingsInto` (memoized;
evicted ONLY on building events — juggling/specialist churn never evicts the fixpoint).

## Event → dirty mapping (the DOMAIN hooks)

| hook site | effect |
|---|---|
| `CvCity::processBuilding` | city: PCT+SPEC+EXTRA(+C_*) dirty + operating buildings evict |
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

- **E — the city SCALAR channels (GP-rate / defense / maintenance) — ✅ OPENED + FULLY ATTRIBUTED (2026-07-03).**
  `Cascade/CvCascadeScalarChannels.{h,cpp}` + the `scalars` block on `/computed/cities/wellbeing`. Measured:
  **gpBase + gpMod EXACT on every probe** (the empire-scope building deposits walk the whole player — the
  player-accumulator semantic); **defense EXACT vs the engine's own current-state recompute** (the stored
  `m_iBuildingDefense` carries +60 phantom at one probe — the DRIFT class, 4th family); **maintenance EXACT on
  every data-backed part** (the stored AREA accumulator is pure phantom — ZERO buildings author area
  maintenance in the whole XML). The PROPERTY channel closed same-day at **7/7 EXACT** (the plot-half unit
  diffusion + per-scaled building sources + a REAL curator data-loss fix: the multi-source same-property merge
  overwrote plain values -- restored via entry lists + regen). **tradeRoutes** opened + attributed: cityExtra
  EXACT; the player remainder is the VOTE class (:7971 -- un-derivable game state, the extraHappiness
  mixed-accumulator treatment). **buildRate CLOSED EXACT (2026-07-04):** the P10 head-order ×2 probe was a
  NAMED port bug, not drift -- the keyed walk (`sc_buildRateKeyed`) matched deposits through the compiled
  `sumKeyed4F`, a FLAT-unit matcher, while every keyed buildRate deposit is PERCENT-unit, so domains/
  unitCombats summed 0 (the "×2" was coincidence: the missing keyed half equalled the matched half). Fixed ON
  the compiled index: `MMKernel::sumKeyed4U` (the keyed matcher with an EXPLICIT unit segment; `sumKeyed4F`
  is its flat parameterization) matches the percent unit, the trait leg threads the PURE_TRAITS sign exactly
  as `sumTrait` derives it, and the :3912 subs-only-with-main nesting is mirrored. (An interim string-walk
  version was reverted the same day — deposit matching RIDES the compiled index, the parser layer the
  state-repositories event→cache routing derives from; string walks work against that grain.) The wellbeing
  `scalars` emit carries the full buildRate attribution (order identity + cascade member parts + every legacy
  getter part incl. `brCombatByTypeLeg`). Verified live on the compiled form: P10-C16394 170==170 with every
  part reconciling, sweep 14/14 EXACT across 5 players (unit AND building orders). **Queued:** slot storage + flips for the attributed channels; the unit plane (needs
  the `unitInput` endpoint, calc-map §12).

- **F — the city SCALAR slots (2026-07-04) — slots + rollup + nets; flips gated on the played-turn proof.**
  The increment-E attributed channels went onto the substrate: `ACCD_SCALAR` (gpBase-buildings / gpMod /
  defense / maintMod / tradeRoutes -- building/religion/corp hooks + epoch + turn) + `ACCD_SCALARSPEC` (the
  gpBase specialist half -- its own component on the specialist hook, the CSPEC analogy, so governor churn
  never pays the building walks), named fields on `CascadeRateSlots`, refresh = the `CascadeScalarChannels`
  calculators called WHOLE (the substrate law; `gpRateBase` split into two exposed component functions whose
  sum IS the oracle). The rate reads' ensure mask is the explicit `ACCD_RATES` (a rate read never pays the
  WB/scalar walks). The player-wide building walks are cached per (player, epoch, turn) on the shared
  **`CvCascadePlayerStamp`** (state-repositories "one pattern everywhere" -- `WbPlayerRollup` converged onto
  the same stamp): `ScPlayerRollup` caches CALLS to the existing `sc_playerBuildings` walk (never re-walks)
  plus the maintenance area/otherArea split RELOCATED from `maintenanceModifier`; tech walks stay
  in-calculator (their eval ctx stays the calling city's). The scalar slots joined the load-end warm-up.
  Verification per validation.md's two shapes: the per-turn **`[MODIFIER/scalar]`** net in the doTurn harness
  (RAW slot reads vs the fresh calculators -- the ensuring accessors would be the tautological-0 trap; capped
  per-city samples carry the five S/C pairs) + the standing-slot twins (`gpBaseSlot`/`gpModSlot`/
  `defenseSlot`/`maintModSlot`/`tradeRoutesSlot`) beside the fresh `*Casc` + legacy fields on
  `/computed/cities/wellbeing`. **Verified at load: SLOT==CASC 5/5 across probed cities**; the legacy diffs
  are the standing attributed classes only (defense drift / maintenance area phantom / tradeRoutes vote).
  **✅ THE HOOK MAP IS PROVEN (2026-07-04, the increment-A method by owner ruling):** a transient
  self-heal-off gate ran the scalar bits on pure hook+epoch-maintained state (turn-roll skipped for
  ACCD_SCALAR*, the net reading via the ENSURING accessors -- non-tautological) through two owner-played
  turns: `[MODIFIER/scalar] checked=22 diverging=0` BOTH turns, zero holes. The gate is removed (the net
  back on raw reads; self-heal restored). **✅ FOUR FLIPS LANDED (2026-07-04, same day):** `getBaseGreatPeopleRate` (slot city-base +
  live national), `getTotalGreatPeopleRateModifier`, `getBuildingDefense`, `getEffectiveMaintenanceModifier`
  return the cascade slots; the legacy bodies live on as `*Legacy` siblings (the net oracles; the endpoint's
  `*Leg` fields + the `/computed` decomposition surfaces read them so aggregates keep equaling their legacy
  parts). Verified live: Slot==Casc on every probe; the `*Legacy` oracles carry exactly the two accepted
  classes, now REPAIRED in play by construction -- the defense stored-drift (+60 at the P10 probe) and the
  maintenance AREA phantom (+22..+39 every city -- cities got cheaper; the ruled drift-repair, not a
  regression). **Deferred, each its own increment:** `getTradeRoutes` (legacy mixes un-derivable VOTE-granted
  routes into the player accumulator -- needs the mixed-accumulator split, the persisted-events-store
  precedent, before its flip can be lossless); buildRate stays UN-slotted (item-keyed; its flip needs
  per-key rollup ledgers).

- **I — the SCOPE-PACKAGES landing (2026-07-04): the WHOLE structure in ONE landing (no increments — the
  owner's build law: increments on a scope/yield chase parity without a critical element).** The full
  [scope-packages.md](scope-packages.md) design landed at once: `CascadeCityPackages` (city-only sums, one
  package per field, flat/percent separate) + `CascadePlayerScope` on CvPlayer + `CascadeWorldScope` on
  CvGame, all on `CvDerivedCacheSet`; the fills ride single-source component splits added to the Calc
  modules (`PercentStack::cityPercent/empirePercent/areaPercentByArea`, `YieldBasePackages::goldenAgeUngated`,
  `CommerceCalc::baseOwn100/stateReligionPool/stateReligionMatch/buildingKeyedLedger`,
  `CascadeWellbeing::gatherCityTerms/playerAreaEmpire/assemble` [ONE verdict assembly, pure over inputs],
  `CascadeScalarChannels::gpModifierCity/maintenanceModifierCity/tradeRoutesCity/fillPlayerScalars/
  fillBuildRate{City,Player}`); reads are bare fetches + the channel combine with LIVE gates (SR/coastal/
  conn/GA/slider/disorder/military); the epochs + `CvCascadePlayerStamp` + the operating buildings/slot stamps + the
  read-side ensure protocol + `s_wbRollup`/`ScPlayerRollup` are DELETED; events mark
  (`buildingProcessed` = conservative city + DERIVED cross-scope masks; `markPlayerScopeAndCities` at the
  three player-event sites); the boundaries are `playerSliceRebuild` (doTurn top) + `worldRebuild`
  (CvGame::doTurn) + the load warm-up (the same ensure run eagerly). FLIPPED in the landing: gpBase/gpMod/
  defense/maintenance re-flips + ALL THREE `getProductionModifier` overloads (buildRate, per-key ledgers on
  compiled ints) — each with its `*Legacy` net oracle; tradeRoutes' getter alone stays legacy (the vote
  split, its own item). **Verified on load (Assert + Release clean first compile; save loads whole through
  the full warm-up):** scalar packages EXACT vs the fresh oracles (0/52×5), buildRate totals casc==leg
  (52/52; a reader part-sum overlap briefly masqueraded as 5 diffs — brPlayerGenericLeg contains the
  military part), wellbeing casc-vs-leg = the two documented accepted classes at increment-D magnitudes.
  **Queued: the owner-played turn proof** (nets + census + the automation feel).

- **H — the SCOPE-PRINCIPLE rebuild (2026-07-04, SUPERSEDED by increment I before landing — the partial
  surgery was reset; the owner ruled the design be stated whole first, then built whole).**
  The rulings, all same-day: a `CvDerivedCache` on EVERY scoped item, LIVING ON the object it caches for;
  ONE freshness philosophy (events mark, the cache knows — the epochs + `CvCascadePlayerStamp` polling
  DELETE); every scope holds ONLY its own scope's sums in ONE uniform package format; **the only live
  calculation is adding the ~5 scope packages at read** (per-city gates — SR-in-city/coastal/connected/area
  — applied live); a scope event invalidates ITS package only (no fan-out); full rebuild = LOAD ONLY; reads
  are bare fetches; recalc at each player's slice start (`playerSliceRebuild`); the derived building mask
  splits percent-vs-flat; NO unit may ever author yield percentages. **Structure that contradicts this is
  REBUILT, never shoehorned.**
  **DONE so far:** header (`CascadePlayerScope` on CvPlayer w/ PSC_SCALAR|PSC_WB bits + `CascadeWorldScope`
  for CvGame + city scalar slots redefined CITY-ONLY: iScGpBaseBld/Spec, iScGpModCity, iScDefense,
  iScMaintModCity, iScTradeCity); accumulator core (epochs/stamps/freshen deleted; acc_ensure = pure Set;
  `markPlayerScopeAndCities`; slice rebuild = marks + eager scalar ensure; `refreshPlayerScope` dispatch;
  `buildingDirtyMask` percent/flat-derived); scalar module (`rebuildPlayerScalar(owner, scope)` +
  ensure-then-fetch sc_rollup) — ⚠ its field names still the OLD package names, must match the new header.
  **REMAINING (the exact worklist):** (1) rebuildPlayerScalar fills the NEW fields (iGpModPlayer incl.
  civics/traits city+empire, iGpModSr, iTradeEmpireAll incl. civics+techs, iTradeCoastalAll incl. civics,
  iTradeWorldFlat, iMaintPlayerAll incl. civics city/empire/area + techs, iMaintConnPct, area maps) — the
  civic/trait sums move INTO the package (evaluated player-ctx; city-conditioned civic deposits would show
  in the net, watched); (2) refreshComponents' scalar branch → the CITY-half walks only; (3) the sc*
  accessors → the package ADDS (gpMod = max(0,100+city+player+SRgate+GAgate); maint = city+playerAll+
  areaPick+connGate; trade = city+empireAll+coastalGate+world); (4) CvGame::m_cascadeWorldScope (member+
  bind+delegate; refresh = Σ living players' ensured packages; marked by world-deposit buildings — extend
  the building-mask path); (5) CvPlayer::m_cascadePlayerScope member+bind+reset+delegate; (6) mark sites:
  setCivics/GA (CvPlayer) + setHasTech (CvTeam) → markPlayerScopeAndCities (city loop masks ALL&~SCALARS —
  scalars need NO fan-out by construction); (7) CvCity reset drops the iEpoch/iTurn lines; operating buildings
  (CvCascadeOperatingBuildings.h + EnablerKernel::operatingBuildings) drop the epoch/turn stamps (slice-start markAll is the
  cadence); (8) wellbeing: WbSplit def moved to the accumulator header (delete the local), s_wbRollup →
  the member maps via ensure(PSC_WB); (9) the [MODIFIER/scalar] net + endpoint slot emits recompose to the
  getter-body adds; the flipped getter bodies (already raw) recompose likewise; (10) build → cycle → owner
  re-test (automation feel + the net + the census counters).
  **NOT yet scope-conforming (their own per-channel rebuilds follow, NOT shoehorned):** the rate EMPFLAT
  (player-scope trait flats stored per city), the WB verdict slots (area/empire parts baked per city — its
  freshness is the ruled end-turn cadence meanwhile), and the rates' fan-out marks on player events.

## Explicitly NOT this build

- The tally stays a read-only accessor (NOT a spine consumer) — unchanged.
- The enabler gates don't flip here (their own increment).
- The leaf math is NOT rewritten — the calculator packages remain the single source of the arithmetic
  (patterns.md single-source law); this layer only decides WHEN they run and stores their results.
