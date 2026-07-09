//------------------------------------------------------------------------------------------------
//  FILE:    CvGameOptionInfo.cpp
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
#include "CvGameOptionInfo.h"


//////////////////////////////////////////////////////////////////////////
//
//	CvGameOptionInfo
//	Game options and their default values
//
//
CvGameOptionInfo::CvGameOptionInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvGameOptionInfo::~CvGameOptionInfo()
{
	GC.removeDelayedResolutionVector(m_aEnforcesGameOptionOnTypes);
	GC.removeDelayedResolutionVector(m_aEnforcesGameOptionOffTypes);
}


void CvGameOptionInfo::getDataMembers(CvInfoUtil& util)
{
	// HYBRID migration: the two EnforcesGameOption*Types vectors stay hand-written in
	// read()/copyNonDefaults()/getCheckSum()/dtor — they are self-referencing GameOptionTypes FKs
	// read via SetOptionalVectorWithDelayedResolution, and CvInfoUtil has no delayed-resolution
	// vector wrapper yet. Declared in the legacy getCheckSum order (bDefault, bVisible).
	util
		.add(m_bDefault, L"bDefault")
		.add(m_bVisible, L"bVisible", true)
	;
}


bool CvGameOptionInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->SetOptionalVectorWithDelayedResolution(m_aEnforcesGameOptionOnTypes, L"EnforcesGameOptionOnTypes");
	pXML->SetOptionalVectorWithDelayedResolution(m_aEnforcesGameOptionOffTypes, L"EnforcesGameOptionOffTypes");

	return true;
}


void CvGameOptionInfo::copyNonDefaults(const CvGameOptionInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	// Legacy quirk preserved by the wrapper: bVisible defaults to true, so the merge copies the
	// source value whenever ours is still true ("if (getVisible())" == compare-against-default).
	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	//TB's Tags
	GC.copyNonDefaultDelayedResolutionVector(m_aEnforcesGameOptionOnTypes, pClassInfo->m_aEnforcesGameOptionOnTypes);
	GC.copyNonDefaultDelayedResolutionVector(m_aEnforcesGameOptionOffTypes, pClassInfo->m_aEnforcesGameOptionOffTypes);
}


void CvGameOptionInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum); // m_bDefault, m_bVisible (legacy order)

	//TB's Tags (hand-written: delayed-resolution vectors are not declarable yet)
	CheckSumC(iSum, m_aEnforcesGameOptionOnTypes);
	CheckSumC(iSum, m_aEnforcesGameOptionOffTypes);
}

