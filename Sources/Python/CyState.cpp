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
#include "Engine/CvUnit.h"                          // the selection reads answer a unit's owner + id
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"  // getHeadSelectedCity/Unit -- the CURRENT SELECTION
#include "AI/CvPlayerAI.h"          // GET_PLAYER
#include "Infos/CvInfoKinds.h"      // the NUM_<FAMILY>_KINDS the groups are sized by
#include "UI/CvBuildingFilters.h"   // BuildingFilterTypes -- the city screen's list VIEW state
#include "UI/CvUnitFilters.h"       // UnitFilterTypes -- ditto

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

// ---- GROUPS both scopes carry: iCity >= 0 reads the CITY, iCity < 0 reads the EMPIRE ----

python::list CyState::getYields(int iPlayer, int iCity) const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getYields(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getYields(values); }
	return cys_toList(values);
}

python::list CyState::getCommerces(int iPlayer, int iCity) const
{
	int values[NUM_COMMERCE_TYPES] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getCommerces(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getCommerces(values); }
	return cys_toList(values);
}

python::list CyState::getWellbeing(int iPlayer, int iCity) const
{
	int values[NUM_WELLBEING_CHANNELS] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getWellbeing(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getWellbeing(values); }
	return cys_toList(values);
}

python::list CyState::getDefenseKinds(int iPlayer, int iCity) const
{
	int values[NUM_DEFENSE_KINDS] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getDefenseKinds(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getDefenseKinds(values); }
	return cys_toList(values);
}

python::list CyState::getMaintenanceKinds(int iPlayer, int iCity) const
{
	int values[NUM_MAINTENANCE_KINDS] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getMaintenanceKinds(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getMaintenanceKinds(values); }
	return cys_toList(values);
}

python::list CyState::getBuildRateKinds(int iPlayer, int iCity) const
{
	int values[NUM_BUILD_RATE_KINDS] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getBuildRateKinds(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getBuildRateKinds(values); }
	return cys_toList(values);
}

python::list CyState::getCombatKinds(int iPlayer, int iCity) const
{
	int values[NUM_COMBAT_KINDS] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getCombatKinds(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getCombatKinds(values); }
	return cys_toList(values);
}

python::list CyState::getExperienceKinds(int iPlayer, int iCity) const
{
	int values[NUM_EXPERIENCE_KINDS] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getExperienceKinds(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getExperienceKinds(values); }
	return cys_toList(values);
}

python::list CyState::getRevolutionKinds(int iPlayer, int iCity) const
{
	int values[NUM_REVOLUTION_KINDS] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getRevolutionKinds(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getRevolutionKinds(values); }
	return cys_toList(values);
}

python::list CyState::getTradeRouteKinds(int iPlayer, int iCity) const
{
	int values[NUM_TRADE_ROUTE_KINDS] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getTradeRouteKinds(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getTradeRouteKinds(values); }
	return cys_toList(values);
}

python::list CyState::getScalars(int iPlayer, int iCity) const
{
	int values[NUM_INFO_SCALARS] = { 0 };
	if (iCity >= 0) { const CvCity* p = cys_city(iPlayer, iCity); if (p) p->getScalars(values); }
	else            { const CvPlayer* p = cys_player(iPlayer);    if (p) p->getScalars(values); }
	return cys_toList(values);
}

// ---- CITY-only groups ----

python::list CyState::getHealKinds(int iPlayer, int iCity) const
{
	int values[NUM_HEAL_KINDS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->getHealKinds(values);
	return cys_toList(values);
}

python::list CyState::getUnderworldKinds(int iPlayer, int iCity) const
{
	int values[NUM_UNDERWORLD_KINDS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->getUnderworldKinds(values);
	return cys_toList(values);
}

python::list CyState::getVisionKinds(int iPlayer, int iCity) const
{
	int values[NUM_VISION_KINDS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->getVisionKinds(values);
	return cys_toList(values);
}

python::list CyState::getRealizedWellbeing(int iPlayer, int iCity, int iExtraPopulation) const
{
	int values[NUM_WELLBEING_CHANNELS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->realizedWellbeing(iExtraPopulation, values);
	return cys_toList(values);
}

python::list CyState::getYieldModifiers(int iPlayer, int iCity) const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity)
	{
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			values[iYield] = pCity->getBaseYieldRateModifier((YieldTypes)iYield);
		}
	}
	return cys_toList(values);
}

int CyState::getSight(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->sight() : 0;
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
	int values[2] = { -1, -1 };
	const CvUnit* pUnit = gDLL->getInterfaceIFace()->getHeadSelectedUnit();
	if (pUnit != NULL)
	{
		values[0] = (int)pUnit->getOwner();
		values[1] = pUnit->getID();
	}
	return cys_toList(values);
}

// ---- ENUMERATION ----

python::list CyState::getCityIds(int iPlayer) const
{
	python::list cityIds = python::list();
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer)
	{
		int iIter = 0;
		for (const CvCity* pCity = pPlayer->firstCity(&iIter); pCity != NULL; pCity = pPlayer->nextCity(&iIter))
		{
			cityIds.append(pCity->getID());
		}
	}
	return cityIds;
}

// ---- CITY RANK groups ----
//
// A rank is the city's ORDINAL position among its owner's cities for one channel (1 = highest). The whole group
// comes back in one call, indexed by the engine enum, so no channel is ever named in the call.
// ⚠ Rank 0 is the "no city" answer here, and it is not a real rank -- the engine ranks from 1.

python::list CyState::getYieldRateRanks(int iPlayer, int iCity) const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity)
	{
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			values[iYield] = pCity->findYieldRateRank((YieldTypes)iYield);
		}
	}
	return cys_toList(values);
}

python::list CyState::getBaseYieldRateRanks(int iPlayer, int iCity) const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity)
	{
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			values[iYield] = pCity->findBaseYieldRateRank((YieldTypes)iYield);
		}
	}
	return cys_toList(values);
}

python::list CyState::getCommerceRateRanks(int iPlayer, int iCity) const
{
	int values[NUM_COMMERCE_TYPES] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity)
	{
		for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
		{
			values[iCommerce] = pCity->findCommerceRateRank((CommerceTypes)iCommerce);
		}
	}
	return cys_toList(values);
}

// ---- CITY plain FACTS ----

python::list CyState::getCityPosition(int iPlayer, int iCity) const
{
	int values[2] = { -1, -1 };   // -1,-1 = no such city; a real plot coordinate is never negative
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity)
	{
		values[0] = pCity->getX();
		values[1] = pCity->getY();
	}
	return cys_toList(values);
}

int CyState::getCityPopulation(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->getPopulation() : 0;
}

int64_t CyState::getCityRealPopulation(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->getRealPopulation() : 0;
}

int CyState::getGreatPeopleRate(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->getGreatPeopleRate() : 0;
}

int CyState::getGreatPeopleProgress(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->getGreatPeopleProgress() : 0;
}

int CyState::getGreatPeopleUnitProgress(int iPlayer, int iCity, int iUnit) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iUnit < 0 || iUnit >= GC.getNumUnitInfos())
	{
		return 0;
	}
	return pCity->getGreatPeopleUnitProgress((UnitTypes)iUnit);
}

int CyState::getGreatPeopleThresholdNonMilitary(int iPlayer) const
{
	const CvPlayer* pPlayer = cys_player(iPlayer);
	return pPlayer ? pPlayer->greatPeopleThresholdNonMilitary() : 0;
}

int CyState::getMilitaryHappinessUnits(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->getMilitaryHappinessUnits() : 0;
}

bool CyState::isOccupation(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->isOccupation() : false;
}

// ---- The city's RAW-STATE groups ----

python::list CyState::getCountdowns(int iPlayer, int iCity) const
{
	int values[NUM_CITY_COUNTDOWN_KINDS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->getCountdowns(values);
	return cys_toList(values);
}

python::list CyState::getOrder(int iPlayer, int iCity) const
{
	int values[NUM_CITY_ORDER_READS] = { 0 };
	values[ORDER_READ_TYPE] = NO_ORDER;
	values[ORDER_READ_ID]   = -1;
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->getOrderRead(values);
	return cys_toList(values);
}

python::list CyState::getGrowth(int iPlayer, int iCity) const
{
	int values[NUM_CITY_GROWTH_READS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->getGrowthRead(values);
	return cys_toList(values);
}

python::list CyState::getCulture(int iPlayer, int iCity) const
{
	int values[NUM_CITY_CULTURE_READS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->getCultureRead(values);
	return cys_toList(values);
}

bool CyState::isCityRevealed(int iPlayer, int iCity, int iTeam) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iTeam < 0 || iTeam >= MAX_TEAMS)
	{
		return false;
	}
	return pCity->isRevealed((TeamTypes)iTeam, false);
}

bool CyState::isEmphasize(int iPlayer, int iCity, int iEmphasize) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iEmphasize < 0 || iEmphasize >= GC.getNumEmphasizeInfos())
	{
		return false;
	}
	return pCity->AI_isEmphasize((EmphasizeTypes)iEmphasize);
}

python::list CyState::getHurryQuote(int iPlayer, int iCity, int iHurry) const
{
	int values[NUM_CITY_HURRY_QUOTES] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity && iHurry >= 0 && iHurry < GC.getNumHurryInfos())
	{
		pCity->getHurryQuote((HurryTypes)iHurry, values);
	}
	return cys_toList(values);
}

// ---- The city screen's VIEW state ----
// ⚠ The engine's getters here are non-const, so the city is resolved non-const too -- these READ, and the
// const_cast is the engine's signature showing through, not an intent to write.

bool CyState::isProductionAutomated(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->isProductionAutomated() : false;
}

int CyState::getOrderQueueLength(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->getOrderQueueLength() : 0;
}

bool CyState::getBuildingListFilterActive(int iPlayer, int iCity, int iFilter) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iFilter < 0 || iFilter >= NUM_BUILDING_FILTERS) return false;
	return pCity->getBuildingListFilterActive((BuildingFilterTypes)iFilter);
}

int CyState::getBuildingListSorting(int iPlayer, int iCity) const
{
	CvCity* pCity = const_cast<CvCity*>(cys_city(iPlayer, iCity));
	return pCity ? (int)pCity->getBuildingListSorting() : 0;
}

bool CyState::getUnitListFilterActive(int iPlayer, int iCity, int iFilter) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iFilter < 0 || iFilter >= NUM_UNIT_FILTERS) return false;
	return pCity->getUnitListFilterActive((UnitFilterTypes)iFilter);
}

int CyState::getUnitListGrouping(int iPlayer, int iCity) const
{
	CvCity* pCity = const_cast<CvCity*>(cys_city(iPlayer, iCity));
	return pCity ? (int)pCity->getUnitListGrouping() : 0;
}

int CyState::getUnitListSorting(int iPlayer, int iCity) const
{
	CvCity* pCity = const_cast<CvCity*>(cys_city(iPlayer, iCity));
	return pCity ? (int)pCity->getUnitListSorting() : 0;
}

// ---- EMPIRE-only groups ----

python::list CyState::getUpkeepKinds(int iPlayer) const
{
	int values[NUM_UPKEEP_KINDS] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getUpkeepKinds(values);
	return cys_toList(values);
}

python::list CyState::getCostKinds(int iPlayer) const
{
	int values[NUM_COSTS_KINDS] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getCostKinds(values);
	return cys_toList(values);
}

python::list CyState::getStateReligionKinds(int iPlayer) const
{
	int values[NUM_STATE_RELIGION_KINDS] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getStateReligionKinds(values);
	return cys_toList(values);
}

python::list CyState::getDiplomacyKinds(int iPlayer) const
{
	int values[NUM_DIPLOMACY_KINDS] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getDiplomacyKinds(values);
	return cys_toList(values);
}

python::list CyState::getDurationKinds(int iPlayer) const
{
	int values[NUM_DURATIONS_KINDS] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getDurationKinds(values);
	return cys_toList(values);
}

python::list CyState::getAirKinds(int iPlayer) const
{
	int values[NUM_AIR_KINDS] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getAirKinds(values);
	return cys_toList(values);
}

python::list CyState::getCaptureKinds(int iPlayer) const
{
	int values[NUM_CAPTURE_KINDS] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getCaptureKinds(values);
	return cys_toList(values);
}

python::list CyState::getCargoKinds(int iPlayer) const
{
	int values[NUM_CARGO_KINDS] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getCargoKinds(values);
	return cys_toList(values);
}

python::list CyState::getExtraYieldThresholds(int iPlayer) const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getExtraYieldThresholds(values);
	return cys_toList(values);
}

python::list CyState::getLessYieldThresholds(int iPlayer) const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	const CvPlayer* pPlayer = cys_player(iPlayer);
	if (pPlayer) pPlayer->getLessYieldThresholds(values);
	return cys_toList(values);
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

std::wstring CyState::getPlayerName(int iPlayer) const
{
	const CvPlayer* pPlayer = cys_player(iPlayer);
	return pPlayer ? std::wstring(pPlayer->getName()) : std::wstring();
}

std::wstring CyState::getCityName(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? std::wstring(pCity->getName()) : std::wstring();
}

// The publication. ONE class, id-based, no CyCity/CyPlayer anywhere in a signature -- so the legacy wrappers can
// be cut away without touching this ([DEC-cy-not-fixed]: the replacement is a NEW surface, never a widened
// binding).
void CyState::pythonPublish()
{
	python::class_<CyState>("CyState")
		// groups both scopes carry -- iCity >= 0 reads the CITY, iCity < 0 reads the EMPIRE
		.def("getYields",                &CyState::getYields)
		.def("getCommerces",             &CyState::getCommerces)
		.def("getWellbeing",             &CyState::getWellbeing)
		.def("getDefenseKinds",          &CyState::getDefenseKinds)
		.def("getMaintenanceKinds",      &CyState::getMaintenanceKinds)
		.def("getBuildRateKinds",        &CyState::getBuildRateKinds)
		.def("getCombatKinds",           &CyState::getCombatKinds)
		.def("getExperienceKinds",       &CyState::getExperienceKinds)
		.def("getRevolutionKinds",       &CyState::getRevolutionKinds)
		.def("getTradeRouteKinds",       &CyState::getTradeRouteKinds)
		.def("getScalars",               &CyState::getScalars)
		// CITY-only
		.def("getHealKinds",             &CyState::getHealKinds)
		.def("getUnderworldKinds",       &CyState::getUnderworldKinds)
		.def("getVisionKinds",           &CyState::getVisionKinds)
		.def("getRealizedWellbeing",     &CyState::getRealizedWellbeing)
		.def("getYieldModifiers",        &CyState::getYieldModifiers)
		.def("getSight",                 &CyState::getSight)
		// ENUMERATION + CITY rank groups + plain city facts
		.def("getHeadSelectedCityId",    &CyState::getHeadSelectedCityId)
		.def("getHeadSelectedUnitId",    &CyState::getHeadSelectedUnitId)
		.def("getCityIds",               &CyState::getCityIds)
		.def("getYieldRateRanks",        &CyState::getYieldRateRanks)
		.def("getBaseYieldRateRanks",    &CyState::getBaseYieldRateRanks)
		.def("getCommerceRateRanks",     &CyState::getCommerceRateRanks)
		.def("getCityPosition",          &CyState::getCityPosition)
		.def("getCityPopulation",        &CyState::getCityPopulation)
		.def("getCityRealPopulation",    &CyState::getCityRealPopulation)
		.def("getGreatPeopleRate",       &CyState::getGreatPeopleRate)
		.def("getGreatPeopleProgress",   &CyState::getGreatPeopleProgress)
		.def("getGreatPeopleUnitProgress", &CyState::getGreatPeopleUnitProgress)
		.def("getGreatPeopleThresholdNonMilitary", &CyState::getGreatPeopleThresholdNonMilitary)
		.def("getMilitaryHappinessUnits",&CyState::getMilitaryHappinessUnits)
		.def("isOccupation",             &CyState::isOccupation)
		.def("getCountdowns",            &CyState::getCountdowns)
		.def("getOrder",                 &CyState::getOrder)
		.def("getGrowth",                &CyState::getGrowth)
		.def("getCulture",               &CyState::getCulture)
		.def("isCityRevealed",           &CyState::isCityRevealed)
		.def("isEmphasize",              &CyState::isEmphasize)
		.def("getHurryQuote",            &CyState::getHurryQuote)
		.def("isProductionAutomated",    &CyState::isProductionAutomated)
		.def("getOrderQueueLength",      &CyState::getOrderQueueLength)
		.def("getBuildingListFilterActive", &CyState::getBuildingListFilterActive)
		.def("getBuildingListSorting",   &CyState::getBuildingListSorting)
		.def("getUnitListFilterActive",  &CyState::getUnitListFilterActive)
		.def("getUnitListGrouping",      &CyState::getUnitListGrouping)
		.def("getUnitListSorting",       &CyState::getUnitListSorting)
		// EMPIRE-only
		.def("getUpkeepKinds",           &CyState::getUpkeepKinds)
		.def("getCostKinds",             &CyState::getCostKinds)
		.def("getStateReligionKinds",    &CyState::getStateReligionKinds)
		.def("getDiplomacyKinds",        &CyState::getDiplomacyKinds)
		.def("getDurationKinds",         &CyState::getDurationKinds)
		.def("getAirKinds",              &CyState::getAirKinds)
		.def("getCaptureKinds",          &CyState::getCaptureKinds)
		.def("getCargoKinds",            &CyState::getCargoKinds)
		.def("getExtraYieldThresholds",  &CyState::getExtraYieldThresholds)
		.def("getLessYieldThresholds",   &CyState::getLessYieldThresholds)
		// plain live facts
		.def("getActivePlayer",          &CyState::getActivePlayer)
		.def("getGameTurn",              &CyState::getGameTurn)
		.def("isPlayerAlive",            &CyState::isPlayerAlive)
		.def("getPlayerTeam",            &CyState::getPlayerTeam)
		.def("isFinalInitialized",       &CyState::isFinalInitialized)
		.def("getMAX_PLAYERS",           &CyState::getMAX_PLAYERS)
		.def("getMAX_PC_PLAYERS",        &CyState::getMAX_PC_PLAYERS)
		.def("getMAX_TEAMS",             &CyState::getMAX_TEAMS)
		.def("getMAX_PC_TEAMS",          &CyState::getMAX_PC_TEAMS)
		.def("getBARBARIAN_PLAYER",      &CyState::getBARBARIAN_PLAYER)
		.def("getDefineINT",             &CyState::getDefineINT)
		.def("getDefineFLOAT",           &CyState::getDefineFLOAT)
		.def("getAIAutoPlay",            &CyState::getAIAutoPlay)
		.def("getPlayerName",            &CyState::getPlayerName)
		.def("getCityName",              &CyState::getCityName)
		;

	// The EMPIRE selector, named rather than left as a bare -1 at every call site: a script reads the empire by
	// passing it where a city id would go, and the name is what makes that legible.
	python::scope().attr("STATE_EMPIRE") = -1;
}
