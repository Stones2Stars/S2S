# Perf wiring — PROFILE_FUNC → the [PERF] spine (parked to pre-publish, with one pre-cut slice)

> **Status:** PARKED (owner ruling 2026-07-04: *"performance is not primary concern yet, but it will be
> something that I want to look at before we publish live, when everything is in and legacy is gone"*).
> **⚠ The one slice that must NOT wait for pre-publish: the comparative cascade-vs-legacy pair timing —
> its measurement window CLOSES AT EACH LEGACY CUT** (owner concern, same day: *"legacy is probably doing
> a couple of things faster than we are doing with cascade, and we want to know what of that we can
> use"*). Capture each channel's pair numbers before its cut; the full wiring lands pre-publish.

## What exists today (verified against the code 2026-07-04)

- **The `[MODIFIER/perf]` census** (`Cascade/CvCascadePerfCount.{h,cpp}`): per-turn whole-turn CALL COUNTERS
  (facts recomputes vs cache hits, yieldRate100/percentStack/commerceRate100 computes, condition-evaluator
  leaf evals, accumulator refreshes, wellbeing computes) + `PerfAccumTimer` stopwatch accumulators
  (gPerfLogLevel-gated, ms×10 ints), flushed once per turn by the modifier shadow as a spine line teed to
  `/events`. Every perf ruling on the #430 branch was made from these numbers.
- **`PROFILE_FUNC`/`PROFILE(name)`** (`Tools/FProfiler.h`): compiled ONLY under `FP_PROFILE_ENABLE`
  (`fbuild.bff` — the `Profile` config; `ProfileExtra` additionally promotes `PROFILE_EXTRA_FUNC`, the
  ultra-hot-site markers). In Assert/Release/FinalRelease every site preprocesses to NOTHING. When compiled
  in, the internal profiler (`USE_INTERNAL_PROFILER`, impl in `CvGameCoreDLL.cpp`) takes QPC-based scoped
  samples with parent linkage (total/main-thread/avg/child/self ms + call counts) — but its SINK is a
  one-shot TSV dump to `IFP_log.txt` (`IFPEnd`, `CvGameCoreDLL.cpp:488`) per `startProfilingDLL`/
  `stopProfilingDLL` window gated by `GC.isDLLProfilerEnabled()`. No `gPerfLogLevel`, no `[PERF]` tag, no
  spine, no `/events`.
- ~~No whole-turn wall-time number exists anywhere~~ **CLOSED same day** — the census extension below added `turnMsX10` (see "LANDED 2026-07-04").

## The pre-publish build (the parked work)

1. **Availability:** compile the internal profiler into `Release` (later `FinalRelease`) with a cheap
   runtime early-out at the top of the sample path (one branch per scope when idle; the existing
   `GC.isDLLProfilerEnabled()` / `gPerfLogLevel` is the gate). `PROFILE_EXTRA_FUNC` sites stay compiled out
   of Release exactly as designed.
2. **Sink:** a per-turn flush at the `doTurn` boundary (where `CascadePerf` flushes) emitting the top-N
   SELF-time samples + the root/whole-turn total as gated `[PERF/turn]` spine lines teed to `/events` —
   hard per-turn numbers in ordinary play, bit-comparable across the frozen test save. The `IFP_log.txt`
   full table stays for deep hunts.
3. **Marriage with the census:** `CascadePerf` keeps its semantic counters (memo hits etc. — things a
   profiler cannot know); the PROFILE reuse supersedes the hand-rolled `PerfAccumTimer` stopwatches; the
   whole-turn root total becomes the headline regression number.
4. **Unattended-safety rider:** the profiler's overflow path pops a blocking `::MessageBox`
   (`CvGameCoreDLL.cpp:515` + the `dumpProfileStack` twin) — becomes a logged error (agent-driven runs).

Cost honesty: gate off = a branch per scope; gate on = QPC pairs per scope (real overhead on hot paths,
opt-in measurement time). Observational only — no OOS surface.

**The objective every number here serves:
[DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)** — turn time is the metric;
load time is the currency that pays for it ("only 1 game load, but many many many turns"). The whole-turn
root total is the headline number; per-load costs (warm-up, readJson, index compile) are never optimized at
turn time's expense.

## ✅ LANDED 2026-07-04 — the census extension + the store (the pre-cut slice is LIVE)

Owner directive same day ("wire up perf logging how it has been done up until now; worth setting up a
simple app that stores all our perf numbers hereon in"): the CENSUS-pattern wiring landed immediately —
a SECOND per-turn `[MODIFIER/perf]` line carries `turn` + `turnMsX10` (flush-to-flush WALL time -- ⚠ in
interactive play this includes the human's between-turn time; clean on scripted/autoplay benches) +
`legacyRateMsX10`/`legacyWbMsX10` (the *Legacy oracle calls timed in the shadow nets, beside the existing
cascade-side ms). Also fixed: `CascadePerf::reset()` never zeroed `wbCompute`/`wbComputeMs` (all prior
wbN/wbMsX10 readings were cumulative). **The store lives in StoneBase** (the GameTracker port, StoneBase
AGENTS.md "The tracker/perf recorder"): the /events listener persists one row per turn to the owner's
Postgres (`perf_turns` + key-value `perf_fields`; CSV fallback). First rows verified live end-to-end.
First pair numbers on record: legacy oracle reads ~40-60ms/turn vs the shadow CALCULATOR ~500ms/turn
(stored-accumulator O(1) fetch vs from-scratch walk — NB the calculator is the shadow oracle, not the
flipped slot-read path the game runs on). Only the PROFILE_FUNC internal-profiler rework below remains
parked to pre-publish.

## The PRE-CUT slice — comparative cascade-vs-legacy pair timing (does NOT wait)

Every flipped getter carries its `*Legacy` sibling in the same build, and the shadow nets already call
BOTH sides — so per-family pair stopwatches (cascade-body ms vs legacy-body ms accumulated per turn on the
existing `[MODIFIER/perf]` line) give "what is legacy faster at", per channel, before any cut. **Each
channel's pair numbers are captured before its legacy accumulators are deleted** — after the cut the
comparison is unmeasurable.

What the comparison informs (the transferable-technique question): legacy's speed = O(1) incremental
event-fed accumulator reads + conditions never re-evaluated + serialized warm state. The serialization +
unbounded-staleness pair is the convicted drift disease and does NOT transfer ([DEC-derived-never-trusted]).
The DELTA-APPLICATION technique DOES transfer in sanctioned form — it is already the spec'd §1 end-state
([modifier-substrate.md](../structural-cleanup/modifier-substrate.md): domain events apply the source's
deposit/withdraw DELTAS; a bounded per-turn pass re-checks CONDITIONED deposits; load-time rebuild replaces
serialization, the ruled easy trade). The current component-recompute substrate is the coarse-but-honest v1;
the delta-apply refinement is the legacy speed claimed without the legacy disease, and the pair numbers say
where it is worth it.

## See also
- [state-repositories.md](../../architecture/state-repositories.md) — the derived-cache model + the
  turn-end unified rebuild end-state this wiring measures.
- [logging.md](../../specs/logging.md) — the gated `[TAG]`/spine surface the sink rides.
- [modifier-substrate.md](../structural-cleanup/modifier-substrate.md) — the accumulator whose regression
  numbers this exists to watch.
