//------------------------------------------------------------------------------------------------
//  FILE:    CvInterfaceModeInfo.cpp
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
#include "CvInterfaceModeInfo.h"


//======================================================================================================
//					CvInterfaceModeInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvInterfaceModeInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvInterfaceModeInfo::CvInterfaceModeInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvInterfaceModeInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvInterfaceModeInfo::~CvInterfaceModeInfo()
{
}


int CvInterfaceModeInfo::getCursorIndex() const
{
	return m_iCursorIndex;
}


int CvInterfaceModeInfo::getMissionType() const
{
	return m_iMissionType;
}


bool CvInterfaceModeInfo::getVisible() const
{
	return m_bVisible;
}


bool CvInterfaceModeInfo::getGotoPlot() const
{
	return m_bGotoPlot;
}


bool CvInterfaceModeInfo::getHighlightPlot() const
{
	return m_bHighlightPlot;
}


bool CvInterfaceModeInfo::getSelectType() const
{
	return m_bSelectType;
}


bool CvInterfaceModeInfo::getSelectAll() const
{
	return m_bSelectAll;
}


bool CvInterfaceModeInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvInterfaceModeInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.addEnumAsInt(m_iCursorIndex, L"CursorType")
		.addEnumAsInt(m_iMissionType, L"Mission")
		.add(m_bVisible, L"bVisible")
		.add(m_bGotoPlot, L"bGotoPlot")
		.add(m_bHighlightPlot, L"bHighlightPlot")
		.add(m_bSelectType, L"bSelectType")
		.add(m_bSelectAll, L"bSelectAll")
	;
}


void CvInterfaceModeInfo::copyNonDefaults(const CvInterfaceModeInfo* pClassInfo)
{
	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

