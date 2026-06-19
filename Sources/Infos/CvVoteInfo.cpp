//------------------------------------------------------------------------------------------------
//  FILE:    CvVoteInfo.cpp
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
#include "CvVoteInfo.h"


//======================================================================================================
//					CvVoteInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvVoteInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvVoteInfo::CvVoteInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvVoteInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvVoteInfo::~CvVoteInfo()
{
}


int CvVoteInfo::getPopulationThreshold() const
{
	return m_iPopulationThreshold;
}


int CvVoteInfo::getStateReligionVotePercent() const
{
	return m_iStateReligionVotePercent;
}


int CvVoteInfo::getTradeRoutes() const
{
	return m_iTradeRoutes;
}


int CvVoteInfo::getMinVoters() const
{
	return m_iMinVoters;
}


bool CvVoteInfo::isSecretaryGeneral() const
{
	return m_bSecretaryGeneral;
}


bool CvVoteInfo::isVictory() const
{
	return m_bVictory;
}


bool CvVoteInfo::isFreeTrade() const
{
	return m_bFreeTrade;
}


bool CvVoteInfo::isNoNukes() const
{
	return m_bNoNukes;
}


bool CvVoteInfo::isCityVoting() const
{
	return m_bCityVoting;
}


bool CvVoteInfo::isCivVoting() const
{
	return m_bCivVoting;
}


bool CvVoteInfo::isDefensivePact() const
{
	return m_bDefensivePact;
}


bool CvVoteInfo::isOpenBorders() const
{
	return m_bOpenBorders;
}


bool CvVoteInfo::isForcePeace() const
{
	return m_bForcePeace;
}


bool CvVoteInfo::isForceNoTrade() const
{
	return m_bForceNoTrade;
}


bool CvVoteInfo::isForceWar() const
{
	return m_bForceWar;
}


bool CvVoteInfo::isAssignCity() const
{
	return m_bAssignCity;
}


bool CvVoteInfo::isForceCivic(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCivicInfos(), i);
	return algo::any_of_equal(m_aeForceCivic, static_cast<CivicTypes>(i));
}


bool CvVoteInfo::isVoteSourceType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumVoteSourceInfos(), i);
	return algo::any_of_equal(m_aeVoteSourceTypes, static_cast<VoteSourceTypes>(i));
}


void CvVoteInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order so the delegated checksum stays byte-identical.
	util
		.add(m_iPopulationThreshold, L"iPopulationThreshold")
		.add(m_iStateReligionVotePercent, L"iStateReligionVotePercent")
		.add(m_iTradeRoutes, L"iTradeRoutes")
		.add(m_iMinVoters, L"iMinVoters")
		.add(m_bSecretaryGeneral, L"bSecretaryGeneral")
		.add(m_bVictory, L"bVictory")
		.add(m_bFreeTrade, L"bFreeTrade")
		.add(m_bNoNukes, L"bNoNukes")
		.add(m_bCityVoting, L"bCityVoting")
		.add(m_bCivVoting, L"bCivVoting")
		.add(m_bDefensivePact, L"bDefensivePact")
		.add(m_bOpenBorders, L"bOpenBorders")
		.add(m_bForcePeace, L"bForcePeace")
		.add(m_bForceNoTrade, L"bForceNoTrade")
		.add(m_bForceWar, L"bForceWar")
		.add(m_bAssignCity, L"bAssignCity")
		.add(m_aeForceCivic, L"ForceCivics")
		.add(m_aeVoteSourceTypes, L"DiploVotes")
	;
}


bool CvVoteInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvVoteInfo::copyNonDefaults(const CvVoteInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvVoteInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

