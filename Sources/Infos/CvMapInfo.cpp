//------------------------------------------------------------------------------------------------
//  FILE:    CvMapInfo.cpp
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
#include "CvMapInfo.h"


//======================================================================================================
//					CvMapInfo
//======================================================================================================

CvMapInfo::CvMapInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvMapInfo::~CvMapInfo()
{
}


void CvMapInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iGridWidth, L"iGridWidth")
		.add(m_iGridHeight, L"iGridHeight")
		.add(m_iWrapX, L"bWrapX", -1)
		.add(m_iWrapY, L"bWrapY", -1)
		.add(m_bStartRevealed, L"bStartRevealed")
		.add(m_szInitialWBMap, L"InitialWBMap")
		.add(m_szMapScript, L"MapScript")
	;
}


bool CvMapInfo::read(CvXMLLoadUtility* pXML)
{
	CvHotkeyInfo::read(pXML);

	CvInfoUtil(this).readXml(pXML);

	return true;
}

