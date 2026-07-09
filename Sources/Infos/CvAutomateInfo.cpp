//------------------------------------------------------------------------------------------------
//  FILE:    CvAutomateInfo.cpp
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
#include "CvAutomateInfo.h"


//======================================================================================================
//					CvAutomateInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvAutomateInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvAutomateInfo::CvAutomateInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvAutomateInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvAutomateInfo::~CvAutomateInfo()
{
}


int CvAutomateInfo::getCommand() const
{
	return m_iCommand;
}


int CvAutomateInfo::getAutomate() const
{
	return m_iAutomate;
}


bool CvAutomateInfo::getConfirmCommand() const
{
	return m_bConfirmCommand;
}


bool CvAutomateInfo::getVisible() const
{
	return m_bVisible;
}


void CvAutomateInfo::getDataMembers(CvInfoUtil& util)
{
	// No legacy getCheckSum (CvInfoBase's empty default applies), so declaration order follows read
	// order and none is added. m_iCommand/m_iAutomate are int-typed type indices (NO_COMMAND /
	// NO_AUTOMATE = -1 = the wrapper default), resolved immediately at read time.
	util
		.addEnumAsInt(m_iCommand, L"Command")
		.addEnumAsInt(m_iAutomate, L"Automate")
		.add(m_bConfirmCommand, L"bConfirmCommand")
		.add(m_bVisible, L"bVisible")
	;
}


bool CvAutomateInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvAutomateInfo::copyNonDefaults(const CvAutomateInfo* pClassInfo)
{
	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

