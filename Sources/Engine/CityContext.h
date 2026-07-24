#pragma once
#ifndef CV_CITY_CONTEXT_H
#define CV_CITY_CONTEXT_H

//
//	CityContext -- the per-city ISOLATED live state a building's output getters (and the one condition evaluator)
//	read to compute the ACTUAL benefit in this city. Owned by CvCity, kept current by events.
//
//	⛔ It holds COUNTS, never the objects themselves (owner): the building cares HOW MANY river plots / religions /
//	vicinity bonuses it has, never WHICH. A plots-target (or per-keyed) deposit's output is `flat x count(id)`; a
//	gate is just `has(id)` (count > 0). Self-contained -- a raw pointer is passed directly into an info getter, no
//	copy. Passed ALONGSIDE the city's CvPlotGroup (which owns the TRADED / trade-network bonuses): the clean split is
//	CityContext = VICINITY + local, CvPlotGroup = TRADED. CityContext NEVER re-stores traded state.
//
//	Everything keyed is the SAME kind of dictionary (`Dict`, an id->count map), so the read is uniform and each
//	family's key set is OPEN. CITY-scope instance of the per-scope contexts (PlayerContext / PlotContext / TeamContext
//	are the siblings). CvCity ownership + the per-family event wiring land as it is wired up.
//

#include <map>

class CvPlot;

class CityContext
{
public:
	// The uniform keyed dictionary every city fact family uses: id -> count (id = a CASC_PRED_* plot predicate, or a
	// RELIGION / BONUS / CORPORATION type id). `has` is the plain gate; `count` is the scale (plots-target output =
	// flat x count). A dictionary, not a fixed struct, so each family's key set is OPEN -- a new key, never a reshape.
	struct Dict
	{
		std::map<int, int> m;
		int  count(int id) const { std::map<int, int>::const_iterator it = m.find(id); return it != m.end() ? it->second : 0; }
		bool has(int id) const   { return count(id) > 0; }
		void add(int id, int d)  { m[id] += d; }
		void set(int id, int n)  { m[id] = n; }
		void clear()             { m.clear(); }
		bool empty() const       { return m.empty(); }
	};

	CityContext() : m_population(0), m_power(0), m_stateReligion(-1) {}

	// --- always-present city SCALARS -- ints / a single enum, never a dictionary ---
	int  population() const { return m_population; }        void setPopulation(int n) { m_population = n; }
	int  power() const      { return m_power; }             void setPower(int n)      { m_power = n; }        // HAS_POWER = power() > 0
	// The empire STATE RELIGION is a SINGLE enum (a RELIGION id; -1 = NO_RELIGION), not a dictionary (owner) -- there
	// is exactly one. {STATE_RELIGION: R} = stateReligion() == R; STATE_RELIGION_IN_CITY = religions.has(stateReligion()).
	int  stateReligion() const { return m_stateReligion; }  void setStateReligion(int r) { m_stateReligion = r; }

	// --- the keyed DICTIONARIES (same Dict kind) -- the info reads cx.<dict>.has(id) / .count(id) directly ---
	Dict plotAttrs;        // CASC_PRED_* HAS_/IS_ plot predicate -> plot count (event-populated via onPlotChanged)
	Dict religions;        // RELIGION id    -> INFLUENCE (int): store the value, not just presence -- a religion is ACTIVE at/above its
	                       //                 known threshold (no consumer needs the magnitude yet, but it costs nothing). HAS_RELIGION =
	                       //                 has(R) (presence proxy today); STATE_RELIGION_IN_CITY = religions.has(stateReligion())
	Dict holyCity;         // RELIGION id    -> this city is its HOLY CITY (IS_HOLY_CITY = !holyCity.empty(); {IS_HOLY_CITY: R} = holyCity.has(R))
	Dict vicinityBonuses;  // BONUS id       -> supplied in the city's VICINITY (map bonuses + active buildings' provides); connection:"vicinity" -- NOT traded (that is CvPlotGroup)
	Dict corporations;     // CORPORATION id -> ACTIVE in the city (HAS_CORPORATION)
	Dict policies;         // POLICY id      -> the empire ENACTS this policy (json §9) -- the empire-state dictionary the city eval reads

	// EVENT population of plotAttrs -- a plot ENTERED (sign +1) / LEFT (sign -1) the city: fold its stable HAS_/IS_
	// attributes (+/-1). COUNTS only, the plot is never stored. (religions / holyCity / vicinityBonuses / corporations
	// are maintained by their own events -- religion & corp spread, holy-city set, vicinity supply change -- wired at
	// CvCity setup; the Dict add/set surface is what those fire into.)
	void onPlotChanged(const CvPlot* plot, int sign);

	void clear()
	{
		plotAttrs.clear(); religions.clear(); holyCity.clear(); vicinityBonuses.clear(); corporations.clear(); policies.clear();
		m_population = 0; m_power = 0; m_stateReligion = -1;
	}

private:
	int m_population;     // always-present city scalar (POPULATION atoms / per:{POPULATION})
	int m_power;          // always-present city scalar, INT for future volumetric power (HAS_POWER = power() > 0)
	int m_stateReligion;  // the empire's state religion -- a SINGLE RELIGION enum id (-1 = NO_RELIGION), not a dictionary
};

#endif // CV_CITY_CONTEXT_H
