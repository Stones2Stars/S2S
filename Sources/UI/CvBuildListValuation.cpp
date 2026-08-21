#include "CvGameCoreDLL.h"
#include "CvBuildListValuation.h"
#include "AI/CvPlayerAI.h"          // GET_PLAYER -- the standard player fetch (10 other UI files take it here)
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Infos/CvBuildingInfo.h"

const CvCity* CvBuildListValuation::valuationCity(const CvPlayer* pPlayer, const CvCity* pCity)
{
	if (pCity != NULL)
	{
		return pCity;
	}
	if (pPlayer != NULL)
	{
		return pPlayer->getCapitalCity();
	}
	return NULL;
}

bool CvBuildListValuation::buildingFlatYields(BuildingTypes eBuilding, const CvPlayer* pPlayer,
	const CvCity* pCity, int (&aFlatYields)[NUM_YIELD_TYPES])
{
	const CvCity* pValueCity = valuationCity(pPlayer, pCity);
	if (pValueCity == NULL)
	{
		return false;
	}
	const CvPlayer& kOwner = GET_PLAYER(pValueCity->getOwner());

	GC.getBuildingInfo(eBuilding).expectedFlatYields(
		pValueCity->getCityContext(), kOwner.getEmpireContext(),
		pValueCity->plotGroup(pValueCity->getOwner()), aFlatYields);
	return true;
}

bool CvBuildListValuation::buildingYieldModifiers(BuildingTypes eBuilding, const CvPlayer* pPlayer,
	const CvCity* pCity, int (&aYieldModifiers)[NUM_YIELD_TYPES])
{
	const CvCity* pValueCity = valuationCity(pPlayer, pCity);
	if (pValueCity == NULL)
	{
		return false;
	}
	const CvPlayer& kOwner = GET_PLAYER(pValueCity->getOwner());

	GC.getBuildingInfo(eBuilding).expectedYieldModifiers(
		pValueCity->getCityContext(), kOwner.getEmpireContext(),
		pValueCity->plotGroup(pValueCity->getOwner()), aYieldModifiers);
	return true;
}

bool CvBuildListValuation::buildingFlatCommerce(BuildingTypes eBuilding, const CvPlayer* pPlayer,
	const CvCity* pCity, int (&aFlatCommerce)[NUM_COMMERCE_TYPES])
{
	const CvCity* pValueCity = valuationCity(pPlayer, pCity);
	if (pValueCity == NULL)
	{
		return false;
	}
	const CvPlayer& kOwner = GET_PLAYER(pValueCity->getOwner());

	GC.getBuildingInfo(eBuilding).expectedFlatCommerce(
		pValueCity->getCityContext(), kOwner.getEmpireContext(),
		pValueCity->plotGroup(pValueCity->getOwner()), aFlatCommerce);
	return true;
}

bool CvBuildListValuation::buildingWellbeing(BuildingTypes eBuilding, const CvPlayer* pPlayer,
	const CvCity* pCity, int (&aWellbeing)[NUM_WELLBEING_CHANNELS])
{
	const CvCity* pValueCity = valuationCity(pPlayer, pCity);
	if (pValueCity == NULL)
	{
		return false;
	}
	const CvPlayer& kOwner = GET_PLAYER(pValueCity->getOwner());

	GC.getBuildingInfo(eBuilding).expectedWellbeing(
		pValueCity->getCityContext(), kOwner.getEmpireContext(),
		pValueCity->plotGroup(pValueCity->getOwner()), aWellbeing);
	return true;
}

bool CvBuildListValuation::buildingHappinessBalance(BuildingTypes eBuilding, const CvPlayer* pPlayer,
	const CvCity* pCity, int& iBalanceOut)
{
	int aWellbeing[NUM_WELLBEING_CHANNELS];
	if (!buildingWellbeing(eBuilding, pPlayer, pCity, aWellbeing))
	{
		return false;
	}
	iBalanceOut = aWellbeing[WELLBEING_HAPPINESS] - aWellbeing[WELLBEING_ANGER];
	return true;
}

bool CvBuildListValuation::buildingHealthBalance(BuildingTypes eBuilding, const CvPlayer* pPlayer,
	const CvCity* pCity, int& iBalanceOut)
{
	int aWellbeing[NUM_WELLBEING_CHANNELS];
	if (!buildingWellbeing(eBuilding, pPlayer, pCity, aWellbeing))
	{
		return false;
	}
	iBalanceOut = aWellbeing[WELLBEING_HEALTH] - aWellbeing[WELLBEING_UNHEALTH];
	return true;
}
