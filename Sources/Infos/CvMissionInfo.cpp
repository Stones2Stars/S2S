//------------------------------------------------------------------------------------------------
//  FILE:    CvMissionInfo.cpp
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
#include "CvMissionInfo.h"


//======================================================================================================
//					CvMissionInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvMissionInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvMissionInfo::CvMissionInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvMissionInfo::CvMissionInfo(const char * szType) : CvHotkeyInfo()
{
	CvInfoUtil(this).initDataMembers();
	m_szType = szType;
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvMissionInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvMissionInfo::~CvMissionInfo()
{
}


int CvMissionInfo::getTime() const
{
	return m_iTime;
}


bool CvMissionInfo::isSound() const
{
	return m_bSound;
}


bool CvMissionInfo::isTarget() const
{
	return m_bTarget;
}


bool CvMissionInfo::isBuild() const
{
	return m_bBuild;
}


bool CvMissionInfo::getVisible() const
{
	return m_bVisible;
}


const char* CvMissionInfo::getWaypoint() const
{
	return m_szWaypoint;
}


EntityEventTypes CvMissionInfo::getEntityEvent() const
{
	return m_eEntityEvent;
}


void CvMissionInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szWaypoint, L"Waypoint")
		.add(m_iTime, L"iTime")
		.add(m_bSound, L"bSound")
		.add(m_bTarget, L"bTarget")
		.add(m_bBuild, L"bBuild")
		.add(m_bVisible, L"bVisible")
		.addEnum(m_eEntityEvent, L"EntityEventType")
	;
}


bool CvMissionInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvMissionInfo::copyNonDefaults(const CvMissionInfo* pClassInfo)
{
	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

