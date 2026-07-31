#pragma once

#ifndef CV_BUILD_COST_SCALE_H
#define CV_BUILD_COST_SCALE_H

//
//	CvBuildCostScale -- the ONE consuming-system calc for "what does this entity's OWN authored cost data do to
//	its build cost".
//
//	CvBuildingInfo serves the authored `cost` section and NOTHING else: an info never reads game state, so an
//	option-gated composition cannot live there (json.md §9 -- a game option gates AT THE CONSUMING SYSTEM). This
//	class IS that consuming system, the sibling of CvGameSpeedScale, which already owns the other half of the
//	same calculation (the gamespeed/upscaled-cost pace percent).
//
//	⚑ THE THREE COST PLANES stay separate (json.md §6): the entity's own AUTHORED cost is the `cost` section; what
//	CHANGES a cost is the `costs` MODIFIER family; the derived PRICE is engine-computed from the two. This class is
//	the third plane's building half, and it composes ONLY plane 1 with its game option -- it is not a place to
//	fold `costs` deposits, which the cascade already carries.
//
//	⚠ EVERY method returns a PERCENT (100 = the authored cost unmodified), matching CvGameSpeedScale's contract,
//	so a caller multiplies and divides by 100 once. A PERCENT IS NOT SCALED ([DEC-fixedpoint-x100]).
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state
//	(patterns.md static-class law; a namespace risks VC7.1/Boost name-mangling).
//

class CvBuildingInfo;

class CvBuildCostScale
{
public:
	//	The REALISTIC-BUILDING-COST composition: a building's size / count / materials / complexity bands read as
	//	one percent, while GAMEOPTION_REALISTIC_BUILDING_COST is live (100 = no change, and 100 whenever the option
	//	is off, so a caller needs no branch of its own).
	//
	//	⚠ INTEGER, deliberately. The inline derivation this replaces accumulated a `float` multiplier
	//	(`1.0f`, `-= 0.2f`, `iBaseCost * totalModifier`) on a path every build decision runs. Civ4 multiplayer is
	//	deterministic lockstep and CPU-dependent float math desyncs ([DEC-fixedpoint-x100]: all engine math is
	//	integer, no float), so carrying it as whole percent points removes a real OOS hazard. The bands are all
	//	multiples of 5%, so nothing is lost in the conversion and the resulting cost is unchanged.
	//
	//	⚠ The band values are the AUTHORED enum-ish codes, not magnitudes: each field is a small band index where
	//	1 (or an unauthored field) is the neutral middle. That is why this reads them by equality rather than
	//	scaling by them.
	static int buildingCostPercent(const CvBuildingInfo& kBuilding);

private:
	CvBuildCostScale();                                    // never instantiated
	CvBuildCostScale(const CvBuildCostScale&);
	CvBuildCostScale& operator=(const CvBuildCostScale&);
};

#endif // CV_BUILD_COST_SCALE_H
