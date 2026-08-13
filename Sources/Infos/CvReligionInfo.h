#pragma once
#ifndef CV_JSON_RELIGION_INFO_H
#define CV_JSON_RELIGION_INFO_H

//
//	CvReligionInfo -- the RELIGION poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP: the
//	four read categories, nothing else). Styled for the JSON anatomy (json.md §2); every magnitude read is a
//	load-compiled fetch ([DEC-materialize-at-mapfrom]); no legacy getter name returns ([DEC-new-getter-surface]).
//
//	CENSUS SHAPE (Assets/Data/religions, 29 entities): every modifier-family entry a religion authors is
//	CONDITIONED -- the per-commerce bonuses ride {IS_HOLY_CITY: SELF} / {STATE_RELIGION: SELF} gates -- so
//	NOTHING folds into the unconditioned slot sums and this type carries NO per-group point getters: the read
//	surface for religion magnitudes is the base conditioned-list access + the expected* what-if valuations
//	(CvInfo). The §9 `shrine` block (the per-commerce SHRINE values, scaled per city holding the religion --
//	the building declares only the FK relationship) is the one bespoke value plane, materialized below.
//	Religion presence/spread state stays ENGINE-OWNED input (culture-religion-research.md).
//

#include "CvInfo.h"
#include <map>
#include <vector>

class CvReligionInfo : public CvInfo
{
public:
	CvReligionInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 2. CLASSIFICATION -- none (religions author no §8 block) =======================

	// ======================= 3. MODIFIER GROUPS -- conditioned-only (see the census note above) ==============
	// No point getters: every authored entry is conditioned ({IS_HOLY_CITY}/{STATE_RELIGION}), so every
	// unconditioned sum is 0 by construction. Readers walk the base modifierConditioned()/
	// modifierConditionedRange() or ask the expected* endpoints.
	// The §9 SHRINE value plane (bare ×100 member read, materialized once at mapFrom from the `shrine` block):
	// the per-commerce shrine revenue, scaled per city holding the religion by the consumer (the shrine
	// BUILDING carries only the FK -- CvBuildingInfo::getShrineReligion).

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getSpreadFactor() const { return m_iSpreadFactor; }                              // identity.spreadFactor
	const CvWString& getAdjectiveKey() const { return m_szAdjectiveKey; }                // identity.adjective TXT_KEY
	int getFlavorValue(int iFlavor) const;                                               // ai.flavours [{FLAVOR_X: n}]
	const char* getSound() const { return m_szSound.c_str(); }                           // sound.sound
	const char* getTechButton() const { return m_szTechButton.c_str(); }                 // ui.art.techButton
	const char* getGenericTechButton() const { return m_szGenericTechButton.c_str(); }   // ui.art.genericTechButton
	int getTGAIndex() const { return m_iTGAIndex; }                                      // ui.art.tgaIndex
	const char* getMovieFile() const { return m_szMovieFile.c_str(); }                   // ui.art.movie.file
	const char* getMovieSound() const { return m_szMovieSound.c_str(); }                 // ui.art.movie.sound
	const char* getButtonDisabled() const;   // derived: the base button (ui.art.icon) with the "_D.dds" disabled suffix
	// grants-materialized reads (the getters are bare member reads)
	int getFreeUnit() const { return m_iFreeUnit; }             // grants.freeUnit (the founder missionary UNIT_* FK)
	int getNumFreeUnits() const { return m_iNumFreeUnits; }     // grants.numFreeUnits pulse (human count)

	// --- store-inverted tech FK (tech.enables.religions), reconstructed at LOAD by the readJson reverse pass
	// (CvReversePass), which calls the setter below. LOAD-ONLY writer. ---
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	void setTechPrereq(TechTypes eTech) { m_eTechPrereq = eTech; }

	// --- RUNTIME members (set post-load, NOT JSON): the GameFont glyphs, the spread mission id, the
	// shrine-building registry (fed at load by the readJson general reverse pass from each shrine building's
	// §9 `shrine` FK -- CvReversePass calls addShrineBuilding). ---
	int getMissionType() const { return m_iMissionType; }
	void setMissionType(int iMission) { m_iMissionType = iMission; }
	// GameFont glyph: the slot is DERIVED from the TGA index (8550 + tgaIndex*2), NOT the sequential id the
	// symbol pass passes -- reproduces the archived derivation exactly, else the religion icon lands on the
	// wrong/empty GameFont slot. The holy-city glyph is the +1 sibling.
	int getChar() const { return m_iChar; }
	void setChar(int /*iSymbol*/) { m_iChar = 8550 + m_iTGAIndex * 2; }
	int getHolyCityChar() const { return m_iHolyCityChar; }
	void setHolyCityChar(int /*iSymbol*/) { m_iHolyCityChar = 8551 + m_iTGAIndex * 2; }
	const std::vector<BuildingTypes>& getShrineBuildings() const { return reinterpret_cast<const std::vector<BuildingTypes>&>(m_aeShrineBuildings); }
	void addShrineBuilding(int iBuilding) { m_aeShrineBuildings.push_back(iBuilding); }
	void clearShrineBuildings() { m_aeShrineBuildings.clear(); }   // clear-first: the reverse pass runs in BOTH load phases

	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }   // §5 -- triggers + the folded grants

	// The KEEP-legacy property engine's per-turn SOURCES (property-audit.md). The gather roster walks
	// a religion, so the container must exist -- but NO religion authors a PROPERTY_* family today, so it is
	// accurately EMPTY rather than stubbed: it fills the moment such a deposit is curated.
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvTriggers  m_triggers;
	CvModifiers m_modifiers;

	// --- the materialized §9 shrine plane + the intrinsic identity members ---
	int m_iSpreadFactor;
	int m_iTGAIndex;
	int m_iFreeUnit;
	int m_iNumFreeUnits;
	CvWString m_szAdjectiveKey;
	std::map<int, int> m_flavours;
	std::string m_szSound;
	std::string m_szTechButton;
	std::string m_szGenericTechButton;
	std::string m_szMovieFile;
	std::string m_szMovieSound;

	// --- reverse-pass-fed FK + runtime members ---
	TechTypes m_eTechPrereq;
	int m_iMissionType;
	int m_iChar;
	int m_iHolyCityChar;
	std::vector<int> m_aeShrineBuildings;
	CvPropertyManipulators m_PropertyManipulators;   // fed from the PROPERTY_* families (CascadePropertyBridge)
};

#endif // CV_JSON_RELIGION_INFO_H
