#include "CvGameCoreDLL.h"
#include "CvInfos.h"
#include "CvJsonTraitInfo.h"
#include "CvJsonHeritageInfo.h"
#include "Defines/CvDiplomacyClasses.h"

//
// Python interface for info classes (formerly structs)
// These are simple enough to be exposed directly - no wrappers
//

void CyInfoPythonInterface3()
{
	OutputDebugString("Python Extension Module - CyInfoPythonInterface3\n");

	python::class_<CvYieldInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvYieldInfo", python::no_init)

		.def("getChar", &CvYieldInfo::getChar, "int ()")
		.def("getColorType", &CvYieldInfo::getColorType, "int ()")
	;


	python::class_<CvTerrainInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvTerrainInfo", python::no_init)

		.def("getMovementCost", &CvTerrainInfo::getMovementCost, "int ()")
		.def("getDefenseModifier", &CvTerrainInfo::getDefenseModifier, "int ()")

		.def("isWaterTerrain", &CvTerrainInfo::isWaterTerrain, "bool ()")
		.def("isImpassable", &CvTerrainInfo::isImpassable, "bool ()")
		.def("isFound", &CvTerrainInfo::isFound, "bool ()")

		// Arrays
		.def("getYield", &CvTerrainInfo::getYield, "int (int i)")
	;


	python::class_<CvInterfaceModeInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvInterfaceModeInfo", python::no_init)

		.def("getMissionType", &CvInterfaceModeInfo::getMissionType, "int ()")

		.def("getVisible", &CvInterfaceModeInfo::getVisible, "bool ()")
		.def("getGotoPlot", &CvInterfaceModeInfo::getGotoPlot, "bool ()")
		.def("getHighlightPlot", &CvInterfaceModeInfo::getHighlightPlot, "bool ()")
	;


	python::class_<CvLeaderHeadInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvLeaderHeadInfo", python::no_init)

		.def("isNPC", &CvLeaderHeadInfo::isNPC, "bool ()")
		.def("getWonderConstructRand", &CvLeaderHeadInfo::getWonderConstructRand, "int ()")
		.def("getBaseAttitude", &CvLeaderHeadInfo::getBaseAttitude, "int ()")
		.def("getWarmongerRespect", &CvLeaderHeadInfo::getWarmongerRespect, "int ()")
		.def("getMaxWarRand", &CvLeaderHeadInfo::getMaxWarRand, "int ()")
		.def("getMaxWarNearbyPowerRatio", &CvLeaderHeadInfo::getMaxWarNearbyPowerRatio, "int ()")
		.def("getMaxWarDistantPowerRatio", &CvLeaderHeadInfo::getMaxWarDistantPowerRatio, "int ()")
		.def("getMaxWarMinAdjacentLandPercent", &CvLeaderHeadInfo::getMaxWarMinAdjacentLandPercent, "int ()")
		.def("getLimitedWarRand", &CvLeaderHeadInfo::getLimitedWarRand, "int ()")
		.def("getLimitedWarPowerRatio", &CvLeaderHeadInfo::getLimitedWarPowerRatio, "int ()")
		.def("getDogpileWarRand", &CvLeaderHeadInfo::getDogpileWarRand, "int ()")
		.def("getRazeCityProb", &CvLeaderHeadInfo::getRazeCityProb, "int ()")
		.def("getFavoriteCivic", &CvLeaderHeadInfo::getFavoriteCivic, "int ()")
		.def("getFavoriteReligion", &CvLeaderHeadInfo::getFavoriteReligion, "int ()")
		.def("getArtDefineTag", &CvLeaderHeadInfo::getArtDefineTag, "string ()")

		// Arrays
		.def("hasTrait", &CvLeaderHeadInfo::hasTrait, "bool (int i)")
		.def("getFlavorValue", &CvLeaderHeadInfo::getFlavorValue, "int (int i)")
		.def("getDiploPeaceMusicScriptIds", &CvLeaderHeadInfo::getDiploPeaceMusicScriptIds, "int (int i)")

		// Other
		.def("getLeaderHead", &CvLeaderHeadInfo::getLeaderHead, "string ()")
		.def("getButton", &CvLeaderHeadInfo::getButton, "string ()")
	;

	// CvProcessInfos
	python::class_<CvJsonProcessInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvProcessInfo", python::no_init)

		.def("getTechPrereq", &CvJsonProcessInfo::getTechPrereq, "int ()")
		// Arrays
		.def("getProductionToCommerceModifier", &CvJsonProcessInfo::getProductionToCommerceModifier, "int (int i)")
	;


	python::class_<CvVoteInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvVoteInfo", python::no_init)

		.def("getTradeRoutes", &CvVoteInfo::getTradeRoutes, "int ()")

		.def("isSecretaryGeneral", &CvVoteInfo::isSecretaryGeneral, "bool ()")
		.def("isVictory", &CvVoteInfo::isVictory, "bool ()")
		.def("isDefensivePact", &CvVoteInfo::isDefensivePact, "bool ()")
		.def("isOpenBorders", &CvVoteInfo::isOpenBorders, "bool ()")
		.def("isForcePeace", &CvVoteInfo::isForcePeace, "bool ()")

		// Arrays
		.def("isVoteSourceType", &CvVoteInfo::isVoteSourceType, "bool (int i)")
	;


	python::class_<CvJsonProjectInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvProjectInfo", python::no_init)

		.def("getVictoryPrereq", &CvJsonProjectInfo::getVictoryPrereq, "int ()")
		.def("getTechPrereq", &CvJsonProjectInfo::getTechPrereq, "int ()")
		.def("getMaxGlobalInstances", &CvJsonProjectInfo::getMaxGlobalInstances, "int ()")
		.def("getMaxTeamInstances", &CvJsonProjectInfo::getMaxTeamInstances, "int ()")
		.def("getProductionCost", &CvJsonProjectInfo::getProductionCost, "int ()")
		.def("getNukeInterception", &CvJsonProjectInfo::getNukeInterception, "int ()")

		.def("isSpaceship", &CvJsonProjectInfo::isSpaceship, "bool ()")
		.def("isAllowsNukes", &CvJsonProjectInfo::isAllowsNukes, "bool ()")

		.def("getMovieArtDef", &CvJsonProjectInfo::getMovieArtDef, "string ()")
		.def("getCreateSound", &CvJsonProjectInfo::getCreateSound, "string ()")

		// Arrays
		.def("getBonusProductionModifier", &CvJsonProjectInfo::getBonusProductionModifier, "int (int i)")
		.def("getVictoryThreshold", &CvJsonProjectInfo::getVictoryThreshold, "int (int i)")
		.def("getVictoryMinThreshold", &CvJsonProjectInfo::getVictoryMinThreshold, "int (int i)")
		.def("getVictoryDelayPercent", &CvJsonProjectInfo::getVictoryDelayPercent, "int ()")
		.def("getProjectsNeeded", &CvJsonProjectInfo::getProjectsNeeded, "int (int i)")
	;


	python::class_<CvJsonReligionInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvReligionInfo", python::no_init)

		.def("getChar", &CvJsonReligionInfo::getChar, "int ()")
		.def("getHolyCityChar", &CvJsonReligionInfo::getHolyCityChar, "int ()")
		.def("getTechPrereq", &CvJsonReligionInfo::getTechPrereq, "int ()")
		.def("getMissionType", &CvJsonReligionInfo::getMissionType, "int ()")

		.def("getTechButton", &CvJsonReligionInfo::getTechButton, "string ()")
		.def("getGenericTechButton", &CvJsonReligionInfo::getGenericTechButton, "string ()")
		.def("getMovieFile", &CvJsonReligionInfo::getMovieFile, "string ()")
		.def("getMovieSound", &CvJsonReligionInfo::getMovieSound, "string ()")
		.def("getSound", &CvJsonReligionInfo::getSound, "string ()")
		.def("getButtonDisabled", &CvJsonReligionInfo::getButtonDisabled, "string ()")
		.def("getAdjectiveKey", &CvJsonReligionInfo::pyGetAdjectiveKey, "wstring ()")
		// Arrays
		.def("getGlobalReligionCommerce", &CvJsonReligionInfo::getGlobalReligionCommerce, "int (int i)")
		.def("getFlavorValue", &CvJsonReligionInfo::getFlavorValue, "int (int i)")
	;

	python::class_<CvJsonHeritageInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvHeritageInfo", python::no_init)
	;

	python::class_<CvJsonCorporationInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvCorporationInfo", python::no_init)

		.def("getChar", &CvJsonCorporationInfo::getChar, "int ()")
		.def("getHeadquarterChar", &CvJsonCorporationInfo::getHeadquarterChar, "int ()")
		.def("getTechPrereq", &CvJsonCorporationInfo::getTechPrereq, "int ()")
		.def("getObsoleteTech", &CvJsonCorporationInfo::getObsoleteTech, "int ()")

		.def("getMaintenance", &CvJsonCorporationInfo::getMaintenance, "int ()")
		.def("getMissionType", &CvJsonCorporationInfo::getMissionType, "int ()")

		.def("getMovieFile", &CvJsonCorporationInfo::getMovieFile, "string ()")
		.def("getMovieSound", &CvJsonCorporationInfo::getMovieSound, "string ()")
		.def("getSound", &CvJsonCorporationInfo::getSound, "string ()")

		// Arrays
		.def("getPrereqBonuses", &CvJsonCorporationInfo::getPrereqBonuses, python::return_value_policy<python::reference_existing_object>())
		.def("getCommerceProduced", &CvJsonCorporationInfo::getCommerceProduced, "int (int i)")
		.def("getYieldProduced", &CvJsonCorporationInfo::getYieldProduced, "int (int i)")
	;


	python::class_<CvJsonTraitInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvTraitInfo", python::no_init)

		.def("getHealth", &CvJsonTraitInfo::getHealth, "int ()")
		.def("getHappiness", &CvJsonTraitInfo::getHappiness, "int ()")
		.def("getLevelExperienceModifier", &CvJsonTraitInfo::getLevelExperienceModifier, "int ()")
		.def("getGreatPeopleRateModifier", &CvJsonTraitInfo::getGreatPeopleRateModifier, "int ()")
		.def("getGreatGeneralRateModifier", &CvJsonTraitInfo::getGreatGeneralRateModifier, "int ()")
		.def("getNumBuildingProductionModifiers", &CvJsonTraitInfo::getNumBuildingProductionModifiers, "int ()")

		.def("getRevIdxLocal", &CvJsonTraitInfo::getRevIdxLocal, "int ()")
		.def("getRevIdxNational", &CvJsonTraitInfo::getRevIdxNational, "int ()")
		.def("getRevIdxHolyCityGood", &CvJsonTraitInfo::getRevIdxHolyCityGood, "int ()")
		.def("getRevIdxHolyCityBad", &CvJsonTraitInfo::getRevIdxHolyCityBad, "int ()")
		.def("getRevIdxNationalityMod", &CvJsonTraitInfo::getRevIdxNationalityMod, "float ()")
		.def("getRevIdxBadReligionMod", &CvJsonTraitInfo::getRevIdxBadReligionMod, "float ()")
		.def("getRevIdxGoodReligionMod", &CvJsonTraitInfo::getRevIdxGoodReligionMod, "float ()")

		.def("getShortDescription", &CvJsonTraitInfo::getShortDescription, "int (int i)")
		.def("getCommerceChange", &CvJsonTraitInfo::getCommerceChange, "int (int i)")
		.def("getCommerceModifier", &CvJsonTraitInfo::getCommerceModifier, "int (int i)")
		.def("getBuildingProductionModifier", &CvJsonTraitInfo::getBuildingProductionModifier)
		.def("getBuildingHappinessModifiers", &CvJsonTraitInfo::getBuildingHappinessModifiers, python::return_value_policy<python::reference_existing_object>())

		.def("getImprovementYieldChange", &CvJsonTraitInfo::getImprovementYieldChange, "int (int i, int j)")
	;

	python::class_<CvWorldInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvWorldInfo", python::no_init)

		.def("getDefaultPlayers", &CvWorldInfo::getDefaultPlayers, "int ()")
		.def("getTargetNumCities", &CvWorldInfo::getTargetNumCities, "int ()")
		.def("getBuildingPrereqModifier", &CvWorldInfo::getBuildingPrereqModifier, "int ()")
		.def("getWarWearinessModifier", &CvWorldInfo::getWarWearinessModifier, "int ()")
		.def("getGridWidth", &CvWorldInfo::getGridWidth, "int ()")
		.def("getGridHeight", &CvWorldInfo::getGridHeight, "int ()")
		.def("getTerrainGrainChange", &CvWorldInfo::getTerrainGrainChange, "int ()")
		.def("getFeatureGrainChange", &CvWorldInfo::getFeatureGrainChange, "int ()")
		.def("getCorporationMaintenancePercent", &CvWorldInfo::getCorporationMaintenancePercent, "int ()")
		.def("getOceanMinAreaSize", &CvWorldInfo::getOceanMinAreaSize, "int ()")
	;

	python::class_<CvMapInfo, python::bases<CvHotkeyInfo>, boost::noncopyable>("CvMapInfo", python::no_init)

		.def("getGridWidth", &CvMapInfo::getGridWidth, "int ()")
		.def("getGridHeight", &CvMapInfo::getGridHeight, "int ()")
		.def("getWrapX", &CvMapInfo::getWrapX, "int ()")
		.def("getWrapY", &CvMapInfo::getWrapY, "int ()")
		//.def("getMapScript", &CvMapInfo::getMapScript, "string ()")
	;

	python::class_<CvClimateInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvClimateInfo", python::no_init)

		.def("getDesertPercentChange", &CvClimateInfo::getDesertPercentChange, "int ()")
		.def("getJungleLatitude", &CvClimateInfo::getJungleLatitude, "int ()")
		.def("getHillRange", &CvClimateInfo::getHillRange, "int ()")
		.def("getPeakPercent", &CvClimateInfo::getPeakPercent, "int ()")

		.def("getSnowLatitudeChange", &CvClimateInfo::getSnowLatitudeChange, "float ()")
		.def("getTundraLatitudeChange", &CvClimateInfo::getTundraLatitudeChange, "float ()")
		.def("getGrassLatitudeChange", &CvClimateInfo::getGrassLatitudeChange, "float ()")
		.def("getDesertBottomLatitudeChange", &CvClimateInfo::getDesertBottomLatitudeChange, "float ()")
		.def("getDesertTopLatitudeChange", &CvClimateInfo::getDesertTopLatitudeChange, "float ()")
		.def("getRandIceLatitude", &CvClimateInfo::getRandIceLatitude, "float ()")
	;

	python::class_<CvSeaLevelInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvSeaLevelInfo", python::no_init)
		.def("getSeaLevelChange", &CvSeaLevelInfo::getSeaLevelChange, "int ()")
	;

	python::class_<CvAssetInfoBase>("CvAssetInfoBase", python::no_init)
		.def("setPath", &CvAssetInfoBase::setPath, "void (string)")
		.def("getPath", &CvAssetInfoBase::getPath, "string ()")
	;

	python::class_<CvArtInfoAsset, python::bases<CvAssetInfoBase> >("CvArtInfoAsset", python::no_init)
		.def("getButton", &CvArtInfoAsset::getButton, "string ()")
	;

	python::class_<CvArtInfoScalableAsset, python::bases<CvArtInfoAsset, CvScalableInfo>, boost::noncopyable>("CvArtInfoScalableAsset", python::no_init);


	python::class_<CvArtInfoInterface, python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoInterface", python::no_init);


	python::class_<CvArtInfoMovie, python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoMovie", python::no_init);


	python::class_<CvArtInfoMisc, python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoMisc", python::no_init);


	python::class_<CvArtInfoUnit, python::bases<CvArtInfoScalableAsset>, boost::noncopyable>("CvArtInfoUnit", python::no_init)
		.def("getTrainSound", &CvArtInfoUnit::getTrainSound, "string ()")
	;

	python::class_<CvArtInfoBuilding, python::bases<CvArtInfoScalableAsset>, boost::noncopyable>("CvArtInfoBuilding", python::no_init)
	;

	python::class_<CvArtInfoCivilization, python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoCivilization", python::no_init)
		.def("isWhiteFlag", &CvArtInfoCivilization::isWhiteFlag, "bool ()")
	;

	python::class_<CvArtInfoLeaderhead, python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoLeaderhead", python::no_init);


	python::class_<CvArtInfoBonus, python::bases<CvArtInfoScalableAsset>, boost::noncopyable>("CvArtInfoBonus", python::no_init);


	python::class_<CvArtInfoImprovement, python::bases<CvArtInfoScalableAsset>, boost::noncopyable>("CvArtInfoImprovement", python::no_init)
	;

	python::class_<CvArtInfoTerrain, python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoTerrain", python::no_init);


	python::class_<CvArtInfoFeature, python::bases<CvArtInfoScalableAsset>, boost::noncopyable>("CvArtInfoFeature", python::no_init)
	;

	python::class_<CvEmphasizeInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvEmphasizeInfo", python::no_init)
		.def("getYieldChange", &CvEmphasizeInfo::getYieldChange, "int (int i)")
		.def("getCommerceChange", &CvEmphasizeInfo::getCommerceChange, "int (int i)")
	;

	python::class_<CvUpkeepInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvUpkeepInfo", python::no_init)
	;

	python::class_<CvJsonCultureLevelInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvCultureLevelInfo", python::no_init)
		.def("getSpeedThreshold", &CvJsonCultureLevelInfo::getSpeedThreshold, "int ()")
		.def("getLevel", &CvJsonCultureLevelInfo::getLevel, "int ()")
	;

	python::class_<CvEraInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvEraInfo", python::no_init)

		.def("getStartingUnitMultiplier", &CvEraInfo::getStartingUnitMultiplier, "int () -")
		.def("getStartingDefenseUnits", &CvEraInfo::getStartingDefenseUnits, "int () -")
		.def("getStartingWorkerUnits", &CvEraInfo::getStartingWorkerUnits, "int () -")
		.def("getStartingExploreUnits", &CvEraInfo::getStartingExploreUnits, "int () -")
		.def("getStartingGold", &CvEraInfo::getStartingGold, "int () -")
		.def("getFreePopulation", &CvEraInfo::getFreePopulation, "int () -")
		.def("getHistoricalStartYear", &CvEraInfo::getHistoricalStartYear, "int () -")
		.def("getHistoricalEndYear", &CvEraInfo::getHistoricalEndYear, "int () -")
		.def("getNormalSpeedTurns", &CvEraInfo::getNormalSpeedTurns, "int () -")
		.def("getGrowthPercent", &CvEraInfo::getGrowthPercent, "int () -")
		.def("getTrainPercent", &CvEraInfo::getTrainPercent, "int () -")
		.def("getConstructPercent", &CvEraInfo::getConstructPercent, "int () -")
		.def("getCreatePercent", &CvEraInfo::getCreatePercent, "int () -")
		.def("getResearchPercent", &CvEraInfo::getResearchPercent, "int () -")
		.def("getBuildPercent", &CvEraInfo::getBuildPercent, "int () -")
		.def("getImprovementPercent", &CvEraInfo::getImprovementPercent, "int () -")
		.def("getGreatPeoplePercent", &CvEraInfo::getGreatPeoplePercent, "int () -")
		.def("getAnarchyPercent", &CvEraInfo::getAnarchyPercent, "int () -")
		.def("getEventChancePerTurn", &CvEraInfo::getEventChancePerTurn, "int () -")
		.def("getNumSoundtracks", &CvEraInfo::getNumSoundtracks, "int () -")

		.def("isNoGoodies", &CvEraInfo::isNoGoodies, "bool () -")
		.def("isNoAnimals", &CvEraInfo::isNoAnimals, "bool () -")
		.def("isNoBarbUnits", &CvEraInfo::isNoBarbUnits, "bool () -")
		.def("isNoBarbCities", &CvEraInfo::isNoBarbCities, "bool () -")
	;


	python::class_<CvColorInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvColorInfo", python::no_init)

		.def("getColor", &CvColorInfo::getColor, python::return_value_policy<python::reference_existing_object>())
	;

	python::class_<CvAdvisorInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvAdvisorInfo", python::no_init);


	python::class_<CvPlayerColorInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvPlayerColorInfo", python::no_init)

		.def("getColorTypePrimary", &CvPlayerColorInfo::getColorTypePrimary, "int ()")
	;


	python::class_<CvGameText, python::bases<CvInfoBase>, boost::noncopyable>("CvGameText", python::no_init)
		.def("getText", &CvGameText::pyGetText, "wstring ()")
		.def("getNumLanguages", &CvGameText::getNumLanguages, "int ()")
		.staticmethod("getNumLanguages")
	;


	python::class_<CvDiplomacyTextInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvDiplomacyTextInfo", python::no_init)

		.def("getNumResponses", &CvDiplomacyTextInfo::getNumResponses, "int ()")

		.def("getCivilizationTypes", &CvDiplomacyTextInfo::getCivilizationTypes, "bool (int i, int j)")
		.def("getLeaderHeadTypes", &CvDiplomacyTextInfo::getLeaderHeadTypes, "bool (int i, int j)")
		.def("getAttitudeTypes", &CvDiplomacyTextInfo::getAttitudeTypes, "bool (int i, int j)")
		.def("getDiplomacyPowerTypes", &CvDiplomacyTextInfo::getDiplomacyPowerTypes, "bool (int i, int j)")

		.def("getNumDiplomacyText", &CvDiplomacyTextInfo::getNumDiplomacyText, "int (int i)")
		.def("getDiplomacyText", &CvDiplomacyTextInfo::getDiplomacyText, "string (int i, int j)")
	;


	python::class_<CvDiplomacyInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvDiplomacyInfo", python::no_init)

		.def("getNumResponses", &CvDiplomacyInfo::getNumResponses, "int ()")

		.def("getCivilizationTypes", &CvDiplomacyInfo::getCivilizationTypes, "bool (int i, int j)")
		.def("getLeaderHeadTypes", &CvDiplomacyInfo::getLeaderHeadTypes, "bool (int i, int j)")
		.def("getAttitudeTypes", &CvDiplomacyInfo::getAttitudeTypes, "bool (int i, int j)")
		.def("getDiplomacyPowerTypes", &CvDiplomacyInfo::getDiplomacyPowerTypes, "bool (int i, int j)")

		.def("getNumDiplomacyText", &CvDiplomacyInfo::getNumDiplomacyText, "int (int i)")
		.def("getDiplomacyText", &CvDiplomacyInfo::getDiplomacyText, "string (int i, int j)")
	;


	python::class_<CvEffectInfo, python::bases<CvInfoBase, CvScalableInfo>, boost::noncopyable>("CvEffectInfo", python::no_init)

		.def("getPath", &CvEffectInfo::getPath, "string ()")
		.def("setPath", &CvEffectInfo::setPath, "void (string)")
	;


	python::class_<CvControlInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvControlInfo", python::no_init)

		.def("getActionInfoIndex", &CvControlInfo::getActionInfoIndex, "int ()")
	;
}
