#pragma once
#ifndef CV_CITY_CONTEXT_H
#define CV_CITY_CONTEXT_H

//
//	CityContext -- the per-city READ SURFACE the (cx, pg) building-output getters + the one condition evaluator use.
//	Bound to its CvCity by pointer (never a value copy -- passing a bound reference is far cheaper than snapshotting
//	values, owner). The CITY-scope half of the symmetric per-scope contexts (EmpireContext = empire scope), so a
//	reader always knows where to go: city state here, empire state on the owner's EmpireContext.
//
//	⛔ It STORES only the uniquely-owned AGGREGATE -- `plotAttrs`, the per-predicate plot COUNTS (how many river /
//	water / ... plots), which no CvCity accessor provides and which would otherwise be recomputed by every reader.
//	Everything already O(1) on the game object is FORWARDED, never duplicated: population / power / religion /
//	holy-city / corporation / vicinity read through the bound CvCity; state religion + policies through its owner.
//	COUNTS not objects; a keyed/plots-target deposit's output = flat x count, a gate = has.
//

#include "ContextDict.h"

class CvPlot;
class CvCity;

class CityContext
{
public:
	CityContext() : m_city(NULL) {}
	void bind(const CvCity* c) { m_city = c; }   // set once by the owning CvCity; the pointer IS the owner (never dangles)

	// --- STORED: the uniquely-owned aggregate -- the HAS_/IS_ plot-predicate COUNTS, event-maintained (onPlotChanged) ---
	ContextDict plotAttrs;
	// A plot ENTERED (sign +1) / LEFT (sign -1) the city's owned worked-radius set: fold its stable HAS_/IS_ attributes
	// (+/-1) into plotAttrs. COUNTS only; the plot is never stored.
	void onPlotChanged(const CvPlot* plot, int sign);
	void clear() { plotAttrs.clear(); }   // m_city is a binding, not cleared

	// --- FORWARDED: read through the bound CvCity / its owner -- no stored copy. Defined out-of-line (CityContext.cpp) ---
	int  population() const;                  // CvCity::getPopulation
	int  power() const;                       // CvCity::getPowerCount (HAS_POWER = power() > 0)
	bool hasReligion(int eReligion) const;    // CvCity::isHasReligion
	bool isHolyCityOf(int eReligion) const;   // CvCity::isHolyCity(eReligion)   ({IS_HOLY_CITY: R})
	bool isHolyCityAny() const;               // CvCity::isHolyCity()            (bare IS_HOLY_CITY)
	bool hasCorporation(int eCorp) const;     // CvCity::isHasCorporation
	bool hasVicinityBonus(int eBonus) const;  // CvCity::hasVicinityBonus (connection:"vicinity"; traded stays on CvPlotGroup)
	int  stateReligion() const;               // owner CvPlayer::getStateReligion  (STATE_RELIGION_IN_CITY = hasReligion(stateReligion()))
	bool hasPolicy(int ePolicy) const;        // owner EmpireContext::policies.has  (empire aggregate, not mirrored here)

private:
	const CvCity* m_city;   // the bound game object; forwarding accessors read it -- never a value copy
};

#endif // CV_CITY_CONTEXT_H
