#pragma once
#ifndef CV_JSON_CULTURE_LEVEL_INFO_H
#define CV_JSON_CULTURE_LEVEL_INFO_H

//
//	CvCultureLevelInfo -- the CULTURE-LEVEL poco rebuilt to the exemplar surface (patterns.md § THE GETTER
//	SETUP: the four read categories, nothing else). A city's culture tier: the tier's own city-scope defense
//	(the one census modifier family, read as a compiled point fetch -- docs/architecture/patterns.md §The coherent surface (scope is a separate axis)), the per-city
//	wonder-category caps (json.md §4.4, riding the composed `allowed`), the culture threshold + radius
//	intrinsics, and the entity-level game-option gate (the composed CvGate). The alternate-Info swap
//	(replacedBy) + the tier's enables ride the composed edges. No legacy getter name returns
//	(docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)).
//

#include "CvInfo.h"

class CvCultureLevelInfo : public CvInfo
{
public:
	CvCultureLevelInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvAllowed*   getAllowed()   const { return &m_allowed; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvGate*      getGate()      const { return &m_gate; }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. The census
	// participation is ONE family: defense.city.amount.percent -- unconditioned, so the point read serves.)
	int getDefense(DefenseKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DEFENSE, eKind, eScope, infoDefenseUnit(eKind)); }

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getCityRadius() const { return m_iCityRadius; }              // identity.cityRadius (override, not additive)
	// The BASE culture-point threshold (identity.cultureThreshold). The legacy per-GameSpeed threshold TABLE
	// was a REDUNDANT precompute of base × GameSpeed.speedPercent/100 (curator COLLAPSE, verified identical) --
	// "the multiplier lives on the gamespeed" (owner): the per-speed value is derived at read (see the .cpp).
	int getCultureThreshold() const { return m_iCultureThreshold; }
	int getSpeedThreshold(int iSpeed) const;                         // base × GameSpeed.speedPercent / 100
	// The per-city wonder-category caps (json §4.4), materialized from the composed `allowed` at mapFrom with
	// the legacy 0-for-absent convention (the base allowedCap convention is -1 = absent).
	int getMaxWorldWonders() const    { return m_iMaxWorldWonders; }
	int getMaxTeamWonders() const     { return m_iMaxTeamWonders; }
	int getMaxNationalWonders() const { return m_iMaxNationalWonders; }
	// The entity-level `enabled` GAMEOPTION gate, extracted for the one consumer that asks for the single
	// option id (CvGlobals::cacheGameSpecificValues); the gate itself is the composed CvGate. Materialized at
	// mapFrom (docs/architecture/patterns.md §Materialize at mapFrom) -- the getter is a bare read (see the .cpp).
	int getPrereqGameOption() const { return m_iPrereqGameOption; }

	// --- RUNTIME member (set at load by the tier-ordering pass, NOT JSON) ---
	int getLevel() const { return m_iLevel; }
	void setLevel(int iLevel) { m_iLevel = iLevel; }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvAllowed*   mutAllowed()   { return &m_allowed; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvGate*      mutGate()      { return &m_gate; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvAllowed   m_allowed;
	CvModifiers m_modifiers;
	CvGate      m_gate;

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	int m_iCityRadius;
	int m_iCultureThreshold;
	int m_iMaxWorldWonders;
	int m_iMaxTeamWonders;
	int m_iMaxNationalWonders;
	int m_iPrereqGameOption;
	int m_iLevel;   // runtime tier ordinal
};

#endif // CV_JSON_CULTURE_LEVEL_INFO_H
