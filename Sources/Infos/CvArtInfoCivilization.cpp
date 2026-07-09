//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoCivilization.cpp
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
#include "CvArtInfoCivilization.h"


/////////////////////////////////////////////////////////////////////////////////////////////
// CvArtInfoCivilization
/////////////////////////////////////////////////////////////////////////////////////////////

CvArtInfoCivilization::CvArtInfoCivilization()
{
	CvInfoUtil(this).initDataMembers();
}


CvArtInfoCivilization::~CvArtInfoCivilization()
{
}


bool CvArtInfoCivilization::isWhiteFlag() const
{
	return m_bWhiteFlag;
}


void CvArtInfoCivilization::getDataMembers(CvInfoUtil& util)
{
	CvArtInfoAsset::getDataMembers(util);
	util
		.add(m_bWhiteFlag, L"bWhiteFlag")
	;
}


void CvArtInfoCivilization::copyNonDefaults(const CvArtInfoCivilization* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}

