#pragma once
#ifndef CV_JSON_BONUS_INFO_H
#define CV_JSON_BONUS_INFO_H

//
//	CvJsonBonusInfo -- the JSON real poco for resources (BONUS_*). Live-caller surface. The units/buildings a bonus
//	ENABLES ride the CvJsonInfo base availability model (enables.*); this poco holds the bonus's own values +
//	map-generation placement. No cascade here. HUMAN-native values.
//
//	Live callers (verified 2026-07-07): getYieldChange -> plot yield; getHealth/getHappiness -> CvCity; getTechReveal
//	-> reveal state; getBonusClassType -> resource grouping; the placement fields -> CvGame worldgen.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / MapCategoryTypes
#include <vector>

class CvJsonBonusInfo : public CvJsonInfo
{
public:
	CvJsonBonusInfo();

	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
	int getBonusClassType() const { return m_iBonusClassType; }
	int getHealth() const { return m_iHealth; }
	int getHappiness() const { return m_iHappiness; }
	// EXE-bound art surface (mapscript/EXE map gen -- served by the CvBonusInfo shim leaf, cascade-engine-430.md §3)
	const char* getArtDefineTag() const { return m_szArtDefineTag.c_str(); }
	// NB techReveal/techCityTrade/techObsolete are NOT here: they are the TECH's edges (curate_bonus.py:26-27 stores
	// them as tech.enables.bonuses / tech.obsoletes.bonuses) -- availability the cascade reads off the tech.

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

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }

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
	int m_aiYieldChange[NUM_YIELD_TYPES];  // food/production/commerce .plot.flat
	int m_iBonusClassType;                 // identity.bonusClassType (BONUSCLASS_ id)
	int m_iHealth;                         // health.empire.flat (connected-resource benefit, curate_bonus.py:64)
	int m_iHappiness;                      // happiness.empire.flat
	std::string m_szArtDefineTag;          // world.art.icon (ART_DEF_* tag; the EXE map-gen art lookup key)
	int m_iMinAreaSize, m_iMinLatitude, m_iMaxLatitude, m_iPlacementOrder, m_iTilesPer;
	int m_iUniqueRange, m_iGroupRange, m_iGroupRand;   // mapGeneration.*
	bool m_bOneArea, m_bHills, m_bPeaks, m_bFlatlands, m_bBonusCoastalOnly, m_bNoRiverSide, m_bNormalize;
	std::vector<MapCategoryTypes> m_aeMapCategories;
};

#endif // CV_JSON_BONUS_INFO_H
