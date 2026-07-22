//------------------------------------------------------------------------------------------------
//  FILE:    CvVoteInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"    // GC -- the bounds asserts in the array getters
#include "CvVoteInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdStr / jsonResolveId


CvVoteInfo::CvVoteInfo()
	: m_iPopulationThreshold(0)
	, m_iStateReligionVotePercent(0)
	, m_iTradeRoutes(0)
	, m_iMinVoters(0)
	, m_bSecretaryGeneral(false)
	, m_bVictory(false)
	, m_bFreeTrade(false)
	, m_bNoNukes(false)
	, m_bCityVoting(false)
	, m_bCivVoting(false)
	, m_bDefensivePact(false)
	, m_bOpenBorders(false)
	, m_bForcePeace(false)
	, m_bForceNoTrade(false)
	, m_bForceWar(false)
	, m_bAssignCity(false)
{
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


// #430: threshold.* -> the pass rules; role -> the two class bools (SG XOR victory); effect.* -> the on-pass
// outcome payload (bool toggles + tradeRoutes + forceCivics[] civic FKs); voteSource[] -> the DiploVote source
// FKs; mode.* -> the tally-mode bools. All scalars carry a legacy default of 0 (the curator elides zeros).
void CvVoteInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + section dispatch
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// Idempotency -- the full-registry pass re-runs mapFrom, so clear the accumulating vectors first.
	m_aeVoteSourceTypes.clear();
	m_aeForceCivic.clear();

	picojson::object::const_iterator it = o.find("voteSource");
	if (it != o.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>()) { const int rid = jsonResolveId(a[i].get<std::string>()); if (rid >= 0) m_aeVoteSourceTypes.push_back((VoteSourceTypes)rid); }
	}

	if (const picojson::object* t = jsonChildObj(o, "threshold"))
	{
		m_iPopulationThreshold       = jsonIdInt(*t, "population");
		m_iMinVoters                 = jsonIdInt(*t, "minVoters");
		m_iStateReligionVotePercent  = jsonIdInt(*t, "stateReligionPercent");
	}

	// role (SG / victory) XOR effect (outcome) -- mutually exclusive in the data (curate_vote.py). Decode the
	// single `role` string back into the two engine bools; the JSON carries NO bSecretaryGeneral/bVictory keys.
	std::string role;
	if (jsonIdStr(o, "role", role))
	{
		m_bSecretaryGeneral = (role == "secretaryGeneral");
		m_bVictory          = (role == "victory");
	}

	if (const picojson::object* e = jsonChildObj(o, "effect"))
	{
		m_bFreeTrade     = jsonIdBool(*e, "freeTrade");
		m_bNoNukes       = jsonIdBool(*e, "noNukes");
		m_bDefensivePact = jsonIdBool(*e, "defensivePact");
		m_bOpenBorders   = jsonIdBool(*e, "openBorders");
		m_bForcePeace    = jsonIdBool(*e, "forcePeace");
		m_bForceNoTrade  = jsonIdBool(*e, "forceNoTrade");
		m_bForceWar      = jsonIdBool(*e, "forceWar");
		m_bAssignCity    = jsonIdBool(*e, "assignCity");
		m_iTradeRoutes   = jsonIdInt(*e, "tradeRoutes");

		it = e->find("forceCivics");
		if (it != e->end() && it->second.is<picojson::array>())
		{
			const picojson::array& a = it->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int rid = jsonResolveId(a[i].get<std::string>()); if (rid >= 0) m_aeForceCivic.push_back((CivicTypes)rid); }
		}
	}

	if (const picojson::object* m = jsonChildObj(o, "mode"))
	{
		m_bCityVoting = jsonIdBool(*m, "cityVoting");
		m_bCivVoting  = jsonIdBool(*m, "civVoting");
	}
}
