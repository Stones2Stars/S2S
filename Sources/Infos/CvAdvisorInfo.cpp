//------------------------------------------------------------------------------------------------
//  FILE:    CvAdvisorInfo.cpp
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
#include "CvAdvisorInfo.h"


//======================================================================================================
//					CvAdvisorInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvAdvisorInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvAdvisorInfo::CvAdvisorInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvAdvisorInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvAdvisorInfo::~CvAdvisorInfo()
{
}


const char* CvAdvisorInfo::getTexture() const
{
	return m_szTexture;
}


int CvAdvisorInfo::getNumCodes() const
{
	return m_vctEnableDisableCodes.size();
}


int CvAdvisorInfo::getEnableCode(uint32_t uiCode) const
{
	FASSERT_BOUNDS(0, m_vctEnableDisableCodes.size(), uiCode);
	return m_vctEnableDisableCodes[uiCode].first;
}


int CvAdvisorInfo::getDisableCode(uint32_t uiCode) const
{
	FASSERT_BOUNDS(0, m_vctEnableDisableCodes.size(), uiCode);
	return m_vctEnableDisableCodes[uiCode].second;
}


// Declarative fields (#196); m_vctEnableDisableCodes stays hand-written in read() and
// copyNonDefaults(): its <EventCodes> elements are direct children of the info node (no
// container root tag, so the struct-vector wrapper shape doesn't apply), and the elements
// are std::pair<int, int>, which has no getDataMembers.
// No legacy getCheckSum, so declaration order follows the legacy read() order.
void CvAdvisorInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szTexture, L"Texture")
	;
}


bool CvAdvisorInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	if(pXML->TryMoveToXmlFirstChild())
	{
		while (pXML->TryMoveToXmlNextSibling(L"EventCodes"))
		{
			int iEnableCode, iDisableCode;
			pXML->GetChildXmlValByName(&iEnableCode, L"iEnableCode");
			pXML->GetChildXmlValByName(&iDisableCode, L"iDisableCode");
			m_vctEnableDisableCodes.push_back(std::make_pair(iEnableCode, iDisableCode));
		}
		pXML->MoveToXmlParent();
	}

	return true;
}


void CvAdvisorInfo::copyNonDefaults(const CvAdvisorInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if ( getNumCodes() == 0 )  //Only copy old values if the new doesn't hold a tag
	{
		for ( int iI = 0; iI < pClassInfo->getNumCodes(); iI++)
		{
			m_vctEnableDisableCodes.push_back(std::make_pair(pClassInfo->getEnableCode(iI), pClassInfo->getDisableCode(iI)));
		}
	}
}

