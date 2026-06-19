//------------------------------------------------------------------------------------------------
//  FILE:    CvSpecialUnitInfo.cpp
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
#include "CvSpecialUnitInfo.h"



//======================================================================================================
//					CvSpecialUnitInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvSpecialUnitInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvSpecialUnitInfo::CvSpecialUnitInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvSpecialUnitInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvSpecialUnitInfo::~CvSpecialUnitInfo()
{
}


bool CvSpecialUnitInfo::isValid() const
{
	return m_bValid;
}


bool CvSpecialUnitInfo::isCityLoad() const
{
	return m_bCityLoad;
}


bool CvSpecialUnitInfo::isSMLoadSame() const
{
	return m_bSMLoadSame;
}


int CvSpecialUnitInfo::getCombatPercent() const
{
	return m_iCombatPercent;
}


int CvSpecialUnitInfo::getWithdrawalChange() const
{
	return m_iWithdrawalChange;
}


// Arrays


void CvSpecialUnitInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_bValid, L"bValid")
		.add(m_bCityLoad, L"bCityLoad")
		.add(m_bSMLoadSame, L"bSMLoadSame")
		.add(m_iCombatPercent, L"iCombatPercent")
		.add(m_iWithdrawalChange, L"iWithdrawalChange")
	;
}


bool CvSpecialUnitInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvSpecialUnitInfo::copyNonDefaults(const CvSpecialUnitInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvSpecialUnitInfo::getCheckSum(uint32_t& iSum) const
{
	// NOTE: kept explicit (not delegated to CvInfoUtil) to preserve the exact legacy checksum, which
	// historically omits m_bCityLoad. Folding it in would change the value.
	CheckSum(iSum, m_bValid);
	CheckSum(iSum, m_bSMLoadSame);
	CheckSum(iSum, m_iCombatPercent);
	CheckSum(iSum, m_iWithdrawalChange);
}

