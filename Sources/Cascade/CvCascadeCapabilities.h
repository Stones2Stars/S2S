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
//	(the getter-contract cut, cutover.md) and their event-maintained counters + processTech applies are deleted.
//	⚠ Engine-side compositions stay in the GETTERS, never here: permanentAlliance/vassals fold their game options
//	in CvTeam (capabilities.md).
//
//	PERF: queries are hot (isTerrainTrade rides pathing/trade-network loops), so the union is CACHED per team --
//	on the ONE CvDerivedCacheSet protocol (scope-packages.md §3b: owner-side storage on CvTeam, the component owns
//	the dirty protocol; never serialized). setHasTech/reset MARK; queries ENSURE (the operating buildings idiom -- gate reads
//	are decision-time; the clean-path cost is one int test, same as the retired hand-rolled bValid flag).
//

#include "Defines/CvEnums.h"
#include "Infrastructure/CvDerivedCache.h"
#include <set>
#include <string>
#include <vector>

class CvTeam;

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
	CCF_SET_SCIENCE_RATE, CCF_SET_CULTURE_RATE, CCF_SET_ESPIONAGE_RATE,   // the commerce sliders (capabilities.md)
	CCF_HAS_LANGUAGE,                                                     // gates needLanguage heritages
	CCF_HAS_CENTERED_MAP,   // minimap centering + round globe (sole live grantor TECH_GEOMETRY -- latch ≡ derived)
	CCF_COUNT
};

// The team's cached capability union -- OWNER-SIDE storage on CvTeam (the scope-packages idiom: a mutable
// member, never serialized, all-dirty from bind/reset; the embedded CvDerivedCacheSet owns the dirty protocol).
// Sources today: held techs + the universal TECH_GAME_START start node; civic/building grantors join the union
// when data authors them (mark sites extend likewise).
struct CascadeTeamCaps
{
	std::set<std::string> caps;
	std::set<std::string> trade;
	std::set<int> tradeTerrains;
	std::set<std::string> work;
	bool aFlag[CCF_COUNT];               // precomputed hot-path flags (O(1) reads; no strings after rebuild)
	std::vector<bool> terrainTrade;      // per-terrain bit vector (indexed by TerrainTypes; the pather-adjacent read)
	// the DERIVED-from-tech corp revenue modifier (the ruled self-containment fix, cutover.md Rulings #4:
	// never the legacy team accumulator). ⏳ INTERIM static-Info read (the L5-seed class) -- Σ held techs'
	// CvJsonTechInfo::getCorporationRevenueModifier (one authoring: TECH_STOCK_BROKERING +15); the durable home
	// is the curated JSON plug when the corp-system rework ports its data.
	int corpRevenueMod;

	CvDerivedCacheSet<CvTeam> set;       // the ONE dirty protocol (bind in CvTeam's ctor)

	CascadeTeamCaps() : corpRevenueMod(0) { for (int i = 0; i < CCF_COUNT; ++i) aFlag[i] = false; }
};

class CascadeCapabilities
{
public:
	static bool flag(TeamTypes eTeam, CascadeCapFlag eFlag);          // O(1) hot-path read (precomputed at rebuild)
	static bool capability(TeamTypes eTeam, const char* szKey);       // the flat `capabilities` block (cold callers)
	static bool canTradeItem(TeamTypes eTeam, const char* szKey);     // the `canTrade` block (cold callers)
	static bool canTradeOnTerrain(TeamTypes eTeam, TerrainTypes eT);  // O(1) hot-path read (per-terrain bit vector)
	static bool canWorkOn(TeamTypes eTeam, const char* szKey);        // the `canWorkOn` block (cold callers)
	static int corporationRevenueModifier(TeamTypes eTeam);          // derived-from-tech (never the legacy accumulator)

	// The CacheSet refresh target (CvTeam::cascadeRefreshCaps delegates here): rebuilds the union + the
	// precomputed hot-path reads from CURRENT state, fully defining every field (contract rule 2).
	static void refreshInto(const CvTeam& kTeam, CascadeTeamCaps& c);
};

#endif
