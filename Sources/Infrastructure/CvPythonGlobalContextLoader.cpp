#include "CvGameCoreDLL.h"
#include "CvPythonGlobalContextLoader.h"
#include "CvGameCoreDLL.h"
#include "Python/CyGlobalContext.h"
#include "CvBonusInfo.h"
#include "CvBuildingInfo.h"
#include "CvImprovementInfo.h"
#include "CvHeritageInfo.h"
#include "CvInfos.h"
#include "Tools/CvRandom.h"
#include "Python/CyGame.h"
#include "Python/CyGlobalContext.h"
#include "Python/CyMap.h"
#include "Python/CyPlayer.h"
#include "Python/CyTeam.h"
#include "Defines/CvDiplomacyClasses.h"
#include "CvPlayerOptionInfo.h"
#include "CvTraitInfo.h"
#include <boost/python/overloads.hpp>


BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(CyGlobalContext_getInfoTypeForString_overloads, CyGlobalContext::getInfoTypeForString, 1, 2)

void CvPythonGlobalContextLoader::CyGlobalContextPythonInterface1(boost::python::class_<CyGlobalContext>& inst)
{
	OutputDebugString("Python Extension Module - CyGlobalContextPythonInterface1\n");
	inst
		.def("isDebugBuild", &CyGlobalContext::isDebugBuild, "() - returns true if running a debug build")
		.def("getGame", &CyGlobalContext::getCyGame, boost::python::return_value_policy<boost::python::reference_existing_object>(), "() - CyGame()")
		.def("getMap", &CyGlobalContext::getCyMap, boost::python::return_value_policy<boost::python::reference_existing_object>(), "() - CyMap()")
		.def("getMaps", &CyGlobalContext::getMaps)
		.def("getNumMapsInitialized", &CyGlobalContext::getNumMapsInitialized)
		.def("getPlayer", &CyGlobalContext::getCyPlayer, boost::python::return_value_policy<boost::python::reference_existing_object>(), "(iPlayer) - iPlayer instance")
		.def("getActivePlayer", &CyGlobalContext::getCyActivePlayer, boost::python::return_value_policy<boost::python::reference_existing_object>(), "() - active player instance")
		.def("getASyncRand", &CyGlobalContext::getCyASyncRand, boost::python::return_value_policy<boost::python::reference_existing_object>(), "Non-Synch'd random #")
		.def("getTeam", &CyGlobalContext::getCyTeam, boost::python::return_value_policy<boost::python::reference_existing_object>(), "(iTeam) - iTeam instance")

		// infos
		.def("getNumEffectInfos", &CyGlobalContext::getNumEffectInfos, "int () - Number of effect infos")

		.def("getNumTerrainInfos", &CyGlobalContext::getNumTerrainInfos, "() - Total Terrain Infos XML\\Terrain\\CIV4TerrainInfos.xml")


		.def("getNumBonusInfos", &CyGlobalContext::getNumBonusInfos, "() - Total Bonus Infos XML\\Terrain\\CIV4BonusInfos.xml")

		.def("getNumMapBonuses", &CyGlobalContext::getNumMapBonuses, "() - Total map Bonuses")
		.def("getMapBonus", &CyGlobalContext::getMapBonus, "(mapBonusIndex) - BonusType for mapBonusIndex")

		.def("getNumFeatureInfos", &CyGlobalContext::getNumFeatureInfos, "() - Total Feature Infos XML\\Terrain\\CIV4FeatureInfos.xml")

		.def("getNumUpkeepInfos", &CyGlobalContext::getNumUpkeepInfos, "int () - Number of upkeep infos")

		.def("getNumCultureLevelInfos", &CyGlobalContext::getNumCultureLevelInfos, "int () - Number of culture level infos")

		.def("getNumEraInfos", &CyGlobalContext::getNumEraInfos, "int () - Number of era infos")

		.def("getNumWorldInfos", &CyGlobalContext::getNumWorldInfos, "int () - Number of world infos")

		.def("getNumClimateInfos", &CyGlobalContext::getNumClimateInfos, "int () - Number of climate infos")

		.def("getNumSeaLevelInfos", &CyGlobalContext::getNumSeaLevelInfos, "int () - Number of seal level infos")

		.def("getNumPlayableCivilizationInfos", &CyGlobalContext::getNumPlayableCivilizationInfos, "() - Total # of Playable Civs")
		.def("getNumCivilizationInfos", &CyGlobalContext::getNumCivilizatonInfos, "() - Total Civilization Infos XML\\Civilizations\\CIV4CivilizationInfos.xml")

		.def("getNumLeaderHeadInfos", &CyGlobalContext::getNumLeaderHeadInfos, "() - Total LeaderHead Infos XML\\Civilizations\\CIV4LeaderHeadInfos.xml")

		.def("getNumTraitInfos", &CyGlobalContext::getNumTraitInfos, "() - Total Civilization Infos XML\\Civilizations\\CIV4TraitInfos.xml")

		.def("getNumUnitInfos", &CyGlobalContext::getNumUnitInfos, "() - Total Unit Infos XML\\Units\\CIV4UnitInfos.xml")

		.def("getNumSpecialUnitInfos", &CyGlobalContext::getNumSpecialUnitInfos, "() - Total SpecialUnit Infos XML\\Units\\CIV4SpecialUnitInfos.xml")



		.def("getNumRouteInfos", &CyGlobalContext::getNumRouteInfos, "() - Total Route Infos XML\\Misc\\CIV4RouteInfos.xml")

		.def("getNumImprovementInfos", &CyGlobalContext::getNumImprovementInfos, "() - Total Improvement Infos XML\\Terrain\\CIV4ImprovementInfos.xml")

		.def("getNumGoodyInfos", &CyGlobalContext::getNumGoodyInfos, "() - Total Goody Infos XML\\GameInfo\\CIV4GoodyInfos.xml")

		.def("getNumBuildInfos", &CyGlobalContext::getNumBuildInfos, "() - Total Build Infos XML\\Units\\CIV4BuildInfos.xml")

		.def("getNumHandicapInfos", &CyGlobalContext::getNumHandicapInfos, "() - Total Handicap Infos XML\\GameInfo\\CIV4HandicapInfos.xml")

		.def("getNumGameSpeedInfos", &CyGlobalContext::getNumGameSpeedInfos, "() - Total Game speed Infos XML\\GameInfo\\CIV4GameSpeedInfo.xml")

		.def("getNumBuildingInfos", &CyGlobalContext::getNumBuildingInfos, "() - Total Building Infos XML\\Buildings\\CIV4BuildingInfos.xml")

		.def("getNumUnitCombatInfos", &CyGlobalContext::getNumUnitCombatInfos, "() - Total Unit Combat Infos XML\\Units\\CIV4UnitCombatInfos.xml")
		.def("getUnitCombatInfo", &CyGlobalContext::getUnitCombatInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "(UnitCombatID) - CvInfo for UnitCombatID")

		.def("getDomainInfo", &CyGlobalContext::getDomainInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "(DomainID) - CvInfo for DomainID")

		.def("getNumActionInfos", &CyGlobalContext::getNumActionInfos, "() - Total Action Infos XML\\Units\\CIV4ActionInfos.xml")

		.def("getNumPromotionLineInfos", &CyGlobalContext::getNumPromotionLineInfos, "() - Total Promotion Line Infos XML\\Units\\CIV4PromotionLineInfos.xml")





		.def("setIsBug", &CyGlobalContext::setIsBug, "void () - init BUG on dll side")
		.def("refreshOptionsBUG", &CyGlobalContext::refreshOptionsBUG, "void () - refresh some key BUG options")
		;
}
void CvPythonGlobalContextLoader::CyGlobalContextPythonInterface2(boost::python::class_<CyGlobalContext>& inst)
{
	OutputDebugString("Python Extension Module - CyGlobalContextPythonInterface2\n");
	inst
		// global defines.xml
		.def("isDCM_ACTIVE_DEFENSE", &CyGlobalContext::isDCM_ACTIVE_DEFENSE, "bool ()")
		.def("isDCM_FIGHTER_ENGAGE", &CyGlobalContext::isDCM_FIGHTER_ENGAGE, "bool ()")

		.def("isIDW_ENABLED", &CyGlobalContext::isIDW_ENABLED, "bool ()")
		.def("isIDW_EMERGENCY_DRAFT_ENABLED", &CyGlobalContext::isIDW_EMERGENCY_DRAFT_ENABLED, "bool ()")
		.def("isIDW_NO_BARBARIAN_INFLUENCE", &CyGlobalContext::isIDW_NO_BARBARIAN_INFLUENCE, "bool ()")
		.def("isIDW_NO_NAVAL_INFLUENCE", &CyGlobalContext::isIDW_NO_NAVAL_INFLUENCE, "bool ()")
		.def("isIDW_PILLAGE_INFLUENCE_ENABLED", &CyGlobalContext::isIDW_PILLAGE_INFLUENCE_ENABLED, "bool ()")

		.def("isSS_ENABLED", &CyGlobalContext::isSS_ENABLED, "bool ()")
		.def("isSS_BRIBE", &CyGlobalContext::isSS_BRIBE, "bool ()")
		.def("isSS_ASSASSINATE", &CyGlobalContext::isSS_ASSASSINATE, "bool ()")

		.def("getDefineINT", &CyGlobalContext::getDefineINT, "int ( string szName )")
		.def("getDefineFLOAT", &CyGlobalContext::getDefineFLOAT, "float ( string szName )")
		.def("setDefineINT", &CyGlobalContext::setDefineINT, "void ( string szName, int iValue )")
		.def("setDefineFLOAT", &CyGlobalContext::setDefineFLOAT, "void setDefineFLOAT( string szName, float fValue )")
		.def("setNoUpdateDefineFLOAT", &CyGlobalContext::setNoUpdateDefineFLOAT, "void setDefineFLOAT( string szName, float fValue )")

		.def("getMAX_PC_PLAYERS", &CyGlobalContext::getMAX_PC_PLAYERS, "int ()")
		.def("getMAX_PLAYERS", &CyGlobalContext::getMAX_PLAYERS, "int ()")
		.def("getMAX_PC_TEAMS", &CyGlobalContext::getMAX_PC_TEAMS, "int ()")
		.def("getMAX_TEAMS", &CyGlobalContext::getMAX_TEAMS, "int ()")
		.def("getBARBARIAN_PLAYER", &CyGlobalContext::getBARBARIAN_PLAYER, "int ()")
		.def("getNUM_CITY_PLOTS", &CyGlobalContext::getNUM_CITY_PLOTS, "int ()")

#define EXPOSE_FUNC(unused_, VAR) \
	_EXPOSE_FUNC(get##VAR)

#define _EXPOSE_FUNC(name) \
		.def(#name, &CyGlobalContext::name)

		DO_FOR_EACH_EXPOSED_INT_GLOBAL_DEFINE(EXPOSE_FUNC)
		DO_FOR_EACH_EXPOSED_INFO_TYPE(EXPOSE_FUNC)
		;
}
void CvPythonGlobalContextLoader::CyGlobalContextPythonInterface3(boost::python::class_<CyGlobalContext>& inst)
{
	OutputDebugString("Python Extension Module - CyGlobalContextPythonInterface3\n");

	inst
		.def("switchMap", &CyGlobalContext::switchMap, "void (int)")
		.def("getMapByIndex", &CyGlobalContext::getMapByIndex, boost::python::return_value_policy<boost::python::reference_existing_object>(), "CyMap (int)")

		.def("getAttitudeInfo", &CyGlobalContext::getAttitudeInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "AttitudeInfo (int id)")
		.def("getMemoryInfo", &CyGlobalContext::getMemoryInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "MemoryInfo (int id)")



		.def("getNumHurryInfos", &CyGlobalContext::getNumHurryInfos, "() - Total Hurry Infos")

		.def("getNumConceptInfos", &CyGlobalContext::getNumConceptInfos, "int () - NumConceptInfos")
		.def("getConceptInfo", &CyGlobalContext::getConceptInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "Concept Info () - Returns info object")

		.def("getNumNewConceptInfos", &CyGlobalContext::getNumNewConceptInfos, "int () - NumNewConceptInfos")
		.def("getNewConceptInfo", &CyGlobalContext::getNewConceptInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "New Concept Info () - Returns info object")

		.def("getNumCalendarInfos", &CyGlobalContext::getNumCalendarInfos, "int () - Returns NumCalendarInfos")
		.def("getCalendarInfo", &CyGlobalContext::getCalendarInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "CalendarInfo () - Returns Info object")

		.def("getNumGameOptionInfos", &CyGlobalContext::getNumGameOptionInfos, "int () - Returns NumGameOptionInfos")
		.def("getGameOptionInfo", &CyGlobalContext::getGameOptionInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "GameOptionInfo () - Returns Info object")

		.def("getNumMPOptionInfos", &CyGlobalContext::getNumMPOptionInfos, "int () - Returns NumMPOptionInfos")
		.def("getMPOptionInfo", &CyGlobalContext::getMPOptionInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "MPOptionInfo () - Returns Info object")

		.def("getNumForceControlInfos", &CyGlobalContext::getNumForceControlInfos, "int () - Returns NumForceControlInfos")
		.def("getForceControlInfo", &CyGlobalContext::getForceControlInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "ForceControlInfo () - Returns Info object")

		.def("getNumSeasonInfos", &CyGlobalContext::getNumSeasonInfos, "int () - Returns NumSeasonInfos")
		.def("getSeasonInfo", &CyGlobalContext::getSeasonInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "SeasonInfo () - Returns Info object")

		.def("getNumDenialInfos", &CyGlobalContext::getNumDenialInfos, "int () - Returns NumDenialInfos")
		.def("getDenialInfo", &CyGlobalContext::getDenialInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "DenialInfo () - Returns Info object")
		;
}
void CvPythonGlobalContextLoader::CyGlobalContextPythonInterface4(boost::python::class_<CyGlobalContext>& inst)
{
	OutputDebugString("Python Extension Module - CyGlobalContextPythonInterface4\n");
	inst
		.def("getNumMissionInfos", &CyGlobalContext::getNumMissionInfos, "() - Total Mission Infos XML\\Units\\CIV4MissionInfos.xml")

		.def("getNumCommandInfos", &CyGlobalContext::getNumCommandInfos, "() - Total Command Infos XML\\Units\\CIV4CommandInfos.xml")

		.def("getNumControlInfos", &CyGlobalContext::getNumControlInfos, "() - Total Control Infos XML\\Units\\CIV4ControlInfos.xml")

		.def("getNumPromotionInfos", &CyGlobalContext::getNumPromotionInfos, "() - Total Promotion Infos XML\\Units\\CIV4PromotionInfos.xml")

		.def("getNumTechInfos", &CyGlobalContext::getNumTechInfos, "() - Total Technology Infos XML\\Technologies\\CIV4TechInfos.xml")

		.def("getNumSpecialBuildingInfos", &CyGlobalContext::getNumSpecialBuildingInfos, "() - Total Special Building Infos")

		.def("getNumReligionInfos", &CyGlobalContext::getNumReligionInfos, "() - Total Religion Infos XML\\GameInfo\\CIV4ReligionInfos.xml")

		.def("getNumHeritageInfos", &CyGlobalContext::getNumHeritageInfos, "() - Total Heritage Infos XML\\GameInfo\\HeritageInfos.xml")

		.def("getNumCorporationInfos", &CyGlobalContext::getNumCorporationInfos, "() - Total Corporation Infos XML\\GameInfo\\CIV4CorporationInfos.xml")

		.def("getNumVictoryInfos", &CyGlobalContext::getNumVictoryInfos, "() - Total Victory Infos XML\\GameInfo\\CIV4VictoryInfos.xml")

		.def("getNumSpecialistInfos", &CyGlobalContext::getNumSpecialistInfos, "() - Total Specialist Infos XML\\Units\\CIV4SpecialistInfos.xml")

		.def("getNumCivicOptionInfos", &CyGlobalContext::getNumCivicOptionInfos, "() - Total Civic Infos XML\\Misc\\CIV4CivicOptionInfos.xml")

		.def("getNumCivicInfos", &CyGlobalContext::getNumCivicInfos, "() - Total Civic Infos XML\\Misc\\CIV4CivicInfos.xml")

		.def("getNumDiplomacyInfos", &CyGlobalContext::getNumDiplomacyInfos, "() - Total diplomacy Infos XML\\GameInfo\\CIV4DiplomacyInfos.xml")

		.def("getNumProjectInfos", &CyGlobalContext::getNumProjectInfos, "() - Total Project Infos XML\\GameInfo\\CIV4ProjectInfos.xml")

		.def("getNumVoteInfos", &CyGlobalContext::getNumVoteInfos, "() - Total VoteInfos")

		.def("getNumProcessInfos", &CyGlobalContext::getNumProcessInfos, "() - Total ProcessInfos")

		.def("getNumEmphasizeInfos", &CyGlobalContext::getNumEmphasizeInfos, "() - Total EmphasizeInfos")


		.def("getUnitAIInfo", &CyGlobalContext::getUnitAIInfo, boost::python::return_value_policy<boost::python::reference_existing_object>(), "UnitAIInfo (int id)")



		.def("getInfoTypeForString", &CyGlobalContext::getInfoTypeForString, CyGlobalContext_getInfoTypeForString_overloads())

		.def("getNumFlavorTypes", &CyGlobalContext::getNumFlavorTypes)
		.def("getFlavorType", &CyGlobalContext::getFlavorType)
		.def("getFlavorTypes", &CyGlobalContext::getFlavorTypes)

		.def("getNumPlayerColorInfos", &CyGlobalContext::getNumPlayerColorInfos, "int () - Returns number of PlayerColorInfos")

		.def("getNumEventTriggerInfos", &CyGlobalContext::getNumEventTriggerInfos, "int () - Returns number of EventTriggerInfos")

		.def("getNumEventInfos", &CyGlobalContext::getNumEventInfos, "int () - Returns number of EventInfos")

		.def("getNumEspionageMissionInfos", &CyGlobalContext::getNumEspionageMissionInfos, "int () - Returns number of EspionageMissionInfos")

		.def("getNumMainMenus", &CyGlobalContext::getNumMainMenus, "int () - Returns number")
		.def("getMainMenus", &CyGlobalContext::getMainMenus, boost::python::return_value_policy<boost::python::reference_existing_object>(), "MainMenus () - Returns info object")

		.def("getNumVoteSourceInfos", &CyGlobalContext::getNumVoteSourceInfos, "int ()")

		.def("getNumVoteSourceInfos", &CyGlobalContext::getNumVoteSourceInfos, "int ()")

		.def("getArtStyleTypes", &CyGlobalContext::getArtStyleTypes, "string () - Returns enum string")

		.def("getNumPropertyInfos", &CyGlobalContext::getNumPropertyInfos, "int () - Returns number of PropertyInfos")
	;
}
