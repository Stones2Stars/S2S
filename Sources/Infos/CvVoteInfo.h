#pragma once
#ifndef CV_VOTE_INFO_H
#define CV_VOTE_INFO_H

//
//	CvVoteInfo -- the VOTE poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP: the four read
//	categories, nothing else). A diplomatic proposal/resolution, all bespoke §9 blocks (json.md §9): the
//	`effect` block (the on-pass outcome payload) held as ONE typed unit mirroring the authored keys; the
//	`threshold` pass rules as one parameterized read; the `role` vocabulary as a typed enum; the `voteSource`
//	DIPLOVOTE_* FKs as a typed list. Carries ZERO cascade modifiers -- the effect payload feeds
//	CvGame::processVote directly. JSON-fed (Assets/Data/votes/*.json via mapFrom); no XML read
//	(DEC-no-xml-into-game). Type + identity text keys ride the base CvInfo reading.
//

#include "CvInfo.h"            // the JSON-info base (mapFrom); on /I -> bare include
#include "Defines/CvEnums.h"   // CivicTypes / VoteSourceTypes FKs
#include <vector>

// The `threshold` block's pass rules -- ONE parameterized read over the block's authored keys.
enum VoteThresholdKind
{
	VOTE_THRESHOLD_POPULATION,               // threshold.population -- percent of world population voting yes
	VOTE_THRESHOLD_MIN_VOTERS,               // threshold.minVoters -- minimum eligible voter count to table it
	VOTE_THRESHOLD_STATE_RELIGION_PERCENT,   // threshold.stateReligionPercent -- religious-source eligibility percent
	NUM_VOTE_THRESHOLD_KINDS
};

// The `effect` block's boolean outcomes -- ONE parameterized test over the block's authored keys
// (the coherent-surface rule: a groupable boolean set is never N hand-named getters).
enum VoteEffectKind
{
	VOTE_EFFECT_ASSIGN_CITY,      // effect.assignCity
	VOTE_EFFECT_DEFENSIVE_PACT,   // effect.defensivePact
	VOTE_EFFECT_FORCE_NO_TRADE,   // effect.forceNoTrade
	VOTE_EFFECT_FORCE_PEACE,      // effect.forcePeace
	VOTE_EFFECT_FORCE_WAR,        // effect.forceWar
	VOTE_EFFECT_FREE_TRADE,       // effect.freeTrade
	VOTE_EFFECT_NO_NUKES,         // effect.noNukes
	VOTE_EFFECT_OPEN_BORDERS,     // effect.openBorders
	NUM_VOTE_EFFECT_KINDS
};

// The `role` vocabulary (role XOR effect in the data): a chairperson election, a victory resolution, or
// neither (an effect-carrying resolution).
enum VoteRole
{
	VOTE_ROLE_NONE,
	VOTE_ROLE_SECRETARY_GENERAL,   // role: "secretaryGeneral"
	VOTE_ROLE_VICTORY              // role: "victory"
};

class CvVoteInfo : public CvInfo
{
public:

	CvVoteInfo();

	virtual void mapFrom(const picojson::value& entity);

	// The bespoke §9 `effect` block as ONE typed unit mirroring the authored keys.
	struct Effect
	{
		Effect();
		void reset();

		bool flags[NUM_VOTE_EFFECT_KINDS];
		int tradeRoutes;                       // effect.tradeRoutes -- extra routes while the resolution holds
		std::vector<CivicTypes> forceCivics;   // effect.forceCivics -- CIVIC_* FKs forced on every member
	};

	// ======================= 1. SECTIONS -- the whole typed `effect` unit (json.md §9) =======================
	// (categories 2/3 are absent by the data: votes author no §8 classification and no §6 modifier families)
	const Effect& getEffect() const { return m_effect; }
	// the grouped reads over the unit -- one getter per group, parameterized over the block's keys
	bool effectApplies(VoteEffectKind eKind) const { return m_effect.flags[eKind]; }
	bool forcesCivic(int iCivic) const;   // membership over effect.forceCivics (bounds-asserted, .cpp)
	// the `threshold` pass rules -- one parameterized read
	int getThreshold(VoteThresholdKind eKind) const { return m_thresholds[eKind]; }
	// the `voteSource` FKs -- the DIPLOVOTE_* sources this resolution can appear at
	const std::vector<VoteSourceTypes>& getVoteSources() const { return m_voteSources; }
	bool hasVoteSource(int iVoteSource) const;   // membership (bounds-asserted, .cpp)

	// ======================= 4. INTRINSIC -- bare typed reads ================================================
	VoteRole getRole() const { return m_eRole; }                       // role
	int getTradeRoutes() const { return m_effect.tradeRoutes; }        // effect.tradeRoutes (lone numeric)

private:
	// --- the bespoke §9 units (materialized once at mapFrom) ---
	Effect m_effect;
	int m_thresholds[NUM_VOTE_THRESHOLD_KINDS];
	VoteRole m_eRole;
	std::vector<VoteSourceTypes> m_voteSources;
};

#endif // CV_VOTE_INFO_H
