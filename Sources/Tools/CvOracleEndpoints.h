#pragma once
#ifndef CV_ORACLE_ENDPOINTS_H
#define CV_ORACLE_ENDPOINTS_H

//
//	OracleEndpoints -- the /computed documents of the derived-state planes, TWO PER PLANE: the STORED values
//	the events built, and the ORACLE's fresh from-source recompute (state-repositories.md, the endpoint
//	oracle; docs/specs/http-endpoints.md for the routes).
//
//	⛔ THE DLL NEVER COMPARES THEM. Each side is rendered by the SAME renderer into the SAME shape, so an
//	external consumer fetches both and diffs them field by field; a disagreement is a MISSED EMIT, named by
//	scope + channel + owner. It is an OBSERVATION a reader makes about two served numbers, never a happening --
//	no diff, no log line, no event, no field exists for it in here. A PULL cannot grow the consumer that
//	"handles" a value known to be wrong by correcting it; a PUSH would ([DEC-no-self-heal]).
//
//	The oracle side always recomputes into a buffer this module owns and hands in, so serving it can never
//	write into the stored state -- that is structural, not a discipline to remember.
//
//	These render on the GAME THREAD (the server's single-slot mailbox calls them); they read live game objects
//	and are read-only. Purely-organizational static-methods class (patterns.md).
//

#include "Defines/CvString.h"

class OracleEndpoints
{
public:
	// Which of the two documents to render. The shape is identical either way -- only where the numbers come
	// from differs (the stored slots, or a from-source recompute into scratch).
	enum OracleSide
	{
		ORACLE_SIDE_STORED = 0,
		ORACLE_SIDE_ORACLE
	};

	// The cascade PACKAGES plane: the scoped objects' channel-indexed flat/percent slots + receiver sums.
	// iPlayer -1 = the active player. iCity -1 = every city of that player (no plot rows); a city id restricts
	// the document to that city AND adds its workable plots' packages, the plot scope's way in.
	static CvString cascadePackages(int iPlayer, int iCity, OracleSide eSide);

	// The ENABLER's per-city operating set: active / obsolete / provided + the provider ref-count.
	static CvString enablerOperating(int iPlayer, int iCity, OracleSide eSide);

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
	static CvString teamCapabilities(int iPlayer, OracleSide eSide);
};

#endif // CV_ORACLE_ENDPOINTS_H
