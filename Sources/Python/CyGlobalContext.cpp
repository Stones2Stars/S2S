//
// Python wrapper class for global vars and fxns
// Author - Mustafa Thamer
//


#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "CvBuildingInfo.h"
#include "CvBonusInfo.h"
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "Defines/CvDiplomacyClasses.h"
#include "CvUnitCombatInfo.h"
#include "CvPlayerOptionInfo.h"
#include "Engine/CvMap.h"
#include "AI/CvPlayerAI.h"
#include "AI/CvTeamAI.h"
#include "CyGame.h"
#include "CyGlobalContext.h"
#include "CyMap.h"
#include "CyPlayer.h"
#include "CyTeam.h"

std::vector<CyPlayer> g_cyPlayers;
std::vector<CyTeam>   g_cyTeams;
std::vector<CyMap>	  g_cyMaps;

void CyGlobalContext::initStatics()
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < MAX_PLAYERS; i++)
		g_cyPlayers.push_back(CyPlayer(&GET_PLAYER((PlayerTypes)i)));

	for (int i = 0; i < MAX_TEAMS; i++)
		g_cyTeams.push_back(CyTeam(&GET_TEAM((TeamTypes)i)));

	for (int i = 0; i < NUM_MAPS; i++)
		g_cyMaps.push_back(CyMap((MapTypes)i));
}

CyGlobalContext& CyGlobalContext::getInstance()
{
	static CyGlobalContext globalContext;
	return globalContext;
}

bool CyGlobalContext::isDebugBuild() const
{
#ifdef _DEBUG
	return true;
#else
	return false;
#endif
}

int CyGlobalContext::getInfoTypeForString(const char* szInfoType, bool bHideAssert) const
{
	return GC.getInfoTypeForString(szInfoType, bHideAssert);
}

int CyGlobalContext::getNumFlavorTypes() const
{
	return GC.getNumFlavorTypes();
}

const char* CyGlobalContext::getFlavorType(FlavorTypes e) const
{
	return GC.getFlavorTypes(e).c_str();
}

const python::list CyGlobalContext::getFlavorTypes() const
{
	PROFILE_EXTRA_FUNC();
	python::list l = python::list();
	const CvString*& flavorTypes = GC.getFlavorTypes();

	for (int i = 0, num = GC.getNumFlavorTypes(); i < num; i++)
	{
		l.append(flavorTypes[i].c_str());
	}
	return l;
}


const CvInfoBase* CyGlobalContext::getUnitCombatInfo(int i) const
{
	return (i>=0 && i<GC.getNumUnitCombatInfos()) ? &GC.getUnitCombatInfo((UnitCombatTypes)i) : NULL;
}

const CvInfoBase* CyGlobalContext::getDomainInfo(int i) const
{
	return (i>=0 && i<NUM_DOMAIN_TYPES) ? &GC.getDomainInfo((DomainTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getUnitAIInfo(int i) const
{
	return (i>=0 && i<NUM_UNITAI_TYPES) ? &GC.getUnitAIInfo((UnitAITypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getAttitudeInfo(int i) const
{
	return (i>=0 && i<NUM_ATTITUDE_TYPES) ? &GC.getAttitudeInfo((AttitudeTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getMemoryInfo(int i) const
{
	return (i>=0 && i<NUM_MEMORY_TYPES) ? &GC.getMemoryInfo((MemoryTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getConceptInfo(int i) const
{
	return (i>=0 && i<GC.getNumConceptInfos()) ? &GC.getConceptInfo((ConceptTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getNewConceptInfo(int i) const
{
	return (i>=0 && i<GC.getNumNewConceptInfos()) ? &GC.getNewConceptInfo((NewConceptTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getCalendarInfo(int i) const
{
	return (i>=0 && i<GC.getNumCalendarInfos()) ? &GC.getCalendarInfo((CalendarTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getGameOptionInfo(int i) const
{
	return (i>=0 && i<GC.getNumGameOptionInfos()) ? &GC.getGameOptionInfo((GameOptionTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getMPOptionInfo(int i) const
{
	return (i>=0 && i<GC.getNumMPOptionInfos()) ? &GC.getMPOptionInfo((MultiplayerOptionTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getForceControlInfo(int i) const
{
	return (i>=0 && i<GC.getNumForceControlInfos()) ? &GC.getForceControlInfo((ForceControlTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getSeasonInfo(int i) const
{
	return (i>=0 && i<GC.getNumSeasonInfos()) ? &GC.getSeasonInfo((SeasonTypes)i) : NULL;
}


const CvInfoBase* CyGlobalContext::getDenialInfo(int i) const
{
	return (i>=0 && i<GC.getNumDenialInfos()) ? &GC.getDenialInfo((DenialTypes)i) : NULL;
}

// The publication: the CONFIG half only. Every info accessor and every Cy* handle is deliberately absent --
// a script asking for entity data gets an AttributeError here and goes to the library, which is exactly the
// point of keeping the two apart ([DEC-cy-not-fixed]). The DEFINES are served BY NAME, so a new define is
// data rather than a new method.
void CyGlobalContext::pythonPublish()
{
#define PUBLISH_CY_GET_METHOD(dataType, VAR) .def("get"#VAR, &CyGlobalContext::get##VAR)

	python::class_<CyGlobalContext>("CyGlobalContext")
		.def("isDebugBuild", &CyGlobalContext::isDebugBuild)
		.def("getInfoTypeForString", &CyGlobalContext::getInfoTypeForString)
		.def("getNumFlavorTypes", &CyGlobalContext::getNumFlavorTypes)
		.def("getFlavorType", &CyGlobalContext::getFlavorType)
		.def("getFlavorTypes", &CyGlobalContext::getFlavorTypes)
		.def("getArtStyleTypes", &CyGlobalContext::getArtStyleTypes)
		.def("getMapBonus", &CyGlobalContext::getMapBonus)
		.def("getNumMapBonuses", &CyGlobalContext::getNumMapBonuses)
		.def("getDefineINT", &CyGlobalContext::getDefineINT)
		.def("getDefineFLOAT", &CyGlobalContext::getDefineFLOAT)
		.def("setDefineINT", &CyGlobalContext::setDefineINT)
		.def("setDefineFLOAT", &CyGlobalContext::setDefineFLOAT)
		.def("setNoUpdateDefineFLOAT", &CyGlobalContext::setNoUpdateDefineFLOAT)
		.def("setIsBug", &CyGlobalContext::setIsBug)
		.def("refreshOptionsBUG", &CyGlobalContext::refreshOptionsBUG)
		.def("getMAX_PC_PLAYERS", &CyGlobalContext::getMAX_PC_PLAYERS)
		.def("getMAX_PLAYERS", &CyGlobalContext::getMAX_PLAYERS)
		.def("getMAX_PC_TEAMS", &CyGlobalContext::getMAX_PC_TEAMS)
		.def("getMAX_TEAMS", &CyGlobalContext::getMAX_TEAMS)
		.def("getBARBARIAN_PLAYER", &CyGlobalContext::getBARBARIAN_PLAYER)
		.def("getBARBARIAN_TEAM", &CyGlobalContext::getBARBARIAN_TEAM)
		.def("getNUM_CITY_PLOTS", &CyGlobalContext::getNUM_CITY_PLOTS)
		.def("isDCM_ACTIVE_DEFENSE", &CyGlobalContext::isDCM_ACTIVE_DEFENSE)
		.def("isDCM_FIGHTER_ENGAGE", &CyGlobalContext::isDCM_FIGHTER_ENGAGE)
		.def("isIDW_ENABLED", &CyGlobalContext::isIDW_ENABLED)
		.def("isIDW_EMERGENCY_DRAFT_ENABLED", &CyGlobalContext::isIDW_EMERGENCY_DRAFT_ENABLED)
		.def("isIDW_NO_BARBARIAN_INFLUENCE", &CyGlobalContext::isIDW_NO_BARBARIAN_INFLUENCE)
		.def("isIDW_NO_NAVAL_INFLUENCE", &CyGlobalContext::isIDW_NO_NAVAL_INFLUENCE)
		.def("isIDW_PILLAGE_INFLUENCE_ENABLED", &CyGlobalContext::isIDW_PILLAGE_INFLUENCE_ENABLED)
		.def("isSS_ENABLED", &CyGlobalContext::isSS_ENABLED)
		.def("isSS_BRIBE", &CyGlobalContext::isSS_BRIBE)
		.def("isSS_ASSASSINATE", &CyGlobalContext::isSS_ASSASSINATE)
		// the registry COUNTS -- hand-written in the header, so the macros below do NOT cover them
		.def("getNumActionInfos", &CyGlobalContext::getNumActionInfos)
		.def("getNumBonusInfos", &CyGlobalContext::getNumBonusInfos)
		.def("getNumBuildInfos", &CyGlobalContext::getNumBuildInfos)
		.def("getNumBuildingInfos", &CyGlobalContext::getNumBuildingInfos)
		.def("getNumCalendarInfos", &CyGlobalContext::getNumCalendarInfos)
		.def("getNumCivicInfos", &CyGlobalContext::getNumCivicInfos)
		.def("getNumCivicOptionInfos", &CyGlobalContext::getNumCivicOptionInfos)
		.def("getNumCivilizatonInfos", &CyGlobalContext::getNumCivilizatonInfos)
		.def("getNumClimateInfos", &CyGlobalContext::getNumClimateInfos)
		.def("getNumCommandInfos", &CyGlobalContext::getNumCommandInfos)
		.def("getNumConceptInfos", &CyGlobalContext::getNumConceptInfos)
		.def("getNumControlInfos", &CyGlobalContext::getNumControlInfos)
		.def("getNumCorporationInfos", &CyGlobalContext::getNumCorporationInfos)
		.def("getNumCultureLevelInfos", &CyGlobalContext::getNumCultureLevelInfos)
		.def("getNumDenialInfos", &CyGlobalContext::getNumDenialInfos)
		.def("getNumDiplomacyInfos", &CyGlobalContext::getNumDiplomacyInfos)
		.def("getNumEffectInfos", &CyGlobalContext::getNumEffectInfos)
		.def("getNumEmphasizeInfos", &CyGlobalContext::getNumEmphasizeInfos)
		.def("getNumEraInfos", &CyGlobalContext::getNumEraInfos)
		.def("getNumEspionageMissionInfos", &CyGlobalContext::getNumEspionageMissionInfos)
		.def("getNumEventInfos", &CyGlobalContext::getNumEventInfos)
		.def("getNumEventTriggerInfos", &CyGlobalContext::getNumEventTriggerInfos)
		.def("getNumFeatureInfos", &CyGlobalContext::getNumFeatureInfos)
		.def("getNumForceControlInfos", &CyGlobalContext::getNumForceControlInfos)
		.def("getNumGameOptionInfos", &CyGlobalContext::getNumGameOptionInfos)
		.def("getNumGameSpeedInfos", &CyGlobalContext::getNumGameSpeedInfos)
		.def("getNumGoodyInfos", &CyGlobalContext::getNumGoodyInfos)
		.def("getNumHandicapInfos", &CyGlobalContext::getNumHandicapInfos)
		.def("getNumHeritageInfos", &CyGlobalContext::getNumHeritageInfos)
		.def("getNumHurryInfos", &CyGlobalContext::getNumHurryInfos)
		.def("getNumImprovementInfos", &CyGlobalContext::getNumImprovementInfos)
		.def("getNumLeaderHeadInfos", &CyGlobalContext::getNumLeaderHeadInfos)
		.def("getNumMPOptionInfos", &CyGlobalContext::getNumMPOptionInfos)
		.def("getNumMainMenus", &CyGlobalContext::getNumMainMenus)
		.def("getNumMissionInfos", &CyGlobalContext::getNumMissionInfos)
		.def("getNumNewConceptInfos", &CyGlobalContext::getNumNewConceptInfos)
		.def("getNumPlayableCivilizationInfos", &CyGlobalContext::getNumPlayableCivilizationInfos)
		.def("getNumPlayerColorInfos", &CyGlobalContext::getNumPlayerColorInfos)
		.def("getNumProcessInfos", &CyGlobalContext::getNumProcessInfos)
		.def("getNumProjectInfos", &CyGlobalContext::getNumProjectInfos)
		.def("getNumPromotionInfos", &CyGlobalContext::getNumPromotionInfos)
		.def("getNumPromotionLineInfos", &CyGlobalContext::getNumPromotionLineInfos)
		.def("getNumPropertyInfos", &CyGlobalContext::getNumPropertyInfos)
		.def("getNumReligionInfos", &CyGlobalContext::getNumReligionInfos)
		.def("getNumRouteInfos", &CyGlobalContext::getNumRouteInfos)
		.def("getNumSeaLevelInfos", &CyGlobalContext::getNumSeaLevelInfos)
		.def("getNumSeasonInfos", &CyGlobalContext::getNumSeasonInfos)
		.def("getNumSpecialBuildingInfos", &CyGlobalContext::getNumSpecialBuildingInfos)
		.def("getNumSpecialUnitInfos", &CyGlobalContext::getNumSpecialUnitInfos)
		.def("getNumSpecialistInfos", &CyGlobalContext::getNumSpecialistInfos)
		.def("getNumTechInfos", &CyGlobalContext::getNumTechInfos)
		.def("getNumTerrainInfos", &CyGlobalContext::getNumTerrainInfos)
		.def("getNumTraitInfos", &CyGlobalContext::getNumTraitInfos)
		.def("getNumUnitCombatInfos", &CyGlobalContext::getNumUnitCombatInfos)
		.def("getNumUnitInfos", &CyGlobalContext::getNumUnitInfos)
		.def("getNumUpkeepInfos", &CyGlobalContext::getNumUpkeepInfos)
		.def("getNumVictoryInfos", &CyGlobalContext::getNumVictoryInfos)
		.def("getNumVoteInfos", &CyGlobalContext::getNumVoteInfos)
		.def("getNumVoteSourceInfos", &CyGlobalContext::getNumVoteSourceInfos)
		.def("getNumWorldInfos", &CyGlobalContext::getNumWorldInfos)
		DO_FOR_EACH_EXPOSED_INT_GLOBAL_DEFINE(PUBLISH_CY_GET_METHOD)
		DO_FOR_EACH_EXPOSED_INFO_TYPE(PUBLISH_CY_GET_METHOD)
		;

#undef PUBLISH_CY_GET_METHOD
}
