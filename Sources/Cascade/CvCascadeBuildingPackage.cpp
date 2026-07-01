//
//	BuildingPackage -- StoneBase BuildingPackage.cs (see the header). Ported VERBATIM from CvCascadeModifierMath.cpp's
//	file-static cvModifierBuildingFlat; promoted to a declared surface (the single-source law, patterns.md). LOGIC
//	unchanged: only the signature + the MMKernel-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeBuildingPackage.h"
#include "CvCascadeMMKernel.h"
#include "CvJsonInfo.h"                // CvJsonInfo
#include "Repos/InfoRepo.h"            // InfoRepo<CvBuildingInfo>::get().get(id)
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Infos/CvBuildingInfo.h"

// AFTER tier -- BuildingPackage (modifier.md §2a / calc-map §1.4): Σ ACTIVE buildings' {ch}.city.flat +
// {ch}.city.perPopulation × population, ×100. The lone AFTER term in the §1 yield rate (added flat OUTSIDE the percent
// stack); also the per-building own-flat the §2 commerce splitter folds in as BASE. Same value, two tier tags.
long BuildingPackage::buildingFlat(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const std::string wantCity = channel + ".city";
	const int pop = pCity->getPopulation();
	const int nB = GC.getNumBuildingInfos();
	long sum = 0;
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;   // present + non-dormant in THIS city (cascade-computed)
		const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (d == NULL) continue;
		sum += MMKernel::sumUnit100(d, wantCity, "flat", ec);
		sum += MMKernel::sumUnit100(d, wantCity, "perPopulation", ec) * pop;
	}
	return sum;
}
