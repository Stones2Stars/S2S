//
//	CascadeAccumulator -- the #430 modifier scope accumulator (see the header + modifier-substrate.md).
//	Standing per-city component slots (ON CvCity, CvDerivedCacheSet-driven); event-driven freshness; O(1) clean
//	reads. The calculator packages are the single-source recompute functions.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeAccumulator.h"
#include "CvCascadePerfCount.h"       // per-turn call counters (the [MODIFIER/perf] census)
#include "CvCascadeYieldBasePackages.h"
#include "CvCascadeBuildingPackage.h"
#include "CvCascadePercentStack.h"
#include "CvCascadeCommerceCalc.h"    // baseExtra100 + channel + the CombineSplit kernel
#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx
#include "CvCascadeEnablerKernel.h"   // computeCityBuildingFacts (memoized; evicted by the building hook)
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"              // GC.getGame()
#include "Engine/CvCity.h"            // CITY_MAX_YIELD_RATE + m_cascadeRateSlots
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"              // GET_TEAM
#include <string>
#include <set>

static int s_iEpoch = 0;                     // the GLOBAL fallback epoch (game reset / non-player-attributable)
static int s_aiPlayerEpoch[MAX_PLAYERS];     // per-player epochs (civics / GA / team techs) -- zero-init statics

void CascadeAccumulator::dirtyCity(const CvCity* pCity, int iMask)
{
	if (pCity == NULL) return;
	// NO cross-component chaining: the commerce combine pulls the commerce YIELD fresh from the yield slots at
	// read time; the slider is live at combine (no hook exists for it at all).
	pCity->m_cascadeRateSlots.set.markDirty(iMask);
}

void CascadeAccumulator::bumpEpoch()
{
	++s_iEpoch;
}

void CascadeAccumulator::bumpPlayerEpoch(PlayerTypes ePlayer)
{
	if (ePlayer >= 0 && ePlayer < MAX_PLAYERS) ++s_aiPlayerEpoch[ePlayer];
}

static const char* acc_channel(int y)
{
	static const char* a[NUM_YIELD_TYPES] = { "food", "production", "commerce" };
	return a[y];
}

// The §2a combine over the standing components + the LIVE inputs -- EXACTLY YieldRate::yieldRate100's
// expression. LIVE at combine: the trade-route yield (an O(1) engine accumulator) and the WORKED-PLOT BASE
// (CvCity::getPlotYield -- Σ worked plots × O(1) clean CvPlot caches, the state-repositories.md pull model).
static long acc_combine(const CascadeRateSlots& st, const CvCity* pCity, YieldTypes eY)
{
	const int plots = pCity->getPlotYield(eY);
	const int trade = YieldBasePackages::tradeRoute(eY, pCity);
	long combine = (plots + trade + st.aEmpFlat[eY] + st.aSpec[eY]) * st.aPct[eY]
	             + 100L * (st.aExtra100[eY] / 100);
	if (combine < 100) combine = 100;
	if (combine > CITY_MAX_YIELD_RATE) combine = CITY_MAX_YIELD_RATE;
	return combine;
}

// Epoch/turn pre-check + the Set's pull: the turn roll is the §3 re-check cadence (and the self-heal for
// unhooked mutations); the epoch reaches every city of an affected player. The Set's ensure() then refreshes
// exactly the dirty components via CvCity::cascadeRefreshRates -> refreshComponents below.
static void acc_ensure(const CvCity* pCity)
{
	CascadeRateSlots& st = pCity->m_cascadeRateSlots;
	const int iTurn = GC.getGame().getGameTurn();
	const int iEpoch = s_iEpoch + s_aiPlayerEpoch[(int)pCity->getOwner()];   // both monotonic -- the sum never collides
	if (st.iEpoch != iEpoch || st.iTurn != iTurn)
	{
		st.iEpoch = iEpoch;
		st.iTurn = iTurn;
		st.set.markAllDirty();
	}
	st.set.ensure();
}

void CascadeAccumulator::refreshComponents(const CvCity* pCity, int iMask)
{
	if (pCity == NULL || iMask == 0) return;
	++CascadePerf::accRefresh;
	CascadeRateSlots& st = pCity->m_cascadeRateSlots;

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
		if (iMask & ACCD_PCT)     { MMBreak bk; st.aPct[y] = PercentStack::percentStack(ch, pCity, bk); }
		if (iMask & ACCD_SPEC)    st.aSpec[y] = YieldBasePackages::specialist(ch, pCity, ec);
		if (iMask & ACCD_EXTRA)   st.aExtra100[y] = BuildingPackage::buildingFlat(ch, pCity, ec);
		if (iMask & ACCD_EMPFLAT) st.aEmpFlat[y] = YieldBasePackages::freeCity(ch, player, ec)
		                                         + YieldBasePackages::goldenAge(ch, player, ec);
	}
	// The commerce-side PLUGIN NUMBERS -- per channel, each package standing alone (owner 2026-07-03); the
	// combine (slider split, disorder, percent apply) happens at READ time over these + the live yield slots.
	if (iMask & (ACCD_CSPEC | ACCD_CBASE | ACCD_CPCT))
	{
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		{
			const std::string ch = CommerceCalc::channel(c);
			if (iMask & ACCD_CSPEC) st.aCSpec100[c] = 100L * YieldBasePackages::specialist(ch, pCity, ec);
			if (iMask & ACCD_CBASE) st.aCBase100[c] = CommerceCalc::baseExtra100(ch, pCity, ec);
			if (iMask & ACCD_CPCT)  { MMBreak bk; st.aCPct[c] = PercentStack::percentStack(ch, pCity, bk); }
		}
	}
}

long CascadeAccumulator::yieldRate100(const CvCity* pCity, YieldTypes eY)
{
	if (pCity == NULL || eY < 0 || eY >= NUM_YIELD_TYPES) return 0;
	acc_ensure(pCity);
	return acc_combine(pCity->m_cascadeRateSlots, pCity, eY);
}

long CascadeAccumulator::commerceRate100(const CvCity* pCity, CommerceTypes eC)
{
	if (pCity == NULL || eC < 0 || eC >= NUM_COMMERCE_TYPES) return 0;
	acc_ensure(pCity);
	const CascadeRateSlots& st = pCity->m_cascadeRateSlots;
	// the §2 CombineSplit kernel (single-sourced in CommerceCalc) over the plugin numbers: the commerce YIELD
	// comes fresh from the yield slots; slider + disorder are read live inside the kernel.
	const long yc100 = acc_combine(st, pCity, YIELD_COMMERCE);
	const long prate = acc_combine(st, pCity, YIELD_PRODUCTION) / 100;
	return CommerceCalc::combineSplit(eC, pCity, yc100, prate, st.aCSpec100[eC] + st.aCBase100[eC], (int)st.aCPct[eC]);
}
