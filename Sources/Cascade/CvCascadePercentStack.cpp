//
//	PercentStack -- StoneBase PercentStack.cs (see the header). Ported VERBATIM from CvCascadeModifierMath.cpp's
//	file-static mm_percentStack; promoted to a declared surface (the single-source law, patterns.md). LOGIC unchanged:
//	only the signature + the MMKernel-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadePerfCount.h"   // per-turn call counters + stopwatches (owner 2026-07-02: repeat-calc hunt)
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "CvCascadePercentStack.h"
#include "CvCascadeMMKernel.h"
#include "CvJsonInfo.h"                // CvJsonInfo + CvCascadeDeposit
#include "Repos/InfoRepo.h"            // InfoRepo<CvXInfo>::get().get(id)
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvProjectInfo.h"      // InfoRepo<CvProjectInfo> tag + ProjectTypes (the projects empire.percent loop)
#include "AI/CvPlayerAI.h"             // GET_PLAYER
#include "AI/CvTeamAI.h"              // GET_TEAM
#include "CvCascadeConditionEval.h"    // CvCascadeEvalCtx + cascadeIsBuildingActive
#include "CvCascadeEnablerKernel.h"    // EnablerKernel::wireFacts (the standing cascade active set + vicinity provides)
#include "CvCascadeCityFacts.h"        // CascadeCityFacts -- the areaPercentByArea player-city walk
#include "CvCascadeDepositIndex.h"     // DepositIndex -- the compiled deposit index (the candidate prefilter)
#include "Engine/CvArea.h"             // area()->getID() -- the area-map grouping
#include <map>
#include <vector>

// PER-CHANNEL PERCENT-CANDIDATE CACHE (the compiled-deposit-index increment): which building ids carry ANY
// scope-wide percent deposit for this channel at city/area/empire is STATIC readJson data -- computed once per
// channel by ONE scan of the building repo (compiled-int matches), so the stack walks candidates instead of all
// ~5202 building infos per call (the modifier-substrate.md "deep perf lever"). Same one-shot-readJson caveat as
// the basePlot candidate cache.
static const std::vector<int>& ps_channelCands(int chanId, int segCity, int segArea, int segEmpire, int segPercent)
{
	static std::map<int, std::vector<int> > s_cands;
	std::map<int, std::vector<int> >::iterator it = s_cands.find(chanId);
	if (it == s_cands.end())
	{
		std::vector<int> cands;
		const int nB = GC.getNumBuildingInfos();
		if (chanId >= 0)
			for (int b = 0; b < nB; ++b)
			{
				const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
				if (d == NULL) continue;
				for (size_t i = 0; i < d->deposits.size(); ++i)
				{
					const CvCascadeDeposit& dep = d->deposits[i];
					if (dep.unitId != segPercent || dep.nSeg != 2 || dep.seg[0] != chanId) continue;
					if (dep.seg[1] != segCity && dep.seg[1] != segArea && dep.seg[1] != segEmpire) continue;
					cands.push_back(b);
					break;
				}
			}
		it = s_cands.insert(std::make_pair(chanId, cands)).first;
	}
	return it->second;
}

// The CITY-REALIZED whole stack (see the header): the percentStack walk, returned as the raw Σ (the bucket
// sum -- percentStack's return is max(0, 100 + this)). ONE walk, two entry points.
long PercentStack::cityRealizedPercent(const std::string& channel, const CvCity* pCity, MMBreak& bk)
{
	percentStack(channel, pCity, bk);
	return (long)bk.bCity + bk.bArea + bk.bEmpire + bk.civic + bk.trait;
}

// The percent stack for one channel at one city: max(0, 100 + Σ percent) over active city buildings (city+area),
// empire buildings (empire), adopted civics (empire), and the player's active traits (empire). Fills the breakdown.
int PercentStack::percentStack(const std::string& channel, const CvCity* pCity, MMBreak& bk)
{
	++CascadePerf::pctStack;
	PerfAccumTimer perfT(CascadePerf::pctStackMs);
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ec;                               // the live-engine eval target for the deposit conditions
	ec.city = pCity; ec.plot = pCity->plot(); ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
	EnablerKernel::wireFacts(pCity, ec);               // the STANDING cascade facts (active set + vicinity provides)
	const std::string wantCity = channel + ".city";
	const std::string wantArea = channel + ".area";
	const std::string wantEmpire = channel + ".empire";

	// candidates only (see ps_channelCands): a building with NO scope-wide percent deposit in this channel sums 0
	const std::vector<int>& cands = ps_channelCands(DepositIndex::lookupSegment(channel),
		DepositIndex::lookupSegment("city"), DepositIndex::lookupSegment("area"),
		DepositIndex::lookupSegment("empire"), DepositIndex::lookupSegment("percent"));
	for (size_t ci = 0; ci < cands.size(); ++ci)
	{
		const BuildingTypes eB = (BuildingTypes)cands[ci];
		const bool active = cascadeIsBuildingActive((int)eB, ec); // non-dormant, in this city (cascade-computed)
		const bool owned = player.getBuildingCount(eB) > 0;       // anywhere in the empire
		if (!active && !owned) continue;
		const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get((int)eB);
		if (d == NULL) continue;
		if (active) { bk.bCity += MMKernel::sumPercent(d, wantCity, ec); bk.bArea += MMKernel::sumPercent(d, wantArea, ec); }
		if (owned) bk.bEmpire += MMKernel::sumPercent(d, wantEmpire, ec);
	}
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = player.getCivics((CivicOptionTypes)co);
		if (c == NO_CIVIC) continue;
		const CvJsonInfo* d = InfoRepo<CvCivicInfo>::get().get((int)c);
		if (d != NULL) bk.civic += MMKernel::sumPercent(d, wantEmpire, ec);
	}
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!player.hasTrait((TraitTypes)t)) continue;
		bk.trait += MMKernel::sumTrait(MMKernel::traitData(t), wantEmpire, "percent", ec);
	}
	// CIVIC BUILDING-KEYED percent (owner.getBuildingCommerceModifier, StoneBase BuildingKeyedSourcePercent): folded into
	// modBuilding per active building -> the city/building tier (bCity), matching StoneBase's ModifierBreakdown bucket.
	bk.bCity += MMKernel::buildingKeyedSourcePercent(channel, pCity, ec);
	// PROJECT empire-scope percent (StoneBase projectEmpire; the engine's project modifier accumulator, empire-scope rolls
	// down to every city). Projects are TEAM-owned in the engine -> read the team's project count. Yield channels find none
	// (commerce-impacting only). Projects are mapped into InfoRepo<CvProjectInfo> (generic CvJsonInfo deposits) by readJson.
	const CvTeam& team = GET_TEAM(player.getTeam());
	for (int pj = 0; pj < GC.getNumProjectInfos(); ++pj)
	{
		if (team.getProjectCount((ProjectTypes)pj) <= 0) continue;
		const CvJsonInfo* d = InfoRepo<CvProjectInfo>::get().get(pj);
		if (d != NULL) bk.bEmpire += MMKernel::sumPercent(d, wantEmpire, ec);
	}
	return std::max(0, 100 + bk.bCity + bk.bArea + bk.bEmpire + bk.civic + bk.trait);
}
