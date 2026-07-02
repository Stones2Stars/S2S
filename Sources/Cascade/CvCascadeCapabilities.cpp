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
#include <set>
#include <string>

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
	CascadeCapCache() : bValid(false) {}
};
static CascadeCapCache s_cache[MAX_TEAMS];

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
		c.bValid = true;
	}
	return c;
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
	return ccap_get(eTeam).tradeTerrains.count((int)eT) != 0;
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
