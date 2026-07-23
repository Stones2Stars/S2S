# Perf wiring — the CENSUS is the perf surface; `PROFILE_FUNC` is permanently dead

> **Status: ACTIVE (unparked 2026-07-06).** Owner ruling 2026-07-06: *"perf-profile-wiring should not be
> parked; we will not reinstate `PROFILE_FUNC` ever again, but we do want performance wiring — a fair bit of
> it is in place."* So this is live #430 work: the census-based perf surface is the wiring we build out, and
> the internal `PROFILE_*` profiler is **never** coming back. This doc is coupled to the legacy removal (the pre-cut
> pair-timing slice below closes at each legacy cut), so it lives with the [structural-cleanup](README.md)
> bulldozer set and dies when the legacy is gone.
>
> **⛔ RULING (owner 2026-07-05, HARDENED 2026-07-06): NEVER use the internal profiler; NEVER reinstate the
> `PROFILE_*` macro family.** The census / `[MODIFIER/perf]` gated logging suits our purposes better and is
> the *only* perf surface we build on. The one attempt to ship the internal profiler in **Release** (behind a
> runtime gate) **caused a memory-allocation-failure crash on end-turn** and was reverted the same day
> (2026-07-05). The cause is that the `PROFILE_*` macro family is **kraken bait**: `PROFILE_FUNC`/`PROFILE`
> are live in Profile but no-ops in Release/Assert (same line, config-dependent behaviour), and the 3
> `PROFILE_BEGIN` sites (`CvGame::update` per-FRAME, `doTurn`, `CvPlotGroup`) call `IFPBeginSample`/`EndSample`
> DIRECTLY, **bypassing any `CProfileScope` gate** — so compiling the profiler into Release ran those ungated
> per-frame with a critical-section per call. As of 2026-07-06 there is **no plan to fix-and-reinstate it**:
> the profiler is dead, and **[#449](https://github.com/Stones2Stars/S2S/issues/449) is its REMOVAL**, not a
> fix that brings it back. Do not add, un-gate, or Release-compile any `PROFILE_*` / `IFP*` / internal-profiler
> path.
>
> **✅ THE PERF SURFACE (what we DO use — the wiring already in place):** the census —
> `Cascade/CvCascadePerfCount` + the per-turn `[MODIFIER/perf]` spine lines: call counts
> (operating buildings/yieldRate/pctStack/commerceRate/condEval/…), the `PerfAccumTimer` ms buckets, the **condEval CALLER
> split** (`ceRates/ceWb/ceScalars/ceOperatingBuildings/ceFrontB/…` — the 2026-07-05 outlier-attribution lap), the
> **ENABLER-FRONTIER fill counts + ms** (`front{B,U,PP,P}Fills/MsX10`, `promoFills/MsX10`), the flush-to-flush
> `turnMsX10` ([DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)), the
> **automation window** (`autoMissions`/`autoMissionMs`), the flipped scalar-getter read/refresh counters, and
> the legacy-vs-cascade pair ms. This is gated (`gPlayerLogLevel`/`gPerfLogLevel`), teed to `/events`, and
> stored by StoneBase — the sufficient, non-invasive surface. It STAYS; only the internal profiler is out.

## What exists today (verified against the code 2026-07-06)

- **The `[MODIFIER/perf]` census** (`Cascade/CvCascadePerfCount.{h,cpp}`): per-turn whole-turn CALL COUNTERS
  (operating buildings recomputes vs cache hits, yieldRate100/percentStack/commerceRate100 computes, condition-evaluator
  leaf evals, accumulator refreshes, wellbeing computes) + `PerfAccumTimer` stopwatch accumulators
  (gPerfLogLevel-gated, ms×10 ints), flushed once per turn as a `[MODIFIER/perf]` spine line teed to
  `/events` — the perf counters feed the `(scope,channel)` calc-count gate + the StoneBase performance dashboard
  ([DEC-calc-count-gate](../../architecture/decisions.md#dec-calc-count-gate)). Every perf ruling on the #430 branch was made from these numbers.
- **The condEval CALLER-DOMAIN split** (`CascadeCondCaller` + `CascadeCondScope`, the flip-era outlier lap):
  a scoped tag set at the outermost compute entries attributes every eval to whoever initiated the chain
  (`ceOther/ceRates/ceWb/ceScalars/ceOperatingBuildings/ceFrontB/ceFrontU/ceFrontPP/ceFrontP/ceCanBuild/cePromo`); `ceOther`
  is the honest residual — a big OTHER names the next tag, never a silent gap. Plus the ENABLER-FRONTIER fill
  counters/stopwatches (`front{B,U,PP,P}Fills/MsX10` + `promoFills/MsX10`).
- **The whole-turn wall time** (`turnMsX10`, landed 2026-07-04): flush-to-flush WALL time on the second
  `[MODIFIER/perf]` line, beside `legacyRateMsX10`/`legacyWbMsX10` (the legacy oracle calls timed in the
  pre-cut pair nets). ⚠ In interactive play `turnMsX10` includes the human's between-turn time; it is clean on
  scripted/autoplay benches.
- **The store lives in StoneBase** (the GameTracker port): the `/events` listener persists one row per turn to
  the owner's Postgres (`perf_turns` + key-value `perf_fields`; CSV fallback). Verified live end-to-end.

## The wiring roadmap (what "we do want performance wiring" means, post-2026-07-06)

The internal-profiler reinstatement is off the table permanently, so the build-out is entirely census-side:

1. **Extend the census, never the profiler.** New hot surfaces get a `CvCascadePerfCount` counter + (when the
   ms matter) a `PerfAccumTimer` bucket + a `[MODIFIER/perf]` field, gated like every existing one. This is
   the pattern every perf number on the branch already follows.
2. **Attribute outliers by caller, not by call-stack sampling.** Where a hot compute (`condEval` was the
   6.8M/turn outlier) needs a "who called this" breakdown, add a `CascadeCondScope`-style scoped tag at the
   outermost entry — the census gives the per-caller split a profiler would have sampled, without the
   per-scope QPC overhead or the crash surface.
3. **The whole-turn root total is the headline regression number** ([DEC-turn-time-is-king]) — `turnMsX10` on
   the scripted bench, stored per turn, is the number a change is judged against. Per-load costs (warm-up,
   readJson, index compile) are never optimized at turn time's expense.

Cost honesty: the census is counters + gated stopwatches — an int increment per counted event always, a QPC
pair only when `gPerfLogLevel` is on. Observational only, no OOS surface.

## The PRE-CUT slice — comparative cascade-vs-legacy pair timing (does NOT wait)

> **⚠ This slice's measurement window CLOSES AT EACH LEGACY CUT** (owner concern, 2026-07-05: *"legacy is
> probably doing a couple of things faster than we are doing with cascade, and we want to know what of that we
> can use"*). Capture each channel's pair numbers **before** its legacy accumulators are deleted — after the
> cut the comparison is unmeasurable.

Every flipped getter carries its `*Legacy` sibling in the same build, and the pre-cut pair nets already call BOTH
sides — so per-family pair stopwatches (cascade-body ms vs legacy-body ms accumulated per turn on the existing
`[MODIFIER/perf]` line) give "what is legacy faster at", per channel, before any cut.

First pair numbers on record (2026-07-04): legacy oracle reads ~40–60 ms/turn vs the from-scratch CALCULATOR
~500 ms/turn (stored-accumulator O(1) fetch vs from-scratch walk — NB the calculator is the pre-cut comparison oracle, not
the flipped slot-read path the game runs on).

What the comparison informs (the transferable-technique question): legacy's speed = O(1) incremental
event-fed accumulator reads + conditions never re-evaluated + serialized warm state. The serialization +
unbounded-staleness pair is the convicted drift disease and does NOT transfer
([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted)). The
DELTA-APPLICATION technique DOES transfer in sanctioned form — it is already the spec'd §1 end-state
([modifier.md](../../specs/modifier.md) §1: domain events apply the source's deposit/withdraw DELTAS; a
bounded per-turn pass re-checks CONDITIONED deposits; load-time rebuild replaces serialization, the ruled easy
trade). The current component-recompute substrate is the coarse-but-honest v1; the delta-apply refinement is
the legacy speed claimed without the legacy disease, and the pair numbers say where it is worth it.

## See also

- [state-repositories.md](../../architecture/state-repositories.md) — the derived-cache model + the turn-end
  unified rebuild end-state this wiring measures.
- [logging.md](../../specs/logging.md) — the gated `[TAG]`/spine surface the sink rides.
- [modifier.md](../../specs/modifier.md) — the modifier machine whose regression numbers this exists to watch.
