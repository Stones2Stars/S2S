#pragma once
#ifndef CV_JSON_RELIGION_INFO_H
#define CV_JSON_RELIGION_INFO_H

//
//	CvReligionInfo -- the per-type cascade info for RELIGIONS. Live values: the STATE-religion + HOLY-CITY per-
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

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // NUM_COMMERCE_TYPES / COMMERCE_*
#include <vector>

class CvReligionInfo : public CvInfo
{
public:
	CvReligionInfo();

	std::map<std::string, int> shrineCommerce;   // shrine {channel:value} -- the cascade multiplies by world religion-levels

	int getStateReligionCommerce(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiStateReligionCommerce[i] : 0; }
	const int* getStateReligionCommerceArray() const { return m_aiStateReligionCommerce; }
	int getHolyCityCommerce(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiHolyCityCommerce[i] : 0; }
	const int* getHolyCityCommerceArray() const { return m_aiHolyCityCommerce; }
	int getGlobalReligionCommerce(int i) const;   // int-index accessor over shrineCommerce (by channel name)
	const int* getGlobalReligionCommerceArray() const { return m_aiGlobalReligionCommerce; }   // shrine {gold,research,culture,espionage}, materialized from shrineCommerce

	int getSpreadFactor() const { return m_iSpreadFactor; }               // identity.spreadFactor
	const CvWString& getAdjectiveKey() const { return m_szAdjectiveKey; } // identity.adjective (CvInfoBase has no adjective)
	int getFlavorValue(int i) const;                                      // ai.flavours [{FLAVOR:n}]
	const char* getSound() const { return m_szSound.c_str(); }            // sound.sound
	const char* getTechButton() const { return m_szTechButton.c_str(); }               // ui.art.techButton
	const char* getGenericTechButton() const { return m_szGenericTechButton.c_str(); } // ui.art.genericTechButton

	// grant thin accessors (grants.freeUnit / grants.numFreeUnits ride the composed CvGrants unit)
	int getFreeUnit() const;
	int getNumFreeUnits() const;

	// RUNTIME members (set post-load, NOT JSON): the pedia char glyphs, the mission id, the shrine-building registry.
	int getMissionType() const { return m_iMissionType; }   void setMissionType(int i) { m_iMissionType = i; }
	// GameFont glyph: the slot is DERIVED from the TGA index (8550 + tgaIndex*2), NOT the sequential id the symbol
	// pass passes -- reproduce the archived CvReligionInfo::setChar exactly (SourceArchive/Infos/CvReligionInfo.cpp:79),
	// else the religion icon lands on the wrong/empty GameFont slot (missing icon). Holy-city glyph = the +1 sibling.
	int getChar() const { return m_iChar; }                 void setChar(int /*i*/) { m_iChar = 8550 + m_iTGAIndex * 2; }
	int getHolyCityChar() const { return m_iHolyCityChar; } void setHolyCityChar(int /*i*/) { m_iHolyCityChar = 8551 + m_iTGAIndex * 2; }
	const std::vector<BuildingTypes>& getShrineBuildings() const { return reinterpret_cast<const std::vector<BuildingTypes>&>(m_aeShrineBuildings); }
	void addShrineBuilding(int iBuilding) { m_aeShrineBuildings.push_back(iBuilding); }

	// --- mirrored legacy CvReligionInfo getters (consumer surface) ---
	std::wstring pyGetAdjectiveKey() const { return getAdjectiveKey(); }   // real data: CvWString derives std::wstring
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }  // property engine (self-contained; XML-era manip data deferred)

	int getTGAIndex() const { return m_iTGAIndex; }             // ui.art.tgaIndex
	const char* getMovieFile() const { return m_szMovieFile.c_str(); }   // ui.art.movie.file
	const char* getMovieSound() const { return m_szMovieSound.c_str(); } // ui.art.movie.sound
	const char* getButtonDisabled() const;   // derived: base button (ui.art.icon) with the "_D.dds" disabled suffix (mirrors legacy)

	// curate_religion.py DROPs TechPrereq, store-inverting it onto tech.enables.religions. Reconstructed at LOAD by the
	// loadJson tech-FK reverse-index pass (the Route<-bonus pattern), which calls setTechPrereq.
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	void setTechPrereq(TechTypes e) { m_eTechPrereq = e; }   // load-time reverse-index writer (loadJson)

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvGrants*    getGrants()    const { return &m_grants; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvGrants*    mutGrants()    { return &m_grants; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvEdges     m_edges;
	CvGrants    m_grants;
	CvModifiers m_modifiers;
	int m_aiStateReligionCommerce[NUM_COMMERCE_TYPES];   // {c}.city.flat entries enabled:{STATE_RELIGION:self}
	int m_aiHolyCityCommerce[NUM_COMMERCE_TYPES];        // {c}.city.flat entries enabled:{IS_HOLY_CITY:self}
	int m_aiGlobalReligionCommerce[NUM_COMMERCE_TYPES];  // shrine per-commerce, materialized from shrineCommerce at load
	int m_iSpreadFactor;
	int m_iTGAIndex;
	int m_iFreeUnit, m_iNumFreeUnits;   // grants.freeUnit / numFreeUnits, materialized at mapFrom
	CvWString m_szAdjectiveKey;
	std::map<int, int> m_flavours;
	std::string m_szSound, m_szTechButton, m_szGenericTechButton, m_szMovieFile, m_szMovieSound;
	int m_iMissionType, m_iChar, m_iHolyCityChar;   // runtime (not JSON)
	TechTypes m_eTechPrereq;   // store-inverted tech.enables.religions, reconstructed at load (loadJson)
	std::vector<int> m_aeShrineBuildings;           // runtime (buildings self-register at load)
	CvPropertyManipulators m_PropertyManipulators;  // STUB empty -- property engine, XML-era manipulator data deferred
};

#endif // CV_JSON_RELIGION_INFO_H
