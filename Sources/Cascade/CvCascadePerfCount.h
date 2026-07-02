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
	static int facts;         // computeCityBuildingFacts entries
	static int factsMemoHit;  // ... of which served by the turn memo
	static int yieldRate;     // YieldRate::yieldRate100 computes
	static int pctStack;      // PercentStack::percentStack computes
	static int commerceRate;  // CommerceCalc::commerceRate100 computes
	static int condEval;      // cascadeEvalCondition leaf evaluations
	static int accRefresh;    // CascadeAccumulator component-refresh passes (dirty-triggered recomputes)

	static double factsMs;        // stopwatch accumulators (PerfAccumTimer targets)
	static double yieldRateMs;
	static double pctStackMs;
	static double commerceRateMs;

	static void reset();
};

#endif // CV_CASCADE_PERF_COUNT_H
