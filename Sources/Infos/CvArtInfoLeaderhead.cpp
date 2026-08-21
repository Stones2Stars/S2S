//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoLeaderhead.cpp
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
#include "CvArtInfoLeaderhead.h"


/////////////////////////////////////////////////////////////////////////////////////////////
// CvArtInfoLeaderhead
/////////////////////////////////////////////////////////////////////////////////////////////

const char* CvArtInfoLeaderhead::getNoShaderNIF() const
{
	return m_szNoShaderNIF;
}


void CvArtInfoLeaderhead::setNoShaderNIF(const char* szNIF)
{
	m_szNoShaderNIF = szNIF;
}


const char* CvArtInfoLeaderhead::getBackgroundKFM() const
{
	return m_szBackgroundKFM;
}


void CvArtInfoLeaderhead::setBackgroundKFM(const char* szKFM)
{
	m_szBackgroundKFM = szKFM;
}


void CvArtInfoLeaderhead::getDataMembers(CvInfoUtil& util)
{
	CvArtInfoAsset::getDataMembers(util);
	util
		.add(m_szNoShaderNIF, L"NoShaderNIF")
		.add(m_szBackgroundKFM, L"BackgroundKFM")
	;
}


void CvArtInfoLeaderhead::copyNonDefaults(const CvArtInfoLeaderhead* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}

