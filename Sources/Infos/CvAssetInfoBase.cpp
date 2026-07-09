//------------------------------------------------------------------------------------------------
//  FILE:    CvAssetInfoBase.cpp
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
#include "CvAssetInfoBase.h"


/////////////////////////////////////////////////////////////////////////////////////////////
// CvAssetInfoBase
/////////////////////////////////////////////////////////////////////////////////////////////

const char* CvAssetInfoBase::getTag() const
{
	return getType();
}


void CvAssetInfoBase::setTag(const char* szDesc)
{
	m_szType = szDesc;
}


const char* CvAssetInfoBase::getPath() const
{
	return m_szPath;
}


void CvAssetInfoBase::setPath(const char* szDesc)
{
	m_szPath = szDesc;
}


void CvAssetInfoBase::getDataMembers(CvInfoUtil& util)
{
	// 'tag' is the same as 'type' (read by CvInfoBase::read).
	util
		.add(m_szPath, L"Path")
	;
}

