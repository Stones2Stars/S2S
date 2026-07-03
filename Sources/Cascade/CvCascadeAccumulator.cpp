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
	long aSpec[NUM_YIELD_TYPES];      // specialist totals (human units; own sub-stack inside)
	long aExtra100[NUM_YIELD_TYPES];  // building flats + perPop (×100)
	long aEmpFlat[NUM_YIELD_TYPES];   // free-city + golden-age trait flats (human units)
	long aCSpec100[NUM_COMMERCE_TYPES]; // commerce specialist terms (×100) -- the hot commerce-side plugin
	long aCBase100[NUM_COMMERCE_TYPES]; // commerce baseExtra (religion/corp/GA/building block/playerExtra, ×100)
	long aCPct[NUM_COMMERCE_TYPES];     // commerce percent stacks (max(0, 100 + Σ))
	int iDirty; int iEpoch; int iTurn;
	AccCityState() : iDirty(ACCD_ALL), iEpoch(-1), iTurn(-1)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; ++i) { aPct[i] = 100; aSpec[i] = 0; aExtra100[i] = 0; aEmpFlat[i] = 0; }
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c) { aCSpec100[c] = 0; aCBase100[c] = 0; aCPct[c] = 100; }
	}
};
static std::map<int, AccCityState> s_city;   // cid -> standing components
static int s_iEpoch = 0;                     // bumped by player/team-level events

static int acc_cid(const CvCity* pCity) { return ((int)pCity->getOwner()) * 100000 + pCity->getID(); }

void CascadeAccumulator::dirtyCity(const CvCity* pCity, int iMask)
{
	if (pCity == NULL) return;
	// NO cross-component chaining: the commerce combine pulls the commerce YIELD fresh from the yield slots at
	// read time, so a yield-side change never recomputes a commerce package ("the rest of the pipe stays the
	// same" -- owner 2026-07-03). The slider is live at combine -- no hook exists for it at all.
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

// The §2a combine over the standing components + the LIVE inputs -- EXACTLY YieldRate::yieldRate100's
// expression. LIVE at combine: the trade-route yield (an O(1) engine accumulator) and the WORKED-PLOT BASE
// (CvCity::getPlotYield -- Σ worked plots × O(1) clean CvPlot caches, the state-repositories.md pull model;
// its own dirty triggers govern freshness, so worker/juggle churn costs the accumulator nothing).
// The yield components must be clean (acc_refresh ran) before calling.
static long acc_combine(const AccCityState& st, const CvCity* pCity, YieldTypes eY)
{
	const int plots = pCity->getPlotYield(eY);
	const int trade = YieldBasePackages::tradeRoute(eY, pCity);
	long combine = (plots + trade + st.aEmpFlat[eY] + st.aSpec[eY]) * st.aPct[eY]
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
		const std::string ch = acc_channel(y);
		if (st.iDirty & ACCD_PCT)     { MMBreak bk; st.aPct[y] = PercentStack::percentStack(ch, pCity, bk); }
		if (st.iDirty & ACCD_SPEC)    st.aSpec[y] = YieldBasePackages::specialist(ch, pCity, ec);
		if (st.iDirty & ACCD_EXTRA)   st.aExtra100[y] = BuildingPackage::buildingFlat(ch, pCity, ec);
		if (st.iDirty & ACCD_EMPFLAT) st.aEmpFlat[y] = YieldBasePackages::freeCity(ch, player, ec)
		                                             + YieldBasePackages::goldenAge(ch, player, ec);
	}
	// The commerce-side PLUGIN NUMBERS -- per channel, each package standing alone (owner 2026-07-03); the
	// combine (slider split, disorder, percent apply) happens at READ time over these + the live yield slots.
	if (st.iDirty & (ACCD_CSPEC | ACCD_CBASE | ACCD_CPCT))
	{
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		{
			const std::string ch = CommerceCalc::channel(c);
			if (st.iDirty & ACCD_CSPEC) st.aCSpec100[c] = 100L * YieldBasePackages::specialist(ch, pCity, ec);
			if (st.iDirty & ACCD_CBASE) st.aCBase100[c] = CommerceCalc::baseExtra100(ch, pCity, ec);
			if (st.iDirty & ACCD_CPCT)  { MMBreak bk; st.aCPct[c] = PercentStack::percentStack(ch, pCity, bk); }
		}
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
	// the §2 CombineSplit kernel (single-sourced in CommerceCalc) over the plugin numbers: the commerce YIELD
	// comes fresh from the yield slots; slider + disorder are read live inside the kernel.
	const long yc100 = acc_combine(st, pCity, YIELD_COMMERCE);
	const long prate = acc_combine(st, pCity, YIELD_PRODUCTION) / 100;
	return CommerceCalc::combineSplit(eC, pCity, yc100, prate, st.aCSpec100[eC] + st.aCBase100[eC], (int)st.aCPct[eC]);
}
