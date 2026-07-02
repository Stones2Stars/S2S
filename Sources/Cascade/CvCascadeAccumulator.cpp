//
//	CascadeAccumulator -- the #430 modifier scope accumulator (see the header + modifier-substrate.md).
//	Standing per-city component slots over the calculator packages; event-driven freshness; O(1) clean reads.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeAccumulator.h"
#include "CvCascadePerfCount.h"       // per-turn call counters (the [MODIFIER/perf] census)
#include "CvCascadeYieldBasePackages.h"
#include "CvCascadeBuildingPackage.h"
#include "CvCascadePercentStack.h"
#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx
#include "CvCascadeEnablerKernel.h"   // computeCityBuildingFacts (memoized; evicted by the building hook)
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"              // GC.getGame()
#include "Engine/CvCity.h"            // CITY_MAX_YIELD_RATE
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"              // GET_TEAM
#include <map>
#include <set>
#include <string>

struct AccCityState
{
	long aPct[NUM_YIELD_TYPES];       // stored modifier = max(0, 100 + Σpercent) -- the whole §2a stack
	long aPlots[NUM_YIELD_TYPES];     // Σ worked plots' isolated base packages (human units)
	long aSpec[NUM_YIELD_TYPES];      // specialist totals (human units; own sub-stack inside)
	long aExtra100[NUM_YIELD_TYPES];  // building flats + perPop (×100)
	long aEmpFlat[NUM_YIELD_TYPES];   // free-city + golden-age trait flats (human units)
	int iDirty; int iEpoch; int iTurn;
	AccCityState() : iDirty(ACCD_ALL), iEpoch(-1), iTurn(-1)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; ++i) { aPct[i] = 100; aPlots[i] = 0; aSpec[i] = 0; aExtra100[i] = 0; aEmpFlat[i] = 0; }
	}
};
static std::map<int, AccCityState> s_city;   // cid -> standing components
static int s_iEpoch = 0;                     // bumped by player/team-level events

static int acc_cid(const CvCity* pCity) { return ((int)pCity->getOwner()) * 100000 + pCity->getID(); }

void CascadeAccumulator::dirtyCity(const CvCity* pCity, int iMask)
{
	if (pCity == NULL) return;
	const std::map<int, AccCityState>::iterator it = s_city.find(acc_cid(pCity));
	if (it != s_city.end()) it->second.iDirty |= iMask;   // absent = first read computes everything anyway
}

void CascadeAccumulator::bumpEpoch()
{
	++s_iEpoch;
}

static const char* acc_channel(int y)
{
	static const char* a[NUM_YIELD_TYPES] = { "food", "production", "commerce" };
	return a[y];
}

// Recompute the DIRTY components only (the calculator packages are the single-source recompute functions).
// The turn roll + the epoch mark everything dirty -- the §3 dormancy re-check cadence AND the self-heal for
// any mutation the coarse hooks miss (the [SLOT] shadow measures that residual).
static void acc_refresh(const CvCity* pCity, AccCityState& st)
{
	const int iTurn = GC.getGame().getGameTurn();
	if (st.iTurn != iTurn || st.iEpoch != s_iEpoch)
	{
		st.iDirty = ACCD_ALL;
		st.iTurn = iTurn;
		st.iEpoch = s_iEpoch;
	}
	if (st.iDirty == 0) return;
	++CascadePerf::accRefresh;

	// ONE ctx + facts pass serves every dirty component (facts are memoized; the building hook evicts them)
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ec;
	ec.city = pCity; ec.plot = pCity->plot(); ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
	std::set<int> activeB, provB;
	EnablerKernel::computeCityBuildingFacts(pCity, ec, activeB, provB);
	ec.activeBuildings = &activeB; ec.vicinityProvidedBonuses = &provB;

	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		const YieldTypes eY = (YieldTypes)y;
		const std::string ch = acc_channel(y);
		if (st.iDirty & ACCD_PCT)     { MMBreak bk; st.aPct[y] = PercentStack::percentStack(ch, pCity, bk); }
		if (st.iDirty & ACCD_PLOTS)   st.aPlots[y] = YieldBasePackages::basePlot(ch, eY, pCity, ec);
		if (st.iDirty & ACCD_SPEC)    st.aSpec[y] = YieldBasePackages::specialist(ch, pCity, ec);
		if (st.iDirty & ACCD_EXTRA)   st.aExtra100[y] = BuildingPackage::buildingFlat(ch, pCity, ec);
		if (st.iDirty & ACCD_EMPFLAT) st.aEmpFlat[y] = YieldBasePackages::freeCity(ch, player, ec)
		                                             + YieldBasePackages::goldenAge(ch, player, ec);
	}
	st.iDirty = 0;
}

long CascadeAccumulator::yieldRate100(const CvCity* pCity, YieldTypes eY)
{
	if (pCity == NULL || eY < 0 || eY >= NUM_YIELD_TYPES) return 0;
	AccCityState& st = s_city[acc_cid(pCity)];
	acc_refresh(pCity, st);
	// the ONE live INPUT (never stored -- modifier.md §2a: the calc folds it in, never derives it)
	const int trade = YieldBasePackages::tradeRoute(eY, pCity);
	// the §2a combine -- EXACTLY YieldRate::yieldRate100's expression, over the stored components
	long combine = (st.aPlots[eY] + trade + st.aEmpFlat[eY] + st.aSpec[eY]) * st.aPct[eY]
	             + 100L * (st.aExtra100[eY] / 100);
	if (combine < 100) combine = 100;
	if (combine > CITY_MAX_YIELD_RATE) combine = CITY_MAX_YIELD_RATE;
	return combine;
}
