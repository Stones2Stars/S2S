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

int CyGlobalContext::getInfoTypeForString(const char* szInfoType, bool bHideAssert) const
{
	return GC.getInfoTypeForString(szInfoType, bHideAssert);
}

// The publication: the CONFIG half only. Every info accessor and every Cy* handle is deliberately absent --
// a script asking for entity data gets an AttributeError here and goes to the library, which is exactly the
// point of keeping the two apart (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (Cy* is not a fixed contract)). The DEFINES are served BY NAME, so a new define is
// data rather than a new method.


std::wstring CyGlobalContext::getPlayerOptionDescription(int iOption) const
{
	return GC.getPlayerOptionInfo((PlayerOptionTypes)iOption).getDescription();
}

std::wstring CyGlobalContext::getPlayerOptionHelp(int iOption) const
{
	return GC.getPlayerOptionInfo((PlayerOptionTypes)iOption).getHelp();
}

std::wstring CyGlobalContext::getGraphicOptionDescription(int iOption) const
{
	return GC.getGraphicOptionInfo((GraphicOptionTypes)iOption).getDescription();
}

std::wstring CyGlobalContext::getGraphicOptionHelp(int iOption) const
{
	return GC.getGraphicOptionInfo((GraphicOptionTypes)iOption).getHelp();
}


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

CyMap* CyGlobalContext::getMapByIndex(MapTypes eMap) const
{
	FASSERT_BOUNDS(0, NUM_MAPS, eMap);
	return &g_cyMaps[eMap];
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
