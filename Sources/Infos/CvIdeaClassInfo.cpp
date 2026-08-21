//------------------------------------------------------------------------------------------------
//  FILE:    CvIdeaClassInfo.cpp
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
#include "CvIdeaClassInfo.h"


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvIdeaClassInfo
//
//  DESC:   Contains info about Idea Classes
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CvIdeaClassInfo::CvIdeaClassInfo() :
m_bInitialized(true)
{
}


CvIdeaClassInfo::~CvIdeaClassInfo()
{
}


bool CvIdeaClassInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	return true;
}


void CvIdeaClassInfo::copyNonDefaults(const CvIdeaClassInfo* pClassInfo)
{
	//CvInfoBase::copyNonDefaults(pClassInfo);
}


void CvIdeaClassInfo::getCheckSum(uint32_t& iSum) const
{
}

