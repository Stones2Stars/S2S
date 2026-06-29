#pragma once
#ifndef CV_CASCADE_TALLY_H
#define CV_CASCADE_TALLY_H

#include "CvEventSpine.h"
#include <map>
#include <set>

class CvPlayer;

//
//	CvCascadeTally -- the #430 cascade's COUNT machine ("how many?"), the spine's first authoritative DOMAIN consumer
//	(design: docs/specs/tally.md). Counts roll UP; the stored leaf is the PLAYER -- one accumulator per (domain,
//	player), keyed by Type (the empire count, summed across the player's cities). It serializes NOTHING: rebuilt on
//	load from the loaded objects (rebuild()), then maintained INCREMENTALLY by DOMAIN events (BUILDING_COUNT /
//	UNIT_COUNT carry the authoritative new empire count). Built for the two domains the spine emits today (buildings +
//	units); other count domains are added with their emit site + a rebuild contribution.
//
//	This re-introduces -- on a proper footing -- the tally consumer purged with the first prototype. Its SHADOW
//	(shadowDiff) is the in-engine cascade-vs-legacy leg: each turn it diffs the event-maintained counts against the
//	live engine (CvPlayer::getBuildingCount / getUnitCount) and emits a gated [TALLY] line, driven to 0 divergences
//	before the legacy count scans (the getNum* prereq loops) are cut (cascade-engine-430.md §4). NOT a /shadow
//	endpoint (retired) -- it rides the gated logging / event tee, like every other shadow.
//
//	C++03 / VC7.1: a virtual IEventConsumer (no captures, no Boost). EMPIRE reads = the player's own accumulator;
//	TEAM/WORLD = summed over players on read (tally.md §3); CITY/PLOT never go through the tally (direct live read).
//
class CvCascadeTally : public IEventConsumer
{
public:
	int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }   // SELECTIVE: only synced state-change counts
	void onEvent(const CvCascadeEvent& kEvent);

	// Deterministic seed from the loaded objects (tally.md §4) -- call at load (onFinalInitialized), every load.
	void rebuild();

	// EMPIRE-scope reads (the player's own accumulator). Team/world roll-ups added with their consumers.
	int buildingCount(int iPlayer, int iBuilding) const;
	int unitCount(int iPlayer, int iUnit) const;

	// The per-turn in-engine SHADOW: event-maintained counts vs the live engine, as a gated [TALLY] line.
	void shadowDiff() const;

private:
	typedef std::map<int, int> CountMap;        // type -> empire count (only nonzero entries kept)
	std::map<int, CountMap> m_buildings;        // player -> (building -> count)
	std::map<int, CountMap> m_units;            // player -> (unit -> count)

	static void setCount(std::map<int, CountMap>& kDomain, int iPlayer, int iType, int iCount);
	static int getCount(const std::map<int, CountMap>& kDomain, int iPlayer, int iType);
	// Discover the candidate (nonzero) building + unit types a player holds, by scanning its objects (cheap: a city's
	// held-buildings list + the player's units) -- the candidate key set for both rebuild and the shadow diff.
	static void gather(const CvPlayer& kPlayer, std::set<int>& kBuildings, std::set<int>& kUnits);
};

//	The single engine-wide tally (the count sibling of eventSpine()).
CvCascadeTally& cascadeTally();

//	Emit the per-turn tally shadow line (calls cascadeTally().shadowDiff()) -- hooked at the top of CvGame::doTurn.
void cascadeTallyShadow();

#endif // CV_CASCADE_TALLY_H
