//------------------------------------------------------------------------------------------------
//  FILE:    CvEffectInfo.cpp
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
#include "CvEffectInfo.h"



//////////////////////////////////////////////////////////////////////////
//
//	CvEffectInfo			Misc\CIV4EffectInfos.xml
//
//

CvEffectInfo::CvEffectInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvEffectInfo::~CvEffectInfo() {}


// Declares this class's own fields only; the CvScalableInfo mixin keeps its
// hand-written read/copyNonDefaults (called explicitly below).
// No legacy getCheckSum, so declaration order follows the legacy read() order.
void CvEffectInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szPath, L"Path")
		.add(m_fUpdateRate, L"fUpdateRate")
		.add(m_bProjectile, L"bIsProjectile") // legacy read this through an int temp; same 0/1 XML values
		.add(m_fProjectileSpeed, L"fSpeed")
		.add(m_fProjectileArc, L"fArcValue")
		.add(m_bSticky, L"bSticky")
	;
}


bool CvEffectInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvScalableInfo::read(pXML);

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvEffectInfo::copyNonDefaults(const CvEffectInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);
	CvScalableInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

