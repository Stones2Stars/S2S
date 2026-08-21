#include "CvGameCoreDLL.h"
#include "CyImprovementInfo.h"
#include "Infos/CvImprovementInfo.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	The ONE bounds gate for this registry. The id arrives from script, so it is checked here rather than
	//	trusted (docs/architecture/patterns.md §WRITE-ONCE-AT-LOAD: a read never creates, and FASSERT_BOUNDS is compiled out of Release,
	//	which is where this runs).
	const CvImprovementInfo* cyimp_improvement(int iImprovement)
	{
		if (iImprovement < 0 || iImprovement >= GC.getNumImprovementInfos())
		{
			return NULL;
		}
		return &GC.getImprovementInfo((ImprovementTypes)iImprovement);
	}
}

python::list CyImprovementInfo::getBuilds(int iImprovement) const
{
	python::list lBuilds;

	const CvImprovementInfo* pImprovement = cyimp_improvement(iImprovement);
	if (pImprovement == NULL) return lBuilds;

	const std::vector<BuildTypes>& aeBuilds = pImprovement->getBuildTypes();
	for (size_t iBuild = 0; iBuild < aeBuilds.size(); ++iBuild)
	{
		lBuilds.append((int)aeBuilds[iBuild]);
	}
	return lBuilds;
}

bool CyImprovementInfo::isValidOnBonus(int iImprovement, int iBonus) const
{
	const CvImprovementInfo* pImprovement = cyimp_improvement(iImprovement);
	if (pImprovement == NULL) return false;
	if (iBonus < 0 || iBonus >= GC.getNumBonusInfos()) return false;
	return pImprovement->isImprovementBonusMakesValid(iBonus);
}

bool CyImprovementInfo::isValidOnTerrain(int iImprovement, int iTerrain) const
{
	const CvImprovementInfo* pImprovement = cyimp_improvement(iImprovement);
	if (pImprovement == NULL) return false;
	if (iTerrain < 0 || iTerrain >= GC.getNumTerrainInfos()) return false;
	return pImprovement->getTerrainMakesValid(iTerrain);
}

bool CyImprovementInfo::isValidOnFeature(int iImprovement, int iFeature) const
{
	const CvImprovementInfo* pImprovement = cyimp_improvement(iImprovement);
	if (pImprovement == NULL) return false;
	if (iFeature < 0 || iFeature >= GC.getNumFeatureInfos()) return false;
	return pImprovement->getFeatureMakesValid(iFeature);
}

//	The remaining placement questions, each a bare read of the improvement's own authored member. They share one
//	bounds gate, so it is spelled once rather than per body.
#define CYIMP_FLAG(cyName, infoName)                                              \
	bool CyImprovementInfo::cyName(int iImprovement) const                        \
	{                                                                             \
		const CvImprovementInfo* pImprovement = cyimp_improvement(iImprovement);  \
		return pImprovement ? pImprovement->infoName() : false;                   \
	}

CYIMP_FLAG(isValidOnPeak,       isPeakMakesValid)
CYIMP_FLAG(isValidOnHills,      isHillsMakesValid)
CYIMP_FLAG(isWaterOnly,         isWaterImprovement)
CYIMP_FLAG(isPeakOnly,          isPeakImprovement)
CYIMP_FLAG(isFlatlandsOnly,     isRequiresFlatlands)
CYIMP_FLAG(isRiverSideOnly,     isRequiresRiverSide)
CYIMP_FLAG(isRequiresFeature,   isRequiresFeature)
CYIMP_FLAG(isRequiresIrrigation, isRequiresIrrigation)
CYIMP_FLAG(isNoFreshWater,      isNoFreshWater)

#undef CYIMP_FLAG

void CyImprovementInfo::pythonPublish()
{
	python::class_<CyImprovementInfo>("CyImprovementInfo")
		.def("getBuilds",            &CyImprovementInfo::getBuilds)
		.def("isValidOnBonus",       &CyImprovementInfo::isValidOnBonus)
		.def("isValidOnTerrain",     &CyImprovementInfo::isValidOnTerrain)
		.def("isValidOnFeature",     &CyImprovementInfo::isValidOnFeature)
		.def("isValidOnPeak",        &CyImprovementInfo::isValidOnPeak)
		.def("isValidOnHills",       &CyImprovementInfo::isValidOnHills)
		.def("isWaterOnly",          &CyImprovementInfo::isWaterOnly)
		.def("isPeakOnly",           &CyImprovementInfo::isPeakOnly)
		.def("isFlatlandsOnly",      &CyImprovementInfo::isFlatlandsOnly)
		.def("isRiverSideOnly",      &CyImprovementInfo::isRiverSideOnly)
		.def("isRequiresFeature",    &CyImprovementInfo::isRequiresFeature)
		.def("isRequiresIrrigation", &CyImprovementInfo::isRequiresIrrigation)
		.def("isNoFreshWater",       &CyImprovementInfo::isNoFreshWater)
		;
}
