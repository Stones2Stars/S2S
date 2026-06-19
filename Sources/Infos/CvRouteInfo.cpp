//------------------------------------------------------------------------------------------------
//  FILE:    CvRouteInfo.cpp
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
#include "CvRouteInfo.h"


//======================================================================================================
//					CvRouteInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvRouteInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvRouteInfo::CvRouteInfo() :
// Only the non-XML runtime field (m_zobristValue, below) and the hand-written array need explicit
// init here; every declared field is defaulted by initDataMembers() below.
m_piTechMovementChange(NULL)
{
	CvInfoUtil(this).initDataMembers();

	m_zobristValue = GC.getGame().getSorenRand().getInt();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvRouteInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvRouteInfo::~CvRouteInfo()
{
	// m_piYieldChange is owned by its addYields wrapper and freed by uninitDataMembers.
	CvInfoUtil(this).uninitDataMembers();
	SAFE_DELETE_ARRAY(m_piTechMovementChange);
}


int CvRouteInfo::getAdvancedStartCost() const
{
	return m_iAdvancedStartCost;
}


int CvRouteInfo::getValue() const
{
	return m_iValue;
}


int CvRouteInfo::getMovementCost() const
{
	return m_iMovementCost;
}


int CvRouteInfo::getFlatMovementCost() const
{
	return m_iFlatMovementCost;
}


bool CvRouteInfo::isSeaTunnel() const
{
	return m_bSeaTunnel;
}


int CvRouteInfo::getPrereqBonus() const
{
	return m_iPrereqBonus;
}


int CvRouteInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldChange ? m_piYieldChange[i] : 0;
}


int* CvRouteInfo::getYieldChangeArray() const
{
	return m_piYieldChange;
}


int CvRouteInfo::getTechMovementChange(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTechInfos(), i);
	return m_piTechMovementChange ? m_piTechMovementChange[i] : 0;
}


int CvRouteInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvRouteInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvRouteInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


const std::vector<BonusTypes>& CvRouteInfo::getPrereqOrBonuses() const
{
	return m_piPrereqOrBonuses;
}


void CvRouteInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. m_piTechMovementChange (dynamic tech-length int array
	// via SetVariableListTagPair) has no wrapper yet and stays hand-written in read/copyNonDefaults;
	// because it sits mid-order in the legacy checksum, getCheckSum below stays explicit too.
	util
		.add(m_iAdvancedStartCost, L"iAdvancedStartCost", 100)
		.add(m_iValue, L"iValue")
		.add(m_iMovementCost, L"iMovement")
		.add(m_iFlatMovementCost, L"iFlatMovement")
		.add(m_bSeaTunnel, L"bSeaTunnel")
		.addEnumAsInt(m_iPrereqBonus, L"BonusType")
		.addYields(m_piYieldChange, L"Yields")
		.add(m_piPrereqOrBonuses, L"PrereqOrBonuses")
		.add(m_aiCategories, L"Categories")
		.add(m_PropertyManipulators)
	;
}


bool CvRouteInfo::read(CvXMLLoadUtility* pXML)
{
	//shouldHaveType = true;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}
	if (m_szType.empty())
	{
		OutputDebugStringW(pXML->GetXmlTagName());
		OutputDebugString("\n");
		FErrorMsg("error");
	}
	//shouldHaveType = false;

	CvInfoUtil(this).readXml(pXML);

	pXML->SetVariableListTagPair(&m_piTechMovementChange, L"TechMovementChanges", GC.getNumTechInfos());

	return true;
}


void CvRouteInfo::copyNonDefaults(const CvRouteInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const int iDefault = 0;

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	for ( int i = 0;  i < GC.getNumTechInfos(); i++)
	{
		if (getTechMovementChange(i) == iDefault && pClassInfo->getTechMovementChange(i) != iDefault)
		{
			if ( NULL == m_piTechMovementChange )
			{
				CvXMLLoadUtility::InitList(&m_piTechMovementChange,GC.getNumTechInfos(),iDefault);
			}
			m_piTechMovementChange[i] = pClassInfo->getTechMovementChange(i);
		}
	}
}


void CvRouteInfo::getCheckSum(uint32_t& iSum) const
{
	// Explicit (not delegated) to keep the legacy checksum byte-identical: the hand-written
	// m_piTechMovementChange sits mid-order between the declared fields.
	CheckSum(iSum, m_iAdvancedStartCost);

	CheckSum(iSum, m_iValue);
	CheckSum(iSum, m_iMovementCost);
	CheckSum(iSum, m_iFlatMovementCost);

	CheckSum(iSum, m_bSeaTunnel);
	CheckSum(iSum, m_iPrereqBonus);

	// Arrays

	CheckSum(iSum, m_piYieldChange, NUM_YIELD_TYPES);
	CheckSum(iSum, m_piTechMovementChange, GC.getNumTechInfos());
	CheckSumC(iSum, m_piPrereqOrBonuses);
	CheckSumC(iSum, m_aiCategories);

	m_PropertyManipulators.getCheckSum(iSum);
}

