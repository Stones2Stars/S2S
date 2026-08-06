//
//	CyAct -- the Python action surface (see the header for the role, the id rule and the line it does not cross).
//

#include "CvGameCoreDLL.h"
#include "CyAct.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvUnit.h"
#include "Infos/CvBuildInfo.h"
#include "UI/CvBuildingFilters.h"
#include "UI/CvBuildingSort.h"
#include "UI/CvUnitFilters.h"
#include "UI/CvUnitSort.h"
#include "Engine/CvUnitGrouping.h"
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"                           // GET_PLAYER
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"  // selectCity -- the engine action this relays

bool CyAct::selectCity(int iPlayer, int iCity, bool bTestProduction) const
{
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
	{
		return false;
	}
	CvCity* pCity = GET_PLAYER((PlayerTypes)iPlayer).getCity(iCity);
	if (pCity == NULL)
	{
		return false;
	}
	gDLL->getInterfaceIFace()->selectCity(pCity, bTestProduction);
	return true;
}

bool CyAct::selectUnitGroup(int iPlayer, int iUnit, bool bShift, bool bCtrl, bool bAlt) const
{
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
	{
		return false;
	}
	CvUnit* pUnit = GET_PLAYER((PlayerTypes)iPlayer).getUnit(iUnit);
	if (pUnit == NULL)
	{
		return false;
	}
	gDLL->getInterfaceIFace()->selectGroup(pUnit, bShift, bCtrl, bAlt);
	return true;
}

namespace
{
	//	The view-state setters all resolve the same way; a NULL city means the click had no subject and the
	//	action reports that rather than asserting, exactly as the reads do.
	CvCity* cya_city(int iPlayer, int iCity)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
		{
			return NULL;
		}
		return GET_PLAYER((PlayerTypes)iPlayer).getCity(iCity);
	}
}

bool CyAct::setBuildingListFilterActive(int iPlayer, int iCity, int iFilter, bool bActive) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iFilter < 0 || iFilter >= NUM_BUILDING_FILTERS)
	{
		return false;
	}
	pCity->setBuildingListFilterActive((BuildingFilterTypes)iFilter, bActive);
	return true;
}

bool CyAct::setBuildingListSorting(int iPlayer, int iCity, int iSorting) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iSorting < 0 || iSorting >= NUM_BUILDING_SORT)
	{
		return false;
	}
	pCity->setBuildingListSorting((BuildingSortTypes)iSorting);
	return true;
}

bool CyAct::setUnitListFilterActive(int iPlayer, int iCity, int iFilter, bool bActive) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iFilter < 0 || iFilter >= NUM_UNIT_FILTERS)
	{
		return false;
	}
	pCity->setUnitListFilterActive((UnitFilterTypes)iFilter, bActive);
	return true;
}

bool CyAct::setUnitListGrouping(int iPlayer, int iCity, int iGrouping) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iGrouping < 0 || iGrouping >= NUM_UNIT_GROUPING)
	{
		return false;
	}
	pCity->setUnitListGrouping((UnitGroupingTypes)iGrouping);
	return true;
}

bool CyAct::setUnitListSorting(int iPlayer, int iCity, int iSorting) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iSorting < 0 || iSorting >= NUM_UNIT_SORT)
	{
		return false;
	}
	pCity->setUnitListSorting((UnitSortTypes)iSorting);
	return true;
}

bool CyAct::invalidateUnitList(int iPlayer, int iCity) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL) return false;
	pCity->setUnitListInvalid();
	return true;
}

bool CyAct::invalidateBuildingList(int iPlayer, int iCity) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL) return false;
	pCity->setBuildingListInvalid();
	return true;
}

bool CyAct::setUnitPromotion(int iPlayer, int iUnit, int iPromotion, bool bNewValue) const
{
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS) return false;
	if (iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos()) return false;
	CvUnit* pUnit = GET_PLAYER((PlayerTypes)iPlayer).getUnit(iUnit);
	if (pUnit == NULL) return false;
	pUnit->setHasPromotion((PromotionTypes)iPromotion, bNewValue);
	return true;
}

//	---- The SCENARIO APPLY (see the header for why these earn their place) ----
//	Every one resolves the (owner, id) pair and calls the ENGINE'S OWN setter, so the fact the normal path emits
//	is emitted here too. ⛔ None of them writes a member directly: a quieter WB path is exactly the hole the
//	model exists to close ([roadmap] scope decision 1b -- no WB special case anywhere).

bool CyAct::setCityName(int iPlayer, int iCity, std::wstring szName) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL) return false;
	pCity->setName(szName.c_str(), false);
	return true;
}

bool CyAct::setCityPopulation(int iPlayer, int iCity, int iPopulation) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL) return false;
	pCity->setPopulation(iPopulation);
	return true;
}

bool CyAct::setCityStoredFood(int iPlayer, int iCity, int iFood) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL) return false;
	pCity->setFood(iFood);
	return true;
}

bool CyAct::setCityCulture(int iPlayer, int iCity, int iForPlayer, int64_t iCulture) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iForPlayer < 0 || iForPlayer >= MAX_PLAYERS) return false;
	pCity->setCultureTimes100((PlayerTypes)iForPlayer, iCulture, false, false);
	return true;
}

bool CyAct::setCityScriptData(int iPlayer, int iCity, std::string szData) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL) return false;
	pCity->setScriptData(szData);
	return true;
}

bool CyAct::addCityBuilding(int iPlayer, int iCity, int iBuilding) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return false;
	pCity->changeHasBuilding((BuildingTypes)iBuilding, true);
	return true;
}

//	bAnnounce is FALSE: a scenario being loaded is not a founding, so it owes the player no popup. The DOMAIN
//	fact still fires -- that is the setter's job and is exactly what must not be skipped.
bool CyAct::setCityReligion(int iPlayer, int iCity, int iReligion, bool bHolyCity) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iReligion < 0 || iReligion >= GC.getNumReligionInfos()) return false;
	pCity->setHasReligion((ReligionTypes)iReligion, true, false, true);
	if (bHolyCity) GC.getGame().setHolyCity((ReligionTypes)iReligion, pCity, false);
	return true;
}

bool CyAct::setCityCorporation(int iPlayer, int iCity, int iCorporation, bool bHeadquarters) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iCorporation < 0 || iCorporation >= GC.getNumCorporationInfos()) return false;
	pCity->setHasCorporation((CorporationTypes)iCorporation, true, false, true);
	if (bHeadquarters) GC.getGame().setHeadquarters((CorporationTypes)iCorporation, pCity, false);
	return true;
}

//	bUnattributed = TRUE: a scenario's free specialists have no live source to die with, which is precisely the
//	UNATTRIBUTED ledger's meaning ([legacy-grant-apply-sites.md]: genuine one-shot state).
bool CyAct::addCityFreeSpecialist(int iPlayer, int iCity, int iSpecialist, int iChange) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iSpecialist < 0 || iSpecialist >= GC.getNumSpecialistInfos()) return false;
	pCity->changeFreeSpecialistCount((SpecialistTypes)iSpecialist, iChange, true);
	return true;
}

bool CyAct::pushCityOrder(int iPlayer, int iCity, int iOrderType, int iId) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iId < 0) return false;
	pCity->pushOrder((OrderTypes)iOrderType, iId, -1, false, false, false, true);
	return true;
}

//	The engine exposes a CHANGE for damage and a SET for the timer; the scenario states an absolute in both
//	cases, so the damage one is expressed as the delta from where it stands.
bool CyAct::setCityDefenseDamage(int iPlayer, int iCity, int iDamage) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL) return false;
	pCity->changeDefenseDamage(iDamage - pCity->getDefenseDamage());
	return true;
}

bool CyAct::setCityOccupation(int iPlayer, int iCity, int iTurns) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL) return false;
	pCity->setOccupationTimer(iTurns);
	return true;
}

bool CyAct::setCityGrantedExtra(int iPlayer, int iCity, int iKind, int iValue) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL) return false;
	switch (iKind)
	{
	case GRANTED_EXTRA_HAPPINESS:    pCity->changeExtraHappiness(iValue - pCity->getExtraHappiness()); return true;
	case GRANTED_EXTRA_HEALTH:       pCity->changeExtraHealth(iValue - pCity->getExtraHealth()); return true;
	}
	return false;
}

//	⚠ The engine's setBuildingYieldChange/CommerceChange are CHANGERS despite the name (they add), so the
//	scenario's absolute is applied as the delta. Getting this backwards doubles a reloaded scenario's grants.
bool CyAct::setBuildingGrantedYield(int iPlayer, int iCity, int iBuilding, int iYield, int iValue) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return false;
	if (iYield < 0 || iYield >= NUM_YIELD_TYPES) return false;
	const int iNow = pCity->getBuildingYieldChange((BuildingTypes)iBuilding, (YieldTypes)iYield);
	pCity->setBuildingYieldChange((BuildingTypes)iBuilding, (YieldTypes)iYield, iValue - iNow);
	return true;
}

bool CyAct::setBuildingGrantedCommerce(int iPlayer, int iCity, int iBuilding, int iCommerce, int iValue) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return false;
	if (iCommerce < 0 || iCommerce >= NUM_COMMERCE_TYPES) return false;
	const int iNow = pCity->getBuildingCommerceChange((BuildingTypes)iBuilding, (CommerceTypes)iCommerce);
	pCity->setBuildingCommerceChange((BuildingTypes)iBuilding, (CommerceTypes)iCommerce, iValue - iNow);
	return true;
}

bool CyAct::setBuildingGrantedWellbeing(int iPlayer, int iCity, int iBuilding, int iKind, int iValue) const
{
	CvCity* pCity = cya_city(iPlayer, iCity);
	if (pCity == NULL || iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return false;
	switch (iKind)
	{
	case BUILDING_GRANTED_HAPPINESS:
		pCity->setBuildingHappyChange((BuildingTypes)iBuilding, iValue - pCity->getBuildingHappyChange((BuildingTypes)iBuilding));
		return true;
	case BUILDING_GRANTED_HEALTH:
		pCity->setBuildingHealthChange((BuildingTypes)iBuilding, iValue - pCity->getBuildingHealthChange((BuildingTypes)iBuilding));
		return true;
	}
	return false;
}

bool CyAct::setBuildDisabled(int iBuild, bool bDisabled) const
{
	if (iBuild < 0 || iBuild >= GC.getNumBuildInfos()) return false;
	GC.getBuildInfo((BuildTypes)iBuild).setDisabled(bDisabled);
	return true;
}

void CyAct::pythonPublish()
{
	OutputDebugString("Python Extension Module - CyAct\n");

	python::class_<CyAct>("CyAct")
		.def("selectCity", &CyAct::selectCity)
		.def("selectUnitGroup", &CyAct::selectUnitGroup)
		.def("setBuildingListFilterActive", &CyAct::setBuildingListFilterActive)
		.def("setBuildingListSorting", &CyAct::setBuildingListSorting)
		.def("setUnitListFilterActive", &CyAct::setUnitListFilterActive)
		.def("setUnitListGrouping", &CyAct::setUnitListGrouping)
		.def("setUnitListSorting", &CyAct::setUnitListSorting)
		.def("invalidateUnitList", &CyAct::invalidateUnitList)
		.def("invalidateBuildingList", &CyAct::invalidateBuildingList)
		.def("setBuildDisabled", &CyAct::setBuildDisabled)
		.def("setUnitPromotion", &CyAct::setUnitPromotion)
		// the SCENARIO APPLY
		.def("setCityName", &CyAct::setCityName)
		.def("setCityPopulation", &CyAct::setCityPopulation)
		.def("setCityStoredFood", &CyAct::setCityStoredFood)
		.def("setCityCulture", &CyAct::setCityCulture)
		.def("setCityScriptData", &CyAct::setCityScriptData)
		.def("addCityBuilding", &CyAct::addCityBuilding)
		.def("setCityReligion", &CyAct::setCityReligion)
		.def("setCityCorporation", &CyAct::setCityCorporation)
		.def("addCityFreeSpecialist", &CyAct::addCityFreeSpecialist)
		.def("pushCityOrder", &CyAct::pushCityOrder)
		.def("setCityDefenseDamage", &CyAct::setCityDefenseDamage)
		.def("setCityOccupation", &CyAct::setCityOccupation)
		.def("setCityGrantedExtra", &CyAct::setCityGrantedExtra)
		.def("setBuildingGrantedYield", &CyAct::setBuildingGrantedYield)
		.def("setBuildingGrantedCommerce", &CyAct::setBuildingGrantedCommerce)
		.def("setBuildingGrantedWellbeing", &CyAct::setBuildingGrantedWellbeing)
		;
}
