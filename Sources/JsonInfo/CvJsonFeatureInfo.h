#pragma once
#ifndef CV_JSON_FEATURE_INFO_H
#define CV_JSON_FEATURE_INFO_H

//
//	CvJsonFeatureInfo -- the JSON real poco for terrain FEATURES (the CvXInfo replacement). Live-caller surface only,
//	mapped from the curator's real shapes: the plot yield/health/defense/culture/vision families, and the `identity`
//	placement fields. HUMAN-native values (the cascade ×100s on its side). Availability (requires/enables/…) rides the
//	CvJsonInfo base. No cascade here.
//
//	Live callers (verified 2026-07-07): getYieldChange -> CvCity/CvPlot yields; getHealthPercent -> CvCity health;
//	getDefenseModifier -> CvPlot combat; getMovementCost -> unit move cost; getCultureDistance -> CvCity; validTerrains/
//	isTerrain -> feature placement; popDestroys/the placement flags -> CvPlot/CvGame worldgen; seeThroughChange -> vision.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / TerrainTypes / MapCategoryTypes
#include <vector>

class CvJsonFeatureInfo : public CvJsonInfo
{
public:
	CvJsonFeatureInfo();

	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
	int getMovementCost() const { return m_iMovementCost; }
	int getDefenseModifier() const { return m_iDefenseModifier; }
	int getHealthPercent() const { return m_iHealthPercent; }
	int getCultureDistance() const { return m_iCultureDistance; }
	int getSeeThroughChange() const { return m_iSeeThroughChange; }
	int getPopDestroys() const { return m_iPopDestroys; }

	bool isImpassable() const { return m_bImpassable; }
	bool isNoCity() const { return m_bNoCity; }
	bool isNoImprovement() const { return m_bNoImprovement; }
	bool isNoBonus() const { return m_bNoBonus; }
	bool isCountsAsPeak() const { return m_bCountsAsPeak; }
	bool isRequiresFlatlands() const { return m_bRequiresFlatlands; }
	bool isAddsFreshWater() const { return m_bAddsFreshWater; }
	bool isNukeImmune() const { return m_bNukeImmune; }

	// valid terrains (the feature may appear on these) -- getNumVarieties()/isTerrain() consumers
	bool isTerrain(int iTerrain) const;
	const std::vector<TerrainTypes>& getValidTerrains() const { return m_aeValidTerrains; }
	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }
	int getZobristValue() const { return m_iZobristValue; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	int m_aiYieldChange[NUM_YIELD_TYPES];  // food/production/commerce .plot.flat
	int m_iMovementCost;                   // identity.movementCost
	int m_iDefenseModifier;                // defense.plot.amount.percent
	int m_iHealthPercent;                  // health.plot.percent
	int m_iCultureDistance;                // cultureDistance.plot.flat
	int m_iSeeThroughChange;               // vision.plot.seeThrough.flat
	int m_iPopDestroys;                    // identity.popDestroys
	int m_iZobristValue;                   // ⏳ map-hash: needs the exact legacy zobrist computation (OOS)
	bool m_bImpassable, m_bNoCity, m_bNoImprovement, m_bNoBonus, m_bCountsAsPeak;   // identity placement flags
	bool m_bRequiresFlatlands, m_bAddsFreshWater, m_bNukeImmune;                    // identity placement flags
	std::vector<TerrainTypes> m_aeValidTerrains;   // identity.validTerrains (resolved TERRAIN_ ids)
	std::vector<MapCategoryTypes> m_aeMapCategories;
	// ⏳ NOT yet mapped (need their curator JSON source confirmed before adding -- not silently defaulted):
	//    getRiverYieldChange (river-conditioned yield), the growth/appearance/disappearance/spread probabilities.
};

#endif // CV_JSON_FEATURE_INFO_H
