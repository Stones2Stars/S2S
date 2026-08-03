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
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"  // getHeadSelectedCity/Unit -- the CURRENT SELECTION
#include "AI/CvPlayerAI.h"          // GET_PLAYER
#include "Infos/CvInfoKinds.h"      // the NUM_<FAMILY>_KINDS the groups are sized by
#include "UI/CityOutputHistory.h"  // the city admin tab's recent-output rows
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

int CyState::getLiberationPlayer(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? (int)pCity->getLiberationPlayer(false) : -1;
}

int64_t CyState::getMaintenance(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? pCity->getMaintenanceTimes100() : 0;
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

python::list CyState::getSelectedUnitIds() const
{
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

python::list CyState::getUnitListGroups(int iPlayer, int iCity) const
{
	CvCity* pCity = const_cast<CvCity*>(cys_city(iPlayer, iCity));
	return cys_listGroups(pCity, &CvCity::getUnitListGroupNum, &CvCity::getUnitListNumInGroup,
	                      &CvCity::getUnitListType);
}

python::list CyState::getBuildingListGroups(int iPlayer, int iCity) const
{
	CvCity* pCity = const_cast<CvCity*>(cys_city(iPlayer, iCity));
	return cys_listGroups(pCity, &CvCity::getBuildingListGroupNum, &CvCity::getBuildingListNumInGroup,
	                      &CvCity::getBuildingListType);
}

python::list CyState::getBuildingInCity(int iPlayer, int iCity, int iBuilding) const
{
	int values[NUM_CITY_BUILDING_READS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity && iBuilding >= 0 && iBuilding < GC.getNumBuildingInfos())
	{
		pCity->getBuildingInCity((BuildingTypes)iBuilding, values);
	}
	return cys_toList(values);
}

python::list CyState::getUnitInCity(int iPlayer, int iCity, int iUnit) const
{
	int values[NUM_CITY_UNIT_READS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity && iUnit >= 0 && iUnit < GC.getNumUnitInfos())
	{
		pCity->getUnitInCity((UnitTypes)iUnit, values);
	}
	return cys_toList(values);
}

int CyState::getProductionTurnsLeft(int iPlayer, int iCity, int iOrder, int iType, int iNum) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iType < 0) return 0;
	switch (iOrder)
	{
	case ORDER_TRAIN:
		if (iType < GC.getNumUnitInfos()) return pCity->getProductionTurnsLeft((UnitTypes)iType, iNum);
		break;
	case ORDER_CONSTRUCT:
		if (iType < GC.getNumBuildingInfos()) return pCity->getProductionTurnsLeft((BuildingTypes)iType, iNum);
		break;
	case ORDER_CREATE:
		if (iType < GC.getNumProjectInfos()) return pCity->getProductionTurnsLeft((ProjectTypes)iType, iNum);
		break;
	default:
		break;
	}
	return 0;
}

python::list CyState::getSpecialistInCity(int iPlayer, int iCity, int iSpecialist) const
{
	int values[NUM_CITY_SPECIALIST_READS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity && iSpecialist >= 0 && iSpecialist < GC.getNumSpecialistInfos())
	{
		pCity->getSpecialistInCity((SpecialistTypes)iSpecialist, values);
	}
	return cys_toList(values);
}

int CyState::getBestUnit(int iPlayer, int iCity) const
{
	CvCity* pCity = const_cast<CvCity*>(cys_city(iPlayer, iCity));
	if (pCity == NULL) return -1;
	int iBestValue = 0;
	return (int)pCity->AI_bestUnit(iBestValue);
}

int CyState::getBestUnitForRole(int iPlayer, int iCity, int iUnitAI) const
{
	CvCity* pCity = const_cast<CvCity*>(cys_city(iPlayer, iCity));
	if (pCity == NULL || iUnitAI < 0 || iUnitAI >= NUM_UNITAI_TYPES) return -1;
	int iBestValue = 0;
	return (int)pCity->AI_bestUnitAI((UnitAITypes)iUnitAI, iBestValue);
}

python::list CyState::getCityOutputHistory(int iPlayer, int iCity) const
{
	python::list rows = python::list();
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL) return rows;
	const CityOutputHistory* pHistory = pCity->getCityOutputHistory();
	if (pHistory == NULL) return rows;
	const int iSize = (int)CityOutputHistory::getCityOutputHistorySize();
	for (int iHistory = 0; iHistory < iSize; ++iHistory)
	{
		const int iTurn = (int)pHistory->getRecentOutputTurn((uint16_t)iHistory);
		if (iTurn < 1)
		{
			break;   // the history is filled front-to-back; the first empty slot ends it
		}
		python::list entries = python::list();
		const int iNum = (int)pHistory->getCityOutputHistoryNumEntries((uint16_t)iHistory);
		for (int iEntry = 0; iEntry < iNum; ++iEntry)
		{
			python::list pair = python::list();
			pair.append((int)pHistory->getCityOutputHistoryEntry((uint16_t)iHistory, (uint16_t)iEntry, true));
			pair.append((int)pHistory->getCityOutputHistoryEntry((uint16_t)iHistory, (uint16_t)iEntry, false));
			entries.append(pair);
		}
		python::list row = python::list();
		row.append(iTurn);
		row.append(entries);
		rows.append(row);
	}
	return rows;
}

python::list CyState::getCityCounts(int iPlayer, int iCity) const
{
	int values[NUM_CITY_COUNT_READS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->getCityCounts(values);
	return cys_toList(values);
}

python::list CyState::getTradeRoutes(int iPlayer, int iCity) const
{
	python::list rows = python::list();
	CvCity* pCity = const_cast<CvCity*>(cys_city(iPlayer, iCity));
	if (pCity == NULL) return rows;
	const int iMax = pCity->getNumTradeRouteSlots();
	for (int i = 0; i < iMax; ++i)
	{
		CvCity* pPartner = pCity->getTradeCity(i);
		if (pPartner == NULL) continue;
		python::list row = python::list();
		row.append((int)pPartner->getOwner());
		row.append(pPartner->getID());
		row.append(pCity->calculateTradeProfitTimes100(pPartner));
		rows.append(row);
	}
	return rows;
}

python::list CyState::getCityReligions(int iPlayer, int iCity) const
{
	python::list rows = python::list();
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL) return rows;
	for (int i = 0; i < GC.getNumReligionInfos(); ++i)
	{
		if (!pCity->isHasReligion((ReligionTypes)i)) continue;
		python::list row = python::list();
		row.append(i);
		row.append(pCity->isHolyCity((ReligionTypes)i) ? 1 : 0);
		rows.append(row);
	}
	return rows;
}

python::list CyState::getCityCorporations(int iPlayer, int iCity) const
{
	python::list rows = python::list();
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL) return rows;
	for (int i = 0; i < GC.getNumCorporationInfos(); ++i)
	{
		if (!pCity->isHasCorporation((CorporationTypes)i)) continue;
		python::list row = python::list();
		row.append(i);
		row.append(pCity->isHeadquarters((CorporationTypes)i) ? 1 : 0);
		rows.append(row);
	}
	return rows;
}

int64_t CyState::getCultureForPlayer(int iPlayer, int iCity, int iForPlayer) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iForPlayer < 0 || iForPlayer >= MAX_PLAYERS) return 0;
	return pCity->getCultureTimes100((PlayerTypes)iForPlayer);
}

int CyState::getCulturePercent(int iPlayer, int iCity, int iForPlayer) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iForPlayer < 0 || iForPlayer >= MAX_PLAYERS) return 0;
	const CvPlot* pPlot = pCity->plot();
	return pPlot ? pPlot->calculateCulturePercent((PlayerTypes)iForPlayer) : 0;
}

int CyState::getTradeYield(int iPlayer, int iCity, int iYield, int iProfitTimes100) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iYield < 0 || iYield >= NUM_YIELD_TYPES) return 0;
	return pCity->calculateTradeYield((YieldTypes)iYield, iProfitTimes100);
}

int CyState::getNumBonuses(int iPlayer, int iCity, int iBonus) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iBonus < 0 || iBonus >= GC.getNumBonusInfos()) return 0;
	return pCity->getNumBonuses((BonusTypes)iBonus);
}

bool CyState::hasCorporation(int iPlayer, int iCity, int iCorporation) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iCorporation < 0 || iCorporation >= GC.getNumCorporationInfos()) return false;
	return pCity->isHasCorporation((CorporationTypes)iCorporation);
}

int CyState::getProjectProduction(int iPlayer, int iCity, int iProject) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || iProject < 0 || iProject >= GC.getNumProjectInfos()) return 0;
	return pCity->getProjectProduction((ProjectTypes)iProject);
}

int CyState::getHandicap(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	return pCity ? (int)pCity->getHandicapType() : -1;
}

python::list CyState::getCityProperties(int iPlayer, int iCity) const
{
	python::list rows = python::list();
	CvCity* pCity = const_cast<CvCity*>(cys_city(iPlayer, iCity));
	if (pCity == NULL) return rows;
	CvProperties* pProperties = pCity->getProperties();
	if (pProperties == NULL) return rows;
	const int iNum = pProperties->getNumProperties();
	for (int i = 0; i < iNum; ++i)
	{
		const PropertyTypes eProperty = pProperties->getProperty(i);
		python::list row = python::list();
		row.append((int)eProperty);
		row.append(pProperties->getValueByProperty(eProperty));
		row.append(pProperties->getChangeByProperty(eProperty));
		rows.append(row);
	}
	return rows;
}

python::list CyState::getCityFlags(int iPlayer, int iCity) const
{
	int values[NUM_CITY_FLAGS] = { 0 };
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity) pCity->getCityFlags(values);
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

// ---- THE UNIT PLANE ----

python::list CyState::getUnitRead(int iPlayer, int iUnit) const
{
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
	int values[NUM_UNIT_FLAGS] = { 0 };
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit) pUnit->getUnitFlags(values);
	return cys_toList(values);
}

std::wstring CyState::getUnitNameNoDesc(int iPlayer, int iUnit) const
{
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? std::wstring(pUnit->getNameNoDesc()) : std::wstring();
}

std::string CyState::getUnitScriptData(int iPlayer, int iUnit) const
{
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? pUnit->getScriptData() : std::string();
}

std::wstring CyState::getUnitName(int iPlayer, int iUnit) const
{
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	return pUnit ? std::wstring(pUnit->getName()) : std::wstring();
}

python::list CyState::getPlotUnitIds(int iX, int iY) const
{
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
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iTeam < 0 || iTeam >= MAX_TEAMS)
	{
		return false;
	}
	return pUnit->isInvisible((TeamTypes)iTeam, false);
}

bool CyState::hasUnitPromotion(int iPlayer, int iUnit, int iPromotion) const
{
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->isHasPromotion((PromotionTypes)iPromotion);
}

bool CyState::isUnitPromotionOverridden(int iPlayer, int iUnit, int iPromotion) const
{
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->isPromotionOverriden((PromotionTypes)iPromotion);
}

bool CyState::isUnitActionRecommended(int iPlayer, int iUnit, int iAction) const
{
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
	const CvUnit* pUnit = cys_unit(iPlayer, iUnit);
	if (pUnit == NULL) return false;
	const int iNumUnits = GC.getNumUnitInfos();
	for (int iToUnit = 0; iToUnit < iNumUnits; ++iToUnit)
	{
		if (pUnit->canUpgrade((UnitTypes)iToUnit, true)) return true;
	}
	return false;
}

bool CyState::canUnitUpgrade(int iPlayer, int iUnit, int iToUnit, bool bTestVisible) const
{
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

std::wstring CyState::getProductionName(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || pCity->getProductionName() == NULL) return std::wstring();
	return std::wstring(pCity->getProductionName());
}

std::wstring CyState::getProductionNameKey(int iPlayer, int iCity) const
{
	const CvCity* pCity = cys_city(iPlayer, iCity);
	if (pCity == NULL || pCity->getProductionNameKey() == NULL) return std::wstring();
	return std::wstring(pCity->getProductionNameKey());
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
		.def("getLiberationPlayer",      &CyState::getLiberationPlayer)
		.def("getMaintenance",           &CyState::getMaintenance)
		// ENUMERATION + CITY rank groups + plain city facts
		.def("getHeadSelectedCityId",    &CyState::getHeadSelectedCityId)
		.def("getHeadSelectedUnitId",    &CyState::getHeadSelectedUnitId)
		.def("getSelectedUnitIds",       &CyState::getSelectedUnitIds)
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
		.def("getCountdowns",            &CyState::getCountdowns)
		.def("getOrder",                 &CyState::getOrder)
		.def("getGrowth",                &CyState::getGrowth)
		.def("getCulture",               &CyState::getCulture)
		.def("getCityFlags",             &CyState::getCityFlags)
		.def("getBuildingInCity",        &CyState::getBuildingInCity)
		.def("getUnitInCity",            &CyState::getUnitInCity)
		.def("getProductionTurnsLeft",   &CyState::getProductionTurnsLeft)
		.def("getSpecialistInCity",      &CyState::getSpecialistInCity)
		.def("getCityCounts",            &CyState::getCityCounts)
		.def("getTradeRoutes",           &CyState::getTradeRoutes)
		.def("getCityReligions",         &CyState::getCityReligions)
		.def("getCityCorporations",      &CyState::getCityCorporations)
		.def("getCultureForPlayer",      &CyState::getCultureForPlayer)
		.def("getCulturePercent",        &CyState::getCulturePercent)
		.def("getTradeYield",            &CyState::getTradeYield)
		.def("getBestUnit",              &CyState::getBestUnit)
		.def("getBestUnitForRole",       &CyState::getBestUnitForRole)
		.def("getCityOutputHistory",     &CyState::getCityOutputHistory)
		.def("getNumBonuses",            &CyState::getNumBonuses)
		.def("hasCorporation",           &CyState::hasCorporation)
		.def("getProjectProduction",     &CyState::getProjectProduction)
		.def("getHandicap",              &CyState::getHandicap)
		.def("getCityProperties",        &CyState::getCityProperties)
		.def("getUnitListGroups",        &CyState::getUnitListGroups)
		.def("getBuildingListGroups",    &CyState::getBuildingListGroups)
		.def("getUnitRead",              &CyState::getUnitRead)
		.def("getUnitFlags",             &CyState::getUnitFlags)
		.def("getUnitName",              &CyState::getUnitName)
		.def("getUnitNameNoDesc",        &CyState::getUnitNameNoDesc)
		.def("getUnitScriptData",        &CyState::getUnitScriptData)
		.def("getPlotUnitIds",           &CyState::getPlotUnitIds)
		.def("isUnitInvisible",          &CyState::isUnitInvisible)
		.def("hasUnitPromotion",         &CyState::hasUnitPromotion)
		.def("isUnitPromotionOverridden",&CyState::isUnitPromotionOverridden)
		.def("isUnitActionRecommended",  &CyState::isUnitActionRecommended)
		.def("canUnitUpgrade",           &CyState::canUnitUpgrade)
		.def("canUnitUpgradeToAny",      &CyState::canUnitUpgradeToAny)
		.def("isCityRevealed",           &CyState::isCityRevealed)
		.def("isEmphasize",              &CyState::isEmphasize)
		.def("getHurryQuote",            &CyState::getHurryQuote)
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
		.def("getProductionName",        &CyState::getProductionName)
		.def("getProductionNameKey",     &CyState::getProductionNameKey)
		;

	// The EMPIRE selector, named rather than left as a bare -1 at every call site: a script reads the empire by
	// passing it where a city id would go, and the name is what makes that legible.
	python::scope().attr("STATE_EMPIRE") = -1;
}
