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
//	([DEC-unit-modifiers-on-top]). Every refreshed slot logs its (scope, channel)
//	(CascadeCalcCount, [DEC-calc-count-gate]) and the rebuild announces itself (emitCacheRebuilt).
//
//	RECEIVER sum slots rebuild here too -- reading the scope packages through their OWN lazy dirty-check (no
//	dependency-ordered pass; a sum can never sit stale behind a clean package) and combining through the ONE
//	combine seam (InfoValuation::cityRate / realizedPercentStack -- the calc surface owns the math).
//
//	Purely-organizational static-methods class (patterns.md); game-thread only.
//

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
	static void refreshPlot(const CvPlot& plot, int64_t iMask);
	static void refreshCity(const CvCity& city, int64_t iMask);
	static void refreshEmpire(const CvPlayer& player, int64_t iMask);
	static void refreshTeam(const CvTeam& team, int64_t iMask);
	static void refreshArea(const CvArea& area, PlayerTypes ePlayer, int64_t iMask);

private:
	// One city RECEIVER channel's realized total from the ~5 scope packages (the §2a combine through
	// InfoValuation::cityRate) -- the city receiver rebuild's per-channel body.
	static long rebuildCityChannelSum(const CvCity& city, int iChannel, const CvCascadeEvalCtx& evalCtx);
};

#endif // CV_CASCADE_GATHER_H
