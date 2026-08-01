#pragma once
#ifndef CV_JSON_CORPORATION_INFO_H
#define CV_JSON_CORPORATION_INFO_H

//
//	CvCorporationInfo -- the CORPORATION poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP:
//	the four read categories, nothing else). Styled for the JSON anatomy (json.md §2); every magnitude read is a
//	load-compiled fetch ([DEC-materialize-at-mapfrom]); no legacy getter name returns ([DEC-new-getter-surface]).
//
//	CENSUS SHAPE (Assets/Data/corporations, 23 entities): every modifier-family entry a corporation authors is
//	CONDITIONED -- the per-city output rides {HAS_CORPORATION: SELF} gates with per:{anyOf: consumed bonuses}
//	scalers, and the HQ revenue rides {IS_HEADQUARTERS: SELF} + per:"CORPORATION_LEVEL" (rulings 4+10, the
//	{IS_HOLY_CITY} pattern). NOTHING folds into the unconditioned slot sums, so this type carries NO per-group
//	point getters -- the read surface for corp magnitudes is the base conditioned-list access + the expected*
//	what-if valuations (CvInfo), plus the two mapFrom-materialized value planes below (scanned ONCE from the
//	compiled entries -- the one sanctioned load-time scan source, patterns.md § Materialize at mapFrom).
//	Corp active/dormant stays an ENGINE-OWNED input (isActiveCorporation -- culture-religion-research.md).
//

#include "CvInfo.h"
#include <map>
#include <vector>

class CvCorporationInfo : public CvInfo
{
public:
	CvCorporationInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvProvides*  getProvides()  const { return &m_provides; }   // §5a provides.bonuses (the produced bonus)
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvRequires*  getRequires()  const { return &m_requires; }   // §4.3 requires.spread (served while no corp authors it -- owner ruling 2026-07-17)

	// ======================= 2. CLASSIFICATION -- none (corporations author no §8 block) ====================

	// ======================= 3. MODIFIER GROUPS -- conditioned-only (see the census note above) ==============
	// No point getters: every authored entry is conditioned, so every unconditioned sum is 0 by construction.
	// Readers walk the base modifierConditioned()/modifierConditionedRange() or ask the expected* endpoints.
	// The TWO materialized value planes (bare ×100 member reads, filled once at mapFrom from entries()):
	// -- the HQ-city revenue plane (rulings 4+10): the per-CORPORATION_LEVEL commerce value of the
	//    {IS_HEADQUARTERS: SELF} entries; the consumer applies × corp level at the HQ city itself.
	int getHeadquartersCommerce(CommerceTypes eCommerce) const { return m_aiHeadquartersCommerce[(int)eCommerce]; }
	// -- the consumed-bonus set (the per:{anyOf:...} scaler's union): WHICH bonuses this corp's output scales
	//    over -- the spread/dormancy gate's "≥1 prereq bonus present" set and the AI's consumption view.
	const std::vector<int>& getConsumedBonuses() const { return m_aeConsumedBonuses; }

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getSpreadFactor() const { return m_iSpreadFactor; }                                   // identity.spreadFactor (spread-weight config)
	int getCompetingSpreadCostPercent() const { return m_iCompetingSpreadCostPercent; }       // identity.competingSpreadCostPercent
	int getSpreadCost() const { return m_iSpreadCost; }                                       // cost.spread (executive spread gold)
	const char* getSound() const { return m_szSound.c_str(); }                                // sound.sound
	int getTGAIndex() const { return m_iTGAIndex; }                                           // ui.art.tgaIndex
	const char* getMovieFile() const { return m_szMovieFile.c_str(); }                        // ui.art.movie.file
	const char* getMovieSound() const { return m_szMovieSound.c_str(); }                      // ui.art.movie.sound
	// requires.spread per-building count atoms ({type:BUILDING_X, scope:empire, min:N}, json §4.3) -- the
	// executive-spread gate's per-building COUNT need; 0 = no requirement (materialized at mapFrom).
	int getSpreadBuildingCount(int iBuilding) const
	{
		std::map<int, int>::const_iterator countIt = m_spreadBuildingCounts.find(iBuilding);
		return countIt != m_spreadBuildingCounts.end() ? countIt->second : 0;
	}
	// The same plane as a LIST: the handful of buildings this corp actually names, so a consumer asking
	// "what does it require" walks those rather than every building id asking each one in turn (the
	// own-data inversion). Ordered by building id, so a reader sees them in registry order.
	const std::map<int, int>& getSpreadBuildingCounts() const { return m_spreadBuildingCounts; }

	// --- store-inverted tech FKs (tech.enables.corporations / tech.obsoletes.corporations), reconstructed at
	// LOAD by the readJson reverse pass (CvReversePass), which calls the setters below. LOAD-ONLY writers. ---
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	TechTypes getObsoleteTech() const { return m_eObsoleteTech; }
	void setTechPrereq(TechTypes eTech) { m_eTechPrereq = eTech; }
	void setObsoleteTech(TechTypes eTech) { m_eObsoleteTech = eTech; }

	// --- RUNTIME members (set post-load, NOT JSON): the GameFont glyphs + the spread mission id ---
	int getChar() const { return m_iChar; }
	void setChar(int iSymbol);                        // TGA-derived slot, religion-block offset (.cpp)
	int getHeadquarterChar() const { return m_iHeadquarterChar; }
	void setHeadquarterChar(int iSymbol);
	int getMissionType() const { return m_iMissionType; }
	void setMissionType(int iMission) { m_iMissionType = iMission; }

	// The KEEP-legacy property engine's per-turn SOURCES (property-audit.md). The gather roster walks
	// a corporation, so the container must exist -- but NO corporation authors a PROPERTY_* family today, so it is
	// accurately EMPTY rather than stubbed: it fills the moment such a deposit is curated.
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvProvides*  mutProvides()  { return &m_provides; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvRequires*  mutRequires()  { return &m_requires; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvProvides  m_provides;
	CvModifiers m_modifiers;
	CvRequires  m_requires;

	// --- the materialized value planes (filled once at mapFrom from the compiled entries) ---
	int m_aiHeadquartersCommerce[NUM_COMMERCE_TYPES];
	std::vector<int> m_aeConsumedBonuses;
	std::map<int, int> m_spreadBuildingCounts;

	// --- the intrinsic identity members ---
	int m_iSpreadFactor;
	int m_iCompetingSpreadCostPercent;
	int m_iSpreadCost;
	int m_iTGAIndex;
	std::string m_szSound;
	std::string m_szMovieFile;
	std::string m_szMovieSound;

	// --- reverse-pass-fed FKs + runtime members ---
	TechTypes m_eTechPrereq;
	TechTypes m_eObsoleteTech;
	int m_iChar;
	int m_iHeadquarterChar;
	int m_iMissionType;
	CvPropertyManipulators m_PropertyManipulators;   // fed from the PROPERTY_* families (CascadePropertyBridge)
};

#endif // CV_JSON_CORPORATION_INFO_H
