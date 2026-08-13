//
//	CyState -- the Python live-state surface (see the header for the role, the grammar and the boost rule).
//	Every body here is a BARE RELAY of a maintained group read: resolve the owner, fill the caller-owned array,
//	hand it back as a list. Nothing gates, ensures or recomputes -- so a missed invalidation surfaces in script as
//	a visibly wrong number, exactly as it does on the C++ side.
//

#include "CvGameCoreDLL.h"
#include "CyState.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvGame.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvUnit.h"       // the unit plane + the selection reads
#include "Engine/CvPlot.h"
#include "Engine/CvMap.h"        // the plot enumeration resolves its plot through the map
#include "Infos/CvPlayerColorInfo.h"   // the empire-colour hop: PLAYERCOLOR_ -> its primary COLOR_
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"
#include <ctime>  // getHeadSelectedCity/Unit -- the CURRENT SELECTION
#include "AI/BetterBTSAI.h"   // PERF_SCOPE -- the ONE instrument, gated by gPerfLogLevel
#include "AI/CvPlayerAI.h"          // GET_PLAYER
#include "AI/CvTeamAI.h"            // GET_TEAM -- the team-scope reads (a tech's realized research cost)
#include "Infos/CvInfoKinds.h"      // the NUM_<FAMILY>_KINDS the groups are sized by
#include "UI/CityOutputHistory.h"  // the city admin tab's recent-output rows
#include "UI/CvBuildingFilters.h"   // BuildingFilterTypes -- the city screen's list VIEW state
#include "UI/CvUnitFilters.h"       // UnitFilterTypes -- ditto
#include "Data/CvInfoValuation.h"   // CityRateTerms -- the ONE city-yield decomposition

extern const char* g_szLastCyRead;

namespace
{
	// Resolve without asserting: a script may legitimately hold a stale id, and the honest answer for something
	// that does not exist is an all-zero group, not a crash -- the same discipline CyEnabler applies to HIDDEN.
	const CvPlayer* cys_player(int iPlayer)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS) return NULL;
		return &GET_PLAYER((PlayerTypes)iPlayer);
	}

	const CvCity* cys_city(int iPlayer, int iCity)
	{
		const CvPlayer* pPlayer = cys_player(iPlayer);
		return pPlayer ? pPlayer->getCity(iCity) : NULL;
	}

	// Non-const because a couple of the engine's unit accessors are (getHotKeyNumber); these READ.
	CvUnit* cys_unit(int iPlayer, int iUnit)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS) return NULL;
		return GET_PLAYER((PlayerTypes)iPlayer).getUnit(iUnit);
	}

	// The whole group out, in one call. N is deduced from the array the group read filled, so a family that
	// grows a channel needs no edit here.
	template <int N>
	python::list cys_toList(const int (&values)[N])
	{
		python::list list = python::list();
		for (int i = 0; i < N; ++i)
		{
			list.append(values[i]);
		}
		return list;
	}
}

// ---- THE CURRENT SELECTION ----

python::list CyState::getHeadSelectedCityId() const
{
	int values[2] = { -1, -1 };
	const CvCity* pCity = gDLL->getInterfaceIFace()->getHeadSelectedCity();
	if (pCity != NULL)
	{
		values[0] = (int)pCity->getOwner();
		values[1] = pCity->getID();
	}
	return cys_toList(values);
}

python::list CyState::getHeadSelectedUnitId() const
{
	PERF_SCOPE("CyState::getHeadSelectedUnitId", -1);
	g_szLastCyRead = "CyState::getHeadSelectedUnitId";
	int values[2] = { -1, -1 };
	const CvUnit* pUnit = gDLL->getInterfaceIFace()->getHeadSelectedUnit();
	if (pUnit != NULL)
	{
		values[0] = (int)pUnit->getOwner();
		values[1] = pUnit->getID();
	}
	return cys_toList(values);
}

python::list CyState::getSelectedUnitIds() const
{
	PERF_SCOPE("CyState::getSelectedUnitIds", -1);
	g_szLastCyRead = "CyState::getSelectedUnitIds";
	python::list list = python::list();
	const int iCount = gDLL->getInterfaceIFace()->getLengthSelectionList();
	for (int i = 0; i < iCount; ++i)
	{
		const CvUnit* pUnit = gDLL->getInterfaceIFace()->getSelectionUnit(i);
		if (pUnit == NULL)
		{
			continue;
		}
		python::list pair = python::list();
		pair.append((int)pUnit->getOwner());
		pair.append(pUnit->getID());
		list.append(pair);
	}
	return list;
}

// ---- ENUMERATION ----

// ---- CITY RANK groups ----
//
// A rank is the city's ORDINAL position among its owner's cities for one channel (1 = highest). The whole group
// comes back in one call, indexed by the engine enum, so no channel is ever named in the call.
// ⚠ Rank 0 is the "no city" answer here, and it is not a real rank -- the engine ranks from 1.

// ---- CITY plain FACTS ----

python::list CyState::getUnitPosition(int iPlayer, int iUnit) const
{
	int values[2] = { -1, -1 };   // -1,-1 = no such unit, or the unit is OFF-MAP (a real state)
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit)
	{
		//	⛔ THE ON-MAP TEST IS THE COORDINATE RANGE, never `plot() != NULL` alone: plot() answers NULL for
		//	exactly ONE pair (INVALID_PLOT_COORD) and resolves any OTHER out-of-range value to a WRONG plot
		//	([unit-lifecycle.md]). A unit carrying a stored -1 would otherwise hand back a real but wrong tile,
		//	which is worse than answering "off map" -- and saves do contain such units.
		const int iX = pUnit->getX();
		const int iY = pUnit->getY();
		if (iX >= 0 && iY >= 0 && iX < GC.getMap().getGridWidth() && iY < GC.getMap().getGridHeight())
		{
			values[0] = iX;
			values[1] = iY;
		}
	}
	return cys_toList(values);
}

int CyState::getGreatPeopleThresholdNonMilitary(int iPlayer) const
{
	const CvPlayer* pPlayer = cys_player(iPlayer);
	return pPlayer ? pPlayer->greatPeopleThresholdNonMilitary() : 0;
}

// ---- The city's RAW-STATE groups ----

namespace
{
	//	The two build lists differ only in which accessors they walk, so the walk itself is written once.
	template <class TGroupNum, class TInGroup, class TAt>
	python::list cys_listGroups(CvCity* pCity, TGroupNum groupNum, TInGroup inGroup, TAt at)
	{
		python::list groups = python::list();
		if (pCity == NULL)
		{
			return groups;
		}
		const int iGroups = (pCity->*groupNum)();
		for (int iGroup = 0; iGroup < iGroups; ++iGroup)
		{
			python::list entries = python::list();
			const int iCount = (pCity->*inGroup)(iGroup);
			for (int iPos = 0; iPos < iCount; ++iPos)
			{
				entries.append((int)(pCity->*at)(iGroup, iPos));
			}
			groups.append(entries);
		}
		return groups;
	}
}

// ---- THE UNIT PLANE ----

python::list CyState::getUnitRead(int iPlayer, int iUnit) const
{
	PERF_SCOPE("CyState::getUnitRead", -1);
	g_szLastCyRead = "CyState::getUnitRead";
	int values[NUM_UNIT_READS] = { 0 };
	values[UNIT_READ_TYPE]     = -1;
	values[UNIT_READ_ACTIVITY] = (int)NO_ACTIVITY;
	values[UNIT_READ_AUTOMATE] = (int)NO_AUTOMATE;
	values[UNIT_READ_MISSION]  = (int)NO_MISSION;
	CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit) pUnit->getUnitRead(values);
	return cys_toList(values);
}

python::list CyState::getUnitFlags(int iPlayer, int iUnit) const
{
	PERF_SCOPE("CyState::getUnitFlags", -1);
	g_szLastCyRead = "CyState::getUnitFlags";
	int values[NUM_UNIT_FLAGS] = { 0 };
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit) pUnit->getUnitFlags(values);
	return cys_toList(values);
}

std::wstring CyState::getUnitNameNoDesc(int iPlayer, int iUnit) const
{
	g_szLastCyRead = "CyState::getUnitNameNoDesc";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? std::wstring(pUnit->getNameNoDesc()) : std::wstring();
}

std::string CyState::getUnitScriptData(int iPlayer, int iUnit) const
{
	g_szLastCyRead = "CyState::getUnitScriptData";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? pUnit->getScriptData() : std::string();
}

std::wstring CyState::getUnitName(int iPlayer, int iUnit) const
{
	PERF_SCOPE("CyState::getUnitName", -1);
	g_szLastCyRead = "CyState::getUnitName";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? std::wstring(pUnit->getName()) : std::wstring();
}

python::list CyState::getPlotUnitIds(int iX, int iY) const
{
	PERF_SCOPE("CyState::getPlotUnitIds", -1);
	g_szLastCyRead = "CyState::getPlotUnitIds";
	python::list list = python::list();
	const CvPlot* pPlot = GC.getMap().plot(iX, iY);
	if (pPlot == NULL)
	{
		return list;
	}
	const int iNumUnits = pPlot->getNumUnits();
	for (int i = 0; i < iNumUnits; ++i)
	{
		const CvUnit* pUnit = pPlot->getUnitByIndex(i);
		if (pUnit == NULL)
		{
			continue;
		}
		python::list pair = python::list();
		pair.append((int)pUnit->getOwner());
		pair.append(pUnit->getID());
		list.append(pair);
	}
	return list;
}

bool CyState::isUnitInvisible(int iPlayer, int iUnit, int iTeam) const
{
	PERF_SCOPE("CyState::isUnitInvisible", -1);
	g_szLastCyRead = "CyState::isUnitInvisible";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iTeam < 0 || iTeam >= MAX_TEAMS)
	{
		return false;
	}
	return pUnit->isInvisible((TeamTypes)iTeam, false);
}

python::list CyState::getUnitPromotions(int iPlayer, int iUnit) const
{
	PERF_SCOPE("CyState::getUnitPromotions", -1);
	g_szLastCyRead = "CyState::getUnitPromotions";
	python::list ids = python::list();
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL) return ids;
	//	⛔ WALK WHAT THE UNIT HOLDS -- do NOT sweep the promotion registry asking "do you have this one?". The
	//	unit keys only the promotions it actually carries, and isHasPromotion is a keyed LOOKUP, so a registry
	//	sweep is ~1500 map searches per unit per redraw to rediscover a list the unit already has.
	const std::map<PromotionTypes, PromotionKeyedInfo>& held = pUnit->getPromotionKeyedInfo();
	for (std::map<PromotionTypes, PromotionKeyedInfo>::const_iterator it = held.begin(); it != held.end(); ++it)
	{
		if (it->second.m_bHasPromotion && !pUnit->isPromotionOverriden(it->first))
		{
			ids.append((int)it->first);
		}
	}
	return ids;
}


bool CyState::isUnitHiddenNationality(int iPlayer, int iUnit) const
{
	g_szLastCyRead = "CyState::isUnitHiddenNationality";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? pUnit->isHiddenNationality() : false;
}

bool CyState::isUnitDead(int iPlayer, int iUnit) const
{
	g_szLastCyRead = "CyState::isUnitDead";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? pUnit->isDead() : true;
}

int CyState::getNumVisiblePotentialEnemyDefenders(int iPlayer, int iUnit, int iX, int iY) const
{
	g_szLastCyRead = "CyState::getNumVisiblePotentialEnemyDefenders";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL) return 0;
	const CvPlot* pPlot = GC.getMap().plot(iX, iY);
	return pPlot ? pPlot->getNumVisiblePotentialEnemyDefenders(pUnit) : 0;
}

int CyState::getUnitVisualOwner(int iPlayer, int iUnit) const
{
	g_szLastCyRead = "CyState::getUnitVisualOwner";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? (int)pUnit->getVisualOwner() : -1;
}

int CyState::getUnitBaseCombatStr(int iPlayer, int iUnit) const
{
	g_szLastCyRead = "CyState::getUnitBaseCombatStr";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? pUnit->baseCombatStrHuman() : 0;
}

bool CyState::hasUnitPromotion(int iPlayer, int iUnit, int iPromotion) const
{
	g_szLastCyRead = "CyState::hasUnitPromotion";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->isHasPromotion((PromotionTypes)iPromotion);
}

bool CyState::hasUnitCombat(int iPlayer, int iUnit, int iUnitCombat) const
{
	g_szLastCyRead = "CyState::hasUnitCombat";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iUnitCombat < 0 || iUnitCombat >= GC.getNumUnitCombatInfos())
	{
		return false;
	}
	return pUnit->isHasUnitCombat((UnitCombatTypes)iUnitCombat);
}

bool CyState::isUnitPromotionValid(int iPlayer, int iUnit, int iPromotion) const
{
	g_szLastCyRead = "CyState::isUnitPromotionValid";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->isPromotionValid((PromotionTypes)iPromotion);
}

bool CyState::canUnitAcquirePromotion(int iPlayer, int iUnit, int iPromotion) const
{
	g_szLastCyRead = "CyState::canUnitAcquirePromotion";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->canAcquirePromotion((PromotionTypes)iPromotion);
}

bool CyState::isUnitPromotionOverridden(int iPlayer, int iUnit, int iPromotion) const
{
	g_szLastCyRead = "CyState::isUnitPromotionOverridden";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->isPromotionOverriden((PromotionTypes)iPromotion);
}

bool CyState::isUnitActionRecommended(int iPlayer, int iUnit, int iAction) const
{
	PERF_SCOPE("CyState::isUnitActionRecommended", -1);
	g_szLastCyRead = "CyState::isUnitActionRecommended";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	//	BOTH bounds: the action id indexes the action registry, so an unchecked upper bound is an out-of-bounds
	//	read rather than a wrong answer -- and FASSERT_BOUNDS is compiled out of Release, which is where it runs.
	if (pUnit == NULL || iAction < 0 || iAction >= GC.getNumActionInfos())
	{
		return false;
	}
	return pUnit->isActionRecommended(iAction);
}

bool CyState::canUnitUpgradeToAny(int iPlayer, int iUnit) const
{
	PERF_SCOPE("CyState::canUnitUpgradeToAny", -1);
	g_szLastCyRead = "CyState::canUnitUpgradeToAny";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL) return false;
	//	⛔ ASK THE UNIT WHAT IT UPGRADES TO -- do NOT scan the registry asking every type "can I become you?".
	const std::vector<int>& upgrades = pUnit->getUnitInfo().getUpgradesTo();
	for (size_t i = 0; i < upgrades.size(); ++i)
	{
		if (pUnit->canUpgrade((UnitTypes)upgrades[i], true)) return true;
	}
	return false;
}

bool CyState::canUnitUpgrade(int iPlayer, int iUnit, int iToUnit, bool bTestVisible) const
{
	g_szLastCyRead = "CyState::canUnitUpgrade";
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iToUnit < 0 || iToUnit >= GC.getNumUnitInfos())
	{
		return false;
	}
	return pUnit->canUpgrade((UnitTypes)iToUnit, bTestVisible);
}

// ---- The city screen's VIEW state ----
// ⚠ The engine's getters here are non-const, so the city is resolved non-const too -- these READ, and the
// const_cast is the engine's signature showing through, not an intent to write.

int CyState::getBuildingCount(int iPlayer, int iBuilding) const
{
	if (iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return 0;
	const CvPlayer* pPlayer = cys_player(iPlayer);
	return pPlayer ? pPlayer->getBuildingCount((BuildingTypes)iBuilding) : 0;
}

// ---- Plain live FACTS ----

int CyState::getActivePlayer() const
{
	return (int)GC.getGame().getActivePlayer();
}

int CyState::getGameTurn() const
{
	return GC.getGame().getGameTurn();
}

bool CyState::isPlayerAlive(int iPlayer) const
{
	const CvPlayer* pPlayer = cys_player(iPlayer);
	return pPlayer ? pPlayer->isAlive() : false;
}

int CyState::getPlayerTeam(int iPlayer) const
{
	const CvPlayer* pPlayer = cys_player(iPlayer);
	return pPlayer ? (int)pPlayer->getTeam() : -1;
}

int CyState::getTechResearchCost(int iTeam, int iTech) const
{
	if (iTeam < 0 || iTeam >= MAX_TEAMS || iTech < 0 || iTech >= GC.getNumTechInfos())
	{
		return -1;
	}
	return GET_TEAM((TeamTypes)iTeam).getResearchCost((TechTypes)iTech);
}

bool CyState::isFinalInitialized() const
{
	return GC.getGame().isFinalInitialized();
}

int CyState::getAIAutoPlay(int iPlayer) const
{
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS) return 0;
	return GC.getGame().getAIAutoPlay((PlayerTypes)iPlayer);
}

int CyState::getMAX_PLAYERS() const       { return MAX_PLAYERS; }
int CyState::getMAX_PC_PLAYERS() const    { return MAX_PC_PLAYERS; }
int CyState::getMAX_TEAMS() const         { return MAX_TEAMS; }
int CyState::getMAX_PC_TEAMS() const      { return MAX_PC_TEAMS; }
int CyState::getBARBARIAN_PLAYER() const  { return (int)BARBARIAN_PLAYER; }

int CyState::getDefineINT(const std::string& szName) const
{
	return GC.getDefineINT(szName.c_str());
}

float CyState::getDefineFLOAT(const std::string& szName) const
{
	return GC.getDefineFLOAT(szName.c_str());
}

int CyState::getPlayerColorPrimary(int iPlayer) const
{
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer == NULL) return -1;

	const PlayerColorTypes ePlayerColor = pPlayer->getPlayerColor();
	//	A player genuinely may hold no colour, so -1 is an ANSWER here rather than a failure -- every caller
	//	already guards on it before drawing.
	if (ePlayerColor == NO_PLAYERCOLOR || ePlayerColor >= GC.getNumPlayerColorInfos()) return -1;

	return GC.getPlayerColorInfo(ePlayerColor).getColorTypePrimary();
}
std::wstring CyState::getPlayerName(int iPlayer) const
{
	const CvPlayer* pPlayer = cys_player(iPlayer);
	return pPlayer ? std::wstring(pPlayer->getName()) : std::wstring();
}

// The publication -- what is left after the CITY plane moved onto CyCity ([DEC-accessor-homing]).
void CyState::pythonPublish()
{
	python::class_<CyState>("CyState")
		.def("getHeadSelectedCityId",    &CyState::getHeadSelectedCityId)
		.def("getHeadSelectedUnitId",    &CyState::getHeadSelectedUnitId)
		.def("getSelectedUnitIds",       &CyState::getSelectedUnitIds)
		.def("getUnitPosition",          &CyState::getUnitPosition)
		.def("getGreatPeopleThresholdNonMilitary", &CyState::getGreatPeopleThresholdNonMilitary)
		.def("getUnitRead",              &CyState::getUnitRead)
		.def("getUnitFlags",             &CyState::getUnitFlags)
		.def("getUnitName",              &CyState::getUnitName)
		.def("getUnitNameNoDesc",        &CyState::getUnitNameNoDesc)
		.def("getUnitScriptData",        &CyState::getUnitScriptData)
		.def("getPlotUnitIds",           &CyState::getPlotUnitIds)
		.def("isUnitInvisible",          &CyState::isUnitInvisible)
		.def("hasUnitPromotion",         &CyState::hasUnitPromotion)
		.def("isUnitHiddenNationality",  &CyState::isUnitHiddenNationality)
		.def("isUnitDead",               &CyState::isUnitDead)
		.def("getUnitVisualOwner",       &CyState::getUnitVisualOwner)
		.def("getUnitBaseCombatStr",     &CyState::getUnitBaseCombatStr)
		.def("getNumVisiblePotentialEnemyDefenders", &CyState::getNumVisiblePotentialEnemyDefenders)
		.def("getUnitPromotions",        &CyState::getUnitPromotions)
		.def("isUnitPromotionOverridden",&CyState::isUnitPromotionOverridden)
		.def("hasUnitCombat",           &CyState::hasUnitCombat)
		.def("canUnitAcquirePromotion",  &CyState::canUnitAcquirePromotion)
		.def("isUnitPromotionValid",     &CyState::isUnitPromotionValid)
		.def("isUnitActionRecommended",  &CyState::isUnitActionRecommended)
		.def("canUnitUpgrade",           &CyState::canUnitUpgrade)
		.def("canUnitUpgradeToAny",      &CyState::canUnitUpgradeToAny)
		.def("getBuildingCount",         &CyState::getBuildingCount)
		// plain live facts
		.def("getActivePlayer",          &CyState::getActivePlayer)
		.def("getGameTurn",              &CyState::getGameTurn)
		.def("isPlayerAlive",            &CyState::isPlayerAlive)
		.def("getPlayerTeam",            &CyState::getPlayerTeam)
		.def("getTechResearchCost",      &CyState::getTechResearchCost)
		.def("isFinalInitialized",       &CyState::isFinalInitialized)
		.def("getMAX_PLAYERS",           &CyState::getMAX_PLAYERS)
		.def("getMAX_PC_PLAYERS",        &CyState::getMAX_PC_PLAYERS)
		.def("getMAX_TEAMS",             &CyState::getMAX_TEAMS)
		.def("getMAX_PC_TEAMS",          &CyState::getMAX_PC_TEAMS)
		.def("getBARBARIAN_PLAYER",      &CyState::getBARBARIAN_PLAYER)
		.def("getDefineINT",             &CyState::getDefineINT)
		.def("getDefineFLOAT",           &CyState::getDefineFLOAT)
		.def("getAIAutoPlay",            &CyState::getAIAutoPlay)
		.def("getPlayerColorPrimary",    &CyState::getPlayerColorPrimary)
		.def("getPlayerName",            &CyState::getPlayerName)
		;
}
