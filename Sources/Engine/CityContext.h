#pragma once
#ifndef CV_CITY_CONTEXT_H
#define CV_CITY_CONTEXT_H

//
//	CityContext -- the per-city READ SURFACE the (cityContext, plotGroup) building-output getters + the one condition evaluator use.
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
//	⚖ THE HAVE AXIS LIVES HERE (contexts.md): every evaluator atom / enabler gate read of a CITY-scope fact goes
//	through these forwards -- the context is the ONE responsibility home for the city's changeable state; the
//	evaluator never reaches into CvCity ad hoc.
//

#include "ContextDict.h"

class CvPlot;
class CvCity;
struct CvCascadeEvalCtx;

class CityContext
{
public:
	CityContext() : m_city(NULL) {}
	void bind(const CvCity* c) { m_city = c; }   // set once by the owning CvCity; the pointer IS the owner (never dangles)

	// --- STORED: the uniquely-owned aggregate -- the HAS_/IS_ plot-predicate COUNTS, event-maintained (onPlotChanged) ---
	ContextDict plotAttrs;
	// A plot ENTERED (sign +1) / LEFT (sign -1) the city's owned worked-radius set: fold its stable HAS_/IS_ attributes
	// (+/-1) into plotAttrs. COUNTS only; the plot is never stored. Fired from the CvPlot::updateWorkingCity choke
	// point at play; the load reseed folds the same fact from the in-read SEVT_WORKING_CITY_CHANGED DOMAIN events
	// (Engine/ContextConsumer -- DEC-spine-reseed).
	void onPlotChanged(const CvPlot* plot, int sign);
	void clear() { plotAttrs.clear(); }   // m_city is a binding, not cleared

	// --- FORWARDED: read through the bound CvCity / its owner -- no stored copy. Defined out-of-line (CityContext.cpp) ---
	int  population() const;                  // CvCity::getPopulation
	int  power() const;                       // CvCity::getPowerCount (the volumetric count; the engine on/off verdict is isPowered)
	bool isPowered() const;                   // CvCity::isPower -- the HAS_POWER verdict (count OR area clean power, dirty-timer gated)
	bool hasReligion(int eReligion) const;    // CvCity::isHasReligion
	bool isHolyCityOf(int eReligion) const;   // CvCity::isHolyCity(eReligion)   ({IS_HOLY_CITY: R})
	bool isHolyCityAny() const;               // CvCity::isHolyCity()            (bare IS_HOLY_CITY)
	bool hasCorporation(int eCorp) const;     // CvCity::isHasCorporation (presence; spread state)
	bool hasActiveCorporation(int eCorp) const;   // CvCity::isActiveCorporation ({HAS_CORPORATION: X} = ACTIVE, json §3.5)
	bool isHeadquartersOf(int eCorp) const;   // CvCity::isHeadquarters(eCorp)   ({IS_HEADQUARTERS: X})
	bool isHeadquartersAny() const;           // CvCity::isHeadquarters()        (bare IS_HEADQUARTERS)
	bool hasVicinityBonus(int eBonus) const;  // CvCity::hasVicinityBonus (connection:"vicinity"; traded stays on CvPlotGroup)
	// The city's TRADED bonus count -- CvCity::getNumBonuses, the plot-group-backed MAINTAINED number (tech-gate +
	// minted + corp add-on applied; enabler.md the residency/counting rule). A forward of the city's own
	// plot-group relay -- traded state is NEVER mirrored here (contexts.md).
	int  tradedBonusCount(int eBonus) const;
	bool isCapital() const;                   // CvCity::isCapital (IS_CAPITAL)
	bool isGovernmentCenter() const;          // CvCity::isGovernmentCenter (owner-sanctioned engine counter, IS_GOVERNMENT_CENTER)
	bool hasFreshWaterAccess() const;         // CvCity::hasFreshWater -- the provider-building-fed ACCESS counter (HAS_FRESHWATER city leg)
	bool isCoastal(int iMinWaterSize) const;  // CvCity::isCoastal (the HAS_COAST minArea city form)
	int  propertyValue(int eProperty) const;  // CvProperties::getValueByProperty (the PROPERTY_ band read)
	int  areaSize() const;                    // CvCity::area()->getNumTiles (the AREA_SIZE counter)
	int  ownCulturePercent() const;           // plot()->calculateCulturePercent(owner) (the CULTURE_PERCENTAGE counter)
	int  owner() const;                       // CvCity::getOwner (the vicinity scans' owned-plot test)
	int  team() const;                        // CvCity::getTeam (the plot-bonus reveal axis)
	const CvPlot* cityPlot() const;           // CvCity::plot -- the centre tile
	const CvPlot* radiusPlot(int iRingIndex) const;   // CvCity::getCityIndexPlot -- the workable-radius scan source
	bool hasBuilding(int eBuilding) const;    // CvCity::hasBuilding -- the §7 raw-presence has-list (the gate's BUILDING_ atom)
	int  stateReligion() const;               // owner CvPlayer::getStateReligion  (STATE_RELIGION_IN_CITY = hasReligion(stateReligion()))
	bool hasPolicy(int ePolicy) const;        // owner EmpireContext::policies.has  (empire aggregate, not mirrored here)

	// Fill the CITY half of a condition-eval context (ec.city + ec.plot) from the bound city -- the context IS the
	// eval state (the evaluator reads through the ctx it fills). Paired with EmpireContext::fillEvalCtx (player/team).
	void fillEvalCtx(CvCascadeEvalCtx& ec) const;

private:
	const CvCity* m_city;   // the bound game object; forwarding accessors read it -- never a value copy
};

#endif // CV_CITY_CONTEXT_H
