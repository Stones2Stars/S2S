#pragma once
#ifndef CV_JSON_CORPORATION_INFO_H
#define CV_JSON_CORPORATION_INFO_H

//
//	CvCorporationInfo -- the per-type cascade info for CORPORATIONS. Every corp modifier family is authored as a
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

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / NUM_COMMERCE_TYPES / BonusTypes
#include <map>
#include <vector>

class CvCorporationInfo : public CvInfo
{
public:
	CvCorporationInfo();

	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }          // {y}.city.flat, no per (×1)
	int getYieldProduced(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldProduced[i] : 0; }      // {y}.city.flat with per (×100 re-applied)
	int getCommerceChange(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceChange[i] : 0; }
	int getCommerceProduced(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceProduced[i] : 0; }
	int getHeadquarterCommerce(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiHeadquarterCommerce[i] : 0; }  // REAL: {c}.empire.headquarters.perCorporationLevel (populated in mapFrom); the downstream HQ-revenue SCALING is the deferred corp-pass, not this getter
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
	int getHeadquarterChar() const { return m_iHeadquarterChar; }  void setHeadquarterChar(int i);   // TGA-derived slot (.cpp)
	int getMissionType() const { return m_iMissionType; }          void setMissionType(int i) { m_iMissionType = i; }

	// --- mirrored legacy CvCorporationInfo getters (consumer surface; hotkey/action inherited from CvHotkeyInfo) ---
	int getChar() const { return m_iChar; }               // corp display glyph -- runtime-assigned by the CvGameTextMgr symbol pass via setChar (non-XML runtime value, stored not discarded)
	int getTGAIndex() const { return m_iTGAIndex; }       // ui.art.tgaIndex
	const char* getMovieFile() const { return m_szMovieFile.c_str(); }   // ui.art.movie.file
	const char* getMovieSound() const { return m_szMovieSound.c_str(); } // ui.art.movie.sound
	// store-inverted onto the tech (tech.enables.corporations / tech.obsoletes.corporations); reconstructed at LOAD by
	// the loadJson tech-FK reverse-index pass (the Route<-bonus pattern), which calls the setters below.
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	TechTypes getObsoleteTech() const { return m_eObsoleteTech; }
	void setTechPrereq(TechTypes e) { m_eTechPrereq = e; }       // load-time reverse-index writers (loadJson)
	void setObsoleteTech(TechTypes e) { m_eObsoleteTech = e; }
	// curator-gap (curate_corporation.py DROP={TechPrereq,PrereqBonuses,PrereqBuildings}): PrereqBuildings is
	// the corp's per-building COUNT prereq for SPREADING (legacy PrereqBuildings; the CvUnit executive-spread gate
	// reads it per building id). Authored as requires.spread count atoms ({type:BUILDING_X, scope:empire, min:N},
	// json §4.3 -- owner ruling 2026-07-17: the mechanism is served even while no corp authors it); materialized at
	// mapFrom into the count map (0 = no requirement, the legacy unset value).
	int getPrereqBuilding(int i) const
	{ std::map<int, int>::const_iterator it = m_prereqBuildingCounts.find(i); return it != m_prereqBuildingCounts.end() ? it->second : 0; }
	// curator-gap: no PrereqGameOption in the base corp XML and none in curate_corporation.py's tag tables; corps
	// author NO entity-level enabled/disabled gate (the HAS_CORPORATION predicate rides each per-city family entry,
	// not a whole-entity gate), so there is no GAMEOPTION_ atom to walk (contrast CvPromotionInfo getOnGameOption).
	int getPrereqGameOption() const { return NO_GAMEOPTION; }
	// top-level `excludes` (curate_corporation.py EXCLUDES: CompetingCorporations -> json sec9 same-tier corp<->corp
	// exclusion). Emitted address; empty in ALL shipped base XML (no corp authors CompetingCorporations today) -> reads
	// an empty set until data lands. `excludes` classifies CJK_INTRINSIC, so the base skips it for this subclass to parse.
	bool isCompetingCorporation(int i) const { for (int j = 0; j < (int)m_aeExcludes.size(); ++j) if (m_aeExcludes[j] == i) return true; return false; }
	int getFreeUnit() const { return m_iFreeUnit; }   // grants.freeUnit (materialized at mapFrom, as CvReligionInfo)
	int* getYieldChangeArray() const { return const_cast<int*>(m_aiYieldChange); }          // real (the ×1 change array)
	int* getCommerceChangeArray() const { return const_cast<int*>(m_aiCommerceChange); }    // real
	void setChar(int i);                                              // TGA-derived GameFont slot (defined in .cpp; needs GC)
	const std::vector<BonusTypes>& getPrereqBonuses() const   // the per-scaler bonus set, as a BonusTypes view of m_aePrereqBonuses
	{ return reinterpret_cast<const std::vector<BonusTypes>&>(m_aePrereqBonuses); }
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }  // property engine (self-contained; XML-era manip data deferred)

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvProvides*  getProvides()  const { return &m_provides; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvGrants*    getGrants()    const { return &m_grants; }   // grants.freeUnit (getFreeUnit)
	virtual const CvRequires*  getRequires()  const { return &m_requires; } // requires.spread (getPrereqBuilding)

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvProvides*  mutProvides()  { return &m_provides; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvGrants*    mutGrants()    { return &m_grants; }
	virtual CvRequires*  mutRequires()  { return &m_requires; }

private:
	CvEdges     m_edges;
	CvProvides  m_provides;
	CvModifiers m_modifiers;
	CvGrants    m_grants;
	CvRequires  m_requires;
	std::map<int, int> m_prereqBuildingCounts;   // requires.spread BUILDING count atoms, materialized at mapFrom
	int m_aiYieldChange[NUM_YIELD_TYPES];
	int m_aiYieldProduced[NUM_YIELD_TYPES];
	int m_aiCommerceChange[NUM_COMMERCE_TYPES];
	int m_aiCommerceProduced[NUM_COMMERCE_TYPES];
	int m_aiHeadquarterCommerce[NUM_COMMERCE_TYPES];
	int m_iMaintenance, m_iHealth, m_iHappiness, m_iFreeXP, m_iMilitaryProductionModifier;
	int m_iFreeUnit;   // grants.freeUnit, materialized at mapFrom
	std::vector<int> m_aePrereqBonuses;
	int m_iSpreadCost, m_iSpread, m_iCompetingSpreadCostPercent;
	std::string m_szSound, m_szMovieFile, m_szMovieSound;   // sound.sound / ui.art.movie.file / ui.art.movie.sound
	int m_iTGAIndex;                          // ui.art.tgaIndex
	std::vector<int> m_aeExcludes;            // top-level `excludes` -- CompetingCorporations FKs (isCompetingCorporation)
	int m_iHeadquarterChar, m_iMissionType, m_iChar;   // runtime (m_iChar: display glyph assigned by the symbol pass via setChar)
	TechTypes m_eTechPrereq, m_eObsoleteTech; // store-inverted tech FKs, reconstructed at load (loadJson)
	CvPropertyManipulators m_PropertyManipulators;   // STUB empty -- property engine, XML-era manipulator data deferred
};

#endif // CV_JSON_CORPORATION_INFO_H
