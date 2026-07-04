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
#include "CvCascadeEnablerKernel.h"   // EnablerKernel::wireFacts -- the standing per-city building-facts cache
#include "CvCascadeWellbeing.h"       // the §2b wellbeing verdict component (ACCD_WB)
#include "CvCascadeScalarChannels.h"  // the scalar-channel calculators -- the ACCD_SCALAR* recompute fns (increment F)
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

int CascadeAccumulator::epochFor(PlayerTypes ePlayer)
{
	return s_iEpoch + (ePlayer >= 0 && ePlayer < MAX_PLAYERS ? s_aiPlayerEpoch[ePlayer] : 0);   // both monotonic -- the sum never collides
}

bool CvCascadePlayerStamp::freshen(PlayerTypes ePlayer)
{
	const int e = CascadeAccumulator::epochFor(ePlayer);
	const int t = GC.getGame().getGameTurn();
	if (iEpoch == e && iTurn == t) return true;
	iEpoch = e;
	iTurn = t;
	return false;
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
static void acc_ensure(const CvCity* pCity, int iWantMask)
{
	CascadeRateSlots& st = pCity->m_cascadeRateSlots;
	const int iTurn = GC.getGame().getGameTurn();
	const int iEpoch = CascadeAccumulator::epochFor(pCity->getOwner());
	if (st.iEpoch != iEpoch || st.iTurn != iTurn)
	{
		st.iEpoch = iEpoch;
		st.iTurn = iTurn;
		st.set.markAllDirty();
	}
	// MASKED: a rate read never pays the wellbeing walk (unit moves dirty ACCD_WB constantly -- the
	// unmasked form made every post-move yield read recompute wellbeing; measured as a massive
	// unit-automation regression). Each read path ensures only its own components.
	st.set.ensure(iWantMask);
}

void CascadeAccumulator::refreshComponents(const CvCity* pCity, int iMask)
{
	if (pCity == NULL || iMask == 0) return;
	++CascadePerf::accRefresh;
	CascadeRateSlots& st = pCity->m_cascadeRateSlots;

	// ONE ctx serves every dirty component; the facts are the STANDING per-city cache (event-invalidated,
	// same epoch/turn protocol as these slots -- EnablerKernel::cityFacts).
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ec;
	ec.city = pCity; ec.plot = pCity->plot(); ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
	EnablerKernel::wireFacts(pCity, ec);

	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		const std::string ch = acc_channel(y);
		if (iMask & ACCD_PCT)     { MMBreak bk; st.aPct[y] = PercentStack::percentStack(ch, pCity, bk); }
		if (iMask & ACCD_SPEC)    st.aSpec[y] = YieldBasePackages::specialist(ch, pCity, ec);
		if (iMask & ACCD_EXTRA)   st.aExtra100[y] = BuildingPackage::buildingFlat(ch, pCity, ec);
		if (iMask & ACCD_EMPFLAT) st.aEmpFlat[y] = YieldBasePackages::freeCity(ch, player, ec)
		                                         + YieldBasePackages::goldenAge(ch, player, ec);
	}
	// The §2b WELLBEING verdicts -- one component, all four verdicts together (no plot walks; cheap relative to
	// its dirty cadence: building events + population/specialist churn + the epoch + the turn roll).
	if (iMask & ACCD_WB)
	{
		const CascadeWellbeingVerdicts wv = CascadeWellbeing::compute(pCity, ec);
		st.aWb[0] = wv.iHappy; st.aWb[1] = wv.iUnhappy; st.aWb[2] = wv.iGood; st.aWb[3] = wv.iBad;
		st.iWbMilPerUnit = wv.iMilPerUnit;
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
	// The city SCALAR channels (increment F) -- the CascadeScalarChannels calculators ARE the recompute
	// functions, called whole (the substrate law: this layer only decides WHEN they run); their player-wide
	// building walks are rollup-cached per (player, epoch, turn) inside the calculators.
	if (iMask & ACCD_SCALAR)
	{
		st.iScGpBaseBld = CascadeScalarChannels::gpBaseBuildings(pCity, ec);
		st.iScGpMod = CascadeScalarChannels::gpRateModifier(pCity, ec);
		st.iScDefense = CascadeScalarChannels::defenseAmount(pCity, ec);
		st.iScMaintMod = CascadeScalarChannels::maintenanceModifier(pCity, ec);
		st.iScTradeRoutes = CascadeScalarChannels::tradeRouteCount(pCity, ec);
	}
	if (iMask & ACCD_SCALARSPEC)
		st.iScGpBaseSpec = CascadeScalarChannels::gpBaseSpecialists(pCity, ec);
}

long CascadeAccumulator::yieldRate100(const CvCity* pCity, YieldTypes eY)
{
	if (pCity == NULL || eY < 0 || eY >= NUM_YIELD_TYPES) return 0;
	acc_ensure(pCity, ACCD_RATES);   // NEVER the ALL mask: a rate read must not pay the WB/scalar walks
	return acc_combine(pCity->m_cascadeRateSlots, pCity, eY);
}

// ---- the city SCALAR channel reads (increment F): each pays only its own components ----
int CascadeAccumulator::scGpBase(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	acc_ensure(pCity, ACCD_SCALAR | ACCD_SCALARSPEC);
	const CascadeRateSlots& st = pCity->m_cascadeRateSlots;
	return st.iScGpBaseBld + st.iScGpBaseSpec;
}

int CascadeAccumulator::scGpModifier(const CvCity* pCity)
{
	if (pCity == NULL) return 100;
	acc_ensure(pCity, ACCD_SCALAR);
	return pCity->m_cascadeRateSlots.iScGpMod;
}

int CascadeAccumulator::scDefense(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	acc_ensure(pCity, ACCD_SCALAR);
	return pCity->m_cascadeRateSlots.iScDefense;
}

int CascadeAccumulator::scMaintenanceModifier(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	acc_ensure(pCity, ACCD_SCALAR);
	return pCity->m_cascadeRateSlots.iScMaintMod;
}

int CascadeAccumulator::scTradeRoutes(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	acc_ensure(pCity, ACCD_SCALAR);
	return pCity->m_cascadeRateSlots.iScTradeRoutes;
}

int CascadeAccumulator::wellbeing(const CvCity* pCity, int iVerdict)
{
	if (pCity == NULL || iVerdict < 0 || iVerdict > 3) return 0;
	acc_ensure(pCity, ACCD_WB);
	const CascadeRateSlots& st = pCity->m_cascadeRateSlots;
	if (iVerdict >= 2) return st.aWb[iVerdict];   // health has no military term
	// the MILITARY term rides ALONE on top (owner ruling 2026-07-03): the epoch-stable perUnit value × the
	// LIVE O(1) engine counter -- always current, never invalidates any cache.
	const int iMil = st.iWbMilPerUnit * pCity->getMilitaryHappinessUnits();
	return iVerdict == 0
		? std::max(0, st.aWb[0] + std::max(0, iMil))
		: std::max(0, st.aWb[1] - std::min(0, iMil));
}

long CascadeAccumulator::commerceRate100(const CvCity* pCity, CommerceTypes eC)
{
	if (pCity == NULL || eC < 0 || eC >= NUM_COMMERCE_TYPES) return 0;
	acc_ensure(pCity, ACCD_RATES);   // NEVER the ALL mask: a rate read must not pay the WB/scalar walks
	const CascadeRateSlots& st = pCity->m_cascadeRateSlots;
	// the §2 CombineSplit kernel (single-sourced in CommerceCalc) over the plugin numbers: the commerce YIELD
	// comes fresh from the yield slots; slider + disorder are read live inside the kernel.
	const long yc100 = acc_combine(st, pCity, YIELD_COMMERCE);
	const long prate = acc_combine(st, pCity, YIELD_PRODUCTION) / 100;
	return CommerceCalc::combineSplit(eC, pCity, yc100, prate, st.aCSpec100[eC] + st.aCBase100[eC], (int)st.aCPct[eC]);
}
