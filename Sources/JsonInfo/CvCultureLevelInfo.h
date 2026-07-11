#pragma once
#ifndef CV_JSON_CULTURE_LEVEL_INFO_H
#define CV_JSON_CULTURE_LEVEL_INFO_H

//
//	CvCultureLevelInfo -- the JSON real poco for CULTURE LEVELS (a city's culture tier). Carries the tier's own
//	city-scope values + the per-city wonder-category caps (which ride the composed `allowed` unit, json.md §4.4). The
//	prereq game option is the composed entity-level `enabled`/`disabled` gate (CvJsonGate); the alternate-Info swap
//	rides the composed `edges`. HUMAN-native. No cascade here.
//
//	Live callers (verified 2026-07-07): getCityDefenseModifier -> CvCity defense; getCityRadius -> CvCity workable
//	radius; getMax{World,Team,National}Wonders -> CvCity wonder gating + pedia; getSpeedThreshold -> CvGame culture
//	threshold; getLevel -> the runtime tier ordinal.
//

#include "CvInfo.h"

class CvCultureLevelInfo : public CvInfo
{
public:
	CvCultureLevelInfo() : m_iCityDefenseModifier(0), m_iCityRadius(0), m_iCultureThreshold(0), m_iLevel(0) {}

	int getCityDefenseModifier() const { return m_iCityDefenseModifier; }   // defense.city.amount.percent (human %)
	int getCityRadius() const { return m_iCityRadius; }                     // identity.cityRadius (override, not additive)

	// per-city wonder-category caps -- thin accessors over the composed `allowed` unit (json.md §4.4).
	int getMaxWorldWonders() const    { return wonderCap("worldWonders"); }
	int getMaxTeamWonders() const     { return wonderCap("teamWonders"); }
	int getMaxNationalWonders() const { return wonderCap("nationalWonders"); }
	// NB getMaxNationalWondersOCC DROPPED (One-City-Challenge not feasible in this mod, owner 2026-07-01; curator drops it).

	// The legacy per-GameSpeed threshold TABLE was a REDUNDANT precompute of base(Normal) × GameSpeed.speedPercent/100
	// (curator COLLAPSE, verified identical): the poco carries only the base culture-point threshold, and the per-speed
	// value is derived here by re-applying the gamespeed's speedPercent -- "the multiplier lives on the gamespeed"
	// (owner 2026-07-11). See the .cpp (needs GC.getGameSpeedInfo).
	int getSpeedThreshold(int iSpeed) const;   // base identity.cultureThreshold × GameSpeed.speedPercent / 100

	int getLevel() const { return m_iLevel; }        // RUNTIME ordinal -- set at load, NOT JSON-mapped
	void setLevel(int i) { m_iLevel = i; }

	int getPrereqGameOption() const;                            // the entity-level `enabled` GAMEOPTION gate (DEC-entity-gate); see .cpp
	int getMaxNationalWondersOCC() const { return 0; }          // STUB One-City-Challenge cap (curator drops it; OCC infeasible)

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonAllowed*   getAllowed()   const { return &m_allowed; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvJsonGate*      getGate()      const { return &m_gate; }

protected:
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonAllowed*   mutAllowed()   { return &m_allowed; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvJsonGate*      mutGate()      { return &m_gate; }

private:
	// wonder-cap read with the legacy 0-for-absent convention (the base allowedCap returns -1 = uncapped/absent).
	int wonderCap(const char* key) const
	{ std::map<std::string, int>::const_iterator it = m_allowed.all().find(key); return it != m_allowed.all().end() ? it->second : 0; }

	int m_iCityDefenseModifier;   // defense.city.amount.percent
	int m_iCityRadius;            // identity.cityRadius
	int m_iCultureThreshold;      // identity.cultureThreshold (raw culture points)
	int m_iLevel;                 // runtime tier ordinal (not JSON)
	CvJsonEdges     m_edges;
	CvJsonAllowed   m_allowed;
	CvJsonModifiers m_modifiers;
	CvJsonGate      m_gate;
};

#endif // CV_JSON_CULTURE_LEVEL_INFO_H
