//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoImprovement.cpp
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
#include "CvArtInfoImprovement.h"


//////////////////////////////////////////////////////////////////////////
// CvArtInfoImprovement
//////////////////////////////////////////////////////////////////////////

CvArtInfoImprovement::CvArtInfoImprovement()
{
	CvInfoUtil(this).initDataMembers();
}


CvArtInfoImprovement::~CvArtInfoImprovement()
{
}


bool CvArtInfoImprovement::isExtraAnimations() const
{
	return m_bExtraAnimations;
}


void CvArtInfoImprovement::getDataMembers(CvInfoUtil& util)
{
	CvArtInfoScalableAsset::getDataMembers(util);
	util
		.add(m_bExtraAnimations, L"bExtraAnimations")
		.add(m_szShaderNIF, L"SHADERNIF")
	;
}


void CvArtInfoImprovement::copyNonDefaults(const CvArtInfoImprovement* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}

