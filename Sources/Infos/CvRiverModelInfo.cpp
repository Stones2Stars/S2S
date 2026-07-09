//------------------------------------------------------------------------------------------------
//  FILE:    CvRiverModelInfo.cpp
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
#include "CvRiverModelInfo.h"


//======================================================================================================
//					CvRiverModelInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvRiverModelInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvRiverModelInfo::CvRiverModelInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvRiverModelInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvRiverModelInfo::~CvRiverModelInfo()
{
}


const char* CvRiverModelInfo::getModelFile() const
{
	return m_szModelFile;
}


const char* CvRiverModelInfo::getBorderFile() const
{
	return m_szBorderFile;
}


int CvRiverModelInfo::getTextureIndex() const
{
	return m_iTextureIndex;
}


const char* CvRiverModelInfo::getDeltaString() const
{
	return m_szDeltaString;
}


const char* CvRiverModelInfo::getConnectString() const
{
	return m_szConnectString;
}


const char* CvRiverModelInfo::getRotateString() const
{
	return m_szRotateString;
}


void CvRiverModelInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szModelFile, L"ModelFile")
		.add(m_szBorderFile, L"BorderFile")
		.add(m_iTextureIndex, L"TextureIndex")
		.add(m_szDeltaString, L"DeltaType")
		.add(m_szConnectString, L"Connections")
		.add(m_szRotateString, L"Rotations")
	;
}


bool CvRiverModelInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}

void CvRiverModelInfo::copyNonDefaults(const CvRiverModelInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

