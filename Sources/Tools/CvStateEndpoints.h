#pragma once
#ifndef CV_STATE_ENDPOINTS_H
#define CV_STATE_ENDPOINTS_H

//
//	StateEndpoints -- the /computed documents of the derived-state planes: what the EVENTS BUILT, rendered for
//	a reader (docs/specs/http-endpoints.md for the routes).
//
//	⛔ THERE IS NO ORACLE SIDE, AND ONE IS NEVER COMING BACK ([superseded-ideas #33]). Each plane used to serve
//	a second document that recomputed the same values FROM SOURCE, for an external consumer to diff as a
//	missed-emit tripwire. It cannot work: reproducing event-built state means replaying the FULL EVENT CHAIN,
//	and an endpoint has no way to build one -- so the recompute answered a number that was never comparable,
//	and diffing it produced confident nonsense at scale (a measured run: ~1500 "divergent" city slots, the
//	recompute 17-29x high, all of it the instrument). It is the project's most-revived dead idea.
//
//	⚖ WHAT THIS SURFACE IS FOR (owner) -- ONE LEG OF THREE, and two legs is not a check: the LOGS say what
//	landed, the JSON INFO says what a source is authored to deposit, and THIS says WHAT STATE EXPECTS -- who
//	holds what, which gates hold, what the counts are. A deposit is conditioned and scaled, so the authored
//	number alone predicts nothing; correctness is all three agreeing, attributed to a named source with numbers.
//
//	These render on the GAME THREAD (the server's single-slot mailbox calls them); they read live game objects
//	and are read-only. Purely-organizational static-methods class (patterns.md).
//

#include "Defines/CvString.h"

class StateEndpoints
{
public:
	// The cascade PACKAGES plane: the scoped objects' channel-indexed flat/percent slots + receiver sums.
	// iPlayer -1 = the active player. iCity -1 = every city of that player (no plot rows); a city id restricts
	// the document to that city AND adds its workable plots' packages, the plot scope's way in.
	static CvString cascadePackages(int iPlayer, int iCity);

	// The ENABLER's per-city operating set: active / obsolete / provided + the provider ref-count.
	static CvString enablerOperating(int iPlayer, int iCity);
	// The VISIBLE tri-state (LISTED + GREYED) per city, every greyed row carrying the reason that refused it.
	static CvString enablerBuildings(int iPlayer, int iCity);
	// ONE named building's verdict across the player's cities -- "why can I not build this, and where".
	static CvString enablerVerdict(int iPlayer, int iCity, const char* szBuilding);
	// ONE unit's verdict per city, DECOMPOSED into the named gate legs (UnitEnabler::Explain) rather than served
	// as a bare state -- a route that answers one number answers nothing when that number is wrong
	// ([http-endpoints.md](../../docs/specs/http-endpoints.md)).
	static CvString enablerUnits(int iPlayer, int iCity, const char* szUnit);

	// THE CITY YIELD CENSUS -- the same decomposition the yield TOOLTIP renders, served.
	// ⚖ A tooltip IS a census (owner), so the two must be the same document or they are two answers to one
	// question. It carries, per yield channel: every term of the §2a combine, and the city's two bonus lists
	// read LIVE at request time. ⛔ Live is the point: a load-end snapshot and a mid-game read are different
	// questions, and a store that fills during the load and drains afterwards reads correct in the first and
	// empty in the second. Only the second is what a player sees.
	// ⛔ Not the banned route shape: every number here is the cascade's OWN computed term, never a legacy
	// accumulator, so nothing is kept alive by its existence ([http-endpoints.md]).
	static CvString cityYield(int iPlayer, int iCity);

	// The TEAM CAPABILITY union of iPlayer's team (-1 = the active player's team).
	static CvString teamCapabilities(int iPlayer);
};

#endif // CV_STATE_ENDPOINTS_H
