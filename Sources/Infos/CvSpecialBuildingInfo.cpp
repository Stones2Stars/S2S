//------------------------------------------------------------------------------------------------
//  FILE:    CvSpecialBuildingInfo.cpp
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
#include "CvSpecialBuildingInfo.h"


//======================================================================================================
//					CvSpecialBuildingInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvSpecialBuildingInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvSpecialBuildingInfo::CvSpecialBuildingInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvSpecialBuildingInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvSpecialBuildingInfo::~CvSpecialBuildingInfo()
{
	CvInfoUtil(this).uninitDataMembers();
}


void CvSpecialBuildingInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order so the delegated checksum stays byte-identical.
	// NOTE: the legacy hand-written copyNonDefaults compared the three tech FKs against 0 instead of
	// the type-correct -1 (NO_TECH), so a modular merge could never inherit them; the wrappers use -1,
	// fixing that latent modular-merge bug.
	util
		.addEnum(m_iObsoleteTech, L"ObsoleteTech")
		.addEnum(m_iTechPrereq, L"TechPrereq")
		.addEnumAsInt(m_iTechPrereqAnyone, L"TechPrereqAnyone")
		.add(m_iMaxPlayerInstances, L"iMaxPlayerInstances", -1)
		.add(m_bValid, L"bValid")
	;
}


TechTypes CvSpecialBuildingInfo::getObsoleteTech() const
{
	return m_iObsoleteTech;
}


int CvSpecialBuildingInfo::getTechPrereqAnyone() const
{
	return m_iTechPrereqAnyone;
}


int CvSpecialBuildingInfo::getMaxPlayerInstances() const
{
	return m_iMaxPlayerInstances;
}


bool CvSpecialBuildingInfo::isValid() const
{
	return m_bValid;
}


bool CvSpecialBuildingInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvSpecialBuildingInfo::copyNonDefaults(const CvSpecialBuildingInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvSpecialBuildingInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

