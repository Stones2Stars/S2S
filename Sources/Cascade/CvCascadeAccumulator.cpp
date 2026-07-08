//
//	CascadeAccumulator -- the #430 modifier machine over the SCOPE PACKAGES (see the header +
//	docs/plans/structural-cleanup/scope-packages.md). Fills ride the single-source calculator component
//	functions (the substrate law); combines are bare fetches + the channel's formula with live gates;
//	freshness is event marks + the slice/load boundaries. No epochs, no stamps, no read-side ensure.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeAccumulator.h"
#include "CvCascadePerfCount.h"       // per-turn call counters (the [MODIFIER/perf] census)
#include "AI/BetterBTSAI.h"           // PerfAccumTimer -- the refresh stopwatches
#include "CvCascadeYieldBasePackages.h"
#include "CvCascadeBuildingPackage.h"
#include "CvCascadePercentStack.h"
#include "CvCascadeCommerceCalc.h"    // baseOwn100 + the CombineSplit kernel + the pools/ledgers
#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx
#include "CvCascadeEnablerKernel.h"   // EnablerKernel::wireOperatingBuildings / operatingBuildings -- the standing operating buildings cache
#include "CvCascadeWellbeing.h"       // the §2b gather + the ONE verdict assembly
#include "CvCascadeScalarChannels.h"  // the scalar city halves + the player fill + the buildRate ledgers
#include "CvCascadeOperatingBuildings.h"
#include "CvCascadeDepositIndex.h"    // the compiled segments -- the derived event masks + the ledger keys
#include "CvCascadeMMKernel.h"        // MMKernel::applies -- the deposit condition gate (acc_brSelf's buildRate.self read)
#include "CvJsonInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"              // GC.getGame()
#include "Engine/CvCity.h"            // CITY_MAX_YIELD_RATE + m_cascadeCityPackages
#include "Engine/CvPlayer.h"          // m_cascadePlayerScope
#include "Engine/CvGame.h"            // m_cascadeWorldScope
#include "Engine/CvArea.h"
#include "Engine/CvMap.h"             // the coastal gate's ocean-min-size
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvProjectInfo.h"
#include "Infos/CvWorldInfo.h"
#include "Infos/CvTechInfo.h"          // the frontier fills (obsoletes.builds rem-set + promo tech halves)
#include "Infos/CvBuildInfo.h"         // enBuildUnlocked
#include "Infos/CvPromotionInfo.h"     // enPromotionValid
#include "Infos/CvUnitCombatInfo.h"    // enPromotionValid (the unitcombat HAVE leg)
#include "CvCascadeTechCascade.h"      // TechCascade::available -- the researchable frontier
#include "CvCascadeBuildingCascade.h"  // BuildingCascade::buildable -- the constructible frontier
#include "CvCascadeUnitCascade.h"      // UnitCascade::trainable -- the trainable frontier
#include "Engine/CvUnit.h"             // enPromotionValid (held promotions + isPromotionValidLegacy)
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"              // GET_TEAM
#include <string>
#include <set>

static const char* acc_channel(int y)
{
	static const char* a[NUM_YIELD_TYPES] = { "food", "production", "commerce" };
	return a[y];
}

// The per-unit promotion-availability memo entry (file scope: VC7.1 forbids local types as template
// arguments). One entry per unit, turn-scoped table -- see enPromotionValid.
struct AccEnPromoMemo
{
	long sig; bool built;
	std::set<int> uCand, uRem;
	std::map<int, bool> verdicts;
	AccEnPromoMemo() : sig(0), built(false) {}
};

// The map-read that never inserts (the packages are const at read).
static long acc_mapGet(const std::map<int, long>& m, int key)
{
	std::map<int, long>::const_iterator it = m.find(key);
	return it != m.end() ? it->second : 0;
}
static int acc_mapGetI(const std::map<int, int>& m, int key)
{
	std::map<int, int>::const_iterator it = m.find(key);
	return it != m.end() ? it->second : 0;
}

// ===================== the FILLS (the refresh delegate targets) =====================

void CascadeAccumulator::refreshCityPackages(const CvCity* pCity, int iMask)
{
	if (pCity == NULL || iMask == 0) return;
	++CascadePerf::accRefresh;
	CascadeCondScope ccsRates(CC_RATES);   // the condEval split default for this fill; the WB/scalar/frontier branches re-tag
	CascadeCityPackages& st = pCity->m_cascadeCityPackages;

	// ONE ctx serves every dirty package; the operating buildings are the STANDING per-city cache.
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ec;
	ec.city = pCity; ec.plot = pCity->plot(); ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
	EnablerKernel::wireOperatingBuildings(pCity, ec);

	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		const std::string ch = acc_channel(y);
		if (iMask & CPK_YPCT)   { MMBreak bk; st.yPctCity[y] = PercentStack::cityRealizedPercent(ch, pCity, bk); }
		if (iMask & CPK_YSPEC)  st.ySpec[y] = YieldBasePackages::specialist(ch, pCity, ec);
		if (iMask & CPK_YEXTRA) st.yExtra100[y] = BuildingPackage::buildingFlat(ch, pCity, ec);
	}
	if (iMask & (CPK_CSPEC | CPK_CPCT | CPK_CBASE))
	{
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		{
			const std::string ch = CommerceCalc::channel(c);
			if (iMask & CPK_CSPEC) st.cSpec100[c] = 100L * YieldBasePackages::specialist(ch, pCity, ec);
			if (iMask & CPK_CPCT)  { MMBreak bk; st.cPct[c] = PercentStack::cityRealizedPercent(ch, pCity, bk); }
			if (iMask & CPK_CBASE)
			{
				st.cBaseOwn100[c] = CommerceCalc::baseOwn100(ch, pCity, ec);
				// the buildingKeyed REALIZATION: the player grantor ledger × this city's active set. The
				// player package is PULLED fresh here (the downward roll reads upward-fresh state; the
				// grantor mask + the slice boundary keep it marked).
				const CascadePlayerScope& ps = player.m_cascadePlayerScope;
				player.m_cascadePlayerScope.set.ensure(PSC_CFLAT);
				long keyed = 0;
				const OperatingBuildings& operatingBuildings = EnablerKernel::operatingBuildings(pCity);
				for (std::set<int>::const_iterator it = operatingBuildings.active.begin(); it != operatingBuildings.active.end(); ++it)
					keyed += acc_mapGet(ps.cKeyedLedger[c], *it);
				st.cKeyed100[c] = keyed;
			}
		}
		if (iMask & CPK_CBASE) st.iCSrMatch = CommerceCalc::stateReligionMatch(pCity, ec);
	}
	if (iMask & CPK_WB)
	{
		CascadeCondScope ccsWb(CC_WB);
		CascadeWellbeing::gatherCityTerms(pCity, ec, st.wbHap, st.wbHea, st.aiWbCommercePer);
		// assemble the four verdicts AT FILL (the ruled end-turn cadence) -- reads are bare fetches. The
		// player fold maps are PULLED fresh (the upward chain).
		const CascadePlayerScope& ps = player.m_cascadePlayerScope;
		player.m_cascadePlayerScope.set.ensure(PSC_WB);
		// the building-KEYED realization (the Royal-Tomb class): entries pay where the keyed building is active
		CascadeWellbeing::foldBuildingKeyed(ps.wbBuildingKeyedByFam, ec, st.wbHap, st.wbHea);
		static int famHappy = -2, famHealth = -2;
		if (famHappy == -2) { famHappy = DepositIndex::lookupSegment("happiness"); famHealth = DepositIndex::lookupSegment("health"); }
		const int iArea = pCity->area()->getID();
		WbSplit hapArea, hapEmp, heaArea, heaEmp;
		std::map<int, std::map<int, WbSplit> >::const_iterator fit = ps.wbAreaByFam.find(famHappy);
		if (fit != ps.wbAreaByFam.end())
		{
			std::map<int, WbSplit>::const_iterator ait = fit->second.find(iArea);
			if (ait != fit->second.end()) hapArea = ait->second;
		}
		fit = ps.wbAreaByFam.find(famHealth);
		if (fit != ps.wbAreaByFam.end())
		{
			std::map<int, WbSplit>::const_iterator ait = fit->second.find(iArea);
			if (ait != fit->second.end()) heaArea = ait->second;
		}
		std::map<int, WbSplit>::const_iterator eit = ps.wbEmpireByFam.find(famHappy);
		if (eit != ps.wbEmpireByFam.end()) hapEmp = eit->second;
		eit = ps.wbEmpireByFam.find(famHealth);
		if (eit != ps.wbEmpireByFam.end()) heaEmp = eit->second;
		const CascadeWellbeingVerdicts v = CascadeWellbeing::assemble(pCity, st.wbHap, st.wbHea, st.aiWbCommercePer,
			hapArea, hapEmp, heaArea, heaEmp);
		st.aWbVerdict[0] = v.iHappy; st.aWbVerdict[1] = v.iUnhappy; st.aWbVerdict[2] = v.iGood; st.aWbVerdict[3] = v.iBad;
	}
	if (iMask & (CPK_SCFLAT | CPK_SCPCT))
	{
		++CascadePerf::scRefresh;
		PerfAccumTimer perfSc(CascadePerf::scRefreshMs);
		CascadeCondScope ccsSc(CC_SCALARS);
		if (iMask & CPK_SCFLAT)
		{
			st.scGpBaseBld = CascadeScalarChannels::gpBaseBuildings(pCity, ec);
			st.scTradeCity = CascadeScalarChannels::tradeRoutesCity(pCity, ec);
			// the freeSpecialists AMOUNT city half (the ruled seam; building events ride this bit)
			CascadeScalarChannels::fillFreeSpecialistsCity(pCity, ec, st.fsCityAny, st.fsCityByType);
		}
		if (iMask & CPK_SCFLAT)
			st.scTradeCoastalCiv = CascadeScalarChannels::tradeRoutesCoastalCivCity(pCity, ec);
		if (iMask & CPK_SCPCT)
		{
			st.scGpModCity = CascadeScalarChannels::gpModifierCity(pCity, ec);
			st.scGpModSr = CascadeScalarChannels::gpModifierSrCity(pCity, ec);
			st.scDefense = CascadeScalarChannels::defenseAmount(pCity, ec);
			st.scDefBombard = CascadeScalarChannels::defenseBombardCity(pCity, ec);   // L13
			st.scDefMin = CascadeScalarChannels::defenseMinCity(pCity, ec);           // L13
			st.scMaintModCity = CascadeScalarChannels::maintenanceModifierCity(pCity, ec);
		}
	}
	if (iMask & CPK_SCSPEC)
	{
		++CascadePerf::scSpecRefresh;
		PerfAccumTimer perfSc(CascadePerf::scRefreshMs);
		CascadeCondScope ccsSc(CC_SCALARS);
		st.scGpBaseSpec = CascadeScalarChannels::gpBaseSpecialists(pCity, ec);
	}
	if (iMask & CPK_BR)
	{
		CascadeCondScope ccsSc(CC_SCALARS);
		CascadeScalarChannels::fillBuildRateCity(pCity, ec, st.brCityKeyed, st.brCityMilitary, st.brCitySpace,
			st.brSrUnitProd, st.brSrBuildingProd, st.brCityWorldWonder, st.brCityTeamWonder, st.brCityNationalWonder);
	}
	if (iMask & (CPK_FRONT_B | CPK_FRONT_U | CPK_FRONT_PP))
	{
		// the ENABLER frontier sets (#430 THE FLIP): the harness-proven calls VERBATIM at the city ctx --
		// SPLIT per domain (the perf surgery): a canConstruct read pays ONLY the BuildingCascade walk, etc.
		const CvTeam& kTeam = GET_TEAM(player.getTeam());
		if (iMask & CPK_FRONT_B)
		{
			++CascadePerf::frontBFills;
			PerfAccumTimer perfFr(CascadePerf::frontBMs);
			CascadeCondScope ccsFr(CC_FRONT_B);
			st.enBuildable.clear();
			BuildingCascade::buildable(pCity, player, kTeam, st.enBuildable);
		}
		if (iMask & CPK_FRONT_U)
		{
			++CascadePerf::frontUFills;
			PerfAccumTimer perfFr(CascadePerf::frontUMs);
			CascadeCondScope ccsFr(CC_FRONT_U);
			st.enTrainable.clear();
			UnitCascade::trainable(pCity, player, kTeam, st.enTrainable);
		}
		if (iMask & CPK_FRONT_PP)
		{
			++CascadePerf::frontPPFills;
			PerfAccumTimer perfFr(CascadePerf::frontPPMs);
			CascadeCondScope ccsFr(CC_FRONT_PP);
			EnBucketSets candC;
			EnablerKernel::generate(player, pCity, candC);
			st.enCreatable.clear(); st.enMaintainable.clear();
			EnablerKernel::gateSet("projects",  candC, ec, player, kTeam, false, st.enCreatable);
			EnablerKernel::gateSet("processes", candC, ec, player, kTeam, false, st.enMaintainable);
		}
	}
}

void CascadeAccumulator::refreshPlayerScope(const CvPlayer* pPlayer, int iMask)
{
	if (pPlayer == NULL || iMask == 0) return;
	CascadeCondScope ccsRates(CC_RATES);   // the split default (YFLAT/CFLAT); the WB/scalar/frontier branches re-tag
	CascadePlayerScope& ps = pPlayer->m_cascadePlayerScope;

	// the player-ctx (capital) for the empire walks; a cityless player gets fully-zeroed packages
	CvCascadeEvalCtx ec;
	const CvCity* pCap = pPlayer->getCapitalCity();
	if (pCap == NULL) { int iLoop; pCap = pPlayer->firstCity(&iLoop); }
	const bool bHasCtx = pCap != NULL;
	if (bHasCtx)
	{
		ec.city = pCap; ec.plot = pCap->plot(); ec.player = pPlayer; ec.team = &GET_TEAM(pPlayer->getTeam());
		EnablerKernel::wireOperatingBuildings(pCap, ec);
	}

	if (iMask & PSC_YFLAT)
	{
		for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		{
			ps.yFlatFreeCity[y] = bHasCtx ? YieldBasePackages::freeCity(acc_channel(y), *pPlayer, ec) : 0;
			ps.yFlatGoldenAge[y] = bHasCtx ? YieldBasePackages::goldenAgeUngated(acc_channel(y), *pPlayer, ec) : 0;
		}
	}
	if (iMask & PSC_CFLAT)
	{
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		{
			const std::string ch = CommerceCalc::channel(c);
			ps.cPlayerExtra100[c] = bHasCtx ? CommerceCalc::playerExtra(ch, *pPlayer, ec) : 0;
			ps.cGoldenAge[c] = bHasCtx ? YieldBasePackages::goldenAgeUngated(ch, *pPlayer, ec) : 0;
			ps.cSrPool[c] = CommerceCalc::stateReligionPool(ch, *pPlayer);
			if (bHasCtx) CommerceCalc::buildingKeyedLedger(ch, *pPlayer, ec, ps.cKeyedLedger[c]);
			else ps.cKeyedLedger[c].clear();
		}
	}
	if (iMask & PSC_WB)
	{
		CascadeCondScope ccsWb(CC_WB);
		CascadeWellbeing::playerAreaEmpire(*pPlayer, ps.wbAreaByFam, ps.wbEmpireByFam, ps.wbBuildingKeyedByFam);
	}
	if (iMask & PSC_SC)
	{
		CascadeCondScope ccsSc(CC_SCALARS);
		CascadeScalarChannels::fillPlayerScalars(*pPlayer, ps);
	}
	if (iMask & PSC_BR)
	{
		CascadeCondScope ccsSc(CC_SCALARS);
		CascadeScalarChannels::fillBuildRatePlayer(*pPlayer, ps);
	}
	if (iMask & PSC_FRONT_P)
	{
		// the ENABLER player frontier (#430 THE FLIP): the harness-proven BARE player ctx (no city/plot --
		// parity was proven with exactly this shape, never the capital ctx above)
		++CascadePerf::frontPFills;
		PerfAccumTimer perfFr(CascadePerf::frontPMs);
		CascadeCondScope ccsFr(CC_FRONT_P);
		const CvTeam& kTeam = GET_TEAM(pPlayer->getTeam());
		CvCascadeEvalCtx fec;
		fec.player = pPlayer; fec.team = &kTeam;
		EnBucketSets candP;
		EnablerKernel::generate(*pPlayer, NULL, candP);
		ps.enResearchable.clear(); ps.enCivicsOk.clear(); ps.enHurryOk.clear();
		TechCascade::available(*pPlayer, kTeam, ps.enResearchable);
		EnablerKernel::gateSet("civics",  candP, fec, *pPlayer, kTeam, false, ps.enCivicsOk);
		EnablerKernel::gateSet("hurries", candP, fec, *pPlayer, kTeam, false, ps.enHurryOk);
		// the canBuild UNLOCK rem-set (the harness's remBld: obsoletes.builds over held techs)
		ps.enBuildRem.clear();
		for (int t = 0; t < GC.getNumTechInfos(); ++t)
			if (kTeam.isHasTech((TechTypes)t))
				EnablerKernel::addEdge(InfoRepo<CvTechInfo>::get().get(t), "obsoletes.builds", ps.enBuildRem);
	}
	if (iMask & PSC_FRONT_PROMO)
	{
		// the promotion frontier's PLAYER-WIDE tech halves (a 700-tech accumHave walk -- its OWN bit so
		// only promotion picks ever pay it; the per-unit composite folds these + the unit's own halves)
		++CascadePerf::promoFills;
		PerfAccumTimer perfFr(CascadePerf::promoMs);
		CascadeCondScope ccsFr(CC_PROMO);
		const CvTeam& kTeam = GET_TEAM(pPlayer->getTeam());
		ps.enPromoTechCand.clear(); ps.enPromoTechRem.clear();
		EnBucketSets pc, pr;
		for (int t = 0; t < GC.getNumTechInfos(); ++t)
			if (kTeam.isHasTech((TechTypes)t)) EnablerKernel::accumHave(InfoRepo<CvTechInfo>::get().get(t), pc, pr);
		ps.enPromoTechCand.swap(pc["promotions"]);
		ps.enPromoTechRem.swap(pr["promotions"]);
	}
}

void CascadeAccumulator::refreshWorldScope(const CvGame* pGame, int iMask)
{
	if (pGame == NULL || iMask == 0) return;
	CascadeWorldScope& ws = pGame->m_cascadeWorldScope;
	ws.tradeWorldFlat = 0;
	for (int p = 0; p < MAX_PC_PLAYERS; ++p)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)p);
		if (!kP.isAlive()) continue;
		kP.m_cascadePlayerScope.set.ensure(PSC_SC);   // the upward chain: world sums fresh player packages
		ws.tradeWorldFlat += kP.m_cascadePlayerScope.tradeWorldMine;
	}
	// the PROJECT world grants (the Internet class) -- raw team counts × compiled deposits
	ws.tradeWorldFlat += CascadeScalarChannels::tradeRoutesWorldProjects();
}

// ===================== the COMBINES (bare fetches + the channel formula + live gates) =====================

// The §2a rate combine: (plots + trade + BASE flats) × max(0, 100 + Σ percent packages) + truncated EXTRA.
// The plot base is PULLED live from the CvPlot caches; trade is the one live-yield input; the GA gate is live.
static long acc_yieldCombine(const CvCity* pCity, YieldTypes eY)
{
	const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const CascadePlayerScope& ps = owner.m_cascadePlayerScope;
	const int plots = pCity->getPlotYield(eY);
	const int trade = YieldBasePackages::tradeRoute(eY, pCity);
	const long ga = owner.isGoldenAge() ? std::max(0L, ps.yFlatGoldenAge[eY]) : 0;
	long pct = 100 + st.yPctCity[eY];   // the WHOLE stack is city-realized (the Burdigala class)
	if (pct < 0) pct = 0;
	long combine = (plots + trade + ps.yFlatFreeCity[eY] + ga + st.ySpec[eY]) * pct
	             + 100L * (st.yExtra100[eY] / 100);
	if (combine < 100) combine = 100;
	if (combine > CITY_MAX_YIELD_RATE) combine = CITY_MAX_YIELD_RATE;
	return combine;
}

long CascadeAccumulator::yieldRate100(const CvCity* pCity, YieldTypes eY)
{
	if (pCity == NULL || eY < 0 || eY >= NUM_YIELD_TYPES) return 0;
	return acc_yieldCombine(pCity, eY);
}

long CascadeAccumulator::commerceRate100(const CvCity* pCity, CommerceTypes eC)
{
	if (pCity == NULL || eC < 0 || eC >= NUM_COMMERCE_TYPES) return 0;
	const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const CascadePlayerScope& ps = owner.m_cascadePlayerScope;
	const long yc100 = acc_yieldCombine(pCity, YIELD_COMMERCE);
	const long prate = acc_yieldCombine(pCity, YIELD_PRODUCTION) / 100;
	const long ga = owner.isGoldenAge() ? std::max(0L, ps.cGoldenAge[eC]) : 0;
	const long baseExtra = st.cSpec100[eC] + st.cBaseOwn100[eC] + st.cKeyed100[eC]
	                     + ps.cPlayerExtra100[eC] + 100L * ga + 100L * ps.cSrPool[eC] * st.iCSrMatch;
	long pct = 100 + st.cPct[eC];   // the WHOLE stack is city-realized
	if (pct < 0) pct = 0;
	return CommerceCalc::combineSplit(eC, pCity, yc100, prate, baseExtra, (int)pct);
}

int CascadeAccumulator::wellbeing(const CvCity* pCity, int iVerdict)
{
	if (pCity == NULL || iVerdict < 0 || iVerdict > 3) return 0;
	const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
	if (iVerdict >= 2) return st.aWbVerdict[iVerdict];   // health has no military term -- pure bare fetch
	// the MILITARY term rides ALONE on top (DEC-unit-modifiers-on-top): perUnit × the LIVE O(1) counter.
	const int iMil = st.wbHap.iMilitary * pCity->getMilitaryHappinessUnits();
	return iVerdict == 0
		? std::max(0, st.aWbVerdict[0] + std::max(0, iMil))
		: std::max(0, st.aWbVerdict[1] - std::min(0, iMil));
}

int CascadeAccumulator::scGpBase(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	++CascadePerf::scGpBaseReads;
	const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
	return st.scGpBaseBld + st.scGpBaseSpec;
}

int CascadeAccumulator::scGpModifier(const CvCity* pCity)
{
	if (pCity == NULL) return 100;
	++CascadePerf::scGpModReads;
	const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const CascadePlayerScope& ps = owner.m_cascadePlayerScope;
	int iMod = 100 + st.scGpModCity + ps.gpModPlayer;
	const ReligionTypes eState = owner.getStateReligion();
	if (eState != NO_RELIGION && pCity->isHasReligion(eState)) iMod += st.scGpModSr;    // the SR gate, live
	if (owner.isGoldenAge()) iMod += GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER();          // config, live
	return std::max(0, iMod);
}

int CascadeAccumulator::scDefense(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	++CascadePerf::scDefReads;
	return pCity->m_cascadeCityPackages.scDefense;
}

// The freeSpecialists AMOUNTS (the ruled two-part seam, 2026-07-05): city + empire + this-area sums --
// the values that FEED the engine placement at the demolition; the /computed decomposition reads them meanwhile.
int CascadeAccumulator::fsAmountAny(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	owner.m_cascadePlayerScope.set.ensure(PSC_SC);
	const CascadePlayerScope& ps = owner.m_cascadePlayerScope;
	int iCount = pCity->m_cascadeCityPackages.fsCityAny + ps.fsEmpireAny;
	iCount += acc_mapGetI(ps.fsAreaAny, pCity->area()->getID());
	return iCount;
}
int CascadeAccumulator::fsAmountByType(const CvCity* pCity, int eSpecialist)
{
	if (pCity == NULL || eSpecialist < 0) return 0;
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	owner.m_cascadePlayerScope.set.ensure(PSC_SC);
	const std::vector<int>& c = pCity->m_cascadeCityPackages.fsCityByType;
	const std::vector<int>& p = owner.m_cascadePlayerScope.fsEmpireByType;
	return (eSpecialist < (int)c.size() ? c[eSpecialist] : 0) + (eSpecialist < (int)p.size() ? p[eSpecialist] : 0);
}

// The L13 defense wiring (2026-07-05): the wired members behind the flipped defense getters; the /computed
// decomposition reads them for attribution.
int CascadeAccumulator::scDefenseBombard(const CvCity* pCity)
{
	return pCity != NULL ? pCity->m_cascadeCityPackages.scDefBombard : 0;
}
int CascadeAccumulator::scDefenseMin(const CvCity* pCity)
{
	return pCity != NULL ? pCity->m_cascadeCityPackages.scDefMin : 0;
}
int CascadeAccumulator::scDefensePlayer(const CvPlayer* pPlayer)
{
	if (pPlayer == NULL) return 0;
	pPlayer->m_cascadePlayerScope.set.ensure(PSC_SC);
	return pPlayer->m_cascadePlayerScope.defPlayerAll;
}
// The COMPOSED bombard defense -- mirrors the getter getBuildingBombardDefense (CvCity.cpp:10114):
// min(MAX_BOMBARD_DEFENSE, Σbuilding bombard + national trait bombard). The additive parts are the cascade
// folds (scDefBombard city + defPlayerBombard player); the game-option CAP stays an engine-side composition
// at the getter (the capabilities game-option-fold precedent). This IS the flipped getter's body.
int CascadeAccumulator::scBuildingBombardDefense(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	const CvPlayer* pOwner = &GET_PLAYER(pCity->getOwner());
	pOwner->m_cascadePlayerScope.set.ensure(PSC_SC);
	const int iSum = pCity->m_cascadeCityPackages.scDefBombard + pOwner->m_cascadePlayerScope.defPlayerBombard;
	return std::min(GC.getGame().getModderGameOption(MODDERGAMEOPTION_MAX_BOMBARD_DEFENSE), iSum);
}

int CascadeAccumulator::scMaintenanceModifier(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	++CascadePerf::scMaintReads;
	const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
	const CascadePlayerScope& ps = GET_PLAYER(pCity->getOwner()).m_cascadePlayerScope;
	int iMod = st.scMaintModCity + ps.maintPlayerAll;
	const int iArea = pCity->area()->getID();
	iMod += acc_mapGetI(ps.maintAreaPct, iArea);
	iMod += ps.maintOtherAreaTotal - acc_mapGetI(ps.maintOtherAreaPct, iArea);
	if (pCity->isConnectedToCapital() && !pCity->isCapital()) iMod += ps.maintConnPct;  // the conn gate, live
	return iMod;
}

int CascadeAccumulator::scTradeRoutes(const CvCity* pCity)
{
	if (pCity == NULL) return 0;
	const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
	const CascadePlayerScope& ps = GET_PLAYER(pCity->getOwner()).m_cascadePlayerScope;
	int iCount = st.scTradeCity + ps.tradeEmpireAll + GC.getGame().m_cascadeWorldScope.tradeWorldFlat;
	if (pCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
		iCount += ps.tradeCoastalAll + st.scTradeCoastalCiv;                             // the coastal gate, live
	iCount += CascadeScalarChannels::tradeRouteLiveInputs(pCity);   // vote/WB store + INITIAL define, live
	return iCount;
}

// ===================== the ENABLER frontier reads (#430 THE FLIP, owner 2026-07-04 "flip it all") =====================
// ENSURE-ON-READ (the operating buildings idiom, deliberately NOT the rates' bare fetch): gate reads are decision-time,
// and legacy chains builds within a turn (complete A -> queue B the same slice), so a marked frontier
// rebuilds at the read -- a clean bit costs one branch. The sets fill via the harness-proven cascade calls.

bool CascadeAccumulator::enConstruct(const CvCity* pCity, int eBuilding)
{
	if (pCity == NULL || eBuilding < 0) return false;
	pCity->m_cascadeCityPackages.set.ensure(CPK_FRONT_B);
	return pCity->m_cascadeCityPackages.enBuildable.count(eBuilding) != 0;
}

bool CascadeAccumulator::enTrain(const CvCity* pCity, int eUnit)
{
	if (pCity == NULL || eUnit < 0) return false;
	pCity->m_cascadeCityPackages.set.ensure(CPK_FRONT_U);
	return pCity->m_cascadeCityPackages.enTrainable.count(eUnit) != 0;
}

bool CascadeAccumulator::enCreate(const CvCity* pCity, int eProject)
{
	if (pCity == NULL || eProject < 0) return false;
	pCity->m_cascadeCityPackages.set.ensure(CPK_FRONT_PP);
	return pCity->m_cascadeCityPackages.enCreatable.count(eProject) != 0;
}

bool CascadeAccumulator::enMaintain(const CvCity* pCity, int eProcess)
{
	if (pCity == NULL || eProcess < 0) return false;
	pCity->m_cascadeCityPackages.set.ensure(CPK_FRONT_PP);
	return pCity->m_cascadeCityPackages.enMaintainable.count(eProcess) != 0;
}

// The L6 fold's read: the derived trait national GP flat (replaces the m_iNationalGreatPeopleRate ride-in
// in the flipped getBaseGreatPeopleRate; the *Legacy bodies are demolition fodder, unread by the cascade).
// The max(0,·) clamp mirrors the legacy getter exactly.
int CascadeAccumulator::scGpNational(const CvPlayer* pPlayer)
{
	if (pPlayer == NULL) return 0;
	pPlayer->m_cascadePlayerScope.set.ensure(PSC_SC);
	return std::max(0, pPlayer->m_cascadePlayerScope.gpNationalFlat);
}

bool CascadeAccumulator::enResearch(const CvPlayer* pPlayer, int eTech)
{
	if (pPlayer == NULL || eTech < 0) return false;
	pPlayer->m_cascadePlayerScope.set.ensure(PSC_FRONT_P);
	return pPlayer->m_cascadePlayerScope.enResearchable.count(eTech) != 0;
}

bool CascadeAccumulator::enCivic(const CvPlayer* pPlayer, int eCivic)
{
	if (pPlayer == NULL || eCivic < 0) return false;
	pPlayer->m_cascadePlayerScope.set.ensure(PSC_FRONT_P);
	return pPlayer->m_cascadePlayerScope.enCivicsOk.count(eCivic) != 0;
}

bool CascadeAccumulator::enHurry(const CvPlayer* pPlayer, int eHurry)
{
	if (pPlayer == NULL || eHurry < 0) return false;
	pPlayer->m_cascadePlayerScope.set.ensure(PSC_FRONT_P);
	return pPlayer->m_cascadePlayerScope.enHurryOk.count(eHurry) != 0;
}

bool CascadeAccumulator::enFoundReligion(const CvPlayer* pPlayer)
{
	if (pPlayer == NULL) return false;
	return EnablerKernel::canFoundReligion(*pPlayer);   // a cheap player-state predicate -- live, no cache
}

// The canBuild UNLOCK half only (the harness's cascade side verbatim): the rem-set + target-side
// obsolescence + requires.build vs the PLOT ctx (strict state religion). The plot-validity half
// (canHaveImprovement / feature-removal / route / gold / feature-terrain tech gates) STAYS ENGINE.
bool CascadeAccumulator::enBuildUnlocked(const CvPlayer* pPlayer, int eBuild, const CvPlot* pPlot)
{
	if (pPlayer == NULL || eBuild < 0 || pPlot == NULL) return false;
	pPlayer->m_cascadePlayerScope.set.ensure(PSC_FRONT_P);
	const CascadePlayerScope& ps = pPlayer->m_cascadePlayerScope;
	if (ps.enBuildRem.count(eBuild) != 0) return false;
	const CvTeam& kTeam = GET_TEAM(pPlayer->getTeam());
	const CvJsonInfo* j = InfoRepo<CvBuildInfo>::get().get(eBuild);
	if (EnablerKernel::obsoletedByHeldTech(j, kTeam)) return false;
	if (j == NULL || j->requiresBuild() == NULL) return true;
	CascadeCondScope ccs(CC_CANBUILD);   // per-(build,plot) worker-AI reads -- their own census bucket
	CvCascadeEvalCtx pec;
	pec.plot = pPlot; pec.player = pPlayer; pec.team = &kTeam;
	CvCascadeEvalFlags bflags; bflags.strictStateReligionForBuild = true;
	return cascadeEvalCondition(j->requiresBuild(), pec, bflags);
}

bool CascadeAccumulator::enBuildUnlockedFast(const CvPlayer* pPlayer, int eBuild)
{
	if (pPlayer == NULL || eBuild < 0) return false;
	pPlayer->m_cascadePlayerScope.set.ensure(PSC_FRONT_P);
	if (pPlayer->m_cascadePlayerScope.enBuildRem.count(eBuild) != 0) return false;
	const CvTeam& kTeam = GET_TEAM(pPlayer->getTeam());
	if (EnablerKernel::obsoletedByHeldTech(InfoRepo<CvBuildInfo>::get().get(eBuild), kTeam)) return false;
	const CvBuildInfo& kBuild = GC.getBuildInfo((BuildTypes)eBuild);
	if (kBuild.isDisabled()) return false;
	const TechTypes eTechPrereq = (TechTypes)kBuild.getTechPrereq();
	return eTechPrereq == NO_TECH || kTeam.isHasTech(eTechPrereq);
}

// The promotion COMPOSITE (the harness's cascade side verbatim, single source -- the harness now consumes
// this): the frontier half (tech halves cached player-wide + the unit's held promos + its unitcombat) +
// the event-injection-only mirror + requires, over the bespoke legacy half (isPromotionValidLegacy(...,
// bFree=true) -- exactly the tech-gate-skipping ride parity was proven with).
bool CascadeAccumulator::enPromotionValid(const CvUnit* pUnit, int ePromo)
{
	if (pUnit == NULL || ePromo < 0) return false;
	CascadeCondScope ccs(CC_PROMO);   // the composite's own evals (the frontier-half fill re-tags itself)
	const CvPlayer& kPlayer = GET_PLAYER(pUnit->getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	kPlayer.m_cascadePlayerScope.set.ensure(PSC_FRONT_PROMO);
	const CascadePlayerScope& ps = kPlayer.m_cascadePlayerScope;

	// the per-UNIT halves MEMO (the promotion-TREE pattern: one unit × ~600 candidate promos per render;
	// rebuilding the held-promo folds per candidate was the measured UI sluggishness, owner 2026-07-04).
	// Signature = owner+id + held-set checksum + the unitcombat, refreshed by one cheap bool sweep; the
	// accumHave folds run ONLY on signature change. Game-thread statics (the s_enablerRooted precedent).
	// The per-UNIT availability memo, MULTI-SLOT (owner scope ruling 2026-07-04: "the scope of the cascade
	// ends when we have determined what promotions are available; the rest is existing infrastructure").
	// Multi-slot because the pick-refresh sweeps the SELECTION GROUP with units alternating under each
	// promotion action -- a single-slot memo THRASHED (stack × promos rebuilds = the measured 0.2s pick
	// hitch). One entry per unit, turn-scoped (the whole table clears on turn change); a group sweep pays
	// one fold-rebuild per unit total, everything after is map hits.
	static std::map<long, AccEnPromoMemo> s_memo;
	static int s_memoTurn = -1;
	const int iTurnNow = GC.getGame().getGameTurn();
	if (iTurnNow != s_memoTurn) { s_memoTurn = iTurnNow; s_memo.clear(); }

	const int nPromo = GC.getNumPromotionInfos();
	long sig = 0;
	for (int pr = 0; pr < nPromo; ++pr)
		if (pUnit->isHasPromotion((PromotionTypes)pr)) sig += (pr + 1) * 3 + 1;
	const UnitCombatTypes eUC = pUnit->getUnitCombatType();
	sig = sig * 131 + (long)eUC * 7;

	const long lKey = ((long)pUnit->getOwner() << 24) ^ (long)pUnit->getID();
	AccEnPromoMemo& memo = s_memo[lKey];
	if (!memo.built || memo.sig != sig)
	{
		memo.built = true;
		memo.sig = sig;
		memo.verdicts.clear();
		EnBucketSets cand, rem;
		for (int pr = 0; pr < nPromo; ++pr)
			if (pUnit->isHasPromotion((PromotionTypes)pr)) EnablerKernel::accumHave(InfoRepo<CvPromotionInfo>::get().get(pr), cand, rem);
		if (eUC != NO_UNITCOMBAT) EnablerKernel::accumHave(InfoRepo<CvUnitCombatInfo>::get().get((int)eUC), cand, rem);
		memo.uCand.clear(); memo.uRem.clear();
		memo.uCand.swap(cand["promotions"]);
		memo.uRem.swap(rem["promotions"]);
	}
	{
		std::map<int, bool>::const_iterator vit = memo.verdicts.find(ePromo);
		if (vit != memo.verdicts.end()) return vit->second;
	}
	const std::set<int>& uCand = memo.uCand;
	const std::set<int>& uRem = memo.uRem;
	// a promo rooted in NO tech edge anywhere is ALWAYS-unlocked (the PALACE lesson for promotions)
	static std::set<int> s_enablerRooted;
	static bool s_rootedBuilt = false;
	if (!s_rootedBuilt)
	{
		for (int t = 0; t < GC.getNumTechInfos(); ++t)
			EnablerKernel::addEdge(InfoRepo<CvTechInfo>::get().get(t), "enables.promotions", s_enablerRooted);
		s_rootedBuilt = true;
	}
	// the original algebra, copy-free: promoCand = (techCand + unitCand) - (techRem + unitRem);
	// bUnlocked = inCand&&!inRem || !rooted&&!inRem  ==  !inRem && (inCand || !rooted)
	const bool bInRem = ps.enPromoTechRem.count(ePromo) != 0 || uRem.count(ePromo) != 0;
	const bool bInCand = ps.enPromoTechCand.count(ePromo) != 0 || uCand.count(ePromo) != 0;
	const bool bUnlocked = !bInRem && (bInCand || s_enablerRooted.count(ePromo) == 0);
	// the event-injection-only mirror (no qualified-unitcombat list => legacy refuses unless FREE)
	const CvPromotionInfo& kPromo = GC.getPromotionInfo((PromotionTypes)ePromo);
	const bool bEventOnly = kPromo.getNumQualifiedUnitCombatTypes() == 0
		&& !kPromo.isForOffset() && !kPromo.isZeroesXP()
		&& !pUnit->getUnitInfo().getFreePromotions(ePromo)
		&& !kPlayer.isFreePromotion(pUnit->getUnitType(), (PromotionTypes)ePromo);

	CvCascadeEvalCtx ec;
	ec.unit = pUnit; ec.player = &kPlayer; ec.team = &kTeam; ec.plot = pUnit->plot();
	const bool bVerdict = bUnlocked && !bEventOnly
		&& EnablerKernel::requiresMet(InfoRepo<CvPromotionInfo>::get().get(ePromo), ec)
		&& pUnit->isPromotionValidLegacy((PromotionTypes)ePromo, true);
	memo.verdicts[ePromo] = bVerdict;
	return bVerdict;
}

// ---- buildRate: ledger lookups keyed by the head item (+ the item's own bonus-gated self mods, live) ----
// HOT-PATH RULE (generic code, STATIC storage): the AI's production planning calls these per (item × city)
// at ~5202×26 scale -- every segment id resolves ONCE into compiled per-type caches; the read path never
// touches a string or a string-map.

struct AccBrSegs
{
	int chan, self, percent;
	AccBrSegs()
	{
		chan = DepositIndex::lookupSegment("buildRate");
		self = DepositIndex::lookupSegment("self");
		percent = DepositIndex::lookupSegment("percent");
	}
};
static const AccBrSegs& acc_brSegs()
{
	static AccBrSegs s;   // resolved once, after the index exists (first use is post-load)
	return s;
}

// The DENSE ledger read (the precipice-§5 fix): one array index per scope by the GAME ENUM id --
// no tree lookup and no enum->segment conversion (the former per-type key-segment caches deleted).
static int acc_brLookup(const CvCity* pCity, const std::vector<int> CascadeBrLedger::*mKind, int iId)
{
	if (iId < 0) return 0;
	return CascadeBrLedger::at(pCity->m_cascadeCityPackages.brCityKeyed.*mKind, iId)
	     + CascadeBrLedger::at(GET_PLAYER(pCity->getOwner()).m_cascadePlayerScope.brEmpKeyed.*mKind, iId);
}

static int acc_brSelf(const CvJsonInfo* d, const CvCity* pCity)
{
	if (d == NULL) return 0;
	// compiled int matching (buildRate.self.percent) -- no strings, no string-map, on the AI-planning hot path
	const AccBrSegs& sg = acc_brSegs();
	if (sg.chan < 0 || sg.self < 0 || sg.percent < 0) return 0;
	int iSum = 0;
	bool bCtx = false;
	CvCascadeEvalCtx ec;
	const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(d);
	for (size_t i = 0; i < deps.size(); ++i)
	{
		const CascadeDeposit& dep = deps[i];
		if (dep.nSeg != 2 || dep.unitId != sg.percent || dep.seg[0] != sg.chan || dep.seg[1] != sg.self) continue;
		if (!bCtx)   // build the eval ctx lazily -- most items carry no self mods at all
		{
			const CvPlayer& player = GET_PLAYER(pCity->getOwner());
			ec.city = pCity; ec.plot = pCity->plot(); ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
			EnablerKernel::wireOperatingBuildings(pCity, ec);
			bCtx = true;
		}
		if (MMKernel::applies(dep.enabled, dep.disabled, ec))
			iSum += (int)(MMKernel::perScale(dep, ec, dep.value100) / 100);   // §3.7 per (identity when hasPer==false)
	}
	return iSum;
}

int CascadeAccumulator::buildRateUnit(const CvCity* pCity, UnitTypes eUnit)
{
	if (pCity == NULL || eUnit == NO_UNIT) return 0;
	const CvUnitInfo& unit = GC.getUnitInfo(eUnit);
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const CascadePlayerScope& ps = owner.m_cascadePlayerScope;
	int iMod = acc_brSelf(InfoRepo<CvUnitInfo>::get().get((int)eUnit), pCity)
	         + acc_brLookup(pCity, &CascadeBrLedger::units, (int)eUnit);
	if (!unit.isNoNonTypeProdMods())
	{
		iMod += acc_brLookup(pCity, &CascadeBrLedger::domains, (int)unit.getDomainType());
		if (unit.getUnitCombatType() != NO_UNITCOMBAT)   // subs count only with a main combat
		{
			iMod += acc_brLookup(pCity, &CascadeBrLedger::unitCombats, unit.getUnitCombatType());
			foreach_(const UnitCombatTypes eSub, unit.getSubCombatTypes())
				iMod += acc_brLookup(pCity, &CascadeBrLedger::unitCombats, (int)eSub);
		}
		if (unit.isMilitaryProduction())
			iMod += pCity->m_cascadeCityPackages.brCityMilitary + ps.brEmpMilitary;
		const ReligionTypes eState = owner.getStateReligion();
		if (eState != NO_RELIGION && pCity->isHasReligion(eState))
			iMod += pCity->m_cascadeCityPackages.brSrUnitProd;   // the SR gate, live (city-realized field)
	}
	return iMod;
}

int CascadeAccumulator::buildRateBuilding(const CvCity* pCity, BuildingTypes eBuilding)
{
	if (pCity == NULL || eBuilding == NO_BUILDING) return 0;
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	int iMod = acc_brSelf(InfoRepo<CvBuildingInfo>::get().get((int)eBuilding), pCity)
	         + acc_brLookup(pCity, &CascadeBrLedger::buildings, (int)eBuilding);
	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);
	// the L11 folds (2026-07-05): the trait specialBuilding keyed leg + the wonder-category members
	// (the legacy CvPlayer::getProductionModifier(Building) trait walks + max* accumulators)
	iMod += acc_brLookup(pCity, &CascadeBrLedger::specialBuildings, (int)kBuilding.getSpecialBuilding());
	if (::isWorldWonder(eBuilding))    iMod += pCity->m_cascadeCityPackages.brCityWorldWonder;
	if (::isTeamWonder(eBuilding))     iMod += pCity->m_cascadeCityPackages.brCityTeamWonder;
	if (::isNationalWonder(eBuilding)) iMod += pCity->m_cascadeCityPackages.brCityNationalWonder;
	const ReligionTypes eState = owner.getStateReligion();
	if (eState != NO_RELIGION && pCity->isHasReligion(eState))
		iMod += pCity->m_cascadeCityPackages.brSrBuildingProd;   // the SR gate, live (city-realized field)
	return iMod;
}

int CascadeAccumulator::buildRateProject(const CvCity* pCity, ProjectTypes eProject)
{
	if (pCity == NULL || eProject == NO_PROJECT) return 0;
	int iMod = acc_brSelf(InfoRepo<CvProjectInfo>::get().get((int)eProject), pCity);
	if (GC.getProjectInfo(eProject).isSpaceship())
		iMod += pCity->m_cascadeCityPackages.brCitySpace + GET_PLAYER(pCity->getOwner()).m_cascadePlayerScope.brEmpSpace;
	return iMod;
}

// ===================== the EVENT MARKS =====================

void CascadeAccumulator::dirtyCity(const CvCity* pCity, int iMask)
{
	if (pCity == NULL) return;
	pCity->m_cascadeCityPackages.set.markDirty(iMask);
}

// The building event: the CITY mask stays CONSERVATIVE-ALL (a new/lost building can flip OTHER buildings'
// active state through the operate/provides fixpoint, so this building's own deposits do NOT bound the
// city-side effect); the CROSS-SCOPE masks are DERIVED from the building's compiled deposits (percent-vs-flat
// × scope), so a city-only building never touches the player/world packages, and only a genuine grantor
// (empire buildings-keyed commerce) fans out to the sibling cities' CBASE realizations.
void CascadeAccumulator::buildingProcessed(const CvCity* pCity, BuildingTypes eBuilding)
{
	if (pCity == NULL) return;
	// #430 THE PER-TURN FRONTIER CACHE (owner 2026-07-05 + enabler-frontier-perf.md 2026-07-06): a building change
	// does NOT rebuild the buildable OR the trainable frontier -- CPK_FRONT_B **and CPK_FRONT_U** are EXCLUDED from
	// this mark. Instead both boxes are maintained INCREMENTALLY: onBuildingChanged (buildings whose requires
	// reference the changed one) + onBuildingChangedUnits (units the changed building's provides.bonuses / enables /
	// building-prereq reference). This kills the every-completion ~3340-unit re-walk that was 1.4M condEval/turn.
	// CPK_FRONT_PP (projects/processes) is KEPT broad: a completed building CAN enable a project (enables.projects
	// feeds the generate set), there is no per-project incremental primitive, and PP fills are cheap (~42/turn) --
	// so keeping it correct-by-rebuild is the safe floor (rule 3). The full frontier rebuild otherwise stays only
	// for CAN-GET GROWTH (mid-turn tech/civic via markPlayerScopeAndCities).
	pCity->m_cascadeCityPackages.set.markDirty(CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC | CPK_FRONT_B | CPK_FRONT_U));
	EnablerKernel::onBuildingChangedActive(pCity, (int)eBuilding);   // operating buildings: targeted ripple into the authoritative active set (was blanket markAllDirty)
	BuildingCascade::onBuildingChanged(pCity, (int)eBuilding);       // CPK_FRONT_B: targeted (reads the fresh operating buildings)
	UnitCascade::onBuildingChangedUnits(pCity, (int)eBuilding);      // CPK_FRONT_U: targeted (reads the fresh operating buildings)

	const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get((int)eBuilding);
	if (d == NULL) return;
	static int segArea = -2, segEmpire = -2, segWorld = -2, segPercent = -2, segBuildings = -2;
	if (segArea == -2)
	{
		segArea = DepositIndex::lookupSegment("area");
		segEmpire = DepositIndex::lookupSegment("empire");
		segWorld = DepositIndex::lookupSegment("world");
		segPercent = DepositIndex::lookupSegment("percent");
		segBuildings = DepositIndex::lookupSegment("buildings");
	}
	int iPlayerMask = 0, iSiblingMask = 0;
	bool bWorld = false;
	const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(d);
	for (size_t i = 0; i < deps.size(); ++i)
	{
		const CascadeDeposit& dep = deps[i];
		if (dep.seg[1] == segWorld) { bWorld = true; iPlayerMask |= PSC_SC; continue; }
		if (dep.seg[1] != segEmpire && dep.seg[1] != segArea) continue;
		if (dep.unitId == segPercent)
		{
			// empire/area PERCENTS enter every sibling city's CITY-REALIZED stacks (the owned-type walk)
			// + the player building sums (gp/maint)
			iPlayerMask |= PSC_SC | PSC_BR;
			iSiblingMask |= CPK_YPCT | CPK_CPCT | CPK_SCPCT | CPK_BR;
		}
		else
		{
			// empire/area FLATS feed the player building sums (trade) + the wb fold maps + the keyed ledgers
			iPlayerMask |= PSC_SC | PSC_CFLAT | PSC_WB;
			if (dep.nSeg == 4 && dep.seg[2] == segBuildings)
				iSiblingMask |= CPK_CBASE | CPK_WB;   // the guild-grant + Royal-Tomb classes: every city's keyed realization re-fills
		}
	}
	if (iPlayerMask != 0 || iSiblingMask != 0)
	{
		const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
		if (iPlayerMask != 0) owner.m_cascadePlayerScope.set.markDirty(iPlayerMask);
		if (iSiblingMask != 0)
		{
			int iLoop;
			for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
				pc->m_cascadeCityPackages.set.markDirty(iSiblingMask);
		}
	}
	if (bWorld) GC.getGame().m_cascadeWorldScope.set.markAllDirty();
}

void CascadeAccumulator::markPlayerScopeAndCities(PlayerTypes ePlayer)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS) return;
	cascadePolicyStateChanged((int)ePlayer);   // the §9 policy memo re-arms (civic/trait/tech fan-in; the grind fix)
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	kPlayer.m_cascadePlayerScope.set.markAllDirty();
	int iLoop;
	for (const CvCity* pc = kPlayer.firstCity(&iLoop); pc != NULL; pc = kPlayer.nextCity(&iLoop))
	{
		// conditions on city-scope deposits reference techs/civics/GA -- the fan-out is the event's real
		// footprint (spec conditions), so the city packages + operating buildings re-check
		pc->m_cascadeCityPackages.set.markAllDirty();
		EnablerKernel::onPlayerScopeChangedActive(pc);   // operating buildings: targeted ripple for tech/civic/GA-referencing operate (was blanket markAllDirty)
	}
}

// ===================== the TARGETED frontier updates (enabler-frontier-perf.md) =====================

// Part A: the load-end warm-up builds BOTH reverse indices (idempotent), so turn 1's targeted re-checks stand
// on a ready index instead of paying the lazy first-build.
void CascadeAccumulator::buildFrontierIndices()
{
	BuildingCascade::buildIndices();
	UnitCascade::buildIndices();
	EnablerKernel::buildActiveIndex();   // the operate reverse-index for the targeted active-set maintenance
}

// Part C: a city-local HAVE atom flipped -> re-check ONLY the frontier entities that reference it, for BOTH
// domains (buildable + trainable), in place. Each cascade skips its box if a full rebuild is already pending.
void CascadeAccumulator::cityHaveChanged(const CvCity* pCity, int eHaveKind)
{
	if (pCity == NULL) return;
	const CvPlayer& kPlayer = GET_PLAYER(pCity->getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	BuildingCascade::recheckHave(pCity, kPlayer, kTeam, eHaveKind);
	UnitCascade::recheckHave(pCity, kPlayer, kTeam, eHaveKind);
	EnablerKernel::onHaveChangedActive(pCity, eHaveKind);   // operating buildings: targeted ripple for the HAVE-referencing operate
}

// Part B: a unit's empire count changed -> the trainable re-check across the player's cities (empire-scoped caps).
void CascadeAccumulator::unitCountChanged(const CvPlayer& kPlayer, int eUnit)
{
	UnitCascade::onUnitChanged(kPlayer, eUnit);
}

// ===================== the BOUNDARIES =====================

void CascadeAccumulator::playerSliceRebuild(PlayerTypes ePlayer)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS) return;
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	if (!kPlayer.isAlive()) return;
	// The FULL per-player rebuild ("Cascade.RebuildCache(myPlayerId)"): the self-heal must cover ALL
	// packages -- an unhooked mutation (power flips, bonus-network shifts, timers) otherwise stales a
	// package FOREVER (the measured Burdigala class: packages frozen across turns while legacy moved; a
	// narrowed CBASE|WB self-heal was an over-fix -- the original 222s cost lived in the all-infos walks +
	// the string hot paths, both since fixed, NOT in the mark breadth).
	kPlayer.m_cascadePlayerScope.set.markAllDirty();
	kPlayer.m_cascadePlayerScope.set.ensure(PSC_EAGER);   // the frontier stays LAZY (ensure-on-read; the eager rebuild was the measured turn-grind)
	int iLoop;
	for (const CvCity* pc = kPlayer.firstCity(&iLoop); pc != NULL; pc = kPlayer.nextCity(&iLoop))
	{
		pc->m_operatingBuildings.set.ensure();      // SEED on first visit (dirty from reset/load); no-op after -- the package fills read operating buildings
		EnablerKernel::onSliceRebuildActive(pc);   // the bounded per-turn dynamic re-check (replaces the whole-city operating buildings recompute)
		pc->m_cascadeCityPackages.set.markAllDirty();
		pc->m_cascadeCityPackages.set.ensure(CPK_EAGER);   // ditto: a city with a standing build queue never pays a frontier walk
	}
}

void CascadeAccumulator::worldRebuild()
{
	GC.getGame().m_cascadeWorldScope.set.markAllDirty();
	GC.getGame().m_cascadeWorldScope.set.ensure();
}

void CascadeAccumulator::cityCreated(const CvCity* pCity)
{
	if (pCity == NULL) return;
	// Pre-final-init cities are warmed by the load-end warm-up (the flipped getters serve legacy until then).
	if (!GC.getGame().isFinalInitialized()) return;
	// Everything is dirty-from-reset (plus the founding buildings' marks); one eager ensure realizes the
	// city's packages so the founding/capture turn reads real values, not the zero-initialized defaults.
	// (The frontier stays lazy -- the first production-choice read fills it, still same-turn.)
	pCity->m_operatingBuildings.set.ensure();      // operating buildings first: the package fills read them
	pCity->m_cascadeCityPackages.set.ensure(CPK_EAGER);
}
