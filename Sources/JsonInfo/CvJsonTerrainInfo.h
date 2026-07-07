#pragma once
#ifndef CV_JSON_TERRAIN_INFO_H
#define CV_JSON_TERRAIN_INFO_H

//
//	CvJsonTerrainInfo -- the JSON-populated real poco for TERRAINS (the CvXInfo replacement the engine reads normally).
//	Carries ONLY the surface LIVE callers read (owner ruling 2026-07-07: "we only care about live callers, and make the
//	infoclasses legible for humans") -- the dead legacy getters are dropped (e.g. getHealthPercent: every call is on a
//	specialist/feature/improvement, never a terrain). mapFrom maps each member straight from its JSON source; values are
//	HUMAN-native (like the XML poco) -- the cascade applies its own ×100 when it reads this object. No cascade here.
//
//	Live callers (verified 2026-07-07): yields -> CvPlot::calculateYield / CvGame worldgen; movementCost ->
//	CvUnitAI/CvWorkerAI plot move cost; buildModifier -> CvPlot::getBuildTime; defenseModifier -> CvPlot combat +
//	CvPlayerAI; cultureDistance -> CvCity::cultureDistance; distanceToLand/isWaterTerrain/isFreshWaterTerrain/climate ->
//	CvPlot relief + climate; mapCategories -> CvGameCoreUtils sharesMapCategory; zobristValue -> CvPlot map hash.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // YieldTypes/NUM_YIELD_TYPES, ClimateZoneTypes, MapCategoryTypes
#include <vector>

class CvJsonTerrainInfo : public CvJsonInfo
{
public:
	CvJsonTerrainInfo();

	int getYield(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYields[i] : 0; }
	int getMovementCost() const { return m_iMovementCost; }
	int getBuildModifier() const { return m_iBuildModifier; }
	int getDefenseModifier() const { return m_iDefenseModifier; }
	int getCultureDistance() const { return m_iCultureDistance; }

	int getDistanceToLand() const { return m_iDistanceToLand; }
	bool isWaterTerrain() const { return m_iDistanceToLand > 0; }
	bool isFreshWaterTerrain() const { return m_bFreshWaterTerrain; }
	ClimateZoneTypes getClimate() const { return m_eClimate; }

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }
	int getZobristValue() const { return m_iZobristValue; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonModifiers m_modifiers;
	int m_aiYields[NUM_YIELD_TYPES];   // food/production/commerce .plot.flat
	int m_iMovementCost;               // identity.movementCost
	int m_iBuildModifier;              // buildTime.plot.percent
	int m_iDefenseModifier;            // defense.plot.amount.percent
	int m_iCultureDistance;            // cultureDistance.plot.flat
	int m_iDistanceToLand;             // identity.distanceToLand (0 = land; 1/2/… = coast/ocean tiers)
	int m_iZobristValue;               // ⏳ map-hash: needs the exact legacy zobrist computation (OOS-load-bearing)
	bool m_bFreshWaterTerrain;         // identity.freshWaterTerrain
	ClimateZoneTypes m_eClimate;       // identity.climate (CLIMATE_ZONE_*)
	std::vector<MapCategoryTypes> m_aeMapCategories;   // identity.mapCategories (MAPCATEGORY_*)
};

#endif // CV_JSON_TERRAIN_INFO_H
