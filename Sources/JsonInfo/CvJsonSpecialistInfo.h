#pragma once
#ifndef CV_JSON_SPECIALIST_INFO_H
#define CV_JSON_SPECIALIST_INFO_H

//
//	CvJsonSpecialistInfo -- the JSON real poco for SPECIALISTS. Live-caller surface; the specialist's own output is
//	CITY-scope (production.city.flat, greatPeopleRate.city.flat, …) -- note the scope differs from plot-substrate types.
//	No hotkey base (no getSpecialistInfo(...) hotkey caller). HUMAN-native values. No cascade here.
//
//	Live callers (verified 2026-07-07): getYieldChange/getCommerceChange -> city yields; getGreatPeopleRateChange/
//	getGreatPeopleUnitType -> GP; getHealthPercent/getHappinessPercent -> CvCity wellbeing; getExperience/
//	getInsidiousness/getInvestigation -> unit/crime; isSlave/isVisible -> assignment.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / NUM_COMMERCE_TYPES

class CvJsonSpecialistInfo : public CvJsonInfo
{
public:
	CvJsonSpecialistInfo();

	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
	int getCommerceChange(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceChange[i] : 0; }
	int getGreatPeopleRateChange() const { return m_iGreatPeopleRateChange; }
	int getGreatPeopleUnitType() const { return m_iGreatPeopleUnitType; }
	int getExperience() const { return m_iExperience; }
	int getHealthPercent() const { return m_iHealthPercent; }
	int getHappinessPercent() const { return m_iHappinessPercent; }
	int getInsidiousness() const { return m_iInsidiousness; }
	int getInvestigation() const { return m_iInvestigation; }
	bool isSlave() const { return m_bSlave; }
	bool isVisible() const { return m_bVisible; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonModifiers m_modifiers;
	int m_aiYieldChange[NUM_YIELD_TYPES];       // food/production/commerce .city.flat
	int m_aiCommerceChange[NUM_COMMERCE_TYPES]; // gold/research/culture/espionage .city.flat
	int m_iGreatPeopleRateChange;               // greatPeopleRate.city.flat
	int m_iGreatPeopleUnitType;                 // ⏳ the GP unit produced (source to confirm)
	int m_iExperience;                          // ⏳ experience (source to confirm)
	int m_iHealthPercent;                       // ⏳ health (latent /100, fixed-point-and-scales §4c)
	int m_iHappinessPercent;                    // ⏳ happiness
	int m_iInsidiousness, m_iInvestigation;     // identity (TB tags)
	bool m_bSlave, m_bVisible;                  // identity flags
	// ⏳ NOT yet mapped (keyed): getTechHealth / getTechHappiness / getUnitCombatExperienceType.
};

#endif // CV_JSON_SPECIALIST_INFO_H
