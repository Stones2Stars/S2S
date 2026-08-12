#pragma once
#ifndef CV_CASCADE_CONDITION_EVAL_H
#define CV_CASCADE_CONDITION_EVAL_H

//
//	CvCascadeConditionEval -- the PORT of StoneBase `CascadingEnabler/ConditionEvaluator.cs`. Walks a typed
//	[CvCondition] tree and returns whether it holds, reading the per-scope live-state CONTEXTS (CityContext /
//	EmpireContext / PlotContext) wherever the C# reads its `EvalState`/`PlotContext` snapshot. The LOGIC is a faithful transcription
//	(StoneBase is the validated reference; owner ruling 2026-06-30) -- only the state reads differ. A NULL condition
//	is vacuously true; an UNKNOWN predicate is IGNORED (true), never false (json §3.5).
//

#include "CvCondition.h"
#include "CvEdges.h"   // EnEdgeBucket / NUM_EDGEB -- the interned bucket vocabulary the hypothetical keys on
#include <set>

class CvUnit;
class CvPlotGroup;
class CityContext;
class EmpireContext;
class PlotContext;

// THE AS-IF-HELD HYPOTHETICAL -- the GATE twin of the membership overlay (Enabler/CvEnablerOverlay.h). A caller
// asking "would this candidate's `requires` pass if I ALSO held X, and no longer held Y" fills this and hands it
// to the evaluator; every HAVE atom of a covered kind answers from it before the live scope is asked.
//
// ⛔ CALLER-OWNED, and NULL on every ordinary evaluation -- exactly the discipline the membership overlay keeps
// (it never writes the maintained planes; this never writes a context). A hypothetical that mutated a context
// store would leave every other reader evaluating against a game state that never happened, and with no
// self-heal anywhere ([DEC-no-self-heal]) nothing would put it back.
//
// ⚖ WHY IT IS SEPARATE FROM THE MEMBERSHIP OVERLAY rather than one class: a HAVE changes two different things
// and only one of them is this. Whether a candidate is IN THE TREE is the membership formula over the enable/
// remove planes; whether a tree member is ATTAINABLE is its `requires` gate (enabler.md par.1 -- `requires` never
// changes membership). The BONUS axis is the case that forces the split: a bonus is GATE-ONLY, so it has no
// membership meaning at all and the overlay refuses one, while this is precisely where it does its work.
//
// ⚠ ABSENT WINS over present, mirroring the membership formula's removal-wins rule, so the two halves of one
// what-if can never disagree about whether the caller holds something.
struct CvCascadeHypothetical
{
	std::set<int> present[NUM_EDGEB];   // treat as HELD even though the live scope does not hold it
	std::set<int> absent[NUM_EDGEB];    // treat as NOT held even though the live scope does (the swap's other half)

	bool has(int eBucket, int iId, bool bLive) const
	{
		if (iId < 0 || eBucket < 0 || eBucket >= NUM_EDGEB) return bLive;
		if (absent[eBucket].count(iId) != 0) return false;
		if (present[eBucket].count(iId) != 0) return true;
		return bLive;
	}
};

// The eval context = THE CONTEXTS THEMSELVES (StoneBase's `(EvalState s, PlotContext? p)`), handed in by their
// owning objects (CityContext::fillEvalCtx, EmpireContext::fillEvalCtx -- contexts.md: the contexts ARE the eval
// state, never a raw-pointer ctx built beside them). Each context is the isolated live-state SILO of its scope,
// so the evaluator's atom/count reads go through it and there is no game object here to reach past.
// `empireContext` is always set (and answers the TEAM facts too); `cityContext` for city-scope gates AND as the
// vicinity source; `plotContext` is the deposit's/build's TARGET plot (the C# `PlotContext? p`) for per-plot
// predicates; `unit` for unit-scope (the deliberate FUTURE context scope -- unit reads stay raw until it
// exists). A predicate whose context is NULL here is treated as not-present (false) -- the cascade asks a city
// question only at a city scope.
struct CvCascadeEvalCtx
{
	// ⛔ THE CTX CARRIES CONTEXTS, NOT GAME OBJECTS (owner, [contexts.md]). Each scope's context is the isolated
	// live-state SILO its owning object holds as a member (CvCity::m_cityContext, CvPlayer::m_empireContext,
	// CvPlot::m_plotContext), and a reader is handed the SILO -- never the object housing it. Passing the
	// object would hand over the whole god-class with the silo one `->` inside it, which is the same as having
	// no boundary: the isolation has to be STRUCTURAL ("there is no member to reach through"), not a convention
	// two helper functions and a reviewer's memory uphold.
	// ⛔ There is deliberately NO `CvTeam*`: a team is the TECH BRIDGE and owns no live-state surface, so every
	// team fact is asked of the PLAYER (EmpireContext::teamHasTech / teamId / teamMemberCount). A team package
	// exists for DEPOSITS, which is a different axis -- do not read it as licence for a team context.
	// ⚑ A context that cannot answer a needed fact is a CONTEXT GAP to close by adding the forward, never a
	// reason to re-add an object pointer -- that forcing function is the whole point of the structural form.
	const CityContext*   cityContext;
	const EmpireContext* empireContext;
	const PlotContext*   plotContext;
	// ⚠ The ONE acknowledged hole ([contexts.md]): units are the deliberate FUTURE role-specific scope, so this
	// stays raw until that context lands. It is not a precedent for the others.
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
	// The SOURCE BUILDING whose deposit is being resolved (the `religion` slot's shape, one axis over): set
	// per-iteration by the walks that know the id -- the gather's city-building fold and the per-building
	// valuation seam -- and -1 everywhere else. It exists because an entry cannot name its own carrier: neither
	// a compiled CvModEntry nor an info knows its engine id, so a predicate about the SOURCE (existedFor -- how
	// long has THIS building stood) has no other way to ask.
	// ⛔ -1 means "no source in hand", and a source-predicate must answer FALSE there rather than guessing: a
	// scope-wide read that never set it would otherwise age-gate against whatever building came last.
	int sourceBuilding;
	// The CIVIC whose value is being resolved (the `religion`/`sourceBuilding` slot's shape, one axis over):
	// set per-iteration by the walk that knows the civic, on a LOCAL COPY of the ctx. -1 = no civic in hand,
	// and {CIVIC_CATEGORY:X} then answers FALSE -- resolving it against whichever civic a walk reached last
	// would be worse than declining (contexts.md § THE SOURCE SLOTS).
	int civic;
	// The AS-IF-HELD hypothetical (above) -- NULL on every ordinary evaluation, so the normal path pays one
	// null test. Set ONLY by a caller asking a what-if, and never stored anywhere.
	const CvCascadeHypothetical* hypothetical;
	CvCascadeEvalCtx() : cityContext(NULL), empireContext(NULL), plotContext(NULL), unit(NULL), plotGroup(NULL), waivedPrereqBuildings(NULL), activeBuildings(NULL), obsoleteBuildings(NULL), vicinityProvidedBonuses(NULL), buildingAtomsPresence(false), religion(-1), sourceBuilding(-1), civic(-1), hypothetical(NULL) {}
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

// Is a building ACTIVE for `ec.cityContext`? Reads the cascade-computed `ec.activeBuildings` set (present ∧ operate-holds ∧
// ¬dormant), or -- when that precompute is absent -- falls back to raw PRESENCE (hasBuilding, a raw input, NOT
// the engine active-building state). The shared read helper for every modifier calc + the evaluator (single source).
bool cascadeIsBuildingActive(int eBuilding, const CvCascadeEvalCtx& ec);

// Is a building OBSOLETE for `ec.cityContext`? Reads the cascade-computed `ec.obsoleteBuildings` set (present ∧
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

// WHICH ATOM refused -- the leaf a FALSE verdict actually turns on. Returns NULL when the tree holds (nothing
// refused), so a caller tests the pointer rather than evaluating twice.
//
// ⛔ IT IS A NAVIGATION, NEVER A SECOND EVALUATOR ([DEC-single-implementation]). Every truth question it asks
// goes to cascadeEvalCondition above; all this decides is WHICH CHILD to descend into, mirroring that
// function's own arms (including the waived-prereq skip). A consumer re-deriving the cause by re-testing the
// clauses itself would be a second gate implementation, free to disagree with the verdict it claims to explain
// ([enabler.md] par.6) -- which is exactly why this is a declared surface beside the evaluator and not a walk
// inside whichever consumer wanted it first.
//
// ⚠ POLARITY INVERTS UNDER `noneOf`, and that is the whole subtlety: a `noneOf` fails on a child that HOLDS,
// so the offending atom there is a TRUE one. That is why "the tree mentions a tech" is not the same statement
// as "a tech refused it" -- a `noneOf` names the thing it FORBIDS (json.md par.3.4), and CvConditionQuery,
// which answers what a tree MENTIONS, deliberately cannot answer this question.
const CvCondition* cascadeFailingAtom(const CvCondition* c, const CvCascadeEvalCtx& ctx,
	const CvCascadeEvalFlags& flags);

// The TOP-LEVEL CLAUSE decomposition of a requires tree -- what a reader means by "the clauses" of a
// requirement. An `all` root is a LIST of independent clauses; anything else is ONE clause standing whole.
// [DEC-single-implementation]: two consumers need the same decomposition for OPPOSITE reasons and each had
// grown its own -- the tooltip renderer walks the clauses to colour each by its own verdict, the enabler walks
// them to weigh hide-vs-grey across ALL the failures. While both walks were private, one stopped at the first
// failure and the other did not, so clause ORDER decided what the player saw in one place and nothing in the
// other. Only the `all` arm yields separate clauses; an `anyOf`/`noneOf` root is ONE clause, because "any of
// these" is a single requirement and naming one member would misreport what refused.
void cascadeTopLevelClauses(const CvCondition* pRoot, std::vector<const CvCondition*>& kClausesOut);

// The §3.7 counted-kind RELIGION filter's count leg (ruling 23): how many of ec.cityContext's present religions match
// `filter` (each religion tested with ctx.religion set -- the IS_STATE_RELIGION predicate's input). A NULL
// filter counts every present religion; no city -> 0. The ONE religion-count implementation -- the `religion:`
// qualifier's resolver (MMKernel::perScale) and any future consumer share it (DEC-single-implementation).
int cascadeCountCityReligions(const CvCondition* filter, const CvCascadeEvalCtx& ec);

// The ENTITY-LEVEL applicability gate (json.md §2 Applicability; owner 2026-07-08): the entity applies only
// while `enabled` holds (NULL = always) and `disabled` does not (NULL = never). Same evaluator, §3.9 order.
class CvGate;
bool cascadeGateOk(const CvGate* pGate, const CvCascadeEvalCtx& ec, const CvCascadeEvalFlags& flags);

#endif // CV_CASCADE_CONDITION_EVAL_H
