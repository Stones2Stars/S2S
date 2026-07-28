#pragma once

#ifndef CV_BUILD_LIST_VALUATION_H
#define CV_BUILD_LIST_VALUATION_H

//
//	CvBuildListValuation -- the ONE seam the build-list UI values a candidate through.
//
//	patterns.md § THE VALUATION PROTOCOL: the live CONTEXTS go in and the proposed INCREASE comes out. The AI
//	weighting a candidate and the build list's filters/sorts are the TWO CONSUMERS OF ONE CALL, which is what
//	makes the number the player is shown and the number the AI acts on the same number structurally
//	([DEC-single-implementation]) -- so a criterion never re-derives a value, it asks here.
//
//	⚖ THE CITY-LESS VIEW VALUES AGAINST THE CAPITAL (owner). A criterion is asked either for a city's
//	production list (a city is bound) or for the player-level "all buildings" view (none is). The second has no
//	CityContext to evaluate against, and the answer is the AI's own precedent made explicit: value the candidate
//	against the player's CAPITAL. That resolution lives HERE, once, rather than in every criterion.
//
//	A player with no capital has nothing to value against; the fill functions answer false and the criterion
//	ranks the candidate neutral rather than inventing a number.
//
//	⚠ Every value returned is ×100 ([DEC-fixedpoint-x100]) -- a caller mixing one with a human-scale number
//	reduces at ITS point of use, never here.
//

#include "Infos/CvInfoKinds.h"   // the channel/kind vocabulary the groups are indexed by

class CvPlayer;
class CvCity;

class CvBuildListValuation
{
public:
	// The city a criterion values against: the bound one, else the player's capital. NULL when neither exists.
	static const CvCity* valuationCity(const CvPlayer* pPlayer, const CvCity* pCity);

	// The per-GROUP what-if reads, resolved through that city. Each fills the caller's array and returns
	// false (leaving it untouched) when there is no city to value against.
	static bool buildingFlatYields(BuildingTypes eBuilding, const CvPlayer* pPlayer, const CvCity* pCity,
		int (&aFlatYields)[NUM_YIELD_TYPES]);
	static bool buildingYieldModifiers(BuildingTypes eBuilding, const CvPlayer* pPlayer, const CvCity* pCity,
		int (&aYieldModifiers)[NUM_YIELD_TYPES]);
	static bool buildingFlatCommerce(BuildingTypes eBuilding, const CvPlayer* pPlayer, const CvCity* pCity,
		int (&aFlatCommerce)[NUM_COMMERCE_TYPES]);
	static bool buildingWellbeing(BuildingTypes eBuilding, const CvPlayer* pPlayer, const CvCity* pCity,
		int (&aWellbeing)[NUM_WELLBEING_CHANNELS]);

	// The opposing-pair balances the wellbeing group hands out, composed by the caller's rule rather than a
	// second implementation: happiness - anger, health - unhealth (patterns.md rule 6 -- a final-state value is
	// downstream of the group, never a slot in it).
	static bool buildingHappinessBalance(BuildingTypes eBuilding, const CvPlayer* pPlayer, const CvCity* pCity,
		int& iBalanceOut);
	static bool buildingHealthBalance(BuildingTypes eBuilding, const CvPlayer* pPlayer, const CvCity* pCity,
		int& iBalanceOut);
};

#endif
