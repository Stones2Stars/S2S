#pragma once
#ifndef CV_CASCADE_PERF_COUNT_H
#define CV_CASCADE_PERF_COUNT_H

//	CvCascadePerfCount -- per-turn CALL COUNTERS + our own STOPWATCH accumulators for the cascade's hot compute
//	surfaces (owner 2026-07-02: chase the needless repeat calcs BEFORE parity). Counts reveal the repeat-calc
//	multipliers; the accumulated wall-clock (PerfAccumTimer, gPerfLogLevel-gated -- free when perf logging is off)
//	shows where the milliseconds actually go. Flushed + reset once per turn by the modifier shadow as
//	[MODIFIER/perf]. ⛔ DEFINED in CvCascadePerfCount.cpp -- never header-inline statics (the InfoRepo VC7.1
//	inline-static duplication lesson, same day).

struct CascadePerf
{
	static int facts;         // facts RECOMPUTES (recomputeCityFactsInto runs)
	static int factsMemoHit;  // facts READS served by the standing cache (cityFacts calls)
	static int yieldRate;     // YieldRate::yieldRate100 computes
	static int pctStack;      // PercentStack::percentStack computes
	static int commerceRate;  // CommerceCalc::commerceRate100 computes
	static int condEval;      // cascadeEvalCondition leaf evaluations
	static int accRefresh;    // CascadeAccumulator component-refresh passes (dirty-triggered recomputes)
	static int wbCompute;     // CascadeWellbeing::compute runs (the §2b channel -- automation-cost attribution)
	// the flipped scalar getters (2026-07-04): read counts + refresh counts -- the unit-automation
	// slowdown attribution (reads say how hot each getter is; refreshes say how often the slots re-dirty)
	static int scGpBaseReads;
	static int scGpModReads;
	static int scDefReads;
	static int scMaintReads;
	static int scRefresh;     // ACCD_SCALAR refresh passes (the five-calculator recompute)
	static int scSpecRefresh; // ACCD_SCALARSPEC refresh passes (the gpBase specialist term)

	static double factsMs;        // stopwatch accumulators (PerfAccumTimer targets)
	static double yieldRateMs;
	static double pctStackMs;
	static double commerceRateMs;
	static double wbComputeMs;
	// the LEGACY-side pair accumulators (owner 2026-07-04: capture what legacy is faster at BEFORE its cut --
	// the comparison window closes when the legacy body is deleted): the *Legacy oracle calls in the shadow
	// nets, timed beside the cascade-side numbers above (yieldRateMs+commerceRateMs vs legacyRateMs;
	// wbComputeMs vs legacyWbMs).
	static double legacyRateMs;
	static double legacyWbMs;
	static double scRefreshMs;    // the scalar refresh passes' wall clock (both bits)
	// the AUTOMATION window (owner 2026-07-04: "automation felt twice as long" had NO instrument -- the
	// between-turns play was unmeasured): every CvSelectionGroup::autoMission call, counted + timed, flushed
	// with the census at the next turn boundary.
	static int autoMissions;
	static double autoMissionMs;

	static void reset();
};

#endif // CV_CASCADE_PERF_COUNT_H
