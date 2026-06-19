//------------------------------------------------------------------------------------------------
//  FILE:    CvInvisibleInfo.cpp
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
#include "CvInvisibleInfo.h"


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvInvisibleInfo
//
//  DESC:   Contains info about Invisibles
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CvInvisibleInfo::CvInvisibleInfo() :
// m_iChar is a non-XML, runtime-assigned symbol index (see setChar); the XML-backed fields are
// declared in getDataMembers() and defaulted by initDataMembers() below.
m_iChar(0)
{
	CvInfoUtil(this).initDataMembers();
}


CvInvisibleInfo::~CvInvisibleInfo()
{
}


void CvInvisibleInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iFontButtonIndex, L"FontButtonIndex")
		.add(m_bIntrinsic, L"bIntrinsic")
	;
}


bool CvInvisibleInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvInvisibleInfo::copyNonDefaults(const CvInvisibleInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvInvisibleInfo::getCheckSum(uint32_t& iSum) const
{
	// m_iChar is a non-XML runtime field but is part of the legacy checksum; keep it first to preserve
	// the exact checksum, then the declarative XML-backed fields.
	CheckSum(iSum, m_iChar);
	CvInfoUtil(this).checkSum(iSum);
}


void CvInvisibleInfo::setChar(int i)
{
	m_iChar = i;
}

