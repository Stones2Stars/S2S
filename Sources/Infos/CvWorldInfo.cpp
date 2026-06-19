//------------------------------------------------------------------------------------------------
//  FILE:    CvWorldInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvBuildingInfo.h"
#include "CvHeritageInfo.h"
#include "AI/CvGameAI.h"
#include "UI/CvGameTextMgr.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "AI/CvPlayerAI.h"
#include "Infrastructure/CvPython.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvXMLLoadUtilityModTools.h"
#include "Tools/CheckSum.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvWorldInfo.h"



//======================================================================================================
//					CvWorldInfo
//======================================================================================================
CvWorldInfo::CvWorldInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvWorldInfo::~CvWorldInfo()
{
}


int CvWorldInfo::getDefaultPlayers() const
{
	return m_iDefaultPlayers;
}


int CvWorldInfo::getUnitNameModifier() const
{
	return m_iUnitNameModifier;
}


int CvWorldInfo::getTargetNumCities() const
{
	return m_iTargetNumCities;
}


int CvWorldInfo::getBuildingPrereqModifier() const
{
	return m_iBuildingPrereqModifier;
}


int CvWorldInfo::getMaxConscriptModifier() const
{
	return m_iMaxConscriptModifier;
}


int CvWorldInfo::getWarWearinessModifier() const
{
	return m_iWarWearinessModifier;
}


int CvWorldInfo::getGridWidth() const
{
	return m_iGridWidth;
}


int CvWorldInfo::getGridHeight() const
{
	return m_iGridHeight;
}


int CvWorldInfo::getTerrainGrainChange() const
{
	return m_iTerrainGrainChange;
}


int CvWorldInfo::getFeatureGrainChange() const
{
	return m_iFeatureGrainChange;
}


int CvWorldInfo::getTradeProfitPercent() const
{
	return m_iTradeProfitPercent;
}


int CvWorldInfo::getDistanceMaintenancePercent() const
{
	return m_iDistanceMaintenancePercent;
}


int CvWorldInfo::getNumCitiesMaintenancePercent() const
{
	return m_iNumCitiesMaintenancePercent;
}


int CvWorldInfo::getColonyMaintenancePercent() const
{
	return m_iColonyMaintenancePercent;
}


int CvWorldInfo::getCorporationMaintenancePercent() const
{
	return m_iCorporationMaintenancePercent;
}


int CvWorldInfo::getNumCitiesAnarchyPercent() const
{
	return m_iNumCitiesAnarchyPercent;
}


int CvWorldInfo::getAdvancedStartPointsMod() const
{
	return m_iAdvancedStartPointsMod;
}


int CvWorldInfo::getCommandersLevelThresholdsPercent() const
{
	return m_iCommandersLevelThresholdsPercent;
}


int CvWorldInfo::getOceanMinAreaSize() const
{
	return m_iOceanMinAreaSize;
}


int CvWorldInfo::getCityLimitsScalePercent() const
{
	return m_iCityLimitsScalePercent;
}


void CvWorldInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iDefaultPlayers, L"iDefaultPlayers")
		.add(m_iUnitNameModifier, L"iUnitNameModifier")
		.add(m_iTargetNumCities, L"iTargetNumCities")
		.add(m_iBuildingPrereqModifier, L"iBuildingPrereqModifier")
		.add(m_iMaxConscriptModifier, L"iMaxConscriptModifier")
		.add(m_iWarWearinessModifier, L"iWarWearinessModifier")
		.add(m_iGridWidth, L"iGridWidth")
		.add(m_iGridHeight, L"iGridHeight")
		.add(m_iTerrainGrainChange, L"iTerrainGrainChange")
		.add(m_iFeatureGrainChange, L"iFeatureGrainChange")
		.add(m_iTradeProfitPercent, L"iTradeProfitPercent")
		.add(m_iDistanceMaintenancePercent, L"iDistanceMaintenancePercent")
		.add(m_iNumCitiesMaintenancePercent, L"iNumCitiesMaintenancePercent")
		.add(m_iColonyMaintenancePercent, L"iColonyMaintenancePercent")
		.add(m_iCorporationMaintenancePercent, L"iCorporationMaintenancePercent")
		.add(m_iNumCitiesAnarchyPercent, L"iNumCitiesAnarchyPercent")
		.add(m_iAdvancedStartPointsMod, L"iAdvancedStartPointsMod")
		.add(m_iCommandersLevelThresholdsPercent, L"iCommandersLevelThresholdsPercent")
		.add(m_iOceanMinAreaSize, L"iOceanMinAreaSize")
		.add(m_iCityLimitsScalePercent, L"iCityLimitsScalePercent", 100)
	;
}


bool CvWorldInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvWorldInfo::copyNonDefaults(const CvWorldInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvWorldInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}
