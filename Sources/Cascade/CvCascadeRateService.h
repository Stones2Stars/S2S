#pragma once
#ifndef CV_CASCADE_RATE_SERVICE_H
#define CV_CASCADE_RATE_SERVICE_H

//
//	CascadeRates -- the #430 FLIPPED-GETTER rate service (cutover.md ruling 5 step 2; modifier.md).
//
//	Post-flip, CvCity::getYieldRate100 / getCommerceRateTimes100 return THESE values in a running game (the
//	legacy expression stays in-body as the [GETTER] net oracle + the one-commit rollback; the load path stays
//	legacy -- the flip arms at final-init). The service owns the ONE rate memo:
//	 - event-invalidated: a per-city VERSION (bumped by that city's building/pop/specialist/worked-plot
//	   mutators), a global EPOCH (tech/civic/trait/golden-age), and a commerce EPOCH (slider moves -- yields
//	   don't depend on the slider, so they survive a slider bump);
//	 - turn-stamped: any mutation the coarse v1 hooks miss self-heals at the turn boundary, and the armed
//	   [GETTER] net MEASURES the residual staleness as attributable diff lines in the meantime.
//	Facts ride along: invalidateCity evicts that city's EnablerKernel facts-memo entry; invalidateAll clears it.
//

#include "Defines/CvEnums.h"

class CvCity;

class CascadeRates
{
public:
	static long yieldRate100(const CvCity* pCity, YieldTypes eY);
	static long commerceRate100(const CvCity* pCity, CommerceTypes eC);

	static void invalidateCity(const CvCity* pCity);       // building/population change: rates + the city's facts entry
	static void invalidateCityRates(const CvCity* pCity);  // worked-plot/specialist change: rates only (facts don't depend on them)
	static void invalidateAll();                           // tech/civic/trait/golden-age -- global epoch
	static void invalidateCommerce();                      // slider move -- commerce entries only
};

#endif // CV_CASCADE_RATE_SERVICE_H
