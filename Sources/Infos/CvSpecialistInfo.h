#pragma once
#ifndef CV_JSON_SPECIALIST_INFO_H
#define CV_JSON_SPECIALIST_INFO_H

//
//	CvSpecialistInfo -- the JSON real poco for SPECIALISTS. Live-caller surface; the specialist's own output is
//	CITY-scope (production.city.flat, greatPeopleRate.city.flat, …) -- note the scope differs from plot-substrate types.
//	No hotkey base (no getSpecialistInfo(...) hotkey caller). HUMAN-native values. No cascade here.
//
//	Live callers (verified 2026-07-07): getYieldChange/getCommerceChange -> city yields; getGreatPeopleRateChange/
//	getGreatPeopleUnitType -> GP; getHealthPercent/getHappinessPercent -> CvCity wellbeing; getExperience/
//	getInsidiousness/getInvestigation -> unit/crime; isSlave/isVisible -> assignment.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / NUM_COMMERCE_TYPES / TechTypes / UnitCombatTypes / NO_UNITCOMBAT / NO_MISSION
#include "Defines/CvStructs.h" // UnitCombatModifier
#include <map>
#include <string>
#include <vector>

class CvSpecialistInfo : public CvInfo
{
public:
	CvSpecialistInfo();

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

	int getFlavorValue(int i) const { return mapGet(m_flavours, i); }               // ai.flavours {FLAVOR:int}

	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }  // fed from the PROPERTY_* families in mapFrom (city gather, per assigned specialist)

	const char* getTexture() const { return m_szTexture.c_str(); }   // ui.art.texture (the specialist's city-screen glyph)

	int getNumUnitCombatExperienceTypes() const { return (int)m_aUnitCombatExperienceTypes.size(); }
	const UnitCombatModifier& getUnitCombatExperienceType(int iIndex) const;         // experience.city.unitCombats.{UNITCOMBAT}.flat

	// tech-gated KEEP-ON-SELF wellbeing: happiness/health.city.flat conditioned entries {value, enabled:{TECH, scope:team}}
	// (curate_specialist.py:34-36/210-216). ×1 RAW -- CvCity::getExtraTechSpecialist{Happiness,Health} reads un-scaled.
	int getTechHappiness(TechTypes eTech) const { return mapGet(m_techHappiness, eTech); }
	int getTechHealth(TechTypes eTech) const    { return mapGet(m_techHealth, eTech); }

	// RUNTIME (set post-load, NOT JSON; mirrors CvCorporationInfo/CvBuildInfo's runtime mission type)
	int getMissionType() const { return m_iMissionType; }
	void setMissionType(int iNewType) { m_iMissionType = iNewType; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	static int mapGet(const std::map<int, int>& m, int k) { std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }

	CvModifiers m_modifiers;
	int m_aiYieldChange[NUM_YIELD_TYPES];       // food/production/commerce .city.flat
	int m_aiCommerceChange[NUM_COMMERCE_TYPES]; // gold/research/culture/espionage .city.flat
	int m_iGreatPeopleRateChange;               // greatPeopleRate.city.flat
	int m_iGreatPeopleUnitType;                 // identity.greatPeopleUnit (FK; the GP unit this specialist produces)
	int m_iExperience;                          // experience.city.flat (free unit XP; module-rare, absent in current data)
	int m_iHealthPercent;                       // health.city.flat base (÷100 human -> ×100 re-applied; latent /100 consumer, §4c)
	int m_iHappinessPercent;                    // happiness.city.flat base (÷100 human -> ×100 re-applied)
	int m_iInsidiousness, m_iInvestigation;     // identity (TB tags)
	bool m_bSlave, m_bVisible;                  // identity flags
	std::string m_szTexture;                    // ui.art.texture
	std::map<int, int> m_flavours;              // FlavorTypes -> weight (ai.flavours)
	std::map<int, int> m_techHappiness;         // TechTypes -> ×1 keep-on-self happiness (happiness.city.flat conditioned entries)
	std::map<int, int> m_techHealth;            // TechTypes -> ×1 keep-on-self health   (health.city.flat conditioned entries)
	// PROPERTY_* families ARE curator-emitted (PROPERTY_CRIME/DISEASE/...), but the JSON->CvPropertySource reader is
	// unbuilt -- a cross-type deferral (CvCorporationInfo/CvHeritageInfo identical); returns a real empty object.
	CvPropertyManipulators m_PropertyManipulators;
	std::vector<UnitCombatModifier> m_aUnitCombatExperienceTypes;   // experience.city.unitCombats.{UNITCOMBAT}.flat
	int m_iMissionType;                         // RUNTIME-assigned via setMissionType (default NO_MISSION), never serialized/JSON
};

#endif // CV_JSON_SPECIALIST_INFO_H
