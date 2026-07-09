//------------------------------------------------------------------------------------------------
//  FILE:    CvScalableInfo.cpp
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
#include "CvInfoBase.h"


//======================================================================================================
//					CvScalableInfo
//======================================================================================================

// Wrapper defaults mirror the READ defaults (fScale absent => 0.0f, fInterfaceScale absent => 1.0f),
// not the constructor's (1.0f/1.0f) — post-read values are identical to the legacy path.
void CvScalableInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_fScale, L"fScale")
		.add(m_fInterfaceScale, L"fInterfaceScale", 1.0f)
	;
}


// Kept hand-written (in sync with getDataMembers above) for the non-declarative mixin
// consumers CvAttachableInfo and CvEffectInfo. The declarative art-info family reads these
// fields through CvArtInfoScalableAsset::getDataMembers instead and does NOT call this.
bool CvScalableInfo::read(CvXMLLoadUtility* pXML)
{
	pXML->GetOptionalChildXmlValByName(&m_fScale, L"fScale");
	pXML->GetOptionalChildXmlValByName(&m_fInterfaceScale, L"fInterfaceScale", 1.0f);
	return true;
}


void CvScalableInfo::copyNonDefaults(const CvScalableInfo* pClassInfo)
{
	const float fDefault = 0.0f;

	if (getScale() == fDefault) m_fScale = pClassInfo->getScale();
	if (getInterfaceScale() == 1.0f) m_fInterfaceScale = pClassInfo->getInterfaceScale();
}


float CvScalableInfo::getScale() const
{
	return m_fScale;
}


float CvScalableInfo::getInterfaceScale() const
{
	return m_fInterfaceScale;
}

