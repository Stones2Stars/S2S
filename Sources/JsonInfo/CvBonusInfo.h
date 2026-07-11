#pragma once
#ifndef CV_JSON_BONUS_INFO_H
#define CV_JSON_BONUS_INFO_H

//
//	CvBonusInfo -- the JSON real poco for resources (BONUS_*). Live-caller surface. The units/buildings a bonus
//	ENABLES ride the CvInfo base availability model (enables.*); this poco holds the bonus's own values +
//	map-generation placement. No cascade here. HUMAN-native values.
//
//	Live callers (verified 2026-07-07): getYieldChange -> plot yield; getHealth/getHappiness -> CvCity; getTechReveal
//	-> reveal state; getBonusClassType -> resource grouping; the placement fields -> CvGame worldgen.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / MapCategoryTypes / ImprovementTypes / BuildTypes
#include <vector>
#include <set>
#include <utility>     // std::pair (getTradeProvidingImprovements)
#include <algorithm>   // std::find (isProvidedByImprovementType)

class CvArtInfoBonus;

class CvBonusInfo : public CvInfo
{
public:
	CvBonusInfo();

	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
	int* getYieldChangeArray() const { return const_cast<int*>(m_aiYieldChange); }  // legacy int* accessor (setYieldChangeHelp; const_cast mirrors the archived non-const-array return)
	int getBonusClassType() const { return m_iBonusClassType; }
	int getHealth() const { return m_iHealth; }
	int getHappiness() const { return m_iHappiness; }
	int getAIObjective() const { return m_iAIObjective; }        // ai.behaviour.objective (real; e.g. BONUS_CHEMICALS=10).
	                                                             // -1 sentinel when absent (curator emits ai.behaviour only when nonzero -- curate_common AI_BEHAVIOUR).
	int getAITradeModifier() const { return m_iAITradeModifier; } // ai.behaviour.tradeModifier (real; e.g. BONUS_OIL=20)
	int getPercentPerPlayer() const { return m_iPercentPerPlayer; } // identity.player (de_i of legacy iPlayer); wired in mapFrom. 0 today -- NO bonus authors iPlayer (0 across all source XML + JSON).
	int getMinLandPercent() const { return m_iMinLandPercent; }   // mapGeneration.minLandPercent (real; e.g. BONUS_OIL=62)
	int getRandAppearance() const;                                // mapGeneration.constAppearance + RNG over rands (map-gen placement roll)
	bool isMapBonus() const { return m_iConstAppearance > 0 || m_iRandAppearance1 > 0 || m_iRandAppearance2 > 0
	                              || m_iRandAppearance3 > 0 || m_iRandAppearance4 > 0; }  // "spawns on the map" = has any appearance weight

	// runtime GameFont glyph (assigned post-load by CvGameTextMgr's symbol pass -- NOT JSON; mirrors CvReligionInfo)
	int getChar() const { return m_iChar; }   void setChar(int i) { m_iChar = i; }

	// DEAD legacy field: the archive never read m_piImprovementChange from XML either (always NULL -> 0), and 0 bonuses
	// author an ImprovementChange element (0 across source XML + JSON) -- no curator address exists. Faithful 0.
	int getImprovementChange(int /*i*/) const { return 0; }
	// Categories: authored by NO bonus (0 across source XML + JSON). If ever authored, the curator's generic pass lands
	// it at identity.categories; NOT parse-wired here because the array shape can't be verified against any real
	// instance (no-guessing). Real empty member keeps get/num/is self-consistent (and live if a later pass fills it).
	int getCategory(int i) const { return (i >= 0 && i < (int)m_aiCategories.size()) ? m_aiCategories[i] : -1; }
	int getNumCategories() const { return (int)m_aiCategories.size(); }
	bool isCategory(int i) const { return std::find(m_aiCategories.begin(), m_aiCategories.end(), i) != m_aiCategories.end(); }
	// EXE-bound art surface (mapscript/EXE map gen -- served by the CvBonusInfo shim leaf, cascade-engine-430.md §3)
	const char* getArtDefineTag() const { return m_szArtDefineTag.c_str(); }
	DllExport const CvArtInfoBonus* getArtInfo() const;   // EXE map-gen art (merged from the removed shim leaf)
	const char* getButton() const;                        // art-define button (else CvInfoBase's empty m_szButton)
	// techReveal/techCityTrade/techObsolete are DROPPED from the bonus and STORE-INVERTED onto the tech: reveal +
	// cityTrade both -> tech.enables.bonuses (deliberately merged, curate_bonus.py:26 -- indistinguishable, so both
	// read the SAME reconstructed tech), obsolete -> tech.obsoletes.bonuses (verified e.g.
	// TECH_SIMULATION_AWARENESS.enables.bonuses = [BONUS_UNOBTAINIUM]). Reconstructed at LOAD by the cascadeLoadJson
	// tech-FK reverse-index pass (the Route<-bonus pattern), which walks every tech's edges and calls the setters below.
	int getTechReveal() const { return m_eTechReveal; }
	int getTechCityTrade() const { return m_eTechCityTrade; }
	int getTechObsolete() const { return m_eTechObsolete; }
	void setTechReveal(TechTypes e) { m_eTechReveal = e; }         // load-time reverse-index writers (cascadeLoadJson)
	void setTechCityTrade(TechTypes e) { m_eTechCityTrade = e; }
	void setTechObsolete(TechTypes e) { m_eTechObsolete = e; }

	// map-generation placement (CvGame worldgen)
	int getMinAreaSize() const { return m_iMinAreaSize; }
	int getMinLatitude() const { return m_iMinLatitude; }
	int getMaxLatitude() const { return m_iMaxLatitude; }
	int getPlacementOrder() const { return m_iPlacementOrder; }
	int getTilesPer() const { return m_iTilesPer; }
	int getUniqueRange() const { return m_iUniqueRange; }
	int getGroupRange() const { return m_iGroupRange; }
	int getGroupRand() const { return m_iGroupRand; }
	bool isOneArea() const { return m_bOneArea; }
	bool isHills() const { return m_bHills; }
	bool isPeaks() const { return m_bPeaks; }
	bool isFlatlands() const { return m_bFlatlands; }
	bool isBonusCoastalOnly() const { return m_bBonusCoastalOnly; }
	bool isNoRiverSide() const { return m_bNoRiverSide; }
	bool isNormalize() const { return m_bNormalize; }

	// map-gen placement predicates -- mapGeneration.{validTerrains,validFeatures,validPlacementOn}
	// (legacy XML TerrainBooleans/FeatureBooleans/FeatureTerrainBooleans; engine.py's rename table).
	bool isTerrain(int i) const { return m_terrainSet.count(i) != 0; }
	bool isFeature(int i) const { return m_featureSet.count(i) != 0; }
	bool isFeatureTerrain(int i) const { return m_featureTerrainSet.count(i) != 0; }

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }

	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }  // no bonus authors a PropertyManipulators element (0 across source XML + JSON); empty is faithful. Property-engine migration is a separate deferred pass (mirrors CvCorporationInfo).

	// RUNTIME "which improvements trade this bonus" -- populated post-load by CvGlobals's derived-caching pass, one
	// setProvidedByImprovementTypes() per ImprovementInfo::isImprovementBonusTrade hit (CvGlobals.cpp ~L3200). NOT
	// JSON; real backing member kept so the set/get/num/is family is self-consistent (mirrors the archived class).
	void setProvidedByImprovementTypes(const ImprovementTypes eType);   // push_back (non-const setter)
	const std::vector<ImprovementTypes>& getProvidedByImprovementTypes() const { return m_providedByImprovementTypes; }
	ImprovementTypes getProvidedByImprovementType(const int i) const { return m_providedByImprovementTypes[i]; }
	int getNumProvidedByImprovementTypes() const { return (int)m_providedByImprovementTypes.size(); }
	bool isProvidedByImprovementType(const ImprovementTypes i) const
	{ return std::find(m_providedByImprovementTypes.begin(), m_providedByImprovementTypes.end(), i) != m_providedByImprovementTypes.end(); }

	// NOT a curator field: the (improvement,build) trade pairs are a RUNTIME cross-index built from the Build/Improvement
	// infos (archived class built it lazily from GC.getBuildInfo/getImprovementInfo.isImprovementBonusTrade). Returns a
	// real, empty member so the caller's begin()/end() iteration is safe (never NULL-derefs); reconstruct the lazy build
	// once the Build/Improvement JSON getter surface (getImprovement/isImprovementBonusTrade) is wired.
	const std::vector<std::pair<ImprovementTypes, BuildTypes> >* getTradeProvidingImprovements() { return &m_tradeProvidingImprovements; }
	void addTradeProvidingImprovement(ImprovementTypes eImp, BuildTypes eBuild) { m_tradeProvidingImprovements.push_back(std::make_pair(eImp, eBuild)); }  // populated eagerly at load (CvGlobals derived-cache pass), replacing the archived lazy build

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonEdges     m_edges;
	CvJsonModifiers m_modifiers;
	CvPropertyManipulators m_PropertyManipulators;   // empty -- no bonus authors PropertyManipulators (0 across source XML + JSON); property-engine migration deferred (mirrors CvCorporationInfo)
	int m_aiYieldChange[NUM_YIELD_TYPES];  // food/production/commerce .plot.flat
	int m_iBonusClassType;                 // identity.bonusClassType (BONUSCLASS_ id)
	int m_iAIObjective;                    // ai.behaviour.objective (-1 sentinel when the block is absent)
	int m_iAITradeModifier;                // ai.behaviour.tradeModifier
	int m_iHealth;                         // health.empire.flat (connected-resource benefit, curate_bonus.py:64)
	int m_iHappiness;                      // happiness.empire.flat
	int m_iChar;                           // runtime GameFont glyph (setChar; not JSON)
	std::string m_szArtDefineTag;          // world.art.icon (ART_DEF_* tag; the EXE map-gen art lookup key)
	int m_iMinAreaSize, m_iMinLatitude, m_iMaxLatitude, m_iPlacementOrder, m_iTilesPer;
	int m_iUniqueRange, m_iGroupRange, m_iGroupRand;   // mapGeneration.*
	int m_iConstAppearance, m_iRandAppearance1, m_iRandAppearance2, m_iRandAppearance3, m_iRandAppearance4;  // mapGeneration.constAppearance + rands.iRandApp1-4 (randAppearance/isMapBonus)
	int m_iMinLandPercent;                 // mapGeneration.minLandPercent
	int m_iPercentPerPlayer;               // identity.player (legacy iPlayer); 0 -- no bonus authors it
	TechTypes m_eTechReveal, m_eTechCityTrade, m_eTechObsolete;   // store-inverted tech FKs, reconstructed at load (cascadeLoadJson)
	std::vector<int> m_aiCategories;       // identity.categories -- empty (no bonus authors Categories); see getCategory
	bool m_bOneArea, m_bHills, m_bPeaks, m_bFlatlands, m_bBonusCoastalOnly, m_bNoRiverSide, m_bNormalize;
	std::vector<MapCategoryTypes> m_aeMapCategories;
	std::set<int> m_terrainSet;         // mapGeneration.validTerrains (TerrainBooleans) -- isTerrain
	std::set<int> m_featureSet;         // mapGeneration.validFeatures (FeatureBooleans) -- isFeature
	std::set<int> m_featureTerrainSet;  // mapGeneration.validPlacementOn (FeatureTerrainBooleans) -- isFeatureTerrain
	std::vector<ImprovementTypes> m_providedByImprovementTypes;  // runtime (CvGlobals derived-cache pass); NOT JSON
	std::vector<std::pair<ImprovementTypes, BuildTypes> > m_tradeProvidingImprovements;  // empty -- runtime cross-index, not a curator field (see getter)
};

#endif // CV_JSON_BONUS_INFO_H
