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
#include "Infrastructure/CvPythonGlobalContextLoader.h"
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


CyGame* CyGlobalContext::getCyGame() const
{
	static CyGame cyGame(GC.getGame());
	return &cyGame;
}

CyMap* CyGlobalContext::getCyMap() const
{
	static CyMap cyMap;
	return &cyMap;
	//return g_cyMaps[CURRENT_MAP];
}

void CyGlobalContext::switchMap(MapTypes eMap)
{
	GC.switchMap(eMap);
}

CyMap* CyGlobalContext::getMapByIndex(MapTypes eMap) const
{
	FASSERT_BOUNDS(0, NUM_MAPS, eMap);
	return &g_cyMaps[eMap];
}

python::list CyGlobalContext::getMaps() const
{
	python::list l = python::list();

	foreach_(CyMap& mapX, g_cyMaps)
	{
		l.append(mapX);
	}
	return l;
}

int CyGlobalContext::getNumMapsInitialized() const
{
	return algo::count_if(GC.getMaps(), bind(CvMap::plotsInitialized, _1));
}

CyPlayer* CyGlobalContext::getCyPlayer(PlayerTypes ePlayer) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, ePlayer);
	return ePlayer >= 0 && ePlayer < MAX_PLAYERS ? &g_cyPlayers[ePlayer] : NULL;
}

CyPlayer* CyGlobalContext::getCyActivePlayer() const
{
	return getCyPlayer(GC.getGame().getActivePlayer());
}

CvRandom& CyGlobalContext::getCyASyncRand() const
{
	return GC.getASyncRand();
}

CyTeam* CyGlobalContext::getCyTeam(TeamTypes eTeam) const
{
	FASSERT_BOUNDS(0, MAX_TEAMS, eTeam);
	return eTeam < MAX_TEAMS ? &g_cyTeams[eTeam] : NULL;
}

const CvMainMenuInfo* CyGlobalContext::getMainMenus(int i) const
{
	return ((i >= 0 && i < GC.getNumMainMenus()) ? &GC.getMainMenus(i) : NULL);
}

void CyGlobalContext::pythonPublish()
{
	// The publication rides the ORIGINAL loaders, pruned against this class's own header -- so the INFO
	// accessors drop out by construction rather than by a hand-kept list, and cannot creep back in.
	python::class_<CyGlobalContext> gc("CyGlobalContext");
	CvPythonGlobalContextLoader::CyGlobalContextPythonInterface1(gc);
	CvPythonGlobalContextLoader::CyGlobalContextPythonInterface2(gc);
	CvPythonGlobalContextLoader::CyGlobalContextPythonInterface3(gc);
	CvPythonGlobalContextLoader::CyGlobalContextPythonInterface4(gc);
}
