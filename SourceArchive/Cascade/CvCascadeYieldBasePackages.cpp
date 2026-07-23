//
//	YieldBasePackages -- StoneBase YieldBasePackages.cs (see the header). Ported VERBATIM from CvCascadeModifierMath.cpp's
//	file-static cvModifierTradeRoute/FreeCity/GoldenAge/Specialist; promoted to a declared surface (the
//	single-source law, patterns.md). LOGIC unchanged: only the signatures + the MMKernel-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeYieldBasePackages.h"
#include "Data/CvDepositRead.h"
#include "CvInfo.h"                // CvInfo (the spec model the DepositIndex compiled from)
#include "Repos/InfoRepo.h"            // InfoRepo<CvXInfo>::get().get(id)
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "CvBuildingInfo.h"
#include "CvCivicInfo.h"
#include "CvTraitInfo.h"
#include "CvSpecialistInfo.h"   // InfoRepo<CvSpecialistInfo> + GC.getSpecialistInfo (the §1 specialist package)
#include "AI/CvPlayerAI.h"             // GET_PLAYER

// BASE: trade-route yield (TradeRoutePackage) -- the ONE allowed live-yield INPUT (the cascade folds it in, never
// derives it; owner ruling 2026-06-28). Read from the live engine, x1.
int YieldBasePackages::tradeRoute(YieldTypes eY, const CvCity* pCity)
{
	return pCity->getTradeYield(eY);
}

// BASE: free-city yield (FreeCityPackage) -- COMPUTED (not echoed): Σ the player's active traits' {ch}.empire.flat
// (curate_trait YieldChanges). x1. (Active set option-gated + PURE_TRAITS via sumTrait/traitData.)
int YieldBasePackages::freeCity(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec)
{
	const std::string wantEmpire = channel + ".empire";
	int sum = 0;
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!player.hasTrait((TraitTypes)t)) continue;
		sum += MMKernel::sumTrait(MMKernel::traitData(t), wantEmpire, "flat", ec);
	}
	return sum;
}

// BASE: the golden-age trait member ({ch}.empire.goldenAge.flat), UNGATED -- the scope-package fill stores
// this and the isGoldenAge gate applies LIVE at read (a GA flip invalidates nothing flat-side). x1.
int YieldBasePackages::goldenAgeUngated(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec)
{
	const std::string wantGA = channel + ".empire.goldenAge";
	int sum = 0;
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!player.hasTrait((TraitTypes)t)) continue;
		sum += MMKernel::sumTrait(MMKernel::traitData(t), wantGA, "flat", ec);
	}
	return sum;
}

// BASE: specialist yields (SpecialistPackage / calc-map §1.5) -- Σ over the city's assigned+typed-free specialists of
// count × the engine terms: intrinsic × (100+pct)/100 (the percent MULTIPLIES the intrinsic only, ÷100 ONCE) +
// building-local (gated city.flat, no percent) + perType (empire.flat, no percent) + governing-deliverer trait
// (empire.specialists.{SPEC}.flat, active set) + perAll (building/civic/trait empire.specialist.perSpecialist × TOTAL
// specialists). x1; channel-agnostic (reused for §2 commerce). count = getSpecialistCount + getFreeSpecialistCount
// (calc-map §1.5). (Trait reads use the option-gated active set + PURE_TRAITS via sumTrait/traitData; the generic
// building-free-spec output deferred, calc-map §2.)
int YieldBasePackages::specialist(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& player = *ec.player;
	const std::string wantCity = channel + ".city";
	const std::string wantEmpire = channel + ".empire";
	const std::string wantPerAll = channel + ".empire.specialist";   // the perSpecialist member node
	const int nSpec = GC.getNumSpecialistInfos();

	int total = 0, totalSpecialists = 0;
	long specPct100 = 0;   // ×100 accumulator for the intrinsic×(100+pct) part (÷100 ONCE -- engine truncation order)
	for (int s = 0; s < nSpec; ++s)
	{
		const int count = pCity->getSpecialistCount((SpecialistTypes)s) + pCity->getFreeSpecialistCount((SpecialistTypes)s);
		if (count == 0) continue;
		totalSpecialists += count;
		const CvInfo* d = InfoRepo<CvSpecialistInfo>::get().get(s);
		if (d == NULL) continue;
		const int intrinsic  = MMKernel::sumUnconditioned(d, wantCity, "flat");           // own ungated getYield/CommerceChange
		const int local      = MMKernel::sumUnit(d, wantCity, "flat", ec) - intrinsic;    // building-local (gated) -- no percent
		const int empireFlat = MMKernel::sumUnit(d, wantEmpire, "flat", ec);              // perType (building non-local) -- no percent
		const int pct        = MMKernel::sumUnit(d, wantCity, "percent", ec);
		specPct100 += (long)count * intrinsic * (100 + pct);                       // percent MULTIPLIES the intrinsic
		total += count * (local + empireFlat);
	}
	total += (int)(specPct100 / 100);   // single ÷100 (matches the engine's ÷100-once)

	// (3) GOVERNING-DELIVERER trait (active set, keyed by specialist) × that specialist's count.
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!player.hasTrait((TraitTypes)t)) continue;
		const CvTraitInfo* dt = MMKernel::traitData(t);
		if (dt == NULL) continue;
		for (int s = 0; s < nSpec; ++s)
		{
			const int count = pCity->getSpecialistCount((SpecialistTypes)s) + pCity->getFreeSpecialistCount((SpecialistTypes)s);
			if (count == 0) continue;
			const std::string keyAddr = wantEmpire + ".specialists." + GC.getSpecialistInfo((SpecialistTypes)s).getType();
			const int per = MMKernel::sumTrait(dt, keyAddr, "flat", ec);
			if (per != 0) total += count * per;
		}
	}

	// (4) PER-ALL -- building + civic + trait empire.specialist.perSpecialist × TOTAL specialists (the player accumulator,
	// EMPIRE-wide incl. buildings -- a wonder boosts every city's specialists, calc-map §1.5).
	int perAll = 0;
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		if (player.getBuildingCount((BuildingTypes)b) <= 0) continue;
		const CvInfo* db = InfoRepo<CvBuildingInfo>::get().get(b);
		if (db != NULL) perAll += MMKernel::sumUnit(db, wantPerAll, "perSpecialist", ec);
	}
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = player.getCivics((CivicOptionTypes)co);
		if (c == NO_CIVIC) continue;
		const CvInfo* dc = InfoRepo<CvCivicInfo>::get().get((int)c);
		if (dc != NULL) perAll += MMKernel::sumUnit(dc, wantPerAll, "perSpecialist", ec);
	}
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!player.hasTrait((TraitTypes)t)) continue;
		perAll += MMKernel::sumTrait(MMKernel::traitData(t), wantPerAll, "perSpecialist", ec);   // active set + PURE_TRAITS
	}
	total += totalSpecialists * perAll;
	return total;
}
