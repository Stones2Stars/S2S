#pragma once
#ifndef CV_CASCADE_TALLY_H
#define CV_CASCADE_TALLY_H

//
//	CvCascadeTally -- the #430 cascade's STANDARDIZED AGGREGATE-COUNT access surface ("how many of X?"), tally.md.
//
//	⛔ It is NOT a store (owner ruling 2026-06-30). The game OBJECTS already own + maintain their own counts O(1):
//	`CvPlayer::getBuildingCount` = `m_paiBuildingCount[i]` (incrementally maintained by `changeBuildingCount`, which
//	already emits the DOMAIN count event); `getUnitCount` likewise; techs on `CvTeam`. Re-storing those in a second
//	map duplicated authoritative state, risked OOS drift, and made the cascade-vs-legacy shadow TAUTOLOGICAL (the same
//	number twice). So the tally just READS the object-owned count -- a raw INPUT, never a computed output, so this is
//	NOT the pollution anti-pattern (validation.md) -- and rolls it UP the scope spine (empire -> team -> world, summed
//	on read). ONE predictable surface: the per-domain object accessor + the roll-up live HERE, not re-implemented at
//	every call site (this is what collapses the scattered legacy `getNum*` count loops -- cascade-engine-430.md §4).
//
//	Consequences: it serializes NOTHING and MAINTAINS nothing -- it reads. It is therefore NOT an IEventConsumer and
//	needs no load-time seed/rebuild. The standardized event EMITTERS (the spine DOMAIN events at the change sites) stay
//	-- they serve observability, cache-invalidation, and the out-of-process (StoneBase) replay -- but the in-engine
//	tally needs none of them. city/plot counts read the live CvCity/CvPlot directly, never this surface (tally.md §2).
//
//	C++03 / VC7.1: a purely-organizational static-methods class ([DEC-single-implementation]: a calculator is
//	never an instance); EMPIRE = the player's own object aggregate, TEAM/WORLD = summed over alive players on read.
//

//	The cross-object scope a count rolls up to. city/plot are LOCAL (read the live object directly), never this surface.
enum CascadeCountScope
{
	CASCADE_COUNT_EMPIRE = 0,   // the player's own object aggregate (iEntity = player id) -- the common case
	CASCADE_COUNT_TEAM,         // summed over the team's alive players (iEntity = team id)
	CASCADE_COUNT_WORLD         // summed over all alive players (iEntity ignored)
};

class CvCascadeTally
{
public:
	// "How many of TYPE at SCOPE?" -- reads the object-owned count + rolls UP the spine. Default EMPIRE keeps the
	// common call (a count atom's owner-scope read) unchanged.
	static int buildingCount(int iEntity, int iBuilding, CascadeCountScope eScope = CASCADE_COUNT_EMPIRE);
	static int unitCount(int iEntity, int iUnit, CascadeCountScope eScope = CASCADE_COUNT_EMPIRE);
	// "How many units carry classification TAG at SCOPE?" -- ITERATE-ON-READ (owner ruling 2026-07-20): unlike a
	// unit-TYPE count there is no O(1) object aggregate for a tag, so this walks each in-scope alive player's units
	// testing the unit-info tag bitset (tally.md read-not-store). iTagId = the resolved TAG_* classification id.
	static int countUnitsWithTag(int iEntity, int iTagId, CascadeCountScope eScope = CASCADE_COUNT_EMPIRE);
	// "How many TEAMS hold TECH at SCOPE?" -- techs are TEAM-held, so the count is over teams, never players.
	// WORLD reads the engine's own aggregate `CvGame::countKnownTechNumTeams` (ever-alive teams holding it; techs
	// are monotonic, so held == ever-held); TEAM/EMPIRE are the asking team's held flag, 0 or 1. This is the
	// tally read-not-store rule working as intended -- the aggregate already exists on the object, so the tally
	// reads it and never re-stores it (tally.md §1/§2).
	static int techCount(int iEntity, int iTech, CascadeCountScope eScope = CASCADE_COUNT_EMPIRE);
	// "How many SPECIALISTS at SCOPE?" -- the json §3.7 `per: SPECIALIST` count domain at its CROSS-CITY scopes.
	// ⚠ A CITY-scope specialist count never comes here: a local count reads the live CvCity (tally.md §2), and
	// the city already maintains its specialist population O(1).
	// Like countUnitsWithTag this ITERATES ON READ, for the same reason: no player-side O(1) aggregate exists
	// yet. The tally never grows a side-store to compensate -- when the count is wanted often enough to matter,
	// the fix is to give the PLAYER the aggregate ("let an object care about itself") and read it here instead.
	static int specialistCount(int iEntity, CascadeCountScope eScope = CASCADE_COUNT_EMPIRE);
};

#endif // CV_CASCADE_TALLY_H
