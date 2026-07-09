//------------------------------------------------------------------------------------------------
//  FILE:    CvColorInfo.cpp
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
#include "CvColorInfo.h"


//======================================================================================================
//					CvColorInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvColorInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvColorInfo::CvColorInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvColorInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvColorInfo::~CvColorInfo()
{
}


const NiColorA& CvColorInfo::getColor() const
{
	return m_Color;
}


// The NiColorA member is declared channel-wise: its four public floats are plain scalar
// fields as far as XML loading is concerned (member type and getter are unchanged).
// No legacy getCheckSum, so declaration order follows the legacy read() order.
void CvColorInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_Color.r, L"fRed")
		.add(m_Color.g, L"fGreen")
		.add(m_Color.b, L"fBlue")
		.add(m_Color.a, L"fAlpha")
	;
}


bool CvColorInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvColorInfo::copyNonDefaults(const CvColorInfo* pClassInfo)
{
	// Note (#196): the legacy copyNonDefaults was a no-op (commented out) — a modular
	// replacement entry never inherited color channels from the entry it replaced. The
	// wrapper now merges each channel that is at its default (0.0f), the standard
	// non-default merge semantics (owner-sanctioned behaviour fix).
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

