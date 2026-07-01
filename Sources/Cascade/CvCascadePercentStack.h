#pragma once
#ifndef CV_CASCADE_PERCENT_STACK_H
#define CV_CASCADE_PERCENT_STACK_H

//
//	PercentStack -- StoneBase PercentStack.cs over ModifierMath.SumUnitAtScope: the single additive
//	`modifier = max(0, 100 + Σ {channel}.<scope>.percent)` over every active source (city/area/empire buildings,
//	adopted civics, active traits). Shadowed vs the legacy getBaseYieldRateModifier. See patterns.md (single-source law)
//	+ docs/plans/structural-cleanup/modifier-machine.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include <string>

class CvCity;

// The cascade percent-stack buckets (1b attribution) — so a divergence localises vs legacy modBuilding/modPlayer/modCapital.
struct MMBreak { int bCity, bArea, bEmpire, civic, trait; MMBreak() : bCity(0), bArea(0), bEmpire(0), civic(0), trait(0) {} };

class PercentStack
{
public:
	// The percent stack for one channel at one city: max(0, 100 + Σ percent) over active city buildings (city+area),
	// empire buildings (empire), adopted civics (empire), and the player's active traits (empire). Fills the breakdown.
	static int percentStack(const std::string& channel, const CvCity* pCity, MMBreak& bk);
};

#endif // CV_CASCADE_PERCENT_STACK_H
