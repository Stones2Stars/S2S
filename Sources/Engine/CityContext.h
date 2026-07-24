#pragma once
#ifndef CV_CITY_CONTEXT_H
#define CV_CITY_CONTEXT_H

//
//	CityContext -- the per-city ISOLATED live state a building's output getters (and the one condition evaluator)
//	read to compute the ACTUAL benefit in this city. Owned by CvCity, kept current EVENT-DRIVEN (never a per-turn
//	recompute).
//
//	⛔ It holds COUNTS, never the objects themselves (owner): the building cares HOW MANY river plots / religions /
//	vicinity bonuses it has, never WHICH. A plots-target (or per-keyed) deposit's output is `flat x count(id)`; a
//	gate is just `has(id)`. Self-contained -- a raw pointer is passed directly into an info getter, no copy.
//
//	CITY-scope only. EMPIRE-scope facts (state religion, policies) are NOT mirrored here -- they live on the owner's
//	EmpireContext, which the city eval reaches up the scope chain (owner: don't duplicate empire data per city).
//	TRADED bonuses stay on CvPlotGroup, passed alongside; CityContext carries VICINITY + local only, never traded.
//	Everything keyed uses the shared ContextDict, so the read is uniform and each family's key set is OPEN.
//

#include "ContextDict.h"

class CvPlot;

class CityContext
{
public:
	CityContext() : m_population(0), m_power(0) {}

	// --- always-present city SCALARS (every city has one) -- ints, volumetric-ready ---
	int  population() const { return m_population; }   void setPopulation(int n) { m_population = n; }
	int  power() const      { return m_power; }        void setPower(int n)      { m_power = n; }   // HAS_POWER = power() > 0 (int for future volumetric)

	// --- the keyed CITY dictionaries (shared ContextDict) -- the info reads cx.<dict>.has(id) / .count(id) directly ---
	ContextDict plotAttrs;        // CASC_PRED_* HAS_/IS_ plot predicate -> plot count (event-populated via onPlotChanged)
	ContextDict religions;        // RELIGION id    -> INFLUENCE (presence(1) today; active at/above its threshold when a consumer needs the magnitude). HAS_RELIGION; STATE_RELIGION_IN_CITY = religions.has(empireCtx.stateReligion())
	ContextDict holyCity;         // RELIGION id    -> this city is its HOLY CITY (IS_HOLY_CITY = !holyCity.empty(); {IS_HOLY_CITY: R} = holyCity.has(R))
	ContextDict vicinityBonuses;  // BONUS id       -> supplied in the city's VICINITY (map bonuses + active buildings' provides); connection:"vicinity" -- NOT traded (that is CvPlotGroup)
	ContextDict corporations;     // CORPORATION id -> present in the city (HAS_CORPORATION; active/dormancy gating is a refinement)

	// EVENT population of plotAttrs -- a plot ENTERED (sign +1) / LEFT (sign -1) the city's owned worked-radius set:
	// fold its stable HAS_/IS_ attributes (+/-1). COUNTS only, the plot is never stored. Vicinity is NOT folded here --
	// a vicinity event is its own thing, disconnected from plot state (rides vicinityBonuses via its own event).
	void onPlotChanged(const CvPlot* plot, int sign);

	void clear()
	{
		plotAttrs.clear(); religions.clear(); holyCity.clear(); vicinityBonuses.clear(); corporations.clear();
		m_population = 0; m_power = 0;
	}

private:
	int m_population;   // always-present city scalar (POPULATION atoms / per:{POPULATION})
	int m_power;        // always-present city scalar, INT for future volumetric power (HAS_POWER = power() > 0)
};

#endif // CV_CITY_CONTEXT_H
