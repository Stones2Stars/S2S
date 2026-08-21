//------------------------------------------------------------------------------------------------
//  FILE:    CvRouteModelInfo.cpp
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
#include "CvRouteModelInfo.h"


//======================================================================================================
//					CvRouteModelInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvRouteModelInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvRouteModelInfo::CvRouteModelInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvRouteModelInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvRouteModelInfo::~CvRouteModelInfo()
{
}


RouteTypes CvRouteModelInfo::getRouteType() const		// The route type
{
	return m_eRouteType;
}


const char* CvRouteModelInfo::getModelFile() const
{
	return m_szModelFile;
}


const char* CvRouteModelInfo::getLateModelFile() const
{
	return m_szLateModelFile;
}


const char* CvRouteModelInfo::getModelFileKey() const
{
	return m_szModelFileKey;
}


bool CvRouteModelInfo::isAnimated() const
{
	return m_bAnimated;
}


const char* CvRouteModelInfo::getConnectString() const
{
	return m_szConnectString;
}


const char* CvRouteModelInfo::getModelConnectString() const
{
	return m_szModelConnectString;
}


const char* CvRouteModelInfo::getRotateString() const
{
	return m_szRotateString;
}


void CvRouteModelInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szModelFile, L"ModelFile")
		.add(m_szLateModelFile, L"LateModelFile")
		.add(m_szModelFileKey, L"ModelFileKey")
		.add(m_bAnimated, L"Animated")
		.addEnum(m_eRouteType, L"RouteType")
		.add(m_szConnectString, L"Connections")
		.add(m_szModelConnectString, L"ModelConnections")
		.add(m_szRotateString, L"Rotations")
	;
}


bool CvRouteModelInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvRouteModelInfo::copyNonDefaults(const CvRouteModelInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

