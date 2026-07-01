#pragma once
#ifndef CV_CASCADE_CONDITION_EVAL_H
#define CV_CASCADE_CONDITION_EVAL_H

//
//	CvCascadeConditionEval -- the PORT of StoneBase `CascadingEnabler/ConditionEvaluator.cs`. Walks a typed
//	[CvCascadeCondition] tree and returns whether it holds, reading the LIVE engine (`CvCity`/`CvPlayer`/`CvPlot`/
//	`CvTeam`) wherever the C# reads its `EvalState`/`PlotContext` snapshot. The LOGIC is a faithful transcription
//	(StoneBase is the validated reference; owner ruling 2026-06-30) -- only the state reads differ. A NULL condition
//	is vacuously true; an UNKNOWN predicate is IGNORED (true), never false (json §3.5).
//

#include "CvCascadeCondition.h"
#include <set>

class CvCity;
class CvPlayer;
class CvPlot;
class CvUnit;
class CvTeam;

// The eval context = the live engine objects (StoneBase's `(EvalState s, PlotContext? p)`). `player`+`team` are
// always set; `city` for city-scope gates AND as the vicinity-scan source; `plot` is the deposit's/build's TARGET plot
// (the C# `PlotContext? p`) for per-plot predicates; `unit` for unit-scope. A predicate whose object is NULL here is
// treated as not-present (false) -- the cascade asks a city question only at a city scope.
struct CvCascadeEvalCtx
{
	const CvCity*   city;
	const CvPlayer* player;
	const CvTeam*   team;
	const CvPlot*   plot;
	const CvUnit*   unit;
	// The precomputed WAIVED-prereq BUILDING ids (StoneBase EvalState.ObsoleteBuildings ∪ PrereqWaivedBuildings):
	// a BUILDING prereq atom in this set is SKIPPED by EvalGroup (the engine PrereqInCity/OrBuilding waiver). Computed by
	// the cascade's AugmentState and pointed-to here; NULL = no waivers (the evaluator stays decoupled from InfoRepo).
	const std::set<int>* waivedPrereqBuildings;
	// The cascade-COMPUTED ACTIVE (present ∧ operate-holds ∧ ¬dormant-trigger) building ids for `city`; filled by
	// EnablerKernel::computeActiveBuildings. The evaluator READS it (stays decoupled from InfoRepo). Dormancy is 100%
	// governed by operate enablers -- DERIVED here, NEVER read from the engine active-building/`/state` (DEC-calc-zero-ride-in).
	// NULL = fall back to raw presence (hasBuilding).
	const std::set<int>* activeBuildings;
	CvCascadeEvalCtx() : city(NULL), player(NULL), team(NULL), plot(NULL), unit(NULL), waivedPrereqBuildings(NULL), activeBuildings(NULL) {}
};

// Evaluator flags (StoneBase's init-only props). For a `requires.build` gate set strictStateReligionForBuild=true.
struct CvCascadeEvalFlags
{
	bool strictStateReligionForBuild;   // {STATE_RELIGION:X} is the STRICT build gate (must MATCH), not the lenient modifier compound
	bool ignorePlotScope;               // per-plot PLACEMENT atoms are satisfied (an UNLOCK question, the engine availableBuilds)
	bool ignoreDisabled;                // a group's `disabled` (dormancy) clause is NOT applied (buildability vs operate)
	bool bonusFromPlot;                 // a bare {HAS_BONUS:X} reads THIS plot (plot-substrate yield), not the city's trade set
	CvCascadeEvalFlags()
		: strictStateReligionForBuild(false), ignorePlotScope(false), ignoreDisabled(false), bonusFromPlot(false) {}
};

// Is a building ACTIVE for `ec.city`? Reads the cascade-computed `ec.activeBuildings` set (present ∧ operate-holds ∧
// ¬dormant), or -- when that precompute is absent -- falls back to raw PRESENCE (hasBuilding, a raw input, NOT
// the engine active-building state). The shared read helper for every modifier calc + the evaluator (single source).
bool cascadeIsBuildingActive(int eBuilding, const CvCascadeEvalCtx& ec);

// Evaluate the condition tree against the live engine. `c == NULL` -> true (vacuous).
bool cascadeEvalCondition(const CvCascadeCondition* c, const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& flags);

#endif // CV_CASCADE_CONDITION_EVAL_H
