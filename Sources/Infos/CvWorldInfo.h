#pragma once

#ifndef CV_WORLD_INFO_H
#define CV_WORLD_INFO_H

//
//	CvWorldInfo -- the WORLD (map size) poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP;
//	#430 audit item 15). A world size is a PURE CONFIG entity (state-repositories.md § WORLD is CONFIG): it
//	enables nothing, deposits nothing per-turn, and every value is a parameter an engine formula reads off the
//	selected map size -- so it composes NO section units and the whole surface is category 4, INTRINSIC bare
//	typed reads (config; human values, like CvHandicapInfo's intrinsic block). JSON-fed
//	(Assets/Data/worlds/*.json via mapFrom); no XML read (DEC-no-xml-into-game).
//
//	The authored data is the `identity` block (a map size IS its config numbers -- the cultureLevel
//	cityRadius/cultureThreshold precedent, json.md §7); mapFrom materializes it into the typed members below.
//	The getter NAMES are the sanctioned lone intrinsics the consumers already read (the CITY_LIMIT resolver,
//	the maintenance components, the isCoastal callers, the CyInfoInterface3 bindings) -- kept as-is.
//
//	The legacy class's dead weight is DROPPED (zero consumers, curate_world.py docstring): unitNameModifier,
//	numCitiesMaintenancePercent (only consumer commented out, CvCity.cpp), commandersLevelThresholdsPercent,
//	and the XML-only iNumFreeBuildingBonuses (never had a member).
//

#include "CvInfo.h"   // the JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

class CvWorldInfo : public CvInfo
{
public:

	CvWorldInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 4. INTRINSIC -- bare typed reads (config; human values) ==================
	// EXE-bound (DllExport, the staging-screen player-count read) -- kept out-of-line.
	DllExport int getDefaultPlayers() const;
	int getTargetNumCities() const { return m_iTargetNumCities; }
	int getGridWidth() const { return m_iGridWidth; }
	int getGridHeight() const { return m_iGridHeight; }
	int getTerrainGrainChange() const { return m_iTerrainGrainChange; }
	int getFeatureGrainChange() const { return m_iFeatureGrainChange; }
	int getOceanMinAreaSize() const { return m_iOceanMinAreaSize; }
	int getBuildingPrereqModifier() const { return m_iBuildingPrereqModifier; }
	int getMaxConscriptModifier() const { return m_iMaxConscriptModifier; }
	int getWarWearinessModifier() const { return m_iWarWearinessModifier; }
	int getTradeProfitPercent() const { return m_iTradeProfitPercent; }
	int getDistanceMaintenancePercent() const { return m_iDistanceMaintenancePercent; }
	int getColonyMaintenancePercent() const { return m_iColonyMaintenancePercent; }
	int getCorporationMaintenancePercent() const { return m_iCorporationMaintenancePercent; }
	int getNumCitiesAnarchyPercent() const { return m_iNumCitiesAnarchyPercent; }
	int getAdvancedStartPointsMod() const { return m_iAdvancedStartPointsMod; }
	// Scales civic city limits by map size (100 = no change) under GAMEOPTION_EXP_OVEREXPANSION_PENALTIES.
	int getCityLimitsScalePercent() const { return m_iCityLimitsScalePercent; }

private:
	int m_iDefaultPlayers;
	int m_iTargetNumCities;
	int m_iGridWidth;
	int m_iGridHeight;
	int m_iTerrainGrainChange;
	int m_iFeatureGrainChange;
	int m_iOceanMinAreaSize;
	int m_iBuildingPrereqModifier;
	int m_iMaxConscriptModifier;
	int m_iWarWearinessModifier;
	int m_iTradeProfitPercent;
	int m_iDistanceMaintenancePercent;
	int m_iColonyMaintenancePercent;
	int m_iCorporationMaintenancePercent;
	int m_iNumCitiesAnarchyPercent;
	int m_iAdvancedStartPointsMod;
	int m_iCityLimitsScalePercent;
};

#endif // CV_WORLD_INFO_H
