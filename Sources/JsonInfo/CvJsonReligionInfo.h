#pragma once
#ifndef CV_JSON_RELIGION_INFO_H
#define CV_JSON_RELIGION_INFO_H

//
//	CvJsonReligionInfo -- the per-type cascade info for RELIGIONS. Live values: the STATE-religion + HOLY-CITY per-
//	commerce bonuses (the SAME commerce families, split by the enabled predicate STATE_RELIGION vs IS_HOLY_CITY -- so
//	they collapse to two plain int arrays here), the shrine commerce (× the world religion-levels count, read by the
//	cascade), the spread factor, and art/sound/adjective. The tech prereq + the buildings/units it enables ride the
//	base (tech.enables.religions / this religion's enables.*); the founder free-unit rides base `grants`. HUMAN-native
//	(no ×100). No cascade here.
//
//	Live callers (verified 2026-07-07): getStateReligionCommerce/getHolyCityCommerce -> CvCity commerce; shrineCommerce
//	-> the cascade shrine calc; getSpreadFactor -> spread; getAdjectiveKey/getSound/getTechButton -> UI; the runtime
//	char/mission/shrineBuildings members -> engine display + the shrine-building registry.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // NUM_COMMERCE_TYPES / COMMERCE_*
#include <vector>

class CvJsonReligionInfo : public CvJsonInfo
{
public:
	CvJsonReligionInfo();

	std::map<std::string, int> shrineCommerce;   // shrine {channel:value} -- the cascade multiplies by world religion-levels

	int getStateReligionCommerce(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiStateReligionCommerce[i] : 0; }
	const int* getStateReligionCommerceArray() const { return m_aiStateReligionCommerce; }
	int getHolyCityCommerce(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiHolyCityCommerce[i] : 0; }
	const int* getHolyCityCommerceArray() const { return m_aiHolyCityCommerce; }
	int getGlobalReligionCommerce(int i) const;   // int-index accessor over shrineCommerce (by channel name)

	int getSpreadFactor() const { return m_iSpreadFactor; }               // identity.spreadFactor
	const CvWString& getAdjectiveKey() const { return m_szAdjectiveKey; } // identity.adjective (CvInfoBase has no adjective)
	int getFlavorValue(int i) const;                                      // ai.flavours [{FLAVOR:n}]
	const char* getSound() const { return m_szSound.c_str(); }            // sound.sound
	const char* getTechButton() const { return m_szTechButton.c_str(); }               // ui.art.techButton
	const char* getGenericTechButton() const { return m_szGenericTechButton.c_str(); } // ui.art.genericTechButton

	// grant thin accessors (grants.freeUnit / grants.numFreeUnits ride the composed CvJsonGrants unit)
	int getFreeUnit() const;
	int getNumFreeUnits() const;

	// RUNTIME members (set post-load, NOT JSON): the pedia char glyphs, the mission id, the shrine-building registry.
	int getMissionType() const { return m_iMissionType; }   void setMissionType(int i) { m_iMissionType = i; }
	int getChar() const { return m_iChar; }                 void setChar(int i) { m_iChar = i; }
	int getHolyCityChar() const { return m_iHolyCityChar; } void setHolyCityChar(int i) { m_iHolyCityChar = i; }
	const std::vector<int>& getShrineBuildings() const { return m_aeShrineBuildings; }
	void addShrineBuilding(int iBuilding) { m_aeShrineBuildings.push_back(iBuilding); }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonEdges     m_edges;
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	int m_aiStateReligionCommerce[NUM_COMMERCE_TYPES];   // {c}.city.flat entries enabled:{STATE_RELIGION:self}
	int m_aiHolyCityCommerce[NUM_COMMERCE_TYPES];        // {c}.city.flat entries enabled:{IS_HOLY_CITY:self}
	int m_iSpreadFactor;
	CvWString m_szAdjectiveKey;
	std::map<int, int> m_flavours;
	std::string m_szSound, m_szTechButton, m_szGenericTechButton;
	int m_iMissionType, m_iChar, m_iHolyCityChar;   // runtime (not JSON)
	std::vector<int> m_aeShrineBuildings;           // runtime (buildings self-register at load)
};

#endif // CV_JSON_RELIGION_INFO_H
