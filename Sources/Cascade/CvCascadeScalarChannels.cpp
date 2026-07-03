//
//	CascadeScalarChannels -- the #430 city scalar channels (see the header). Per channel: Σ the curated
//	deposits over the live source sets (ACTIVE buildings via the facts cache; adopted civics; held traits with
//	the PURE_TRAITS filter), conditions evaluated against the live ctx.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeScalarChannels.h"
#include "CvCascadeMMKernel.h"
#include "CvCascadeEnablerKernel.h"   // cityFacts -- the player-wide maintenance walk
#include "CvCascadeCityFacts.h"
#include "CvJsonInfo.h"
#include "CvJsonTraitInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "Engine/CvCity.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvTechInfo.h"
#include "Infos/CvSpecialistInfo.h"

// Σ a unit over the city's ACTIVE buildings at an address (the shared building-source walk).
static int sc_buildings(const std::string& addr, const char* unit, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const int nB = GC.getNumBuildingInfos();
	int iSum = 0;
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;
		const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, ec);
	}
	return iSum;
}

// Σ a unit over ALL the player's cities' ACTIVE buildings at an address (the player-accumulator semantic:
// an empire-scope building deposit feeds the player from ANY city).
static int sc_playerBuildings(const std::string& addr, const char* unit, const CvPlayer& owner, const CvTeam* pTeam)
{
	int iSum = 0, iLoop;
	for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
	{
		const CascadeCityFacts& facts = EnablerKernel::cityFacts(pc);
		CvCascadeEvalCtx pec;
		pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = pTeam;
		pec.activeBuildings = &facts.active; pec.vicinityProvidedBonuses = &facts.provided;
		for (std::set<int>::const_iterator it = facts.active.begin(); it != facts.active.end(); ++it)
		{
			const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
			if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, pec);
		}
	}
	return iSum;
}

// Σ a unit over the player's adopted civics + held traits (pure-filtered) at an address.
static int sc_civicsTraits(const std::string& addr, const char* unit, const CvPlayer& owner, const CvCascadeEvalCtx& ec)
{
	int iSum = 0;
	for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
	{
		const CivicTypes eCivic = owner.getCivics((CivicOptionTypes)i);
		if (eCivic == NO_CIVIC) continue;
		const CvJsonInfo* d = InfoRepo<CvCivicInfo>::get().get(eCivic);
		if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, ec);
	}
	for (int i = 0; i < GC.getNumTraitInfos(); ++i)
	{
		if (!owner.hasTrait((TraitTypes)i)) continue;
		const CvJsonTraitInfo* d = MMKernel::traitData(i);
		if (d != NULL) iSum += MMKernel::sumTrait(d, addr, unit, ec);
	}
	return iSum;
}

int CascadeScalarChannels::gpRateBase(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	// building flats + specialist flats × count (the player national rate is a live input at the combine)
	int iSum = sc_buildings("greatPeopleRate.city", "flat", pCity, ec);
	for (int i = 0; i < GC.getNumSpecialistInfos(); ++i)
	{
		const int iCount = pCity->getSpecialistCount((SpecialistTypes)i) + pCity->getFreeSpecialistCount((SpecialistTypes)i);
		if (iCount == 0) continue;
		const CvJsonInfo* d = InfoRepo<CvSpecialistInfo>::get().get(i);
		if (d != NULL) iSum += iCount * MMKernel::sumUnit(d, "greatPeopleRate.city", "flat", ec);
	}
	return iSum;
}

int CascadeScalarChannels::gpRateModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	// the §9.5 / :7153 stack: 100 + city + player percents...
	int iMod = 100;
	iMod += sc_buildings("greatPeopleRate.city", "percent", pCity, ec);
	iMod += sc_playerBuildings("greatPeopleRate.empire", "percent", owner, ec.team);   // GLOBAL GP mods feed the player from ANY city
	iMod += sc_civicsTraits("greatPeopleRate.city", "percent", owner, ec);
	iMod += sc_civicsTraits("greatPeopleRate.empire", "percent", owner, ec);
	// ...+ the STATE-RELIGION grouped family (civic stateReligion.empire.greatPeopleRate) while the state
	// religion is PRESENT IN THIS CITY (:7160)...
	{
		const ReligionTypes eState = owner.getStateReligion();
		if (eState != NO_RELIGION && pCity->isHasReligion(eState))
			iMod += sc_civicsTraits("stateReligion.empire.greatPeopleRate", "percent", owner, ec);
	}
	// ...+ the golden-age modifier -- a GLOBAL DEFINE, a config input (not data)
	if (owner.isGoldenAge())
		iMod += GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER();
	return std::max(0, iMod);
}

void CascadeScalarChannels::gpModParts(const CvCity* pCity, const CvCascadeEvalCtx& ec, int& iBld, int& iCivTrait, int& iSr)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	iBld = sc_buildings("greatPeopleRate.city", "percent", pCity, ec);
	iCivTrait = sc_playerBuildings("greatPeopleRate.empire", "percent", owner, ec.team);   // seeded with the player-wide building half
	iCivTrait += sc_civicsTraits("greatPeopleRate.city", "percent", owner, ec)
	           + sc_civicsTraits("greatPeopleRate.empire", "percent", owner, ec);
	iSr = 0;
	const ReligionTypes eState = owner.getStateReligion();
	if (eState != NO_RELIGION && pCity->isHasReligion(eState))
		iSr = sc_civicsTraits("stateReligion.empire.greatPeopleRate", "percent", owner, ec);
}

int CascadeScalarChannels::defenseAmount(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	// the building city defense stack (legacy m_iBuildingDefense; natural/bonus/player defense are their
	// own legacy terms at the combine -- netted separately)
	return sc_buildings("defense.city.amount", "percent", pCity, ec);
}

int CascadeScalarChannels::tradeRouteCount(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	int iCount = 0;
	// this city's extra routes (building city flats)
	iCount += sc_buildings("tradeRoutes.city", "flat", pCity, ec);
	// the player-wide global routes (any city's buildings' empire flats + civics/traits)
	iCount += sc_playerBuildings("tradeRoutes.empire", "flat", owner, ec.team);
	iCount += sc_civicsTraits("tradeRoutes.empire", "flat", owner, ec);
	// TECHS feed the player routes too (processTech :30911)
	{
		const CvTeam& team = GET_TEAM(owner.getTeam());
		for (int i = 0; i < GC.getNumTechInfos(); ++i)
		{
			if (!team.isHasTech((TechTypes)i)) continue;
			const CvJsonInfo* d = InfoRepo<CvTechInfo>::get().get(i);
			if (d != NULL) iCount += MMKernel::sumUnit(d, "tradeRoutes.empire", "flat", ec);
		}
	}
	// WORLD routes: ANY player's active world-wonder grants EVERY player (:7410) -- tradeRoutes.world flats
	// summed over all living players' active sets
	for (int p = 0; p < MAX_PC_PLAYERS; ++p)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)p);
		if (!kP.isAlive()) continue;
		iCount += sc_playerBuildings("tradeRoutes.world", "flat", kP, &GET_TEAM(kP.getTeam()));
	}
	// the coastal half pays only in coastal cities
	if (pCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
	{
		iCount += sc_playerBuildings("tradeRoutes.empire.coastal", "flat", owner, ec.team);
		iCount += sc_civicsTraits("tradeRoutes.empire.coastal", "flat", owner, ec);
	}
	return iCount;
}

int CascadeScalarChannels::maintenanceModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	// the effective-modifier stack (:getEffectiveMaintenanceModifier): THIS city's + the player's + the AREA
	// total + connected-to-capital. The player/area/connectedCity halves are PLAYER-WIDE building accumulators
	// (any city's active building feeds them), so those walk ALL the player's cities' active sets.
	int iMod = 0;
	iMod += sc_buildings("maintenance.city", "percent", pCity, ec);
	iMod += sc_civicsTraits("maintenance.city", "percent", owner, ec);
	iMod += sc_civicsTraits("maintenance.empire", "percent", owner, ec);
	iMod += sc_civicsTraits("maintenance.area", "percent", owner, ec);
	// TECHS feed the player maintenance modifier too (processTech CvPlayer:30916)
	{
		const CvTeam& team = GET_TEAM(owner.getTeam());
		for (int i = 0; i < GC.getNumTechInfos(); ++i)
		{
			if (!team.isHasTech((TechTypes)i)) continue;
			const CvJsonInfo* d = InfoRepo<CvTechInfo>::get().get(i);
			if (d != NULL) iMod += MMKernel::sumUnit(d, "maintenance.empire", "percent", ec);
		}
	}
	{
		const bool bConnected = pCity->isConnectedToCapital() && !pCity->isCapital();
		int iLoop;
		for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
		{
			const CascadeCityFacts& facts = EnablerKernel::cityFacts(pc);
			CvCascadeEvalCtx pec;
			pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = ec.team;
			pec.activeBuildings = &facts.active; pec.vicinityProvidedBonuses = &facts.provided;
			const bool bSameArea = pc->area() == pCity->area();
			for (std::set<int>::const_iterator it = facts.active.begin(); it != facts.active.end(); ++it)
			{
				const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
				if (d == NULL) continue;
				iMod += MMKernel::sumUnit(d, "maintenance.empire", "percent", pec);
				if (bSameArea) iMod += MMKernel::sumUnit(d, "maintenance.area", "percent", pec);
				else iMod += MMKernel::sumUnit(d, "maintenance.area.otherArea", "percent", pec);
				if (bConnected) iMod += MMKernel::sumUnit(d, "maintenance.empire.connectedCity", "percent", pec);
			}
		}
	}
	return iMod;
}
