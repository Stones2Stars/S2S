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

class CascadeCapabilities
{
public:
	static bool capability(TeamTypes eTeam, const char* szKey);       // the flat `capabilities` block
	static bool canTradeItem(TeamTypes eTeam, const char* szKey);     // the `canTrade` block
	static bool canTradeOnTerrain(TeamTypes eTeam, TerrainTypes eT);  // the `canTradeOn.terrains` set
	static bool canWorkOn(TeamTypes eTeam, const char* szKey);        // the `canWorkOn` block

	static void invalidate(TeamTypes eTeam);   // call on ANY HAVE change (setHasTech; future grantor kinds likewise)
	static void invalidateAll();               // game (re)load / team reset
};

#endif
