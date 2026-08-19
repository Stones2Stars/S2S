#include "CvGameCoreDLL.h"
#include "CyClimateInfo.h"
#include "Infos/CvClimateInfo.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	The ONE bounds gate for this registry -- the id arrives from a script, so it is checked rather than
	//	trusted (docs/architecture/patterns.md §WRITE-ONCE-AT-LOAD: a read never creates, and FASSERT_BOUNDS is compiled out of Release).
	const CvClimateInfo* cyc_climate(int iClimate)
	{
		if (iClimate < 0 || iClimate >= GC.getNumClimateInfos())
		{
			return NULL;
		}
		return &GC.getClimateInfo((ClimateTypes)iClimate);
	}
}

int CyClimateInfo::getDesertPercentChange(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getDesertPercentChange() : 0;
}

int CyClimateInfo::getJungleLatitude(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getJungleLatitude() : 0;
}

int CyClimateInfo::getHillRange(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getHillRange() : 0;
}

int CyClimateInfo::getPeakPercent(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getPeakPercent() : 0;
}

float CyClimateInfo::getSnowLatitudeChange(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getSnowLatitudeChange() : 0.0f;
}

float CyClimateInfo::getTundraLatitudeChange(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getTundraLatitudeChange() : 0.0f;
}

float CyClimateInfo::getGrassLatitudeChange(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getGrassLatitudeChange() : 0.0f;
}

float CyClimateInfo::getDesertBottomLatitudeChange(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getDesertBottomLatitudeChange() : 0.0f;
}

float CyClimateInfo::getDesertTopLatitudeChange(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getDesertTopLatitudeChange() : 0.0f;
}

float CyClimateInfo::getIceLatitude(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getIceLatitude() : 0.0f;
}

float CyClimateInfo::getRandIceLatitude(int iClimate) const
{
	const CvClimateInfo* pClimate = cyc_climate(iClimate);
	return pClimate ? pClimate->getRandIceLatitude() : 0.0f;
}

void CyClimateInfo::pythonPublish()
{
	python::class_<CyClimateInfo>("CyClimateInfo")
		.def("getDesertPercentChange",       &CyClimateInfo::getDesertPercentChange)
		.def("getJungleLatitude",            &CyClimateInfo::getJungleLatitude)
		.def("getHillRange",                 &CyClimateInfo::getHillRange)
		.def("getPeakPercent",               &CyClimateInfo::getPeakPercent)
		.def("getSnowLatitudeChange",        &CyClimateInfo::getSnowLatitudeChange)
		.def("getTundraLatitudeChange",      &CyClimateInfo::getTundraLatitudeChange)
		.def("getDesertBottomLatitudeChange",&CyClimateInfo::getDesertBottomLatitudeChange)
		.def("getDesertTopLatitudeChange",   &CyClimateInfo::getDesertTopLatitudeChange)
		.def("getRandIceLatitude",           &CyClimateInfo::getRandIceLatitude)
		;
}
