//------------------------------------------------------------------------------------------------
//  FILE:    CvGraphicOptionInfo.cpp
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
#include "CvGraphicOptionInfo.h"


//////////////////////////////////////////////////////////////////////////
//
//	CvPlayerOptionInfo
//	Player options and their default values
//
//

//////////////////////////////////////////////////////////////////////////
//
//	CvGraphicOptionInfo
//	Graphic options and their default values
//
//
CvGraphicOptionInfo::CvGraphicOptionInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvGraphicOptionInfo::~CvGraphicOptionInfo()
{
}


bool CvGraphicOptionInfo::getDefault() const
{
	return m_bDefault;
}


void CvGraphicOptionInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_bDefault, L"bDefault")
	;
}


bool CvGraphicOptionInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvGraphicOptionInfo::copyNonDefaults(const CvGraphicOptionInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvGraphicOptionInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

