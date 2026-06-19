//------------------------------------------------------------------------------------------------
//  FILE:    CvImprovementBonusInfo.cpp
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
#include "CvImprovementBonusInfo.h"


//======================================================================================================
//					CvImprovementBonusInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvImprovementBonusInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvImprovementBonusInfo::CvImprovementBonusInfo() :
m_iDiscoverRand(0),
m_bBonusMakesValid(false),
m_bObsoleteBonusMakesValid(false),
m_bBonusTrade(false),
m_piYieldChange(NULL)
,m_iDepletionRand(0)
{ }


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvImprovementBonusInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvImprovementBonusInfo::~CvImprovementBonusInfo()
{
	SAFE_DELETE_ARRAY(m_piYieldChange);
}


int CvImprovementBonusInfo::getDiscoverRand() const
{
	return m_iDiscoverRand;
}


bool CvImprovementBonusInfo::isBonusMakesValid() const
{
	return m_bBonusMakesValid;
}


bool CvImprovementBonusInfo::isObsoleteBonusMakesValid() const
{
	return m_bObsoleteBonusMakesValid;
}


bool CvImprovementBonusInfo::isBonusTrade() const
{
	return m_bBonusTrade;
}


int CvImprovementBonusInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldChange ? m_piYieldChange[i] : 0;
}


int CvImprovementBonusInfo::getDepletionRand() const
{
	return m_iDepletionRand;
}


void CvImprovementBonusInfo::getCheckSum(uint32_t &iSum) const
{
	CheckSum(iSum, m_iDiscoverRand);

	CheckSum(iSum, m_bBonusMakesValid);
	CheckSum(iSum, m_bObsoleteBonusMakesValid);
	CheckSum(iSum, m_bBonusTrade);

	CheckSum(iSum, m_iDepletionRand);

	// Arrays

	CheckSumI(iSum, NUM_YIELD_TYPES, m_piYieldChange);
}

