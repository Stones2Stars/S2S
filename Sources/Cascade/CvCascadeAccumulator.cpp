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
#include "CvCascadeCommerceCalc.h"    // the §2 assembler -- the C_RATE component's recompute fn
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
	long aCRate[NUM_COMMERCE_TYPES];  // the assembled §2 commerce rates (×100; slider folded at recompute)
	int iDirty; int iEpoch; int iTurn;
	AccCityState() : iDirty(ACCD_ALL), iEpoch(-1), iTurn(-1)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; ++i) { aPct[i] = 100; aPlots[i] = 0; aSpec[i] = 0; aExtra100[i] = 0; aEmpFlat[i] = 0; }
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c) aCRate[c] = 0;
	}
};
static std::map<int, AccCityState> s_city;   // cid -> standing components
static int s_iEpoch = 0;                     // bumped by player/team-level events

static int acc_cid(const CvCity* pCity) { return ((int)pCity->getOwner()) * 100000 + pCity->getID(); }

void CascadeAccumulator::dirtyCity(const CvCity* pCity, int iMask)
{
	if (pCity == NULL) return;
	// commerce RIDES the yield components (§2 splits the modified commerce yield) -- the dependency lives HERE
	if (iMask & (ACCD_PCT | ACCD_PLOTS | ACCD_SPEC | ACCD_EXTRA | ACCD_EMPFLAT)) iMask |= ACCD_CRATE;
	const std::map<int, AccCityState>::iterator it = s_city.find(acc_cid(pCity));
	if (it != s_city.end()) it->second.iDirty |= iMask;   // absent = first read computes everything anyway
}

void CascadeAccumulator::dirtyPlayerCommerce(PlayerTypes ePlayer)
{
	// the slider folds into C_RATE at ITS recompute -- a slider move stales only that player's commerce slots
	const int lo = ((int)ePlayer) * 100000, hi = lo + 100000;
	for (std::map<int, AccCityState>::iterator it = s_city.lower_bound(lo); it != s_city.end() && it->first < hi; ++it)
		it->second.iDirty |= ACCD_CRATE;
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

// The §2a combine over the standing components + the ONE live INPUT (the trade-route yield) -- EXACTLY
// YieldRate::yieldRate100's expression. The yield components must be clean (acc_refresh ran) before calling.
static long acc_combine(const AccCityState& st, const CvCity* pCity, YieldTypes eY)
{
	const int trade = YieldBasePackages::tradeRoute(eY, pCity);
	long combine = (st.aPlots[eY] + trade + st.aEmpFlat[eY] + st.aSpec[eY]) * st.aPct[eY]
	             + 100L * (st.aExtra100[eY] / 100);
	if (combine < 100) combine = 100;
	if (combine > CITY_MAX_YIELD_RATE) combine = CITY_MAX_YIELD_RATE;
	return combine;
}

// Recompute the DIRTY components only (the calculator packages are the single-source recompute functions).
// FLIP PHASE (increment C): the turn roll RETURNS as the §3 re-check cadence -- with real consumers reading
// all turn, it fires at the turn's FIRST read (a genuine once-per-turn conditioned-deposit re-check + the
// self-heal for unhooked inputs: trade drift, religion spread, doubleTime year-crossings), and the end-of-turn
// [SLOT] sweep still measures MID-turn hook coverage non-tautologically. (The shadow phase deliberately ran
// WITHOUT it, proving the hook map on purely event-maintained state: [SLOT] 66/0 then 154/0, 2026-07-03.)
static void acc_refresh(const CvCity* pCity, AccCityState& st)
{
	const int iTurn = GC.getGame().getGameTurn();
	if (st.iEpoch != s_iEpoch || st.iTurn != iTurn)
	{
		st.iDirty = ACCD_ALL;
		st.iEpoch = s_iEpoch;
		st.iTurn = iTurn;
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
	// C_RATE LAST -- it rides the (now clean) yield components (§2 splits the modified commerce yield);
	// the slider folds in HERE (the setCommercePercent hook stales it).
	if (st.iDirty & ACCD_CRATE)
	{
		const long yc100 = acc_combine(st, pCity, YIELD_COMMERCE);
		const long prate = acc_combine(st, pCity, YIELD_PRODUCTION) / 100;
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
			st.aCRate[c] = CommerceCalc::commerceRate100(CommerceCalc::channel(c), (CommerceTypes)c, pCity, ec, yc100, prate);
	}
	st.iDirty = 0;
}

long CascadeAccumulator::yieldRate100(const CvCity* pCity, YieldTypes eY)
{
	if (pCity == NULL || eY < 0 || eY >= NUM_YIELD_TYPES) return 0;
	AccCityState& st = s_city[acc_cid(pCity)];
	acc_refresh(pCity, st);
	return acc_combine(st, pCity, eY);
}

long CascadeAccumulator::commerceRate100(const CvCity* pCity, CommerceTypes eC)
{
	if (pCity == NULL || eC < 0 || eC >= NUM_COMMERCE_TYPES) return 0;
	AccCityState& st = s_city[acc_cid(pCity)];
	acc_refresh(pCity, st);
	return st.aCRate[eC];
}
