#pragma once
#ifndef CV_CASCADE_YIELD_BASE_PACKAGES_H
#define CV_CASCADE_YIELD_BASE_PACKAGES_H

//
//	YieldBasePackages -- StoneBase YieldBasePackages.cs: the §1 BASE yield packages (TradeRoutePackage /
//	FreeCityPackage / GoldenAgePackage / SpecialistPackage) the CvCascadeAccumulator's scope-package fills
//	consume. See patterns.md (single-source law) + docs/plans/structural-cleanup/modifier-machine.md.
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
	// BASE: trade-route yield (TradeRoutePackage) -- the ONE allowed live-yield INPUT (the cascade folds it in, never
	// derives it; owner ruling 2026-06-28). Read from the live engine, x1.
	static int tradeRoute(YieldTypes eY, const CvCity* pCity);

	// BASE: free-city yield (FreeCityPackage) -- COMPUTED (not echoed): Σ the player's active traits' {ch}.empire.flat
	// (curate_trait YieldChanges). x1. (Active set option-gated + PURE_TRAITS via sumTrait/traitData.)
	static int freeCity(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec);

	// BASE: the golden-age trait member, UNGATED (the scope-package fill; the isGoldenAge gate is live at read).
	static int goldenAgeUngated(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec);

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
