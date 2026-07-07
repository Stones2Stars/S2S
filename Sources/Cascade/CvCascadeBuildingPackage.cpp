//
//	BuildingPackage -- StoneBase BuildingPackage.cs (see the header). Ported VERBATIM from CvCascadeModifierMath.cpp's
//	file-static cvModifierBuildingFlat; promoted to a declared surface (the single-source law, patterns.md). LOGIC
//	unchanged: only the signature + the MMKernel-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeBuildingPackage.h"
#include "CvCascadeMMKernel.h"
#include "CvCascadeDepositIndex.h"     // DepositIndex::whenObsoleteFor -- the obsolete tree's compiled records
#include "CvJsonInfo.h"                // CvJsonInfo
#include "Repos/InfoRepo.h"            // InfoRepo<CvBuildingInfo>::get().get(id)
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Infos/CvBuildingInfo.h"

// AFTER tier -- BuildingPackage (modifier.md §2a / calc-map §1.4): Σ ACTIVE buildings' {ch}.city.flat +
// {ch}.city.perPopulation × population, ×100. The lone AFTER term in the §1 yield rate (added flat OUTSIDE the percent
// stack); also the per-building own-flat the §2 commerce splitter folds in as BASE. Same value, two tier tags.
// channel -> engine enum, for the persisted event-store folds below (NO_* = not that plane's channel)
static YieldTypes bp_yieldFromChannel(const std::string& channel)
{
	if (channel == "food") return YIELD_FOOD;
	if (channel == "production") return YIELD_PRODUCTION;
	if (channel == "commerce") return YIELD_COMMERCE;
	return NO_YIELD;
}
static CommerceTypes bp_commerceFromChannel(const std::string& channel)
{
	if (channel == "gold") return COMMERCE_GOLD;
	if (channel == "research") return COMMERCE_RESEARCH;
	if (channel == "culture") return COMMERCE_CULTURE;
	if (channel == "espionage") return COMMERCE_ESPIONAGE;
	return NO_COMMERCE;
}

long BuildingPackage::buildingFlat(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const std::string wantCity = channel + ".city";
	const int pop = pCity->getPopulation();
	const int nB = GC.getNumBuildingInfos();
	// The PERSISTED EVENT/VOTE stores ride in as RAW SAVED STATE (owner ruling 2026-07-04: past
	// parity-hunting -- completeness; a store the cascade skips is silently LOST at the cut). Per-building
	// event/vote yield grants (m_aBuildingYieldChange: applyEvent/processVoteSourceBonus/WB) and event
	// commerce grants (m_aBuildingCommerceChangeEvents) pay while the granted building is ACTIVE, x100 --
	// the legacy tiers verbatim (CvCity:4733 extra-yield fold; getBuildingCommerceByBuilding city half).
	// The EVENT_FULLERENES_1 Oxford specimen (+10 research on the Chemistry Lab) is the live proof case.
	const YieldTypes eStoreY = bp_yieldFromChannel(channel);
	const CommerceTypes eStoreC = (eStoreY == NO_YIELD) ? bp_commerceFromChannel(channel) : NO_COMMERCE;
	long sum = 0;
	for (int b = 0; b < nB; ++b)
	{
		const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (cascadeIsBuildingActive(b, ec))   // present + non-dormant in THIS city (cascade-computed)
		{
			if (d != NULL)
			{
				sum += MMKernel::sumUnit100(d, wantCity, "flat", ec);
				sum += MMKernel::sumUnit100(d, wantCity, "perPopulation", ec) * pop;
			}
			if (eStoreY != NO_YIELD) sum += 100L * pCity->getBuildingYieldChange((BuildingTypes)b, eStoreY);
			else if (eStoreC != NO_COMMERCE) sum += 100L * pCity->getBuildingCommerceChangeEvents((BuildingTypes)b, eStoreC);
		}
		else if (d != NULL && cascadeIsBuildingObsolete(b, ec))   // obsolete -> the whenObsolete tree (json §4.2), read from the obsoletion-process set
		{
			sum += MMKernel::sumUnit100From(DepositIndex::whenObsoleteFor(d), wantCity, "flat", ec);
			sum += MMKernel::sumUnit100From(DepositIndex::whenObsoleteFor(d), wantCity, "perPopulation", ec) * pop;
		}
	}
	return sum;
}
