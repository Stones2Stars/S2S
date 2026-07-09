//------------------------------------------------------------------------------------------------
//  FILE:    CvControlInfo.cpp
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
#include "CvControlInfo.h"



//======================================================================================================
//					CvControlInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvControlInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvControlInfo::CvControlInfo()
{
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvControlInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvControlInfo::~CvControlInfo()
{
}


bool CvControlInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	return true;
}

void CvControlInfo::copyNonDefaults(const CvControlInfo* pClassInfo)
{
	CvHotkeyInfo::copyNonDefaults(pClassInfo);
}

