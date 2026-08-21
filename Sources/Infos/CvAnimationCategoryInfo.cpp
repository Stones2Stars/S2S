//------------------------------------------------------------------------------------------------
//  FILE:    CvAnimationCategoryInfo.cpp
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
#include "CvAnimationCategoryInfo.h"


//======================================================================================================
//					CvAnimationCategoryInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvAnimationCategoryInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvAnimationCategoryInfo::CvAnimationCategoryInfo()
{
	CvInfoUtil(this).initDataMembers();
	// Runtime lazy-resolution sentinel, not an XML default: getCategoryDefaultTo() resolves
	// m_szDefaultTo into m_kCategory.second on first access, after all categories are loaded.
	m_kCategory.second = -7540; // invalid.
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvAnimationCategoryInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvAnimationCategoryInfo::~CvAnimationCategoryInfo()
{
}


int CvAnimationCategoryInfo::getCategoryBaseID( )
{
	return m_kCategory.first;
}


int CvAnimationCategoryInfo::getCategoryDefaultTo( )
{
	if ( m_kCategory.second < -1 )
	{
		// CvXMLLoadUtility *pXML = new CvXMLLoadUtility();
		m_kCategory.second = CvXMLLoadUtility::GetInfoClass( m_szDefaultTo);
	}
	return (int)m_kCategory.second;
}


void CvAnimationCategoryInfo::getDataMembers(CvInfoUtil& util)
{
	// No legacy getCheckSum (CvInfoBase's empty default applies), so declaration order follows read order.
	// m_kCategory.second is NOT declared: it is the runtime cache of the lazy DefaultTo resolution.
	util
		.add(m_szDefaultTo, L"DefaultTo")
		.add(m_kCategory.first, L"BaseID")
	;
}


bool CvAnimationCategoryInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvAnimationCategoryInfo::copyNonDefaults(const CvAnimationCategoryInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	// NOTE: the legacy merge resolved DefaultTo at merge time (getCategoryDefaultTo()) and copied the
	// resolved int from pClassInfo (which could still be the -7540 unresolved sentinel) while never
	// copying m_szDefaultTo itself. The wrapper instead copies the DefaultTo *string* when ours is
	// empty and lets the target resolve lazily — a deliberate behaviour fix (owner-ruled acceptable).
	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

