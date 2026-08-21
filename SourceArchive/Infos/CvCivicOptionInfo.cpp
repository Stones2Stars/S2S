//------------------------------------------------------------------------------------------------
//  FILE:    CvCivicOptionInfo.cpp
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
#include "CvCivicOptionInfo.h"


//======================================================================================================
//					CvCivicOptionInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvCivicOptionInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvCivicOptionInfo::CvCivicOptionInfo() :
	// m_pabTraitNoUpkeep is a deprecated, non-XML array (its read is commented out); every XML-backed
	// field is declared in getDataMembers() and defaulted by initDataMembers() below.
	m_pabTraitNoUpkeep(NULL)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvCivicOptionInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvCivicOptionInfo::~CvCivicOptionInfo()
{
	//SAFE_DELETE_ARRAY(m_pabTraitNoUpkeep);
}


bool CvCivicOptionInfo::isPolicy() const
{
	return m_bPolicy;
}

//bool CvCivicOptionInfo::getTraitNoUpkeep(int i) const
//{
//	FASSERT_BOUNDS(0, GC.getNumTraitInfos(), i);
//	return m_pabTraitNoUpkeep ? m_pabTraitNoUpkeep[i] : false;
//}

void CvCivicOptionInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_bPolicy, L"bPolicy")
	;
}


bool CvCivicOptionInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvCivicOptionInfo::copyNonDefaults(const CvCivicOptionInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

