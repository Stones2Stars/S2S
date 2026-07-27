//
//	CyEnabler -- the Python availability surface (see the header for the role, the grammar and the boost rule).
//	Every body here is a BARE RELAY of a maintained read: resolve the owner, fetch, return. No gate runs, no
//	calculator is called, and nothing is recomputed -- exactly as on the C++ side, so a missed propagation shows
//	up as a visibly wrong verdict in script too, rather than being silently repaired at the boundary.
//

#include "CvGameCoreDLL.h"
#include "CyEnabler.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"          // GET_PLAYER
#include "Enabler/CvEnabler.h"      // EnablerDomain::State -- the tri-state this surface returns

namespace
{
	// Resolve without asserting: a script may legitimately hold a stale id, and the honest answer to "is this
	// available" for something that does not exist is HIDDEN, not a crash.
	const CvPlayer* cye_player(int iPlayer)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS) return NULL;
		return &GET_PLAYER((PlayerTypes)iPlayer);
	}

	const CvCity* cye_city(int iPlayer, int iCity)
	{
		const CvPlayer* pPlayer = cye_player(iPlayer);
		return pPlayer ? pPlayer->getCity(iCity) : NULL;
	}

	python::list cye_toList(const std::vector<int>& ids)
	{
		python::list list = python::list();
		for (size_t i = 0; i < ids.size(); ++i)
		{
			list.append(ids[i]);
		}
		return list;
	}
}

// ---- CITY domains ----

int CyEnabler::getBuildingAvailability(int iPlayer, int iCity, int eBuilding) const
{
	const CvCity* pCity = cye_city(iPlayer, iCity);
	return pCity ? (int)pCity->getBuildingAvailability((BuildingTypes)eBuilding)
	             : (int)EnablerDomain::STATE_HIDDEN;
}

int CyEnabler::getUnitAvailability(int iPlayer, int iCity, int eUnit) const
{
	const CvCity* pCity = cye_city(iPlayer, iCity);
	return pCity ? (int)pCity->getUnitAvailability((UnitTypes)eUnit)
	             : (int)EnablerDomain::STATE_HIDDEN;
}

bool CyEnabler::isBuildingContinuable(int iPlayer, int iCity, int eBuilding) const
{
	const CvCity* pCity = cye_city(iPlayer, iCity);
	return pCity ? pCity->isBuildingContinuable((BuildingTypes)eBuilding) : false;
}

python::list CyEnabler::getAvailableBuildings(int iPlayer, int iCity) const
{
	std::vector<int> ids;
	const CvCity* pCity = cye_city(iPlayer, iCity);
	if (pCity) pCity->getAvailableBuildings(ids);
	return cye_toList(ids);
}

python::list CyEnabler::getAvailableUnits(int iPlayer, int iCity) const
{
	std::vector<int> ids;
	const CvCity* pCity = cye_city(iPlayer, iCity);
	if (pCity) pCity->getAvailableUnits(ids);
	return cye_toList(ids);
}

// ---- PLAYER domains ----

int CyEnabler::getTechAvailability(int iPlayer, int eTech) const
{
	const CvPlayer* p = cye_player(iPlayer);
	return p ? (int)p->getTechAvailability((TechTypes)eTech) : (int)EnablerDomain::STATE_HIDDEN;
}

int CyEnabler::getCivicAvailability(int iPlayer, int eCivic) const
{
	const CvPlayer* p = cye_player(iPlayer);
	return p ? (int)p->getCivicAvailability((CivicTypes)eCivic) : (int)EnablerDomain::STATE_HIDDEN;
}

int CyEnabler::getProjectAvailability(int iPlayer, int eProject) const
{
	const CvPlayer* p = cye_player(iPlayer);
	return p ? (int)p->getProjectAvailability((ProjectTypes)eProject) : (int)EnablerDomain::STATE_HIDDEN;
}

int CyEnabler::getProcessAvailability(int iPlayer, int eProcess) const
{
	const CvPlayer* p = cye_player(iPlayer);
	return p ? (int)p->getProcessAvailability((ProcessTypes)eProcess) : (int)EnablerDomain::STATE_HIDDEN;
}

python::list CyEnabler::getAvailableTechs(int iPlayer) const
{
	std::vector<int> ids;
	const CvPlayer* p = cye_player(iPlayer);
	if (p) p->getAvailableTechs(ids);
	return cye_toList(ids);
}

python::list CyEnabler::getAvailableCivics(int iPlayer) const
{
	std::vector<int> ids;
	const CvPlayer* p = cye_player(iPlayer);
	if (p) p->getAvailableCivics(ids);
	return cye_toList(ids);
}

python::list CyEnabler::getAvailableProjects(int iPlayer) const
{
	std::vector<int> ids;
	const CvPlayer* p = cye_player(iPlayer);
	if (p) p->getAvailableProjects(ids);
	return cye_toList(ids);
}

python::list CyEnabler::getAvailableProcesses(int iPlayer) const
{
	std::vector<int> ids;
	const CvPlayer* p = cye_player(iPlayer);
	if (p) p->getAvailableProcesses(ids);
	return cye_toList(ids);
}

// ---- the carve-outs: the UNLOCKED half only ----

int CyEnabler::getBuildUnlocked(int iPlayer, int eBuild) const
{
	const CvPlayer* p = cye_player(iPlayer);
	return p ? (int)p->getBuildUnlocked((BuildTypes)eBuild) : (int)EnablerDomain::STATE_HIDDEN;
}

int CyEnabler::getPromotionUnlocked(int iPlayer, int ePromotion) const
{
	const CvPlayer* p = cye_player(iPlayer);
	return p ? (int)p->getPromotionUnlocked((PromotionTypes)ePromotion) : (int)EnablerDomain::STATE_HIDDEN;
}

// ---- the empire-wide fan ----

int CyEnabler::getUnitAvailabilityAnywhere(int iPlayer, int eUnit) const
{
	const CvPlayer* p = cye_player(iPlayer);
	return p ? (int)p->getUnitAvailabilityAnywhere((UnitTypes)eUnit) : (int)EnablerDomain::STATE_HIDDEN;
}

int CyEnabler::getBuildingAvailabilityAnywhere(int iPlayer, int eBuilding) const
{
	const CvPlayer* p = cye_player(iPlayer);
	return p ? (int)p->getBuildingAvailabilityAnywhere((BuildingTypes)eBuilding) : (int)EnablerDomain::STATE_HIDDEN;
}
