//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoScalableAsset.cpp
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
#include "CvArtInfoScalableAsset.h"


/////////////////////////////////////////////////////////////////////////////////////////////
// CvArtInfoScalableAsset
/////////////////////////////////////////////////////////////////////////////////////////////

void CvArtInfoScalableAsset::getDataMembers(CvInfoUtil& util)
{
	CvArtInfoAsset::getDataMembers(util);
	CvScalableInfo::getDataMembers(util);
}


bool CvArtInfoScalableAsset::read(CvXMLLoadUtility* pXML)
{
	// The CvInfoUtil delegation in CvArtInfoAsset::read covers the CvScalableInfo mixin fields
	// too (declared above), so the hand-written CvScalableInfo::read is no longer called here.
	return CvArtInfoAsset::read(pXML);
}


void CvArtInfoScalableAsset::copyNonDefaults(const CvArtInfoScalableAsset* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}

