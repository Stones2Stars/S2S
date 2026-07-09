//------------------------------------------------------------------------------------------------
//  FILE:    CvAttachableInfo.cpp
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
#include "CvAttachableInfo.h"



//////////////////////////////////////////////////////////////////////////
//
//	CvAttachableInfo			Misc\CIV4AttachableInfos.xml
//
//
CvAttachableInfo::CvAttachableInfo() :
m_fUpdateRate(0.0f) // never read from XML (dead member, no getter); not declared to CvInfoUtil
{
	CvInfoUtil(this).initDataMembers();
}


CvAttachableInfo::~CvAttachableInfo()
{
}


// Declares this class's own fields only; the CvScalableInfo mixin keeps its
// hand-written read/copyNonDefaults (called explicitly below).
// No legacy getCheckSum, so declaration order follows the legacy read() order.
void CvAttachableInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szPath, L"Path")
	;
}


bool CvAttachableInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvScalableInfo::read(pXML);

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvAttachableInfo::copyNonDefaults(const CvAttachableInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);
	CvScalableInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

