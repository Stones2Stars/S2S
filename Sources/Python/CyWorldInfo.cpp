#include "CvGameCoreDLL.h"
#include "CyWorldInfo.h"
#include "Infos/CvWorldInfo.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	The ONE bounds gate for this registry. A read that cannot be answered returns the caller's neutral value
	//	rather than reaching a registry slot that does not exist -- the id arrives from script, so it is checked
	//	here rather than trusted ([DEC-info-plane-read-only]: a read never creates, and FASSERT_BOUNDS is compiled
	//	out of Release, which is where this runs).
	const CvWorldInfo* cyw_world(int iWorldSize)
	{
		if (iWorldSize < 0 || iWorldSize >= GC.getNumWorldInfos())
		{
			return NULL;
		}
		return &GC.getWorldInfo((WorldSizeTypes)iWorldSize);
	}
}

int CyWorldInfo::getDefaultPlayers(int iWorldSize) const
{
	const CvWorldInfo* pWorld = cyw_world(iWorldSize);
	return pWorld ? pWorld->getDefaultPlayers() : 0;
}

int CyWorldInfo::getTargetNumCities(int iWorldSize) const
{
	const CvWorldInfo* pWorld = cyw_world(iWorldSize);
	return pWorld ? pWorld->getTargetNumCities() : 0;
}

int CyWorldInfo::getOceanMinAreaSize(int iWorldSize) const
{
	const CvWorldInfo* pWorld = cyw_world(iWorldSize);
	return pWorld ? pWorld->getOceanMinAreaSize() : 0;
}

int CyWorldInfo::getTerrainGrainChange(int iWorldSize) const
{
	const CvWorldInfo* pWorld = cyw_world(iWorldSize);
	return pWorld ? pWorld->getTerrainGrainChange() : 0;
}

int CyWorldInfo::getFeatureGrainChange(int iWorldSize) const
{
	const CvWorldInfo* pWorld = cyw_world(iWorldSize);
	return pWorld ? pWorld->getFeatureGrainChange() : 0;
}

int CyWorldInfo::getCorporationMaintenancePercent(int iWorldSize) const
{
	const CvWorldInfo* pWorld = cyw_world(iWorldSize);
	//	100 is this percent's IDENTITY, so an unanswerable read leaves the caller's arithmetic unchanged rather
	//	than silently zeroing every corporation's maintenance.
	return pWorld ? pWorld->getCorporationMaintenancePercent() : 100;
}

void CyWorldInfo::pythonPublish()
{
	python::class_<CyWorldInfo>("CyWorldInfo")
		.def("getDefaultPlayers",                &CyWorldInfo::getDefaultPlayers)
		.def("getTargetNumCities",               &CyWorldInfo::getTargetNumCities)
		.def("getOceanMinAreaSize",              &CyWorldInfo::getOceanMinAreaSize)
		.def("getTerrainGrainChange",            &CyWorldInfo::getTerrainGrainChange)
		.def("getFeatureGrainChange",            &CyWorldInfo::getFeatureGrainChange)
		.def("getCorporationMaintenancePercent", &CyWorldInfo::getCorporationMaintenancePercent)
		;
}
