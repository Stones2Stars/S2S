#pragma once

#ifndef CV_CASCADE_CAPABILITIES_H
#define CV_CASCADE_CAPABILITIES_H

//
//	CascadeCapabilities -- the #430 empire-capability QUERY SURFACE (capabilities.md; json.md §8).
//
//	The derived-on-query model (owner ruling 2026-07-02): nothing is granted or stored -- the empire's active
//	capability set is the UNION over the currently-live HAVE sources' blocks (today: the team's held techs + the
//	universal TECH_GAME_START start node; civic/building grantors join the union when data authors them). Four
//	blocks ride the same mechanic:
//	   capabilities  -- flat abilities (canFoundOnPeaks, canBuildBridges, canSpreadIrrigation, ...)
//	   canTrade      -- what may appear on the trade table (techs/gold/maps/openBorders/rightOfPassage/...)
//	   canTradeOn    -- which TERRAIN_ plot types carry trade (raft->lake, sailing->coast, ...)
//	   canWorkOn     -- which coarse plot classes citizens may work (water/ocean/peaks/space)
//
//	This is the GATE-3 WIRING surface: the legacy CvTeam capability getters FLIP their bodies to these queries
//	(the getter-contract cut, cutover.md ruling 5) and their event-maintained counters + processTech applies are
//	deleted. Parity was proven pre-flip via /computed/teamFlags vs the offline dry-calc (0 diverging, all players).
//	⚠ Engine-side compositions stay in the GETTERS, never here: permanentAlliance/vassals fold their game options
//	in CvTeam (capabilities.md), and mapCentering (a latch with building grantors) is NOT flipped yet.
//
//	PERF: queries are hot (isTerrainTrade rides pathing/trade-network loops), so the union is CACHED per team and
//	invalidated event-style from the single mutation point (CvTeam::setHasTech) + CvTeam::reset.
//

#include "Defines/CvEnums.h"

// The HOT-PATH flag ids (2026-07-02 perf find: the flipped getters ride the PATHFINDER -- isCanPassPeaks via
// CvPlot::isImpassable -- and per-plot worker/city loops; a per-call std::string construction there 4x'd the
// turn). The cache precomputes these as a plain bool array at rebuild; flag() is an O(1) read, as cheap as the
// legacy counter. The string-keyed queries below remain for COLD callers (enabler gates, endpoints, modders).
enum CascadeCapFlag
{
	CCF_CAN_PASS_PEAKS = 0, CCF_MOVE_FAST_PEAKS, CCF_CAN_FOUND_ON_PEAKS, CCF_CAN_FARM_DESERT,
	CCF_SPREAD_IRRIGATION, CCF_IGNORE_IRRIGATION, CCF_BRIDGE_BUILDING, CCF_RIVER_TRADE,
	CCF_REBASE_ANYWHERE, CCF_EXTRA_WATER_SEE_FROM,
	CCF_TRADE_TECHS, CCF_TRADE_GOLD, CCF_TRADE_MAPS, CCF_TRADE_OPEN_BORDERS, CCF_TRADE_RIGHT_OF_PASSAGE,
	CCF_TRADE_DEFENSIVE_PACT, CCF_TRADE_PERMANENT_ALLIANCE, CCF_TRADE_VASSALS, CCF_TRADE_EMBASSY,
	CCF_WORK_WATER,
	CCF_COUNT
};

class CascadeCapabilities
{
public:
	static bool flag(TeamTypes eTeam, CascadeCapFlag eFlag);          // O(1) hot-path read (precomputed at rebuild)
	static bool capability(TeamTypes eTeam, const char* szKey);       // the flat `capabilities` block (cold callers)
	static bool canTradeItem(TeamTypes eTeam, const char* szKey);     // the `canTrade` block (cold callers)
	static bool canTradeOnTerrain(TeamTypes eTeam, TerrainTypes eT);  // O(1) hot-path read (per-terrain bit vector)
	static bool canWorkOn(TeamTypes eTeam, const char* szKey);        // the `canWorkOn` block (cold callers)

	static void invalidate(TeamTypes eTeam);   // call on ANY HAVE change (setHasTech; future grantor kinds likewise)
	static void invalidateAll();               // game (re)load / team reset

	// ---- the IN-BODY getter shadow, now FLIP-WITH-NET (2026-07-02; step 1 ran clean at 5.49M calls / 0
	// diverging incl. the load path + a full turn). Each getter passes its LEGACY counter verdict in and returns
	// the CASCADE verdict (authoritative post-flip); the diff net stays armed — per-(team,flag) diverging counts
	// flush per turn as [CAPSHADOW] spine lines — until the counters are deleted the cycle after a clean net.
	// Diff tallying is gPlayerLogLevel-gated; the cascade verdict is computed regardless (it IS the return).
	static bool shadow(TeamTypes eTeam, CascadeCapFlag eFlag, bool bLegacy);
	static bool shadowTerrain(TeamTypes eTeam, TerrainTypes eT, bool bLegacy);
	static void shadowFlush();   // per-turn (called from the modifier shadow's doTurn site)
};

#endif
