#pragma once
#ifndef CV_CASCADE_GATHER_H
#define CV_CASCADE_GATHER_H

//
//	CascadeGather -- the ONE package-rebuild implementation of the modifier cascade's value-cache plane
//	(state-repositories.md; [DEC-single-implementation]). Every owner's refresh delegate (the
//	CvDerivedCacheSet member-function contract) is a one-line delegation here; no second gather exists.
//
//	A refresh receives the DIRTY MASK (the derived marks -- SourceRoute / the dependency routes) and, per the
//	CvDerivedCache contract rule 2, FULLY DEFINES the masked slots: zero them, then fold every live source's
//	compiled entries at this scope through the shared per-info fold -- conditions through the ONE evaluator
//	(MMKernel::applies, the conditioned evaluation at REBUILD cadence, modifier.md §3), per scalers through
//	the ONE §3.7 resolver (MMKernel::perScale), audience through MMKernel::audienceOk, wellbeing sign-routed
//	to its twin channel at fill (modifier.md §2b). Unit-qualified entries never enter a package
//	([DEC-unit-modifiers-on-top]). Every rebuild announces itself with its (scope, owner, id, mask) through
//	emitCacheRebuilt -- the [CASCADE] rebuilt line is how a package's recompute is observable at all.
//
//	RECEIVER sum slots rebuild here too -- reading the scope packages through their OWN lazy dirty-check (no
//	dependency-ordered pass; a sum can never sit stale behind a clean package) and combining through the ONE
//	combine seam (InfoValuation::cityRate / realizedPercentStack -- the calc surface owns the math).
//
//	THE ORACLE LEG (state-repositories.md, the endpoint oracle) -- the gather*Into entry points run the SAME
//	folds over a CALLER-OWNED CvCascadeSlotValues so an endpoint can serve a fresh from-source recompute beside
//	the stored values and an external consumer can diff them. It is never handed a stored package, which is
//	what makes "the oracle never repairs" structural; and it does not announce a rebuild, because nothing was
//	rebuilt -- a [CASCADE] rebuilt line from a sweep would claim a stored slot had just moved when none did.
//
//	⛔ AN ORACLE RUN IS A FULL RECALC AND READS NOTHING OFF THE STORED SURFACE (owner). Every input, including
//	every CROSS-SCOPE one, is recomputed from source: a city's realized total re-gathers each worked plot, the
//	empire, the team and the (area x player) slot into fresh documents first. An oracle that consumed a stored
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
class CvArea;

struct CvCascadeEvalCtx;

class CascadeGather
{
public:
	// THE REBUILD PATH -- refresh the masked slots of the owner's OWN package (the CvDerivedCacheSet refresh
	// delegate every scope owner binds), counted and announced.
	static void refreshPlot(const CvPlot& plot, int64_t iMask);
	static void refreshCity(const CvCity& city, int64_t iMask);
	static void refreshEmpire(const CvPlayer& player, int64_t iMask);
	static void refreshTeam(const CvTeam& team, int64_t iMask);
	static void refreshArea(const CvArea& area, PlayerTypes ePlayer, int64_t iMask);

	// THE ORACLE -- every slot of one scoped object, recomputed FROM SOURCE into the caller's document. The
	// stored package is neither read as an output nor written.
	static void gatherPlotInto(const CvPlot& plot, CvCascadeSlotValues& kValues);
	static void gatherCityInto(const CvCity& city, CvCascadeSlotValues& kValues);
	static void gatherEmpireInto(const CvPlayer& player, CvCascadeSlotValues& kValues);
	static void gatherTeamInto(const CvTeam& team, CvCascadeSlotValues& kValues);
	static void gatherAreaInto(const CvArea& area, PlayerTypes ePlayer, CvCascadeSlotValues& kValues);

};

#endif // CV_CASCADE_GATHER_H
