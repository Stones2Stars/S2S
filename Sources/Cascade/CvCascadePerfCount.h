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
	CC_FRONT_B,     // RETIRED tag (the whole-set buildable fill died with the city box) -- kept for slot stability
	CC_FRONT_U,     // RETIRED tag (the whole-set trainable fill died with the city box) -- kept for slot stability
	CC_FRONT_PP,    // RETIRED tag (the creatable+maintainable box fills died with the city box) -- kept for slot stability
	CC_FRONT_P,     // the researchable/civics/hurries/buildRem player fill
	CC_CANBUILD,    // RETIRED tag (the canBuild unlock is a bare builds-domain read now) -- kept for slot stability
	CC_PROMO,       // the promotion frontier halves + the per-unit composite
	CC_COUNT
};

// The (scope, channel) CALC-COUNT axis -- [DEC-calc-count-gate] / observability.md. Every package-value recompute
// increments calcCount[scope][channel]; the census flushes the total + marginals (the [MODIFIER/perf] line, 16-field
// cap) and /computed/perf exposes the full scope x channel matrix live. A quiet turn (no state change) touches
// nothing -> ~0; a blanket recompute balloons it; >50k/turn is near-certainly a failure (a blanket has crept back),
// and the histogram names the culprit scope/channel. Steady-state tracks EVENT volume, never entity count. Ungated
// int-increment-per-event (perf-profile-wiring.md: the counter is always-on; only stopwatches are gPerfLogLevel-gated).
// Scope axis = observability.md's nine; channel axis = the owner's family list ("any number modified by game
// mechanics is a yield") -- EXTENSIBLE: add a CCHAN_* when a new modifiable number appears (CCHAN_OTHER is the honest
// residual -- a big OTHER names the next channel to split).
enum CascadeCalcScope
{
	CSCOPE_WORLD = 0,
	CSCOPE_TEAM,
	CSCOPE_EMPIRE,       // the player scope
	CSCOPE_AREA,
	CSCOPE_CITY,
	CSCOPE_PLOT,
	CSCOPE_BUILDING,
	CSCOPE_UNIT,
	CSCOPE_SPECIALIST,
	CSCOPE_COUNT
};
enum CascadeCalcChan
{
	CCHAN_BASE_YIELDS = 0,   // yield packages (percent / specialist / building-flat)
	CCHAN_COMMERCE,          // commerce packages (specialist / percent / base-own / keyed / SR-match)
	CCHAN_WELLBEING,         // health + happiness city terms + verdict assemble
	CCHAN_GP,                // great-people rate (base + modifier + specialist)
	CCHAN_DEFENSE,           // defense amount + bombard + min
	CCHAN_MAINTENANCE,       // maintenance modifier
	CCHAN_TRADE,             // trade routes (city + coastal)
	CCHAN_BUILDRATE,         // buildRate ledgers + members
	CCHAN_FREE_SPECIALISTS,  // freeSpecialists AMOUNT fills (the ruled seam)
	CCHAN_FREE_XP,           // free experience
	CCHAN_PROPERTIES,        // property source magnitudes
	CCHAN_FRONTIER,          // the enabler frontier fills (buildable / trainable / creatable / maintainable / promo)
	CCHAN_OTHER,             // residual bucket (a big OTHER names the next channel to split -- an honest gap)
	CCHAN_COUNT
};

struct CascadePerf
{
	// the (scope, channel) calc-count -- the DEC-calc-count-gate histogram; reset per turn like every census counter
	static long calcCount[CSCOPE_COUNT][CCHAN_COUNT];
	static inline void calc(int eScope, int eChan) { ++calcCount[eScope][eChan]; }
	static inline void calcN(int eScope, int eChan, int n) { calcCount[eScope][eChan] += n; }

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
