//------------------------------------------------------------------------------------------------
//  FILE:    CvCommerceInfo.cpp
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
#include "CvCommerceInfo.h"


//======================================================================================================
//					CvCommerceInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvCommerceInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvCommerceInfo::CvCommerceInfo() :
// m_iChar is a non-XML, runtime-assigned symbol index (see setChar); every XML-backed field is
// declared in getDataMembers() and defaulted by initDataMembers() below.
m_iChar(0)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvCommerceInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvCommerceInfo::~CvCommerceInfo()
{
}


int CvCommerceInfo::getChar() const
{
	return m_iChar;
}


void CvCommerceInfo::setChar(int i)
{
	m_iChar = i;
}


int CvCommerceInfo::getInitialPercent() const
{
	return m_iInitialPercent;
}


int CvCommerceInfo::getInitialHappiness() const
{
	return m_iInitialHappiness;
}


int CvCommerceInfo::getAIWeightPercent() const
{
	return m_iAIWeightPercent;
}


bool CvCommerceInfo::isFlexiblePercent() const
{
	return m_bFlexiblePercent;
}


void CvCommerceInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iInitialPercent, L"iInitialPercent")
		.add(m_iInitialHappiness, L"iInitialHappiness")
		.add(m_iAIWeightPercent, L"iAIWeightPercent")
		.add(m_bFlexiblePercent, L"bFlexiblePercent")
	;
}


bool CvCommerceInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvCommerceInfo::copyNonDefaults(const CvCommerceInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvCommerceInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

