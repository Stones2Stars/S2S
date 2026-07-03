//
//	CascadeScalarChannels -- the #430 city scalar channels (see the header). Per channel: Σ the curated
//	deposits over the live source sets (ACTIVE buildings via the facts cache; adopted civics; held traits with
//	the PURE_TRAITS filter), conditions evaluated against the live ctx.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeScalarChannels.h"
#include "CvCascadeMMKernel.h"
#include "CvJsonInfo.h"
#include "CvJsonTraitInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvCity.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvCivicInfo.h"
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
	// the §9.5 stack: 100 + city + empire percents (buildings + civics + traits; the state-religion /
	// golden-age entries carry their conditions in the data and gate through the evaluator)
	int iMod = 100;
	iMod += sc_buildings("greatPeopleRate.city", "percent", pCity, ec);
	iMod += sc_buildings("greatPeopleRate.empire", "percent", pCity, ec);
	iMod += sc_civicsTraits("greatPeopleRate.city", "percent", owner, ec);
	iMod += sc_civicsTraits("greatPeopleRate.empire", "percent", owner, ec);
	return std::max(0, iMod);
}

int CascadeScalarChannels::defenseAmount(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	// the building city defense stack (legacy m_iBuildingDefense; natural/bonus/player defense are their
	// own legacy terms at the combine -- netted separately)
	return sc_buildings("defense.city.amount", "percent", pCity, ec);
}

int CascadeScalarChannels::maintenanceModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	// the effective-modifier stack (§5): city + empire scopes over buildings + civics + traits (the area
	// members ride the same walks -- area-scope deposits are rare and evaluated per city)
	int iMod = 0;
	iMod += sc_buildings("maintenance.city", "percent", pCity, ec);
	iMod += sc_buildings("maintenance.empire", "percent", pCity, ec);
	iMod += sc_buildings("maintenance.area", "percent", pCity, ec);
	iMod += sc_civicsTraits("maintenance.city", "percent", owner, ec);
	iMod += sc_civicsTraits("maintenance.empire", "percent", owner, ec);
	iMod += sc_civicsTraits("maintenance.area", "percent", owner, ec);
	return iMod;
}
