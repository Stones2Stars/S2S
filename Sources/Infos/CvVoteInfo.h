#pragma once

#ifndef CV_VOTE_INFO_H
#define CV_VOTE_INFO_H

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvVoteInfo
//
//  DESC:   A diplomatic proposal / resolution. #430: JSON-fed (Assets/Data/votes/*.json
//          via mapFrom); no XML read. Carries ZERO cascade modifiers -- its effect
//          payload feeds CvGame::processVote directly (curate_vote.py).
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvVoteInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvVoteInfo();

	int getPopulationThreshold() const { return m_iPopulationThreshold; }
	int getStateReligionVotePercent() const { return m_iStateReligionVotePercent; }
	int getTradeRoutes() const { return m_iTradeRoutes; }
	int getMinVoters() const { return m_iMinVoters; }

	bool isSecretaryGeneral() const { return m_bSecretaryGeneral; }
	bool isVictory() const { return m_bVictory; }
	bool isFreeTrade() const { return m_bFreeTrade; }
	bool isNoNukes() const { return m_bNoNukes; }
	bool isCityVoting() const { return m_bCityVoting; }
	bool isCivVoting() const { return m_bCivVoting; }
	bool isDefensivePact() const { return m_bDefensivePact; }
	bool isOpenBorders() const { return m_bOpenBorders; }
	bool isForcePeace() const { return m_bForcePeace; }
	bool isForceNoTrade() const { return m_bForceNoTrade; }
	bool isForceWar() const { return m_bForceWar; }
	bool isAssignCity() const { return m_bAssignCity; }

	// Arrays

	bool isForceCivic(int i) const;
	bool isVoteSourceType(int i) const;

	virtual void mapFrom(const picojson::value& entity);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	int m_iPopulationThreshold;
	int m_iStateReligionVotePercent;
	int m_iTradeRoutes;
	int m_iMinVoters;

	bool m_bSecretaryGeneral;
	bool m_bVictory;
	bool m_bFreeTrade;
	bool m_bNoNukes;
	bool m_bCityVoting;
	bool m_bCivVoting;
	bool m_bDefensivePact;
	bool m_bOpenBorders;
	bool m_bForcePeace;
	bool m_bForceNoTrade;
	bool m_bForceWar;
	bool m_bAssignCity;

	// Arrays

	std::vector<CivicTypes> m_aeForceCivic;
	std::vector<VoteSourceTypes> m_aeVoteSourceTypes;

};

#endif // CV_VOTE_INFO_H
