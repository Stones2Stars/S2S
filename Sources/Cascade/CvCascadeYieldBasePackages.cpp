//
//	YieldBasePackages -- StoneBase YieldBasePackages.cs (see the header). Ported VERBATIM from CvCascadeModifierMath.cpp's
//	file-static cvModifierBasePlot/TradeRoute/FreeCity/GoldenAge/Specialist; promoted to a declared surface (the
//	single-source law, patterns.md). LOGIC unchanged: only the signatures + the MMKernel-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeYieldBasePackages.h"
#include "CvCascadeMMKernel.h"
#include "CvJsonInfo.h"                // CvJsonInfo + CvCascadeDeposit
#include "Repos/InfoRepo.h"            // InfoRepo<CvXInfo>::get().get(id)
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvPlot.h"
#include "Engine/CvGame.h"            // GC.getGame().isOption (PURE_TRAITS sign)
#include "Infos/CvYieldInfo.h"        // the per-yield config constants (peak/hills/city/pop/goldenAge/minCity changes)
#include "Infos/CvTerrainInfo.h"
#include "Infos/CvFeatureInfo.h"
#include "Infos/CvBonusInfo.h"
#include "Infos/CvImprovementInfo.h"  // + getImprovementUpgrade (the keyed-yield upgrade-ancestor chain)
#include "Infos/CvRouteInfo.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvTraitInfo.h"
#include "Infos/CvSpecialistInfo.h"   // InfoRepo<CvSpecialistInfo> + GC.getSpecialistInfo (the §1 specialist package)
#include "AI/CvPlayerAI.h"             // GET_PLAYER

// BASE: worked-plot yields (PlotPackage / calc-map §10.1) -- Σ over the city's WORKED plots of each plot's ONE isolated
// base package, RE-DERIVED from the substrate JSON + keyed building/civic/trait deposits (NOT the engine's computed
// getPlotYield -- the cascade computes it). Mirrors CvPlot::calculateYield order (Explore-verified 2026-06-30): nature
// (max0 of relief+terrain+feature+bonus) + centre + plots-target + keyed-CITY = the pre-improvement running yield; the
// extra/less + golden-age thresholds test on it; then the improvement addend (floored at -nature) + route-own-flat,
// max(0,·); a CITY-CENTRE plot gets the min-city floor instead of improvement/route. (Traits use the option-gated active
// set + PURE_TRAITS sign filter; the per-plot m_aExtraYield is event-granted, not derivable -> 0, audit-only per calc-map.)
int YieldBasePackages::basePlot(const std::string& channel, YieldTypes eY, const CvCity* pCity, CvCascadeEvalCtx ec)
{
	const CvPlayer& player = *ec.player;
	const TeamTypes eTeam = player.getTeam();
	const CvYieldInfo& yi = GC.getYieldInfo(eY);
	const int peakChange = yi.getPeakChange(), hillsChange = yi.getHillsChange();
	const int cityChange = yi.getCityChange(), popDivisor = yi.getPopulationChangeDivisor();
	const int gaYield = yi.getGoldenAgeYield(), gaThreshold = yi.getGoldenAgeYieldThreshold();
	const int minCity = yi.getMinCity();
	const int extraYield = GC.getDefineINT("EXTRA_YIELD");
	const bool bGolden = player.isGoldenAge();
	const int pop = pCity->getPopulation();
	const int extraThreshold = MMKernel::minPosThreshold("extraYieldThreshold", channel, player, ec);
	const int lessThreshold  = MMKernel::minPosThreshold("lessYieldThreshold", channel, player, ec);
	const int nB = GC.getNumBuildingInfos();

	int total = 0;
	for (int iI = 0; iI < NUM_CITY_PLOTS; ++iI)
	{
		const CvPlot* p = pCity->getCityIndexPlot(iI);
		if (p == NULL || !pCity->isWorkingPlot(p)) continue;
		ec.plot = p;
		const bool isCenter = (p == pCity->plot());

		// directImp = just the plot's improvement; buildingImp = + its upgrade-ancestors (a yield keyed to LUMBERMILL
		// also lands on a worked TREEFARM -- a BUILDING source walks the chain; civic/trait/substrate read direct only).
		std::vector<std::string> directImp, buildingImp;
		if (p->getImprovementType() != NO_IMPROVEMENT)
		{
			const std::string imp0 = GC.getImprovementInfo(p->getImprovementType()).getType();
			directImp.push_back(imp0);
			buildingImp.push_back(imp0);
			ImprovementTypes up = GC.getImprovementInfo(p->getImprovementType()).getImprovementUpgrade();
			int guard = 0;
			while (up != NO_IMPROVEMENT && guard++ < 32)
			{
				buildingImp.push_back(GC.getImprovementInfo(up).getType());
				up = GC.getImprovementInfo(up).getImprovementUpgrade();
			}
		}

		// nature = max(0, relief + terrain + feature + bonus own-plot yields), re-derived from the substrate JSON.
		const PlotTypes ePlot = p->getPlotType();
		const int plotTypeBase = (ePlot == PLOT_PEAK) ? peakChange : (ePlot == PLOT_HILLS) ? hillsChange : 0;
		int natureRaw = plotTypeBase;
		if (p->getTerrainType() != NO_TERRAIN) natureRaw += MMKernel::substratePlotYield(channel, InfoRepo<CvTerrainInfo>::get().get(p->getTerrainType()), p, eTeam, directImp, ec);
		if (p->getFeatureType() != NO_FEATURE) natureRaw += MMKernel::substratePlotYield(channel, InfoRepo<CvFeatureInfo>::get().get(p->getFeatureType()), p, eTeam, directImp, ec);
		if (p->getBonusType(eTeam) != NO_BONUS) natureRaw += MMKernel::substratePlotYield(channel, InfoRepo<CvBonusInfo>::get().get(p->getBonusType(eTeam)), p, eTeam, directImp, ec);
		const int nature = std::max(0, natureRaw);

		const int improvementYield = (p->getImprovementType() != NO_IMPROVEMENT)
			? MMKernel::substratePlotYield(channel, InfoRepo<CvImprovementInfo>::get().get(p->getImprovementType()), p, eTeam, directImp, ec) : 0;
		// route: own plot.flat (stays OUTSIDE the floor) + improvement-keyed (moves INSIDE the floor). Split per StoneBase.
		int routeYield = 0, routeImpKeyed = 0;
		if (p->getRouteType() != NO_ROUTE)
		{
			const CvJsonInfo* dr = InfoRepo<CvRouteInfo>::get().get(p->getRouteType());
			if (dr != NULL)
			{
				routeYield = MMKernel::substratePlotYield(channel, dr, p, eTeam, directImp, ec);
				routeImpKeyed = MMKernel::keyedImprovementOnly(channel, dr, "plot", directImp, ec, true);
			}
		}
		const int routeOwnFlat = routeYield - routeImpKeyed;

		// keyed building/civic/trait deposits, split by engine application stage (CITY-scope keyed lands in the running
		// pre-improvement yield; EMPIRE-scope rolls down; the improvement-keyed empire part moves inside the floor).
		int keyedCity = 0, keyedEmpire = 0, improvementKeyedEmpire = 0, plotsTarget = 0;
		for (int b = 0; b < nB; ++b)
		{
			const BuildingTypes eB = (BuildingTypes)b;
			const bool active = cascadeIsBuildingActive((int)eB, ec);
			const bool owned = player.getBuildingCount(eB) > 0;
			if (!active && !owned) continue;
			const CvJsonInfo* db = InfoRepo<CvBuildingInfo>::get().get(b);
			if (db == NULL) continue;
			if (active)
			{
				keyedCity   += MMKernel::keyedPlotYield(channel, db, "city", p, eTeam, buildingImp, ec, false);
				plotsTarget += MMKernel::plotsTargetYield(channel, db, "city", ec);
			}
			if (owned)
			{
				keyedEmpire            += MMKernel::keyedPlotYield(channel, db, "empire", p, eTeam, buildingImp, ec, false);
				improvementKeyedEmpire += MMKernel::keyedImprovementOnly(channel, db, "empire", buildingImp, ec, false);
				plotsTarget            += MMKernel::plotsTargetYield(channel, db, "empire", ec);
			}
		}
		for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
		{
			const CivicTypes c = player.getCivics((CivicOptionTypes)co);
			if (c == NO_CIVIC) continue;
			const CvJsonInfo* dc = InfoRepo<CvCivicInfo>::get().get((int)c);
			if (dc == NULL) continue;
			keyedEmpire            += MMKernel::keyedPlotYield(channel, dc, "empire", p, eTeam, directImp, ec, false);
			improvementKeyedEmpire += MMKernel::keyedImprovementOnly(channel, dc, "empire", directImp, ec, false);
			plotsTarget            += MMKernel::plotsTargetYield(channel, dc, "empire", ec);
		}
		for (int t = 0; t < GC.getNumTraitInfos(); ++t)
		{
			if (!player.hasTrait((TraitTypes)t)) continue;
			const CvJsonTraitInfo* dt = MMKernel::traitData(t);
			if (dt == NULL) continue;
			const int sgn = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) ? (dt->negativeTrait ? -1 : 1) : 0;   // PURE_TRAITS sign for this trait's keyed deposits
			keyedEmpire            += MMKernel::keyedPlotYield(channel, dt, "empire", p, eTeam, directImp, ec, false, sgn);
			improvementKeyedEmpire += MMKernel::keyedImprovementOnly(channel, dt, "empire", directImp, ec, false, sgn);
			plotsTarget            += MMKernel::plotsTargetYield(channel, dt, "empire", ec, sgn);
		}

		int centre = 0;
		if (isCenter)
		{
			centre += cityChange;
			if (popDivisor != 0) centre += pop / popDivisor;
		}

		const int running = nature + centre + plotsTarget + keyedCity;   // m_aExtraYield (event-granted) -> 0
		int threshold = 0;
		if (extraThreshold > 0 && running >= extraThreshold) threshold += extraYield;
		if (lessThreshold  > 0 && running >= lessThreshold)  threshold -= extraYield;
		const int goldenAge = (bGolden && (running + threshold) >= gaThreshold) ? gaYield : 0;

		const int improvementAddend = std::max(-nature, improvementYield + improvementKeyedEmpire + routeImpKeyed);
		const int keyedEmpireRest = keyedEmpire - improvementKeyedEmpire;
		int plotTotal = std::max(0, running + keyedEmpireRest + threshold + goldenAge + improvementAddend + routeOwnFlat);
		if (isCenter) plotTotal = std::max(plotTotal, minCity);
		total += plotTotal;
	}
	return total;
}

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

// BASE: golden-age yield/commerce (GoldenAgePackage) -- the trait goldenAge member ({ch}.empire.goldenAge.flat) on the
// active trait set while in a golden age, clamped at 0. x1. (Active set option-gated + PURE_TRAITS via sumTrait/traitData.)
int YieldBasePackages::goldenAge(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec)
{
	if (!player.isGoldenAge()) return 0;
	const std::string wantGA = channel + ".empire.goldenAge";
	int sum = 0;
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!player.hasTrait((TraitTypes)t)) continue;
		sum += MMKernel::sumTrait(MMKernel::traitData(t), wantGA, "flat", ec);
	}
	return std::max(0, sum);
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
		const CvJsonInfo* d = InfoRepo<CvSpecialistInfo>::get().get(s);
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
		const CvJsonTraitInfo* dt = MMKernel::traitData(t);
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
		const CvJsonInfo* db = InfoRepo<CvBuildingInfo>::get().get(b);
		if (db != NULL) perAll += MMKernel::sumUnit(db, wantPerAll, "perSpecialist", ec);
	}
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = player.getCivics((CivicOptionTypes)co);
		if (c == NO_CIVIC) continue;
		const CvJsonInfo* dc = InfoRepo<CvCivicInfo>::get().get((int)c);
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
