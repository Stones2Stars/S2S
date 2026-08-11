#include "CvGameCoreDLL.h"
#include "CyMapGenInfo.h"
#include "Infos/CvBonusInfo.h"
#include "Infos/CvFeatureInfo.h"
#include "Infos/CvTerrainInfo.h"
#include "Infos/CvSeaLevelInfo.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	One bounds gate per registry -- every id arrives from a script, so it is checked rather than trusted
	//	([DEC-info-plane-read-only]: a read never creates, and FASSERT_BOUNDS is compiled out of Release).
	const CvBonusInfo* cyg_bonus(int iBonus)
	{
		if (iBonus < 0 || iBonus >= GC.getNumBonusInfos()) return NULL;
		return &GC.getBonusInfo((BonusTypes)iBonus);
	}
	const CvFeatureInfo* cyg_feature(int iFeature)
	{
		if (iFeature < 0 || iFeature >= GC.getNumFeatureInfos()) return NULL;
		return &GC.getFeatureInfo((FeatureTypes)iFeature);
	}
	const CvTerrainInfo* cyg_terrain(int iTerrain)
	{
		if (iTerrain < 0 || iTerrain >= GC.getNumTerrainInfos()) return NULL;
		return &GC.getTerrainInfo((TerrainTypes)iTerrain);
	}
	const CvSeaLevelInfo* cyg_seaLevel(int iSeaLevel)
	{
		if (iSeaLevel < 0 || iSeaLevel >= GC.getNumSeaLevelInfos()) return NULL;
		return &GC.getSeaLevelInfo((SeaLevelTypes)iSeaLevel);
	}
}

int CyBonusInfo::getPlacementOrder(int iBonus) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	//	-1 is the generator's own "never placed" marker, so it is the honest unanswerable value too.
	return pBonus ? pBonus->getPlacementOrder() : -1;
}

int CyBonusInfo::getTilesPer(int iBonus) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	return pBonus ? pBonus->getTilesPer() : 0;
}

int CyBonusInfo::getPercentPerPlayer(int iBonus) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	return pBonus ? pBonus->getPercentPerPlayer() : 0;
}

int CyBonusInfo::getUniqueRange(int iBonus) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	return pBonus ? pBonus->getUniqueRange() : 0;
}

bool CyBonusInfo::isOneArea(int iBonus) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	return pBonus ? pBonus->isOneArea() : false;
}

int CyBonusInfo::getTechCityTrade(int iBonus) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	return pBonus ? pBonus->getTechCityTrade() : NO_TECH;
}

bool CyBonusInfo::isTerrain(int iBonus, int iTerrain) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	return pBonus ? pBonus->isTerrain(iTerrain) : false;
}

bool CyBonusInfo::isFeature(int iBonus, int iFeature) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	return pBonus ? pBonus->isFeature(iFeature) : false;
}

bool CyBonusInfo::isFeatureTerrain(int iBonus, int iTerrain) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	return pBonus ? pBonus->isFeatureTerrain(iTerrain) : false;
}

int CyBonusInfo::getRandAppearance(int iBonus, int iBand) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	//	The info bounds the band itself and answers 0 outside it, so nothing is re-checked here.
	return pBonus ? pBonus->getRandAppearance(iBand) : 0;
}

int CyBonusInfo::getNumRandAppearanceBands() const
{
	return CvBonusInfo::NUM_RAND_APPEARANCE_BANDS;
}

int CyBonusInfo::getConstAppearance(int iBonus) const
{
	const CvBonusInfo* pBonus = cyg_bonus(iBonus);
	return pBonus ? pBonus->getConstAppearance() : 0;
}

int CyFeatureInfo::getAppearanceProbability(int iFeature) const
{
	const CvFeatureInfo* pFeature = cyg_feature(iFeature);
	//	-1 means "the generator never adds this", which is what an unanswerable read should also mean here.
	return pFeature ? pFeature->getAppearanceProbability() : -1;
}

int CyFeatureInfo::getNumVarieties(int iFeature) const
{
	const CvFeatureInfo* pFeature = cyg_feature(iFeature);
	//	A placed feature always has at least one art variety; 0 would make the caller's modulo divide by zero.
	return pFeature ? pFeature->getNumVarieties() : 1;
}

bool CyTerrainInfo::isWaterTerrain(int iTerrain) const
{
	const CvTerrainInfo* pTerrain = cyg_terrain(iTerrain);
	return pTerrain ? pTerrain->isWaterTerrain() : false;
}

int CySeaLevelInfo::getSeaLevelChange(int iSeaLevel) const
{
	const CvSeaLevelInfo* pSeaLevel = cyg_seaLevel(iSeaLevel);
	return pSeaLevel ? pSeaLevel->getSeaLevelChange() : 0;
}

void CyBonusInfo::pythonPublish()
{
	python::class_<CyBonusInfo>("CyBonusInfo")
		.def("getPlacementOrder",         &CyBonusInfo::getPlacementOrder)
		.def("getTilesPer",               &CyBonusInfo::getTilesPer)
		.def("getPercentPerPlayer",       &CyBonusInfo::getPercentPerPlayer)
		.def("getUniqueRange",            &CyBonusInfo::getUniqueRange)
		.def("isOneArea",                 &CyBonusInfo::isOneArea)
		.def("getTechCityTrade",          &CyBonusInfo::getTechCityTrade)
		.def("isTerrain",                 &CyBonusInfo::isTerrain)
		.def("isFeature",                 &CyBonusInfo::isFeature)
		.def("isFeatureTerrain",          &CyBonusInfo::isFeatureTerrain)
		.def("getRandAppearance",         &CyBonusInfo::getRandAppearance)
		.def("getNumRandAppearanceBands", &CyBonusInfo::getNumRandAppearanceBands)
		.def("getConstAppearance",        &CyBonusInfo::getConstAppearance)
		;
}

void CyFeatureInfo::pythonPublish()
{
	python::class_<CyFeatureInfo>("CyFeatureInfo")
		.def("getAppearanceProbability", &CyFeatureInfo::getAppearanceProbability)
		.def("getNumVarieties",          &CyFeatureInfo::getNumVarieties)
		;
}

void CyTerrainInfo::pythonPublish()
{
	python::class_<CyTerrainInfo>("CyTerrainInfo")
		.def("isWaterTerrain", &CyTerrainInfo::isWaterTerrain)
		;
}

void CySeaLevelInfo::pythonPublish()
{
	python::class_<CySeaLevelInfo>("CySeaLevelInfo")
		.def("getSeaLevelChange", &CySeaLevelInfo::getSeaLevelChange)
		;
}
