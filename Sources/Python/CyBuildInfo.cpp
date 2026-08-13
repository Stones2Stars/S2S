#include "CvGameCoreDLL.h"
#include "CyBuildInfo.h"
#include "Infos/CvBuildInfo.h"
#include "Defines/CvGlobals.h"
// Boost 1.32 declares dict_base dll-interface over a non-dll-interface api::object (C4275). The toolchain is
// frozen by the closed EXE, so the header cannot be fixed; the guard is scoped to the include alone, leaving
// C4275 live for our own declarations. Same family as the C4251 disable in CvString.h.
#pragma warning( push )
#pragma warning( disable: 4275 )
#include <boost/python/dict.hpp>
#pragma warning( pop )

namespace
{
	//	The ONE bounds gate for this registry. The id arrives from script, so it is checked here rather than
	//	trusted ([DEC-info-plane-read-only]: a read never creates, and FASSERT_BOUNDS is compiled out of Release,
	//	which is where this runs).
	const CvBuildInfo* cybuild_build(int iBuild)
	{
		if (iBuild < 0 || iBuild >= GC.getNumBuildInfos())
		{
			return NULL;
		}
		return &GC.getBuildInfo((BuildTypes)iBuild);
	}
}

int CyBuildInfo::getImprovement(int iBuild) const
{
	const CvBuildInfo* pBuild = cybuild_build(iBuild);
	return pBuild ? (int)pBuild->getImprovement() : -1;
}

int CyBuildInfo::getRoute(int iBuild) const
{
	const CvBuildInfo* pBuild = cybuild_build(iBuild);
	return pBuild ? (int)pBuild->getRoute() : -1;
}

int CyBuildInfo::getGoldCost(int iBuild) const
{
	const CvBuildInfo* pBuild = cybuild_build(iBuild);
	return pBuild ? pBuild->getGoldCost() : 0;
}

int CyBuildInfo::getTime(int iBuild) const
{
	const CvBuildInfo* pBuild = cybuild_build(iBuild);
	return pBuild ? pBuild->getTime() : 0;
}

bool CyBuildInfo::isConsumesUnit(int iBuild) const
{
	const CvBuildInfo* pBuild = cybuild_build(iBuild);
	return pBuild ? pBuild->isConsumesUnit() : false;
}

int CyBuildInfo::getTechPrereq(int iBuild) const
{
	const CvBuildInfo* pBuild = cybuild_build(iBuild);
	return pBuild ? (int)pBuild->getTechPrereq() : -1;
}

python::list CyBuildInfo::getFeatureRows(int iBuild) const
{
	python::list lRows;

	const CvBuildInfo* pBuild = cybuild_build(iBuild);
	if (pBuild == NULL) return lRows;

	//	The build's OWN authored rows -- never a walk of the feature registry asking this build about each id.
	const std::vector<FeatureStruct>& aRows = pBuild->getProduces().featureRows;
	for (size_t iRow = 0; iRow < aRows.size(); ++iRow)
	{
		const FeatureStruct& row = aRows[iRow];

		python::dict dRow;
		dRow["feature"]    = (int)row.eFeature;
		dRow["tech"]       = (int)row.ePrereqTech;
		dRow["time"]       = row.iTime;
		dRow["production"] = row.iProduction;
		dRow["remove"]     = row.bRemove;
		lRows.append(dRow);
	}
	return lRows;
}

void CyBuildInfo::pythonPublish()
{
	python::class_<CyBuildInfo>("CyBuildInfo")
		.def("getImprovement",  &CyBuildInfo::getImprovement)
		.def("getRoute",        &CyBuildInfo::getRoute)
		.def("getGoldCost",     &CyBuildInfo::getGoldCost)
		.def("getTime",         &CyBuildInfo::getTime)
		.def("isConsumesUnit",  &CyBuildInfo::isConsumesUnit)
		.def("getTechPrereq",   &CyBuildInfo::getTechPrereq)
		.def("getFeatureRows",  &CyBuildInfo::getFeatureRows)
		;
}
