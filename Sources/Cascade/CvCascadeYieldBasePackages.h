#pragma once
#ifndef CV_CASCADE_YIELD_BASE_PACKAGES_H
#define CV_CASCADE_YIELD_BASE_PACKAGES_H

//
//	YieldBasePackages -- StoneBase YieldBasePackages.cs: the §1 BASE yield packages (PlotPackage / TradeRoutePackage /
//	FreeCityPackage / GoldenAgePackage / SpecialistPackage) the YieldRate assembler sums. See patterns.md
//	(single-source law) + docs/plans/structural-cleanup/modifier-machine.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx -- the eval target for deposit conditions
#include <string>

class CvCity;
class CvPlayer;

class YieldBasePackages
{
public:
	// BASE: worked-plot yields (PlotPackage / calc-map §10.1) -- Σ over the city's WORKED plots of each plot's ONE isolated
	// base package, RE-DERIVED from the substrate JSON + keyed building/civic/trait deposits (NOT the engine's computed
	// getPlotYield -- the cascade computes it). Mirrors CvPlot::calculateYield order (Explore-verified 2026-06-30): nature
	// (max0 of relief+terrain+feature+bonus) + centre + plots-target + keyed-CITY = the pre-improvement running yield; the
	// extra/less + golden-age thresholds test on it; then the improvement addend (floored at -nature) + route-own-flat,
	// max(0,·); a CITY-CENTRE plot gets the min-city floor instead of improvement/route. (Traits use the option-gated active
	// set + PURE_TRAITS sign filter; the per-plot m_aExtraYield is event-granted, not derivable -> 0, audit-only per calc-map.)
	static int basePlot(const std::string& channel, YieldTypes eY, const CvCity* pCity, CvCascadeEvalCtx ec);

	// BASE: trade-route yield (TradeRoutePackage) -- the ONE allowed live-yield INPUT (the cascade folds it in, never
	// derives it; owner ruling 2026-06-28). Read from the live engine, x1.
	static int tradeRoute(YieldTypes eY, const CvCity* pCity);

	// BASE: free-city yield (FreeCityPackage) -- COMPUTED (not echoed): Σ the player's active traits' {ch}.empire.flat
	// (curate_trait YieldChanges). x1. (Active set option-gated + PURE_TRAITS via sumTrait/traitData.)
	static int freeCity(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec);

	// BASE: golden-age yield/commerce (GoldenAgePackage) -- the trait goldenAge member ({ch}.empire.goldenAge.flat) on the
	// active trait set while in a golden age, clamped at 0. x1. (Active set option-gated + PURE_TRAITS via sumTrait/traitData.)
	static int goldenAge(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec);

	// BASE: specialist yields (SpecialistPackage / calc-map §1.5) -- Σ over the city's assigned+typed-free specialists of
	// count × the engine terms: intrinsic × (100+pct)/100 (the percent MULTIPLIES the intrinsic only, ÷100 ONCE) +
	// building-local (gated city.flat, no percent) + perType (empire.flat, no percent) + governing-deliverer trait
	// (empire.specialists.{SPEC}.flat, active set) + perAll (building/civic/trait empire.specialist.perSpecialist × TOTAL
	// specialists). x1; channel-agnostic (reused for §2 commerce). count = getSpecialistCount + getFreeSpecialistCount
	// (calc-map §1.5). (Trait reads use the option-gated active set + PURE_TRAITS via sumTrait/traitData; the generic
	// building-free-spec output deferred, calc-map §2.)
	static int specialist(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);
};

#endif // CV_CASCADE_YIELD_BASE_PACKAGES_H
