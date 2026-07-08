#pragma once
#ifndef CV_CASCADE_PERF_COUNT_H
#define CV_CASCADE_PERF_COUNT_H

//	CvCascadePerfCount -- per-turn CALL COUNTERS + our own STOPWATCH accumulators for the cascade's hot compute
//	surfaces (owner 2026-07-02: chase the needless repeat calcs BEFORE parity). Counts reveal the repeat-calc
//	multipliers; the accumulated wall-clock (PerfAccumTimer, gPerfLogLevel-gated -- free when perf logging is off)
//	shows where the milliseconds actually go. Flushed + reset once per turn by the [MODIFIER/perf] census
//	(cvCascadeModifierPerfCensus). ⛔ DEFINED in CvCascadePerfCount.cpp -- never header-inline statics (the
//	InfoRepo VC7.1 inline-static duplication lesson).

// The condEval CALLER-DOMAIN split (the flip-era census: condEval 6.8M/turn is THE outlier; the per-caller
// split lands BEFORE any tuning). A scoped tag set at the outermost compute entries attributes every eval to
// whoever initiated the chain. CC_OTHER is the residual bucket -- a big OTHER means the tag map is incomplete
// (an honest gap that names the next split, never a silent one).
enum CascadeCondCaller
{
	CC_OTHER = 0,   // untagged chains (the completeness check)
	CC_RATES,       // the rate slots' pctStack + deposit walks
	CC_WB,          // wellbeing computes/gathers (city terms + player fold maps + the oracle)
	CC_SCALARS,     // the scalar channel fills (gp/defense/maint/trade/buildRate, city + player)
	CC_OPERATING_BUILDINGS,       // the operating buildings fixpoint (recomputeOperatingBuildingsInto)
	CC_FRONT_B,     // the buildable frontier fill (BuildingCascade::buildable)
	CC_FRONT_U,     // the trainable frontier fill (UnitCascade::trainable)
	CC_FRONT_PP,    // the creatable+maintainable fills (generate + the two gateSets)
	CC_FRONT_P,     // the researchable/civics/hurries/buildRem player fill
	CC_CANBUILD,    // the canBuild UNLOCK reads (enBuildUnlocked -- per-(build,plot), worker-AI hot)
	CC_PROMO,       // the promotion frontier halves + the per-unit composite
	CC_COUNT
};

struct CascadePerf
{
	static int operatingBuildingsRecomputed;  // operating-buildings RECOMPUTES (recomputeOperatingBuildingsInto runs)
	static int operatingBuildingsCacheHits;  // operating buildings READS served by the standing cache (operatingBuildings calls)
	static int pctStack;      // PercentStack::percentStack computes
	static int condEval;      // cascadeEvalCondition leaf evaluations
	static int condEvalBy[CC_COUNT];   // the same evaluations, split by initiating caller domain
	static int condCaller;             // the live scope tag (CascadeCondScope sets/restores it)
	static int accRefresh;    // CascadeAccumulator component-refresh passes (dirty-triggered recomputes)
	static int wbCompute;     // CascadeWellbeing::compute runs (the §2b channel -- automation-cost attribution)
	// the ENABLER FRONTIER fills (the flip-era surfaces the census had NO ms bucket for): fill counts +
	// wall clock per domain -- how often each frontier rebuilds and what a rebuild costs
	static int frontBFills, frontUFills, frontPPFills, frontPFills, promoFills;
	// the flipped scalar getters (2026-07-04): read counts + refresh counts -- the unit-automation
	// slowdown attribution (reads say how hot each getter is; refreshes say how often the slots re-dirty)
	static int scGpBaseReads;
	static int scGpModReads;
	static int scDefReads;
	static int scMaintReads;
	static int scRefresh;     // ACCD_SCALAR refresh passes (the five-calculator recompute)
	static int scSpecRefresh; // ACCD_SCALARSPEC refresh passes (the gpBase specialist term)

	static double operatingBuildingsRecomputeMs;        // stopwatch accumulators (PerfAccumTimer targets)
	static double pctStackMs;
	static double wbComputeMs;
	static double scRefreshMs;    // the scalar refresh passes' wall clock (both bits)
	// the frontier-fill stopwatches (PerfAccumTimer targets; gPerfLogLevel-gated like every ms bucket)
	static double frontBMs, frontUMs, frontPPMs, frontPMs, promoMs;
	// the AUTOMATION window (owner 2026-07-04: "automation felt twice as long" had NO instrument -- the
	// between-turns play was unmeasured): every CvSelectionGroup::autoMission call, counted + timed, flushed
	// with the census at the next turn boundary.
	static int autoMissions;
	static double autoMissionMs;

	static void reset();
};

// The scoped caller tag for the condEval split: set at an outermost compute entry, restores the previous
// tag on exit (nesting-safe -- an inner operating buildings recompute inside a frontier fill attributes to operating buildings while it
// runs, back to the fill after). Cost: two int stores per scope; the attribution itself is one array
// increment per eval. Game-thread only, like every census counter.
struct CascadeCondScope
{
	int iPrev;
	explicit CascadeCondScope(int eCaller) : iPrev(CascadePerf::condCaller) { CascadePerf::condCaller = eCaller; }
	~CascadeCondScope() { CascadePerf::condCaller = iPrev; }
};

#endif // CV_CASCADE_PERF_COUNT_H
