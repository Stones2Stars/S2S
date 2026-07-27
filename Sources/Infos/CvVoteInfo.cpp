//
//	CvVoteInfo -- the vote poco's own typed reading on top of the base section dispatch (see the header).
//	mapFrom materializes the bespoke §9 units (`effect` / `threshold` / `role` / `voteSource`) ONCE
//	([DEC-materialize-at-mapfrom]). Idempotent by contract (reset-first units, unconditional assigns).
//	All scalars carry a load default of 0 (the curator elides zeros).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"    // GC -- the bounds asserts in the membership reads
#include "CvVoteInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdStr / jsonResolveId


CvVoteInfo::Effect::Effect()
{
	reset();
}


// The unit's full redefinition (the mapFrom idempotency contract, CvInfo.h).
void CvVoteInfo::Effect::reset()
{
	for (int iKind = 0; iKind < NUM_VOTE_EFFECT_KINDS; iKind++)
	{
		flags[iKind] = false;
	}
	tradeRoutes = 0;
	forceCivics.clear();
}


CvVoteInfo::CvVoteInfo()
	: m_eRole(VOTE_ROLE_NONE)
{
	for (int iKind = 0; iKind < NUM_VOTE_THRESHOLD_KINDS; iKind++)
	{
		m_thresholds[iKind] = 0;
	}
}


bool CvVoteInfo::forcesCivic(int iCivic) const
{
	FASSERT_BOUNDS(0, GC.getNumCivicInfos(), iCivic);
	return algo::any_of_equal(m_effect.forceCivics, static_cast<CivicTypes>(iCivic));
}


bool CvVoteInfo::hasVoteSource(int iVoteSource) const
{
	FASSERT_BOUNDS(0, GC.getNumVoteSourceInfos(), iVoteSource);
	return algo::any_of_equal(m_voteSources, static_cast<VoteSourceTypes>(iVoteSource));
}


// threshold.* -> the pass rules; role -> the typed enum (the closed two-token vocabulary; role XOR effect in
// the data); effect.* -> the typed outcome unit (bool toggles + tradeRoutes + forceCivics[] civic FKs);
// voteSource[] -> the DIPLOVOTE_* source FKs.
void CvVoteInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + the section dispatch

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member
	m_effect.reset();
	m_voteSources.clear();
	m_eRole = VOTE_ROLE_NONE;
	for (int iKind = 0; iKind < NUM_VOTE_THRESHOLD_KINDS; iKind++)
	{
		m_thresholds[iKind] = 0;
	}

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// voteSource[] -- the DIPLOVOTE_* FKs this resolution can appear at
	picojson::object::const_iterator sourcesIt = entityObj.find("voteSource");
	if (sourcesIt != entityObj.end() && sourcesIt->second.is<picojson::array>())
	{
		const picojson::array& sources = sourcesIt->second.get<picojson::array>();
		for (size_t iSource = 0; iSource < sources.size(); ++iSource)
		{
			if (sources[iSource].is<std::string>())
			{
				const int iSourceId = jsonResolveId(sources[iSource].get<std::string>());
				if (iSourceId >= 0)
				{
					m_voteSources.push_back((VoteSourceTypes)iSourceId);
				}
			}
		}
	}

	if (const picojson::object* pThreshold = jsonChildObj(entityObj, "threshold"))
	{
		m_thresholds[VOTE_THRESHOLD_POPULATION]             = jsonIdInt(*pThreshold, "population");
		m_thresholds[VOTE_THRESHOLD_MIN_VOTERS]             = jsonIdInt(*pThreshold, "minVoters");
		m_thresholds[VOTE_THRESHOLD_STATE_RELIGION_PERCENT] = jsonIdInt(*pThreshold, "stateReligionPercent");
	}

	// role -- the closed two-token vocabulary; an entity without the key is an effect-carrying resolution
	std::string roleName;
	if (jsonIdStr(entityObj, "role", roleName))
	{
		if (roleName == "secretaryGeneral")
		{
			m_eRole = VOTE_ROLE_SECRETARY_GENERAL;
		}
		else if (roleName == "victory")
		{
			m_eRole = VOTE_ROLE_VICTORY;
		}
		else
		{
			// A bespoke block's value vocabulary is CLOSED, so an unrecognized token is authored data that
			// silently became NONE -- the fail-closed-AND-silent shape the ONE census exists to kill. Report it
			// exactly as an unknown section key is reported.
			jsonNoteUnconsumed("role", roleName);
		}
	}

	if (const picojson::object* pEffect = jsonChildObj(entityObj, "effect"))
	{
		m_effect.flags[VOTE_EFFECT_ASSIGN_CITY]    = jsonIdBool(*pEffect, "assignCity");
		m_effect.flags[VOTE_EFFECT_DEFENSIVE_PACT] = jsonIdBool(*pEffect, "defensivePact");
		m_effect.flags[VOTE_EFFECT_FORCE_NO_TRADE] = jsonIdBool(*pEffect, "forceNoTrade");
		m_effect.flags[VOTE_EFFECT_FORCE_PEACE]    = jsonIdBool(*pEffect, "forcePeace");
		m_effect.flags[VOTE_EFFECT_FORCE_WAR]      = jsonIdBool(*pEffect, "forceWar");
		m_effect.flags[VOTE_EFFECT_FREE_TRADE]     = jsonIdBool(*pEffect, "freeTrade");
		m_effect.flags[VOTE_EFFECT_NO_NUKES]       = jsonIdBool(*pEffect, "noNukes");
		m_effect.flags[VOTE_EFFECT_OPEN_BORDERS]   = jsonIdBool(*pEffect, "openBorders");
		m_effect.tradeRoutes = jsonIdInt(*pEffect, "tradeRoutes");

		// effect.forceCivics[] -- the CIVIC_* FKs forced on every member
		picojson::object::const_iterator civicsIt = pEffect->find("forceCivics");
		if (civicsIt != pEffect->end() && civicsIt->second.is<picojson::array>())
		{
			const picojson::array& civics = civicsIt->second.get<picojson::array>();
			for (size_t iCivic = 0; iCivic < civics.size(); ++iCivic)
			{
				if (civics[iCivic].is<std::string>())
				{
					const int iCivicId = jsonResolveId(civics[iCivic].get<std::string>());
					if (iCivicId >= 0)
					{
						m_effect.forceCivics.push_back((CivicTypes)iCivicId);
					}
				}
			}
		}
	}
}
