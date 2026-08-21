//------------------------------------------------------------------------------------------------
//  FILE:    CvCommandInfo.cpp
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
#include "CvCommandInfo.h"


//======================================================================================================
//					CvCommandInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvCommandInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvCommandInfo::CvCommandInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvCommandInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvCommandInfo::~CvCommandInfo()
{
}


int CvCommandInfo::getAutomate() const
{
	return m_iAutomate;
}


bool CvCommandInfo::getConfirmCommand() const
{
	return m_bConfirmCommand;
}


bool CvCommandInfo::getVisible() const
{
	return m_bVisible;
}


bool CvCommandInfo::getAll() const
{
	return m_bAll;
}


void CvCommandInfo::getDataMembers(CvInfoUtil& util)
{
	// No legacy getCheckSum (CvInfoBase's empty default applies), so declaration order follows read
	// order and none is added. m_iAutomate is an int-typed type index (NO_AUTOMATE = -1 = the
	// wrapper default), resolved immediately at read time.
	util
		.addEnumAsInt(m_iAutomate, L"Automate")
		.add(m_bConfirmCommand, L"bConfirmCommand")
		.add(m_bVisible, L"bVisible")
		.add(m_bAll, L"bAll")
	;
}


bool CvCommandInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvCommandInfo::copyNonDefaults(const CvCommandInfo* pClassInfo)
{
	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

