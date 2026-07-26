//
//	CascadeProperty -- the #430 property channel (see the header). The per-city per-property sourced numbers
//	from the curated deposits; the engine keeps the integration (decay/targetLevel/solver ordering).
//

#include "CvGameCoreDLL.h"
#include "Property/CvPropertyChannel.h"
#include "Data/CvDepositRead.h"
#include "Data/CvDepositIndex.h"
#include "CvInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlot.h"
#include "Engine/CvUnit.h"
#include "CvBuildingInfo.h"
#include "CvUnitInfo.h"
#include "CvPropertyInfo.h"

// The interned family segment for a property type (the family IS the PROPERTY_* string), or -1 (never authored).
static int prop_famId(int eProp)
{
	if (eProp < 0 || eProp >= GC.getNumPropertyInfos()) return -1;
	return DepositIndex::lookupSegment(GC.getPropertyInfo((PropertyTypes)eProp).getType());
}

int CascadeProperty::citySourceFlat(int eProp, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const int famId = prop_famId(eProp);
	if (famId < 0 || pCity == NULL) return 0;
	const int scopeCity = DepositIndex::lookupSegment("city");
	const int unitFlat = DepositIndex::lookupSegment("flat");
	int iSum = 0;

	// -- ACTIVE buildings' city flats (constant per-turn sources + the curator-folded construction bag);
	// -- per-scaled entries (the legacy attribute-expression Mult(pop x C) sources) multiply by population --
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (d == NULL) continue;
		// active -> its normal `deposits`; obsolete -> its `whenObsolete` tree (json §4.2; part-1 delivery into the
		// SAME property number the solver consumes -- no combine/solver change). Neither -> dormant, contributes 0.
		const bool bActive = cascadeIsBuildingActive(b, ec);
		const bool bObsolete = !bActive && cascadeIsBuildingObsolete(b, ec);
		if (!bActive && !bObsolete) continue;
		const std::vector<CascadeDeposit>& deps = bObsolete ? DepositIndex::whenObsoleteFor(d) : DepositIndex::depositsFor(d);
		for (size_t i = 0; i < deps.size(); ++i)
		{
			const CascadeDeposit& dep = deps[i];
			if (dep.seg[0] != famId || dep.seg[1] != scopeCity || dep.nSeg != 2 || dep.unitId != unitFlat) continue;
			if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
			int v = dep.value / 100;
			if (dep.hasPer) v *= pCity->getPopulation();   // per:{POPULATION} is the only authored building form today
			iSum += v;
		}
	}

	// ⛔ NO unit walk here (owner ruling 2026-07-03: traveling unit modifiers ride on top -- cityUnitFlat)

	// -- the property's OWN self-deposits (the ATTRIBUTE source: flat per POPULATION etc.) --
	{
		const CvInfo* d = InfoRepo<CvPropertyInfo>::get().get(eProp);
		if (d != NULL)
		{
			const std::vector<CascadeDeposit>& pdeps = DepositIndex::depositsFor(d);
			for (size_t i = 0; i < pdeps.size(); ++i)
			{
				const CascadeDeposit& dep = pdeps[i];
				if (dep.seg[0] != famId || dep.seg[1] != scopeCity || dep.nSeg != 2 || dep.unitId != unitFlat) continue;
				if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
				int v = dep.value / 100;
				// the per count-scaler (the legacy ATTRIBUTE_CONSTANT: ×population is the only authored form today)
				if (dep.hasPer) v *= pCity->getPopulation();
				iSum += v;
			}
		}
	}
	return iSum;
}

int CascadeProperty::cityUnitFlat(int eProp, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const int famId = prop_famId(eProp);
	if (famId < 0 || pCity == NULL) return 0;
	const int scopeCity = DepositIndex::lookupSegment("city");
	const int scopePlot = DepositIndex::lookupSegment("plot");
	const int unitFlat = DepositIndex::lookupSegment("flat");
	int iSum = 0;
	// the SAME_PLOT emission (a criminal's crime lands on the city it stands in) -- LIVE, on top, never cached.
	// BOTH the city-scope AND plot-scope halves count: the city plot DIFFUSES to the city (the legacy
	// getTotalUnitSourcedProperty reads GAMEOBJECT_CITY and GAMEOBJECT_PLOT sources alike).
	foreach_(const CvUnit* pUnit, pCity->plot()->units())
	{
		const CvInfo* d = InfoRepo<CvUnitInfo>::get().get(pUnit->getUnitType());
		if (d == NULL) continue;
		const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(d);
		for (size_t i = 0; i < deps.size(); ++i)
		{
			const CascadeDeposit& dep = deps[i];
			if (dep.seg[0] != famId || (dep.seg[1] != scopeCity && dep.seg[1] != scopePlot)
				|| dep.nSeg != 2 || dep.unitId != unitFlat) continue;
			if (MMKernel::applies(dep.enabled, dep.disabled, ec)) iSum += dep.value / 100;
		}
	}
	return iSum;
}

int CascadeProperty::cityDecayPercent(int eProp)
{
	const int famId = prop_famId(eProp);
	if (famId < 0) return 0;
	const CvInfo* d = InfoRepo<CvPropertyInfo>::get().get(eProp);
	if (d == NULL) return 0;
	return MMKernel::sumUnconditioned(d, std::string(GC.getPropertyInfo((PropertyTypes)eProp).getType()) + ".city", "percent");
}
