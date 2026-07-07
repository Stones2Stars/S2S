#pragma once
#ifndef CV_JSON_CORPORATION_INFO_H
#define CV_JSON_CORPORATION_INFO_H

//
//	CvJsonCorporationInfo -- the per-type cascade info for CORPORATIONS. Every corp modifier family is authored as a
//	HAS_CORPORATION-gated, per:{anyOf:prereqBonuses}-scaled entry (json.md §3.9). Because the gate is UNIFORM (always
//	the corp itself) and the per-list is one shared set, the families collapse to plain typed arrays here, split by
//	`per`-presence into the legacy CHANGE (no per, ×1) vs PRODUCED (per-bonus base) getters -- the surviving callers
//	apply × getNumBonuses(prereqBonuses) themselves. The tech/building/bonus prereqs + the provided bonus ride the
//	base (tech.enables.corporations / provides.bonuses). No cascade here.
//
//	⚠ CORP SCALE MESS (curator, scales §4c -- corp-pass TODO): the PRODUCED yield/commerce values are ÷100-descaled
//	  humans (e.g. 0.75) so the getter re-applies ×100; but `maintenance` is NOT descaled by the curator (raw ×100 in
//	  JSON, e.g. 100) so it is summed as-is. HeadquarterCommerce is DEFERRED to the HQ-revenue pass. The corporation
//	  SYSTEM is slated for a principle-level rework post-migration -- these are faithful-mirror interim shapes.
//
//	Live callers (verified 2026-07-07): getYieldProduced/getCommerceProduced/getMaintenance -> CvCity; getHealth/
//	getHappiness/getFreeXP/getMilitaryProductionModifier -> CvCity; getSpreadCost/getSpread/getSpreadFactor -> spread;
//	getHeadquarterCommerce -> HQ commerce; getPrereqBonus -> the per-scaler bonus set.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / NUM_COMMERCE_TYPES / BonusTypes
#include <vector>

class CvJsonCorporationInfo : public CvJsonInfo
{
public:
	CvJsonCorporationInfo();

	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }          // {y}.city.flat, no per (×1)
	int getYieldProduced(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldProduced[i] : 0; }      // {y}.city.flat with per (×100 re-applied)
	int getCommerceChange(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceChange[i] : 0; }
	int getCommerceProduced(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceProduced[i] : 0; }
	int getHeadquarterCommerce(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiHeadquarterCommerce[i] : 0; }  // {c}.empire.headquarters.perCorporationLevel (⏳ HQ pass)
	const int* getHeadquarterCommerceArray() const { return m_aiHeadquarterCommerce; }

	int getMaintenance() const { return m_iMaintenance; }   // maintenance.city.corporation.flat (⚠ raw ×100, curator not descaled)
	int getHealth() const { return m_iHealth; }             // health.city.flat
	int getHappiness() const { return m_iHappiness; }       // happiness.city.flat
	int getFreeXP() const { return m_iFreeXP; }             // experience.city.flat
	int getMilitaryProductionModifier() const { return m_iMilitaryProductionModifier; }   // buildRate.city.military.percent

	int getPrereqBonus(int i) const { return (i >= 0 && i < (int)m_aePrereqBonuses.size()) ? m_aePrereqBonuses[i] : -1; }  // per.anyOf bonus set
	int getNumPrereqBonuses() const { return (int)m_aePrereqBonuses.size(); }
	int getBonusProduced() const;   // the composed provides.bonuses unit (§5a) -- thin accessor

	int getSpreadCost() const { return m_iSpreadCost; }                       // cost.spread
	int getSpread() const { return m_iSpread; }                              // identity.spreadFactor
	int getSpreadFactor() const { return m_iCompetingSpreadCostPercent; }    // identity.competingSpreadCostPercent (legacy misnomer)
	const char* getSound() const { return m_szSound.c_str(); }               // sound.sound

	// RUNTIME (set post-load, NOT JSON)
	int getHeadquarterChar() const { return m_iHeadquarterChar; }  void setHeadquarterChar(int i) { m_iHeadquarterChar = i; }
	int getMissionType() const { return m_iMissionType; }          void setMissionType(int i) { m_iMissionType = i; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonProvides*  getProvides()  const { return &m_provides; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonProvides*  mutProvides()  { return &m_provides; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonEdges     m_edges;
	CvJsonProvides  m_provides;
	CvJsonModifiers m_modifiers;
	int m_aiYieldChange[NUM_YIELD_TYPES];
	int m_aiYieldProduced[NUM_YIELD_TYPES];
	int m_aiCommerceChange[NUM_COMMERCE_TYPES];
	int m_aiCommerceProduced[NUM_COMMERCE_TYPES];
	int m_aiHeadquarterCommerce[NUM_COMMERCE_TYPES];
	int m_iMaintenance, m_iHealth, m_iHappiness, m_iFreeXP, m_iMilitaryProductionModifier;
	std::vector<int> m_aePrereqBonuses;
	int m_iSpreadCost, m_iSpread, m_iCompetingSpreadCostPercent;
	std::string m_szSound;
	int m_iHeadquarterChar, m_iMissionType;   // runtime
};

#endif // CV_JSON_CORPORATION_INFO_H
