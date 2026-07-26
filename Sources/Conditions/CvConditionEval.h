#pragma once
#ifndef CV_CASCADE_CONDITION_EVAL_H
#define CV_CASCADE_CONDITION_EVAL_H

//
//	CvCascadeConditionEval -- the PORT of StoneBase `CascadingEnabler/ConditionEvaluator.cs`. Walks a typed
//	[CvCondition] tree and returns whether it holds, reading the LIVE engine (`CvCity`/`CvPlayer`/`CvPlot`/
//	`CvTeam`) wherever the C# reads its `EvalState`/`PlotContext` snapshot. The LOGIC is a faithful transcription
//	(StoneBase is the validated reference; owner ruling 2026-06-30) -- only the state reads differ. A NULL condition
//	is vacuously true; an UNKNOWN predicate is IGNORED (true), never false (json §3.5).
//

#include "CvCondition.h"
#include <set>

class CvCity;
class CvPlayer;
class CvPlot;
class CvUnit;
class CvTeam;
class CvPlotGroup;

// The eval context = the live engine objects (StoneBase's `(EvalState s, PlotContext? p)`), FILLED BY THE
// CONTEXTS (CityContext::fillEvalCtx = city/plot, EmpireContext::fillEvalCtx = player/team -- contexts.md: the
// contexts ARE the eval state, never a raw-pointer ctx built beside them). The bound pointers are the BINDING
// handles; the evaluator's atom/count reads go through each object's bound context (city->getCityContext(),
// player->getEmpireContext(), plot->getPlotContext() -- the HAVE axis), never an ad-hoc game-object reach.
// `player`+`team` are always set; `city` for city-scope gates AND as the vicinity-scan source; `plot` is the
// deposit's/build's TARGET plot (the C# `PlotContext? p`) for per-plot predicates; `unit` for unit-scope (units
// are the deliberate FUTURE context scope -- unit reads stay raw until it exists). A predicate whose object is
// NULL here is treated as not-present (false) -- the cascade asks a city question only at a city scope.
struct CvCascadeEvalCtx
{
	const CvCity*   city;
	const CvPlayer* player;
	const CvTeam*   team;
	const CvPlot*   plot;
	const CvUnit*   unit;
	// The trade-network object -- the reserved explicit TRADED-bonus source (contexts.md; traded state is NEVER
	// mirrored into CityContext). Filled by the valuation seam's plotGroup pass-in (InfoValuation::fillEvalCtx).
	// A city-bound ctx answers connection:"trade" through the city's own plot-group-backed maintained count
	// (CityContext::tradedBonusCount); this slot serves the city-less explicit pass-in.
	const CvPlotGroup* plotGroup;
	// The precomputed WAIVED-prereq BUILDING ids (StoneBase EvalState.ObsoleteBuildings ∪ PrereqWaivedBuildings):
	// a BUILDING prereq atom in this set is SKIPPED by EvalGroup (the engine PrereqInCity/OrBuilding waiver). Computed by
	// the cascade's AugmentState and pointed-to here; NULL = no waivers (the evaluator stays decoupled from InfoRepo).
	const std::set<int>* waivedPrereqBuildings;
	// The cascade-COMPUTED ACTIVE (present ∧ operate-holds ∧ ¬dormant-trigger) building ids for `city`; filled by
	// EnablerKernel::recomputeOperatingBuildingsInto (the standing operatingBuildings cache). The evaluator READS it (stays decoupled from InfoRepo). Dormancy is 100%
	// governed by operate enablers -- DERIVED here, NEVER read from the engine active-building/`/state` (DEC-calc-zero-ride-in).
	// NULL = fall back to raw presence (hasBuilding).
	const std::set<int>* activeBuildings;
	// The cascade-COMPUTED OBSOLETE (present ∧ obsoleted-by-held-tech) building ids for `city`; filled by the SAME
	// obsoletion process (recomputeOperatingBuildingsInto / the ripple) that fills activeBuildings. An obsolete building's
	// modifier reads its `whenObsolete` tree (json §4.2) in place of its normal deposits; read via cascadeIsBuildingObsolete.
	// NULL = none (obsolescence needs the team's held techs -- computed cascade-side, no raw-presence fallback).
	const std::set<int>* obsoleteBuildings;
	// The BONUS ids supplied IN-VICINITY by this city's ACTIVE buildings' `provides.bonuses` (json §5a): an active
	// building that provides X (e.g. a tamed-animal building supplying HORSE) ⇒ X is in vicinity. Filled by the enabler
	// (EnablerKernel::recomputeOperatingBuildingsInto, in the SAME pass as activeBuildings -- the standing operatingBuildings cache); the evaluator READS it (stays
	// decoupled from InfoRepo). Computed from JSON, NEVER read from the engine's hasVicinityBonus. NULL = none.
	const std::set<int>* vicinityProvidedBonuses;
	// The ENABLER-GATE atom mode (set by EnablerKernel::requiresMet): a city-scope BUILDING presence atom reads
	// raw PRESENCE -- the §7 object-owned has-list, the engine PrereqInCity/NotInCity mirror (a present-but-
	// DORMANT building still satisfies a positive prereq and still blocks a noneOf exclusion -- the burial-
	// tradition case). Deposits and the operate fixpoint keep the ACTIVE read (a dormant building deposits
	// nothing, json §3.2).
	bool buildingAtomsPresence;
	// The COUNTED RELIGION under test (the §3.7 `religion:` counted-kind filter, ruling 23): set per religion
	// by cascadeCountCityReligions while it evaluates the filter predicate; -1 outside that loop. The
	// IS_STATE_RELIGION predicate reads it against the player's state religion.
	int religion;
	CvCascadeEvalCtx() : city(NULL), player(NULL), team(NULL), plot(NULL), unit(NULL), plotGroup(NULL), waivedPrereqBuildings(NULL), activeBuildings(NULL), obsoleteBuildings(NULL), vicinityProvidedBonuses(NULL), buildingAtomsPresence(false), religion(-1) {}
};

// Evaluator flags (StoneBase's init-only props). For a `requires.build` gate set strictStateReligionForBuild=true.
struct CvCascadeEvalFlags
{
	bool strictStateReligionForBuild;   // {STATE_RELIGION:X} is the STRICT build gate (must MATCH), not the lenient modifier compound
	bool ignorePlotScope;               // per-plot PLACEMENT atoms are satisfied (an UNLOCK question, the engine availableBuilds)
	bool ignoreDisabled;                // a group's `disabled` (dormancy) clause is NOT applied (buildability vs operate)
	bool bonusFromPlot;                 // a bare {HAS_BONUS:X} reads THIS plot (plot-substrate yield), not the city's trade set
	bool testVisible;                   // the VISIBLE (build-list) frontier: the GREYABLE clauses -- a connectable BONUS_ resource
	                                    // and an unadopted CIVIC_ -- are treated as satisfied so the entity shows GREYED, not HIDDEN
	                                    // (enabler.md §6; "grey on resources / unadopted civics"). Hard hides (tech, building,
	                                    // terrain/placement, religion, ...) stay enforced. Mirrors legacy canConstruct(bTestVisible).
	CvCascadeEvalFlags()
		: strictStateReligionForBuild(false), ignorePlotScope(false), ignoreDisabled(false), bonusFromPlot(false), testVisible(false) {}
};

// Is a building ACTIVE for `ec.city`? Reads the cascade-computed `ec.activeBuildings` set (present ∧ operate-holds ∧
// ¬dormant), or -- when that precompute is absent -- falls back to raw PRESENCE (hasBuilding, a raw input, NOT
// the engine active-building state). The shared read helper for every modifier calc + the evaluator (single source).
bool cascadeIsBuildingActive(int eBuilding, const CvCascadeEvalCtx& ec);

// Is a building OBSOLETE for `ec.city`? Reads the cascade-computed `ec.obsoleteBuildings` set (present ∧
// obsoleted-by-held-tech, json §4.2) -- an obsolete building deposits its `whenObsolete` tree in place of its normal
// families and provides nothing. Maintained in the SAME obsoletion process as the active set; NULL set = none.
bool cascadeIsBuildingObsolete(int eBuilding, const CvCascadeEvalCtx& ec);

// THE count implementation ("how many of TYPE/token at SCOPE?") -- the ONE countable core shared by the
// condition count-atoms (ev_countOf) and the §3.7 `per` resolver (MMKernel::perScale); never a parallel count
// path (DEC-single-implementation). Routes by type prefix / catch-all token, tally-resolved at the cross-city
// scopes (tally.md); a type naming no countable domain falls back to presence 0/1.
int cascadeCountOf(int iTypeId, const std::string& sType, CvCascScope eScope, const CvCascadeEvalCtx& ec);

// Evaluate the condition tree against the live engine. `c == NULL` -> true (vacuous).
bool cascadeEvalCondition(const CvCondition* c, const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& flags);

// The §3.7 counted-kind RELIGION filter's count leg (ruling 23): how many of ec.city's present religions match
// `filter` (each religion tested with ctx.religion set -- the IS_STATE_RELIGION predicate's input). A NULL
// filter counts every present religion; no city -> 0. The ONE religion-count implementation -- the `religion:`
// qualifier's resolver (MMKernel::perScale) and any future consumer share it (DEC-single-implementation).
int cascadeCountCityReligions(const CvCondition* filter, const CvCascadeEvalCtx& ec);

// The ENTITY-LEVEL applicability gate (json.md §2 Applicability; owner 2026-07-08): the entity applies only
// while `enabled` holds (NULL = always) and `disabled` does not (NULL = never). Same evaluator, §3.9 order.
class CvGate;
bool cascadeGateOk(const CvGate* pGate, const CvCascadeEvalCtx& ec, const CvCascadeEvalFlags& flags);

#endif // CV_CASCADE_CONDITION_EVAL_H
