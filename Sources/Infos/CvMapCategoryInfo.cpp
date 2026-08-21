//------------------------------------------------------------------------------------------------
//  FILE:    CvMapCategoryInfo.cpp
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
#include "CvMapCategoryInfo.h"


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvMapCategoryInfo
//
//  DESC:   Contains info about Map Categories
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CvMapCategoryInfo::CvMapCategoryInfo()
{
}


CvMapCategoryInfo::~CvMapCategoryInfo()
{
}


bool CvMapCategoryInfo::read(CvXMLLoadUtility* pXML)
{
	CvInfoBase::read(pXML);

	return true;
}


void CvMapCategoryInfo::copyNonDefaults(const CvMapCategoryInfo* pClassInfo)
{
}


void CvMapCategoryInfo::getCheckSum(uint32_t& iSum) const
{
}

