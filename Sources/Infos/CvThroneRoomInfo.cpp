//------------------------------------------------------------------------------------------------
//  FILE:    CvThroneRoomInfo.cpp
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
#include "CvThroneRoomInfo.h"


//======================================================================================================
//					CvThroneRoomInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvThroneRoomInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvThroneRoomInfo::CvThroneRoomInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvThroneRoomInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvThroneRoomInfo::~CvThroneRoomInfo()
{
}


const char* CvThroneRoomInfo::getEvent()
{
	return m_szEvent;
}


const char* CvThroneRoomInfo::getNodeName()
{
	return m_szNodeName;
}


int CvThroneRoomInfo::getFromState()
{
	return m_iFromState;
}


int CvThroneRoomInfo::getToState()
{
	return m_iToState;
}


int CvThroneRoomInfo::getAnimation()
{
	return m_iAnimation;
}


// No legacy getCheckSum, so declaration order follows the legacy read() order.
void CvThroneRoomInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szEvent, L"Event")
		.add(m_iFromState, L"iFromState")
		.add(m_iToState, L"iToState")
		.add(m_szNodeName, L"NodeName")
		.add(m_iAnimation, L"iAnimation")
	;
}


bool CvThroneRoomInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvThroneRoomInfo::copyNonDefaults(CvThroneRoomInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

