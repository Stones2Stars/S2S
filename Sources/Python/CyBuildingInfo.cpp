#include "CvGameCoreDLL.h"
#include "CyBuildingInfo.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvSpecialBuildingInfo.h"
#include "Infos/CvAllowed.h"
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

	//	WHICH scope an `allowed` block caps at, highest-binding first -- the wonder CATEGORY, derived from the
	//	authored cap rather than from a mirror flag ([json.md] §4.4).
	int cyb_selfCapScope(const CvAllowed* pAllowed)
	{
		if (pAllowed == NULL) return -1;
		if (pAllowed->cap(ALLOWEDCAP_WORLD)  >= 0) return ALLOWEDCAP_WORLD;
		if (pAllowed->cap(ALLOWEDCAP_TEAM)   >= 0) return ALLOWEDCAP_TEAM;
		if (pAllowed->cap(ALLOWEDCAP_EMPIRE) >= 0) return ALLOWEDCAP_EMPIRE;
		return -1;
	}

	//	⛔ TWO WAYS TO BE CAPPED, and reading only the first is wrong. A building may carry its own self-cap, OR
	//	belong to a SPECIALBUILDING GROUP that holds the cap for all its members (json.md §4.4: the member authors
	//	identity.specialBuildingType, the GROUP entity holds `allowed`). A grouped wonder authors no cap of its
	//	own, so a self-cap-only test calls it unlimited -- silently, and only for the grouped ones. This mirrors
	//	the enabler's own gate (CvBuildingEnabler bd_groupCapOk), so there is ONE meaning of "capped".
	//
	//	⚠ The CATEGORY count-caps (worldWonders/teamWonders/nationalWonders) are a DIFFERENT axis -- a per-city
	//	bound set by CultureLevel -- and deliberately do not make a building "limited".
	int cyb_capScope(const CvBuildingInfo& kBuilding)
	{
		const int iOwn = cyb_selfCapScope(kBuilding.getAllowed());
		if (iOwn >= 0) return iOwn;

		const int iSpecialBuilding = kBuilding.getSpecialBuildingType();
		if (iSpecialBuilding != NO_SPECIALBUILDING)
		{
			return cyb_selfCapScope(GC.getSpecialBuildingInfo((SpecialBuildingTypes)iSpecialBuilding).getAllowed());
		}
		return -1;
	}
}

int CyBuildingInfo::getSpecialBuilding(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->getSpecialBuildingType() : NO_SPECIALBUILDING;
}

int CyBuildingInfo::getWonderScope(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? cyb_capScope(*pBuilding) : -1;
}

bool CyBuildingInfo::isLimitedWonder(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? cyb_capScope(*pBuilding) >= 0 : false;
}

bool CyBuildingInfo::isRelocatable(int iBuilding) const
{
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->isNoInstanceLimit() : false;
}

int CyBuildingInfo::getVoteSource(int iBuilding) const
{
	//	⚠ The INFO-side getter is still named for the legacy XML tag (`getDiploVoteType`); this read is named for
	//	what the value IS, which is the direction the rename goes ([todo.md]: diploVoteType -> the `voteSource`
	//	section). A consumer therefore never learns the legacy spelling.
	const CvBuildingInfo* pBuilding = cyb_building(iBuilding);
	return pBuilding ? pBuilding->getDiploVoteType() : NO_VOTESOURCE;
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
		.def("getVoteSource",               &CyBuildingInfo::getVoteSource)
		.def("getCost",                     &CyBuildingInfo::getCost)
		.def("getReligion",                 &CyBuildingInfo::getReligion)
		.def("getHeadquartersCorporation",  &CyBuildingInfo::getHeadquartersCorporation)
		;
}
