//------------------------------------------------------------------------------------------------
//  FILE:    CvUpkeepInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvJsonBuildingInfo.h"
#include "CvJsonHeritageInfo.h"
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
#include "CvUpkeepInfo.h"


//------------------------------------------------------------------------------------------------------
//
//  CvUpkeepInfo
//

CvUpkeepInfo::CvUpkeepInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvUpkeepInfo::~CvUpkeepInfo()
{
}


int CvUpkeepInfo::getPopulationPercent() const
{
	return m_iPopulationPercent;
}


int CvUpkeepInfo::getCityPercent() const
{
	return m_iCityPercent;
}


void CvUpkeepInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iPopulationPercent, L"iPopulationPercent")
		.add(m_iCityPercent, L"iCityPercent")
	;
}


bool CvUpkeepInfo::read(CvXMLLoadUtility* pXml)
{
	if (!CvInfoBase::read(pXml))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXml);

	return true;
}


void CvUpkeepInfo::copyNonDefaults(const CvUpkeepInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvUpkeepInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

