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
		;
}
