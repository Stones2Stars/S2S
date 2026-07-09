//------------------------------------------------------------------------------------------------
//  FILE:    CvTurnTimerInfo.cpp
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
#include "CvTurnTimerInfo.h"



//======================================================================================================
//					CvTurnTimerInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvTurnTimerInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvTurnTimerInfo::CvTurnTimerInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvTurnTimerInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvTurnTimerInfo::~CvTurnTimerInfo()
{
}


int CvTurnTimerInfo::getBaseTime() const
{
	return m_iBaseTime;
}


int CvTurnTimerInfo::getCityBonus() const
{
	return m_iCityBonus;
}


int CvTurnTimerInfo::getUnitBonus() const
{
	return m_iUnitBonus;
}


int CvTurnTimerInfo::getFirstTurnMultiplier() const
{
	return m_iFirstTurnMultiplier;
}


void CvTurnTimerInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iBaseTime, L"iBaseTime")
		.add(m_iCityBonus, L"iCityBonus")
		.add(m_iUnitBonus, L"iUnitBonus")
		.add(m_iFirstTurnMultiplier, L"iFirstTurnMultiplier")
	;
}


bool CvTurnTimerInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvTurnTimerInfo::copyNonDefaults(const CvTurnTimerInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvTurnTimerInfo::getCheckSum(uint32_t &iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

