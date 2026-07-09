//------------------------------------------------------------------------------------------------
//  FILE:    CvSlideShowRandomInfo.cpp
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
#include "CvSlideShowRandomInfo.h"


//======================================================================================================
//					CvSlideShowRandomInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvSlideShowRandomInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvSlideShowRandomInfo::CvSlideShowRandomInfo()
{
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvSlideShowRandomInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvSlideShowRandomInfo::~CvSlideShowRandomInfo()
{
}


const char* CvSlideShowRandomInfo::getPath()
{
	return m_szPath;
}


void CvSlideShowRandomInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szPath, L"Path")
	;
}


bool CvSlideShowRandomInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvSlideShowRandomInfo::copyNonDefaults(const CvSlideShowRandomInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

