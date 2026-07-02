//
//	YieldRate -- StoneBase YieldRate.cs (see the header). Ported VERBATIM from CvCascadeModifierMath.cpp's file-static
//	cvModifierYieldRate100; promoted to a declared surface (the single-source law, patterns.md). LOGIC unchanged: only the
//	signature + the package-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadePerfCount.h"   // per-turn call counters + stopwatches (owner 2026-07-02: repeat-calc hunt)
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "CvCascadeYieldRate.h"
#include "CvCascadeYieldBasePackages.h"
#include "CvCascadeBuildingPackage.h"
#include "CvCascadePercentStack.h"    // MMBreak + PercentStack
#include "Engine/CvCity.h"            // CITY_MAX_YIELD_RATE (the engine #define)
#include "AI/CvGameAI.h"              // GC.getGame() (returns CvGameAI&) -- the turn-scoped rate memo

// The §1 YIELD-RATE ASSEMBLER (YieldRate.cs / modifier.md §2a, calc-map §1.2):
//   rate100 = min(CAP, max(100, (Σ BASE + specialist) × max(0,modifier) + 100·⌊AFTER100/100⌋))
// All §1 BASE/AFTER terms now ported (PlotPackage + Specialist + trade + free-city + golden-age + building-flat); the
// combine + clamp mirror StoneBase YieldRate.cs (the verified-final order). The holistic shadow (vs getYieldRate100)
// follows; per StoneBase strategy (modifier-machine §0) parity is judged AFTER the whole calc is in.
long YieldRate::yieldRate100(const std::string& channel, YieldTypes eY, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	++CascadePerf::yieldRate;
	PerfAccumTimer perfT(CascadePerf::yieldRateMs);
	// TURN-SCOPED MEMO (perf census 2026-07-02: 444 calls x ~164ms = 73s/turn, mostly RE-computes of the same
	// (city, channel) -- the getter instrument re-derives what the shadow already computed; every commerce check
	// re-derives the same commerce/production rates per city). Same shadow-phase-only caveat as the facts memo:
	// a mid-turn building change goes stale until the next turn -- fine while legacy stays authoritative; MUST be
	// event-invalidated before any consumer cut. Keyed (city, eY): the channel<->eY mapping is 1:1 on this plane.
	typedef std::map<int, long> RateMemo;
	static RateMemo s_memo;
	static int s_iMemoTurn = -1;
	int iKey = -1;
	if (pCity != NULL)
	{
		const int iTurn = GC.getGame().getGameTurn();
		if (iTurn != s_iMemoTurn) { s_memo.clear(); s_iMemoTurn = iTurn; }
		iKey = (((int)pCity->getOwner()) * 100000 + pCity->getID()) * 4 + (int)eY;
		RateMemo::const_iterator mit = s_memo.find(iKey);
		if (mit != s_memo.end()) return mit->second;
	}

	const long basePlot   = YieldBasePackages::basePlot(channel, eY, pCity, ec);
	const int  trade      = YieldBasePackages::tradeRoute(eY, pCity);
	const int  freeCity   = YieldBasePackages::freeCity(channel, *ec.player, ec);
	const int  goldenAge  = YieldBasePackages::goldenAge(channel, *ec.player, ec);
	const int  specialist = YieldBasePackages::specialist(channel, pCity, ec);
	const long after100   = BuildingPackage::buildingFlat(channel, pCity, ec);
	MMBreak bk;
	const int  modifier   = PercentStack::percentStack(channel, pCity, bk);   // already max(0, 100 + Σ%)

	const long baseSum = basePlot + trade + freeCity + goldenAge;
	long combine = (baseSum + specialist) * (long)modifier + 100L * (after100 / 100);
	if (combine < 100) combine = 100;
	if (combine > CITY_MAX_YIELD_RATE) combine = CITY_MAX_YIELD_RATE;   // the engine #define (CvCity.h:25), NOT a GlobalDefine
	if (iKey != -1) s_memo[iKey] = combine;
	return combine;
}
