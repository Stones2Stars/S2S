//------------------------------------------------------------------------------------------------
//  FILE:    CvSeaLevelInfo.cpp
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
#include "CvSeaLevelInfo.h"


//======================================================================================================
//					CvSeaLevelInfo
//======================================================================================================
CvSeaLevelInfo::CvSeaLevelInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvSeaLevelInfo::~CvSeaLevelInfo()
{
}


int CvSeaLevelInfo::getSeaLevelChange() const
{
	return m_iSeaLevelChange;
}


void CvSeaLevelInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iSeaLevelChange, L"iSeaLevelChange")
	;
}


bool CvSeaLevelInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvSeaLevelInfo::copyNonDefaults(const CvSeaLevelInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

