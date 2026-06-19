//------------------------------------------------------------------------------------------------
//  FILE:    CvVictoryInfo.cpp
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
#include "CvVictoryInfo.h"


//======================================================================================================
//					CvVictoryInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvVictoryInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvVictoryInfo::CvVictoryInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvVictoryInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvVictoryInfo::~CvVictoryInfo() {}


int CvVictoryInfo::getPopulationPercentLead() const
{
	return m_iPopulationPercentLead;
}


int CvVictoryInfo::getLandPercent() const
{
	return m_iLandPercent;
}


int CvVictoryInfo::getMinLandPercent() const
{
	return m_iMinLandPercent;
}


int CvVictoryInfo::getReligionPercent() const
{
	return m_iReligionPercent;
}


int CvVictoryInfo::getCityCulture() const
{
	return m_iCityCulture;
}


int CvVictoryInfo::getNumCultureCities() const
{
	return m_iNumCultureCities;
}


int CvVictoryInfo::getTotalCultureRatio() const
{
	return m_iTotalCultureRatio;
}


int CvVictoryInfo::getVictoryDelayTurns() const
{
	return m_iVictoryDelayTurns;
}


bool CvVictoryInfo::isTargetScore() const
{
	return m_bTargetScore;
}


bool CvVictoryInfo::isEndScore() const
{
	return m_bEndScore;
}


bool CvVictoryInfo::isConquest() const
{
	return m_bConquest;
}


bool CvVictoryInfo::isDiploVote() const
{
	return m_bDiploVote;
}


bool CvVictoryInfo::isPermanent() const
{
	return m_bPermanent;
}


const char* CvVictoryInfo::getMovie() const
{
	return m_szMovie;
}


bool CvVictoryInfo::isTotalVictory() const
{
	return m_bTotalVictory;
}



void CvVictoryInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_bTargetScore, L"bTargetScore")
		.add(m_bEndScore, L"bEndScore")
		.add(m_bConquest, L"bConquest")
		.add(m_bDiploVote, L"bDiploVote")
		.add(m_bPermanent, L"bPermanent")
		.add(m_bTotalVictory, L"bTotalVictory")
		.add(m_iPopulationPercentLead, L"iPopulationPercentLead")
		.add(m_iLandPercent, L"iLandPercent")
		.add(m_iMinLandPercent, L"iMinLandPercent")
		.add(m_iReligionPercent, L"iReligionPercent")
		.add(m_iNumCultureCities, L"iNumCultureCities")
		.add(m_iTotalCultureRatio, L"iTotalCultureRatio")
		.add(m_iVictoryDelayTurns, L"iVictoryDelayTurns")
		.addEnumAsInt(m_iCityCulture, L"CityCulture")
		.add(m_szMovie, L"VictoryMovie")
	;
}


//
// read from xml
//
bool CvVictoryInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvVictoryInfo::copyNonDefaults(const CvVictoryInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvVictoryInfo::getCheckSum(uint32_t& iSum) const
{
	// NOTE: kept explicit (not delegated to CvInfoUtil) to preserve the exact legacy checksum, which
	// historically omits m_bTotalVictory and m_szMovie. Folding those in would change the value.
	CheckSum(iSum, m_iPopulationPercentLead);
	CheckSum(iSum, m_iLandPercent);
	CheckSum(iSum, m_iMinLandPercent);
	CheckSum(iSum, m_iReligionPercent);
	CheckSum(iSum, m_iCityCulture);
	CheckSum(iSum, m_iNumCultureCities);
	CheckSum(iSum, m_iTotalCultureRatio);
	CheckSum(iSum, m_iVictoryDelayTurns);

	CheckSum(iSum, m_bTargetScore);
	CheckSum(iSum, m_bEndScore);
	CheckSum(iSum, m_bConquest);
	CheckSum(iSum, m_bDiploVote);
	CheckSum(iSum, m_bPermanent);
}

