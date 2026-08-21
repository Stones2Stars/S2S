//------------------------------------------------------------------------------------------------
//  FILE:    CvClimateInfo.cpp
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
#include "CvClimateInfo.h"


//======================================================================================================
//					CvClimateInfo
//======================================================================================================
CvClimateInfo::CvClimateInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvClimateInfo::~CvClimateInfo()
{
}


int CvClimateInfo::getDesertPercentChange() const
{
	return m_iDesertPercentChange;
}


int CvClimateInfo::getJungleLatitude() const
{
	return m_iJungleLatitude;
}


int CvClimateInfo::getHillRange() const
{
	return m_iHillRange;
}


int CvClimateInfo::getPeakPercent() const
{
	return m_iPeakPercent;
}


float CvClimateInfo::getSnowLatitudeChange() const
{
	return m_fSnowLatitudeChange;
}


float CvClimateInfo::getTundraLatitudeChange() const
{
	return m_fTundraLatitudeChange;
}


float CvClimateInfo::getGrassLatitudeChange() const
{
	return m_fGrassLatitudeChange;
}


float CvClimateInfo::getDesertBottomLatitudeChange() const
{
	return m_fDesertBottomLatitudeChange;
}


float CvClimateInfo::getDesertTopLatitudeChange() const
{
	return m_fDesertTopLatitudeChange;
}


float CvClimateInfo::getIceLatitude() const
{
	return m_fIceLatitude;
}


float CvClimateInfo::getRandIceLatitude() const
{
	return m_fRandIceLatitude;
}


void CvClimateInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iDesertPercentChange, L"iDesertPercentChange")
		.add(m_iJungleLatitude, L"iJungleLatitude")
		.add(m_iHillRange, L"iHillRange")
		.add(m_iPeakPercent, L"iPeakPercent")
		.add(m_fSnowLatitudeChange, L"fSnowLatitudeChange")
		.add(m_fTundraLatitudeChange, L"fTundraLatitudeChange")
		.add(m_fGrassLatitudeChange, L"fGrassLatitudeChange")
		.add(m_fDesertBottomLatitudeChange, L"fDesertBottomLatitudeChange")
		.add(m_fDesertTopLatitudeChange, L"fDesertTopLatitudeChange")
		.add(m_fIceLatitude, L"fIceLatitude")
		.add(m_fRandIceLatitude, L"fRandIceLatitude")
	;
}


bool CvClimateInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvClimateInfo::copyNonDefaults(const CvClimateInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

