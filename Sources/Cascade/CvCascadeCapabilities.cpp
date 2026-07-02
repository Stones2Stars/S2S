//
//	CascadeCapabilities -- the derived-on-query empire-ability union, cached per team (see the header).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeCapabilities.h"
#include "CvJsonInfo.h"
#include "CvJsonTechInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvTeam.h"
#include "AI/CvTeamAI.h"      // GET_TEAM
#include "Infos/CvTechInfo.h"
#include "Infos/CvTerrainInfo.h"   // the terrain-diff sample name in the [CAPSHADOW] emit
#include "CvEventSpine.h"          // the [CAPSHADOW] spine domain
#include "AI/BetterBTSAI.h"        // gPlayerLogLevel
#include <set>
#include <string>
#include <vector>

// The per-team cached union of every live HAVE source's ability blocks. Rebuilt lazily on the first query after an
// invalidate (setHasTech / reset). Sources today: held techs + the universal TECH_GAME_START start node (techs are
// the only grantor kind in data; capabilities.md keeps civic/building grantors as model headroom -- when data
// authors them, union them here AND invalidate on their change events).
struct CascadeCapCache
{
	bool bValid;
	std::set<std::string> caps;
	std::set<std::string> trade;
	std::set<int> tradeTerrains;
	std::set<std::string> work;
	bool aFlag[CCF_COUNT];               // precomputed hot-path flags (O(1) reads; no strings after rebuild)
	std::vector<bool> terrainTrade;      // per-terrain bit vector (indexed by TerrainTypes; the pather-adjacent read)
	CascadeCapCache() : bValid(false) { for (int i = 0; i < CCF_COUNT; ++i) aFlag[i] = false; }
};
static CascadeCapCache s_cache[MAX_TEAMS];

// flag id -> (which set, which key): resolved ONCE per rebuild, never on the query path.
struct CcapKeyRow { CascadeCapFlag eFlag; int iSet; const char* szKey; };   // iSet: 0=caps 1=trade 2=work
static const CcapKeyRow CCAP_KEYS[] =
{
	{ CCF_CAN_PASS_PEAKS, 0, "canPassPeaks" }, { CCF_MOVE_FAST_PEAKS, 0, "canMoveFastOnPeaks" },
	{ CCF_CAN_FOUND_ON_PEAKS, 0, "canFoundOnPeaks" }, { CCF_CAN_FARM_DESERT, 0, "canFarmDesert" },
	{ CCF_SPREAD_IRRIGATION, 0, "canSpreadIrrigation" }, { CCF_IGNORE_IRRIGATION, 0, "canIgnoreIrrigation" },
	{ CCF_BRIDGE_BUILDING, 0, "canBuildBridges" }, { CCF_RIVER_TRADE, 0, "hasRiverTrade" },
	{ CCF_REBASE_ANYWHERE, 0, "canRebaseAnywhere" }, { CCF_EXTRA_WATER_SEE_FROM, 0, "canSeeFurtherFromWater" },
	{ CCF_TRADE_TECHS, 1, "techs" }, { CCF_TRADE_GOLD, 1, "gold" }, { CCF_TRADE_MAPS, 1, "maps" },
	{ CCF_TRADE_OPEN_BORDERS, 1, "openBorders" }, { CCF_TRADE_RIGHT_OF_PASSAGE, 1, "rightOfPassage" },
	{ CCF_TRADE_DEFENSIVE_PACT, 1, "defensivePact" }, { CCF_TRADE_PERMANENT_ALLIANCE, 1, "permanentAlliance" },
	{ CCF_TRADE_VASSALS, 1, "vassals" }, { CCF_TRADE_EMBASSY, 1, "embassy" },
	{ CCF_WORK_WATER, 2, "water" },
};

static void ccap_union(const CvJsonTechInfo* j, CascadeCapCache& c)
{
	if (j == NULL) return;
	c.caps.insert(j->capabilities.begin(), j->capabilities.end());
	c.trade.insert(j->canTrade.begin(), j->canTrade.end());
	c.tradeTerrains.insert(j->canTradeOnTerrains.begin(), j->canTradeOnTerrains.end());
	c.work.insert(j->canWorkOn.begin(), j->canWorkOn.end());
}

static const CascadeCapCache& ccap_get(TeamTypes eTeam)
{
	CascadeCapCache& c = s_cache[eTeam];
	if (!c.bValid)
	{
		c.caps.clear(); c.trade.clear(); c.tradeTerrains.clear(); c.work.clear();
		// The universal start node: every civ holds TECH_GAME_START (the no-prereq root), so its blocks are
		// universally active (the canSetScienceRate/canSetEspionageRate/base-tradable-terrain defaults live there).
		ccap_union(static_cast<const CvJsonTechInfo*>(&cascadeStartNode()), c);
		const CvTeam& kTeam = GET_TEAM(eTeam);
		for (int t = 0; t < GC.getNumTechInfos(); ++t)
			if (kTeam.isHasTech((TechTypes)t))
				ccap_union(static_cast<const CvJsonTechInfo*>(InfoRepo<CvTechInfo>::get().get(t)), c);
		// Precompute the HOT-PATH reads: the named flags + the per-terrain bit vector. All string/set work
		// happens HERE, once per (team, tech-change) -- the queries below are plain array reads (the pathfinder
		// rides isCanPassPeaks; a per-call string construction 4x'd the turn, 2026-07-02).
		for (int i = 0; i < (int)(sizeof(CCAP_KEYS) / sizeof(CCAP_KEYS[0])); ++i)
		{
			const CcapKeyRow& r = CCAP_KEYS[i];
			const std::set<std::string>& s = (r.iSet == 0) ? c.caps : (r.iSet == 1) ? c.trade : c.work;
			c.aFlag[r.eFlag] = s.count(r.szKey) != 0;
		}
		c.terrainTrade.assign(GC.getNumTerrainInfos(), false);
		for (std::set<int>::const_iterator it = c.tradeTerrains.begin(); it != c.tradeTerrains.end(); ++it)
			if (*it >= 0 && *it < (int)c.terrainTrade.size()) c.terrainTrade[*it] = true;
		c.bValid = true;
	}
	return c;
}

bool CascadeCapabilities::flag(TeamTypes eTeam, CascadeCapFlag eFlag)
{
	if (eTeam < 0 || eTeam >= MAX_TEAMS) return false;
	return ccap_get(eTeam).aFlag[eFlag];
}

bool CascadeCapabilities::capability(TeamTypes eTeam, const char* szKey)
{
	if (eTeam < 0 || eTeam >= MAX_TEAMS) return false;
	return ccap_get(eTeam).caps.count(szKey) != 0;
}

bool CascadeCapabilities::canTradeItem(TeamTypes eTeam, const char* szKey)
{
	if (eTeam < 0 || eTeam >= MAX_TEAMS) return false;
	return ccap_get(eTeam).trade.count(szKey) != 0;
}

bool CascadeCapabilities::canTradeOnTerrain(TeamTypes eTeam, TerrainTypes eT)
{
	if (eTeam < 0 || eTeam >= MAX_TEAMS || eT < 0) return false;
	const CascadeCapCache& c = ccap_get(eTeam);
	return (int)eT < (int)c.terrainTrade.size() && c.terrainTrade[eT];
}

bool CascadeCapabilities::canWorkOn(TeamTypes eTeam, const char* szKey)
{
	if (eTeam < 0 || eTeam >= MAX_TEAMS) return false;
	return ccap_get(eTeam).work.count(szKey) != 0;
}

void CascadeCapabilities::invalidate(TeamTypes eTeam)
{
	if (eTeam >= 0 && eTeam < MAX_TEAMS) s_cache[eTeam].bValid = false;
}

void CascadeCapabilities::invalidateAll()
{
	for (int i = 0; i < MAX_TEAMS; ++i) s_cache[i].bValid = false;
}

// ===================== the IN-BODY getter shadow (see the header) =====================
// [CAPSHADOW] spine domain: per-(team,flag) checked/diverging accumulated at REAL call moments (legacy stays
// authoritative), flushed once per turn from the modifier shadow's doTurn site. Divergence samples carry the
// team + flag + both verdicts -- the named-source map the reverted first flip lacked. Names for the flag ids
// mirror CCAP_KEYS; terrain diffs report the terrain type string.
enum CsEvt { CSE_DIFF = 1, CSE_SHADOW };
enum CsFld { CSF_TEAM = 1, CSF_FLAG, CSF_CASC, CSF_LEG, CSF_N, CSF_CHECKED, CSF_DIVERGING };
static const char* cs_prefix(int evt)
{
	switch (evt)
	{
	case CSE_DIFF:   return "[CAPSHADOW/diff]";
	case CSE_SHADOW: return "[CAPSHADOW/shadow]";
	default:         return "[CAPSHADOW]";
	}
}
static const char* cs_field(int tag, SpineFieldType* peType)
{
	*peType = SFT_INT;
	switch (tag)
	{
	case CSF_TEAM:      return "team";
	case CSF_FLAG:      *peType = SFT_STR; return "flag";
	case CSF_CASC:      return "casc";
	case CSF_LEG:       return "leg";
	case CSF_N:         return "n";
	case CSF_CHECKED:   return "checked";
	case CSF_DIVERGING: return "diverging";
	default:            return NULL;
	}
}
static void cs_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_CAPSHADOW, cs_prefix, "Cascade.log", cs_field); s_reg = true; }
}

// Per-(team,flag) tallies + one terrain tally per team (terrain diffs sample the first diverging terrain id).
static long s_csChecked = 0, s_csDiverging = 0;
static int s_csDivCount[MAX_TEAMS][CCF_COUNT];        // per-(team,flag) diverging call count
static bool s_csDivCasc[MAX_TEAMS][CCF_COUNT];        // the cascade verdict at the first diverging call
static int s_csTerrDiv[MAX_TEAMS];                    // per-team diverging terrain-call count
static int s_csTerrSample[MAX_TEAMS];                 // first diverging terrain id (-1 = none)
static bool s_csInit = false;
static void cs_reset()
{
	s_csChecked = 0; s_csDiverging = 0;
	for (int t = 0; t < MAX_TEAMS; ++t)
	{
		for (int f = 0; f < CCF_COUNT; ++f) { s_csDivCount[t][f] = 0; s_csDivCasc[t][f] = false; }
		s_csTerrDiv[t] = 0; s_csTerrSample[t] = -1;
	}
	s_csInit = true;
}
static const char* cs_flagName(int f)
{
	for (int i = 0; i < (int)(sizeof(CCAP_KEYS) / sizeof(CCAP_KEYS[0])); ++i)
		if ((int)CCAP_KEYS[i].eFlag == f) return CCAP_KEYS[i].szKey;
	return "?";
}

void CascadeCapabilities::shadow(TeamTypes eTeam, CascadeCapFlag eFlag, bool bLegacy)
{
	if (gPlayerLogLevel < 1 || eTeam < 0 || eTeam >= MAX_TEAMS) return;
	if (!s_csInit) cs_reset();
	const bool bCasc = flag(eTeam, eFlag);
	++s_csChecked;
	if (bCasc != bLegacy)
	{
		++s_csDiverging;
		if (s_csDivCount[eTeam][eFlag]++ == 0) s_csDivCasc[eTeam][eFlag] = bCasc;
	}
}

void CascadeCapabilities::shadowTerrain(TeamTypes eTeam, TerrainTypes eT, bool bLegacy)
{
	if (gPlayerLogLevel < 1 || eTeam < 0 || eTeam >= MAX_TEAMS || eT < 0) return;
	if (!s_csInit) cs_reset();
	const bool bCasc = canTradeOnTerrain(eTeam, eT);
	++s_csChecked;
	if (bCasc != bLegacy)
	{
		++s_csDiverging;
		++s_csTerrDiv[eTeam];
		if (s_csTerrSample[eTeam] < 0) s_csTerrSample[eTeam] = (int)eT;
	}
}

void CascadeCapabilities::shadowFlush()
{
	if (gPlayerLogLevel < 1 || !s_csInit) return;
	cs_registerDomain();
	int iShown = 0;
	for (int t = 0; t < MAX_TEAMS && iShown < 24; ++t)
	{
		for (int f = 0; f < CCF_COUNT && iShown < 24; ++f)
			if (s_csDivCount[t][f] > 0)
			{
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CAPSHADOW, CSE_DIFF, 1)
					.addI(CSF_TEAM, t).addStr(CSF_FLAG, cs_flagName(f))
					.addI(CSF_CASC, s_csDivCasc[t][f] ? 1 : 0).addI(CSF_LEG, s_csDivCasc[t][f] ? 0 : 1)
					.addI(CSF_N, s_csDivCount[t][f]));
				++iShown;
			}
		if (s_csTerrDiv[t] > 0 && iShown < 24)
		{
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CAPSHADOW, CSE_DIFF, 1)
				.addI(CSF_TEAM, t).addStr(CSF_FLAG, GC.getTerrainInfo((TerrainTypes)s_csTerrSample[t]).getType())
				.addI(CSF_N, s_csTerrDiv[t]));
			++iShown;
		}
	}
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CAPSHADOW, CSE_SHADOW, 1)
		.addI(CSF_CHECKED, (int)s_csChecked).addI(CSF_DIVERGING, (int)s_csDiverging));
	cs_reset();
}
