#pragma once

#ifndef CV_GAME_SPEED_SCALE_H
#define CV_GAME_SPEED_SCALE_H

//
//	CvGameSpeedScale -- the ONE consuming-system calc for "scale this by the running game's SPEED".
//
//	CvGameSpeedInfo serves the authored percents as ×100 straggler scalars and NOTHING else: an info never
//	reads game state, so the option-gated hammer-cost derivation cannot live there (json.md §9 -- a game
//	option gates AT THE CONSUMING SYSTEM). This class IS that consuming system, held in one place rather
//	than re-derived per call site (docs/architecture/patterns.md §DRY (single implementation)).
//
//	⚠ EVERY method here returns a percent (100 = normal speed), which is what the info already serves: a
//	PERCENT IS NOT SCALED (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model) -- the ×100 exists so an AMOUNT can carry two decimals, and a
//	percentage has none). So nothing is converted here or at any call site.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state
//	(patterns.md static-class law; a namespace risks VC7.1/Boost name-mangling).
//

class CvGameSpeedScale
{
public:
	// The running game speed's master pace percent (`speed.world.percent`). Scales durations, thresholds,
	// anarchy, decay timers and research cost.
	static int speedPercent();

	// The pace percent as it applies to BUILDING AND UNIT PRODUCTION COST -- the speed percent, modified by
	// UPSCALED_HAMMER_COST_MODIFIER while GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS is live.
	static int hammerCostPercent();

	// The mission-yield scale percent (`missionYieldMultiplier.world.percent`) -- what a unit MISSION's
	// one-shot yield payload is scaled by.
	static int missionYieldPercent();

private:
	CvGameSpeedScale();                                  // never instantiated
	CvGameSpeedScale(const CvGameSpeedScale&);
	CvGameSpeedScale& operator=(const CvGameSpeedScale&);
};

#endif // CV_GAME_SPEED_SCALE_H
