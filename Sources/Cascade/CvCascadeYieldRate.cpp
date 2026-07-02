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

// The §1 YIELD-RATE ASSEMBLER (YieldRate.cs / modifier.md §2a, calc-map §1.2):
//   rate100 = min(CAP, max(100, (Σ BASE + specialist) × max(0,modifier) + 100·⌊AFTER100/100⌋))
// All §1 BASE/AFTER terms now ported (PlotPackage + Specialist + trade + free-city + golden-age + building-flat); the
// combine + clamp mirror StoneBase YieldRate.cs (the verified-final order). The holistic shadow (vs getYieldRate100)
// follows; per StoneBase strategy (modifier-machine §0) parity is judged AFTER the whole calc is in.
long YieldRate::yieldRate100(const std::string& channel, YieldTypes eY, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	++CascadePerf::yieldRate;
	PerfAccumTimer perfT(CascadePerf::yieldRateMs);
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
	return combine;
}
