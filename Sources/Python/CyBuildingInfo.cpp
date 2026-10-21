#include "CvGameCoreDLL.h"
#include "CyBuildingInfo.h"
#include "Infos/CvBuildingInfo.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	The ONE bounds gate for this registry. A read that cannot be answered returns the caller's neutral value
	//	rather than reaching a registry slot that does not exist -- the id arrives from script, so it is checked
	//	here rather than trusted (docs/architecture/patterns.md §WRITE-ONCE-AT-LOAD: a read never creates, and FASSERT_BOUNDS is compiled
	//	out of Release, which is where this runs).
	const CvBuildingInfo* cyb_building(int iBuilding)
	{
		if (iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos())
		{
			return NULL;
		}
		return &GC.getBuildingInfo((BuildingTypes)iBuilding);
	}
}

int CyBuildingInfo::getSpecialBuilding(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->getSpecialBuildingType() : NO_SPECIALBUILDING;
}

int CyBuildingInfo::getWonderScope(int iBuilding) const
{
	return cyb_building(iBuilding) ? wonderScope((BuildingTypes)iBuilding) : -1;
}

bool CyBuildingInfo::isLimitedWonder(int iBuilding) const
{
	return cyb_building(iBuilding) ? wonderScope((BuildingTypes)iBuilding) >= 0 : false;
}

bool CyBuildingInfo::isRelocatable(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->isNoInstanceLimit() : false;
}

bool CyBuildingInfo::isEmpireLevel(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->isEmpireLevel() : false;
}

int CyBuildingInfo::getVoteSource(int iBuilding) const
{
	//	⚠ The INFO-side getter is still named for the legacy XML tag (`getDiploVoteType`); this read is named for
	//	what the value IS, which is the direction the rename goes ([todo.md]: diploVoteType -> the `voteSource`
	//	section). A consumer therefore never learns the legacy spelling.
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->getDiploVoteType() : NO_VOTESOURCE;
}

int CyBuildingInfo::getGreatPeopleUnit(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->getGreatPeopleUnitType() : -1;
}

int CyBuildingInfo::getCost(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->getCost() : 0;
}

int CyBuildingInfo::getReligion(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->getReligion() : NO_RELIGION;
}

int CyBuildingInfo::getHeadquartersCorporation(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->getHeadquartersCorporation() : NO_CORPORATION;
}

void CyBuildingInfo::pythonPublish()
{
	python::class_<CyBuildingInfo>("CyBuildingInfo")
		.def("getSpecialBuilding",          &CyBuildingInfo::getSpecialBuilding)
		.def("getWonderScope",              &CyBuildingInfo::getWonderScope)
		.def("isLimitedWonder",             &CyBuildingInfo::isLimitedWonder)
		.def("isRelocatable",               &CyBuildingInfo::isRelocatable)
		.def("isEmpireLevel",               &CyBuildingInfo::isEmpireLevel)
		.def("getVoteSource",               &CyBuildingInfo::getVoteSource)
		.def("getGreatPeopleUnit",          &CyBuildingInfo::getGreatPeopleUnit)
		.def("getCost",                     &CyBuildingInfo::getCost)
		.def("getReligion",                 &CyBuildingInfo::getReligion)
		.def("getHeadquartersCorporation",  &CyBuildingInfo::getHeadquartersCorporation)
		;
}
