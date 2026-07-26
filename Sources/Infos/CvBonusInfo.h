#pragma once
#ifndef CV_JSON_BONUS_INFO_H
#define CV_JSON_BONUS_INFO_H

//
//	CvBonusInfo -- the BONUS (resource) poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP).
//	Styled for the JSON anatomy (json.md §2): the bonus's own plot output and its empire wellbeing flats are
//	compiled modifier reads ([DEC-materialize-at-mapfrom], [DEC-scope-is-an-axis]); the intrinsic set is
//	identity/ai/mapGeneration self-description; the tech relationships are the LOAD-reconstructed forward FKs
//	(store-inverted onto the tech's enables/obsoletes buckets, un-inverted by CvReversePass). What a bonus
//	ENABLES rides the CvInfo base availability model (enables.* edges). No legacy getter name returns
//	([DEC-new-getter-surface]).
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // MapCategoryTypes / ImprovementTypes / BuildTypes / TechTypes
#include <vector>
#include <set>
#include <utility>     // std::pair (getTradeProvidingImprovements)
#include <algorithm>   // std::find (isProvidedByImprovementType)

class CvArtInfoBonus;

class CvBonusInfo : public CvInfo
{
public:
	CvBonusInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 2. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. Census
	// participation: food/production/commerce plot flats -- the bonus's OWN tile output, modifier.md §5
	// plot-substrate own-output -- and health/happiness empire flats, the connected-resource benefit.)
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }
	int getFlatWellbeing(WellbeingChannel eChannel, CvCascScope eScope) const
	{
		if (eChannel == WELLBEING_ANGER || eChannel == WELLBEING_UNHEALTH)
		{
			return 0;
		}
		return m_modifiers.sum(infoWellbeingFamily(eChannel), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT);
	}

	// ======================= 3. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getBonusClassType() const { return m_iBonusClassType; }    // identity.bonusClassType (BONUSCLASS_* FK)
	int getAIObjective() const { return m_iAIObjective; }          // ai.behaviour.objective (-1 sentinel when absent)
	int getAITradeModifier() const { return m_iAITradeModifier; }  // ai.behaviour.tradeModifier
	int getPercentPerPlayer() const { return m_iPercentPerPlayer; }// identity.player (0 today -- no bonus authors it)
	// runtime GameFont glyph (assigned post-load by CvGameTextMgr's symbol pass -- NOT JSON)
	int getChar() const { return m_iChar; }
	void setChar(int iChar) { m_iChar = iChar; }
	// EXE-bound art surface (mapscript/EXE map gen)
	const char* getArtDefineTag() const { return m_szArtDefineTag.c_str(); }
	DllExport const CvArtInfoBonus* getArtInfo() const;
	const char* getButton() const;   // art-define button (else CvInfoBase's empty m_szButton)

	// --- the LOAD-reconstructed tech forward FKs (CvReversePass::rp_reconstructTechForeignKeys: reveal +
	// cityTrade both from the tech's enables.bonuses -- deliberately merged/indistinguishable, first tech
	// wins -- obsolete from obsoletes.bonuses). The setters are the reverse pass's load-window writers. ---
	int getTechReveal() const { return m_eTechReveal; }
	int getTechCityTrade() const { return m_eTechCityTrade; }
	int getTechObsolete() const { return m_eTechObsolete; }
	void setTechReveal(TechTypes eTech) { m_eTechReveal = eTech; }
	void setTechCityTrade(TechTypes eTech) { m_eTechCityTrade = eTech; }
	void setTechObsolete(TechTypes eTech) { m_eTechObsolete = eTech; }

	// --- map-generation placement (CvGame worldgen) ---
	int getMinAreaSize() const { return m_iMinAreaSize; }
	int getMinLatitude() const { return m_iMinLatitude; }
	int getMaxLatitude() const { return m_iMaxLatitude; }
	int getPlacementOrder() const { return m_iPlacementOrder; }
	int getTilesPer() const { return m_iTilesPer; }
	int getUniqueRange() const { return m_iUniqueRange; }
	int getGroupRange() const { return m_iGroupRange; }
	int getGroupRand() const { return m_iGroupRand; }
	int getMinLandPercent() const { return m_iMinLandPercent; }
	int getRandAppearance() const;   // mapGeneration.constAppearance + one RNG draw per rand band
	bool isMapBonus() const
	{
		return m_iConstAppearance > 0 || m_iRandAppearance1 > 0 || m_iRandAppearance2 > 0
			|| m_iRandAppearance3 > 0 || m_iRandAppearance4 > 0;   // "spawns on the map" = any appearance weight
	}
	bool isOneArea() const { return m_bOneArea; }
	bool isHills() const { return m_bHills; }
	bool isPeaks() const { return m_bPeaks; }
	bool isFlatlands() const { return m_bFlatlands; }
	bool isBonusCoastalOnly() const { return m_bBonusCoastalOnly; }
	bool isNoRiverSide() const { return m_bNoRiverSide; }
	bool isNormalize() const { return m_bNormalize; }
	// map-gen placement predicates -- mapGeneration.{validTerrains,validFeatures,validPlacementOn}
	bool isTerrain(int iTerrain) const { return m_terrainSet.count(iTerrain) != 0; }
	bool isFeature(int iFeature) const { return m_featureSet.count(iFeature) != 0; }
	bool isFeatureTerrain(int iTerrain) const { return m_featureTerrainSet.count(iTerrain) != 0; }

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }

	// --- RUNTIME "which improvements trade this bonus" -- populated post-load by CvGlobals's derived-caching
	// pass (one setProvidedByImprovementTypes per ImprovementInfo::isImprovementBonusTrade hit). NOT JSON. ---
	void setProvidedByImprovementTypes(const ImprovementTypes eType);
	const std::vector<ImprovementTypes>& getProvidedByImprovementTypes() const { return m_providedByImprovementTypes; }
	ImprovementTypes getProvidedByImprovementType(const int iIndex) const { return m_providedByImprovementTypes[iIndex]; }
	int getNumProvidedByImprovementTypes() const { return (int)m_providedByImprovementTypes.size(); }
	bool isProvidedByImprovementType(const ImprovementTypes eType) const
	{ return std::find(m_providedByImprovementTypes.begin(), m_providedByImprovementTypes.end(), eType) != m_providedByImprovementTypes.end(); }
	// The (improvement, build) trade pairs -- a RUNTIME cross-index built eagerly at load by the CvGlobals
	// derived-cache pass from the Build/Improvement infos. NOT a curated field.
	const std::vector<std::pair<ImprovementTypes, BuildTypes> >* getTradeProvidingImprovements() { return &m_tradeProvidingImprovements; }
	void addTradeProvidingImprovement(ImprovementTypes eImprovement, BuildTypes eBuild)
	{ m_tradeProvidingImprovements.push_back(std::make_pair(eImprovement, eBuild)); }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvModifiers m_modifiers;

	// --- the intrinsic identity members (materialized once at mapFrom; getters are bare reads) ---
	int m_iBonusClassType;
	int m_iAIObjective;
	int m_iAITradeModifier;
	int m_iPercentPerPlayer;
	int m_iChar;                        // runtime GameFont glyph (setChar; not JSON)
	std::string m_szArtDefineTag;       // world.art.icon (ART_DEF_* tag; the EXE map-gen art lookup key)
	int m_iMinAreaSize;
	int m_iMinLatitude;
	int m_iMaxLatitude;
	int m_iPlacementOrder;
	int m_iTilesPer;
	int m_iUniqueRange;
	int m_iGroupRange;
	int m_iGroupRand;
	int m_iConstAppearance;
	int m_iRandAppearance1;
	int m_iRandAppearance2;
	int m_iRandAppearance3;
	int m_iRandAppearance4;
	int m_iMinLandPercent;
	bool m_bOneArea;
	bool m_bHills;
	bool m_bPeaks;
	bool m_bFlatlands;
	bool m_bBonusCoastalOnly;
	bool m_bNoRiverSide;
	bool m_bNormalize;
	TechTypes m_eTechReveal;            // load-reconstructed tech forward FKs (CvReversePass)
	TechTypes m_eTechCityTrade;
	TechTypes m_eTechObsolete;
	std::vector<MapCategoryTypes> m_aeMapCategories;
	std::set<int> m_terrainSet;         // mapGeneration.validTerrains
	std::set<int> m_featureSet;         // mapGeneration.validFeatures
	std::set<int> m_featureTerrainSet;  // mapGeneration.validPlacementOn
	std::vector<ImprovementTypes> m_providedByImprovementTypes;  // runtime (CvGlobals derived-cache pass); NOT JSON
	std::vector<std::pair<ImprovementTypes, BuildTypes> > m_tradeProvidingImprovements;  // runtime cross-index; NOT JSON
};

#endif // CV_JSON_BONUS_INFO_H
