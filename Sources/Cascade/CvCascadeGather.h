#pragma once
#ifndef CV_CASCADE_GATHER_H
#define CV_CASCADE_GATHER_H

//
//	CascadeGather -- THE ENDPOINT ORACLE of the modifier cascade's value plane, and nothing else
//	(state-repositories.md § THE RECOMPUTE IS AN ENDPOINT ORACLE; [DEC-single-implementation]).
//
//	The gather*Into entry points recompute every slot of one scoped object FROM SOURCE into a CALLER-OWNED
//	CvCascadeSlotValues: fold every live source's compiled entries at this scope through the shared per-info
//	fold -- conditions through the ONE evaluator (MMKernel::applies), per scalers through the ONE §3.7 resolver
//	(MMKernel::perScale), audience through MMKernel::audienceOk, wellbeing sign-routed to its twin channel at
//	fill (modifier.md §2b). Unit-qualified entries never enter a package ([DEC-unit-modifiers-on-top]).
//	RECEIVER sums are gathered the same way, combining through the ONE combine seam (InfoValuation::cityRate /
//	realizedPercentStack -- the calc surface owns the math).
//
//	⛔ IT IS NOT A REBUILD PATH, AND THE STORED SLOTS ARE NEVER WRITTEN HERE. A package is a MAINTAINED SUM
//	filled only by the apply verbs from the facts ([DEC-maintained-sum]), so this class is the INDEPENDENT
//	SECOND DERIVATION the stored-vs-oracle pair diffs against. That independence is the entire value: while a
//	refresh filled the stored side, both served documents came out of these same folds, so the diff could never
//	turn red and verified nothing (the flaw that killed the old comparison-twin surface).
//	⚑ It is never handed a stored package -- which is what makes "the oracle never repairs" STRUCTURAL -- and it
//	announces nothing, because nothing was rebuilt.
//
//	⛔ AN ORACLE RUN IS A FULL RECALC AND READS NOTHING OFF THE STORED SURFACE (owner). Every input, including
//	every CROSS-SCOPE one, is recomputed from source: a city's realized total re-gathers each worked plot, the
//	empire and the team into fresh documents first. An oracle that consumed a stored
//	value would be partly built on the very state it exists to check -- a wrong input silently INHERITED, both
//	sides quietly sharing a derivation, which is exactly the flaw that killed the old comparison-twin surface.
//	INDEPENDENCE IS THE ENTIRE VALUE OF THE ORACLE and nothing is traded for it: attribution is the external
//	reader's job (a drifted low scope diverges on its OWN row, so the differ walks upward from the lowest
//	diverging scope), and COST IS IRRELEVANT -- it is invoked deliberately, never on a turn path, so it is
//	never trimmed, sampled, memoized or made incremental to look cheap.
//
//	TERMINATION: a channel fold reads only compiled deposits and live game state -- never a package -- so it
//	cannot ask for another scope. The only consumers of other scopes are the RECEIVER SUM legs, and they reach
//	other scopes exclusively through the gt_fresh*Document helpers, which gather CHANNELS ONLY and can neither
//	produce nor request a sum. The oracle's call graph is therefore a three-level DAG (empire sums -> per-city
//	realized rate -> channel documents) with no path back into itself.
//
//	Purely-organizational static-methods class (patterns.md); game-thread only.
//

#include "CvCascadeSlotValues.h"
#include "CvCondition.h"   // CvCascScope

class CvPlot;
class CvCity;
class CvPlayer;
class CvTeam;

struct CvCascadeEvalCtx;

class CascadeGather
{
public:
	// THE ORACLE, AND NOTHING ELSE -- every slot of one scoped object, recomputed FROM SOURCE into the caller's
	// document. The stored package is neither read as an output nor written.
	// ⛔ THERE IS NO REBUILD PATH HERE ANY MORE. The stored slots are a MAINTAINED SUM, filled only by the apply
	// verbs from the facts ([DEC-maintained-sum]); this class is now purely the independent second derivation
	// the stored-vs-oracle tripwire diffs against. That independence is the whole point: while a refresh filled
	// the stored side, BOTH served documents came out of these same folds and the diff could not turn red.
	static void gatherPlotInto(const CvPlot& plot, CvCascadeSlotValues& kValues);
	static void gatherCityInto(const CvCity& city, CvCascadeSlotValues& kValues);
	static void gatherEmpireInto(const CvPlayer& player, CvCascadeSlotValues& kValues);
	static void gatherTeamInto(const CvTeam& team, CvCascadeSlotValues& kValues);

};

#endif // CV_CASCADE_GATHER_H
