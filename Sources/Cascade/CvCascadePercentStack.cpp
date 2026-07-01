//
//	PercentStack -- StoneBase PercentStack.cs (see the header). Ported VERBATIM from CvCascadeModifierMath.cpp's
//	file-static mm_percentStack; promoted to a declared surface (the single-source law, patterns.md). LOGIC unchanged:
//	only the signature + the MMKernel-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
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
#include "CvCascadeEnablerKernel.h"    // EnablerKernel::computeCityBuildingFacts (the cascade-computed active set + vicinity provides)

// The percent stack for one channel at one city: max(0, 100 + Σ percent) over active city buildings (city+area),
// empire buildings (empire), adopted civics (empire), and the player's active traits (empire). Fills the breakdown.
int PercentStack::percentStack(const std::string& channel, const CvCity* pCity, MMBreak& bk)
{
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ec;                               // the live-engine eval target for the deposit conditions
	ec.city = pCity; ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
	std::set<int> activeB, provB;                      // cascade-COMPUTED active set + in-vicinity provides (dormancy derived from operate, not the engine)
	EnablerKernel::computeCityBuildingFacts(pCity, ec, activeB, provB);
	ec.activeBuildings = &activeB; ec.vicinityProvidedBonuses = &provB;
	const std::string wantCity = channel + ".city";
	const std::string wantArea = channel + ".area";
	const std::string wantEmpire = channel + ".empire";

	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		const BuildingTypes eB = (BuildingTypes)b;
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
