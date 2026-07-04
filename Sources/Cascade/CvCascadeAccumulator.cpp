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
#include "CvCascadeEnablerKernel.h"   // EnablerKernel::wireFacts / cityFacts -- the standing facts cache
#include "CvCascadeWellbeing.h"       // the §2b gather + the ONE verdict assembly
#include "CvCascadeScalarChannels.h"  // the scalar city halves + the player fill + the buildRate ledgers
#include "CvCascadeCityFacts.h"
#include "CvCascadeDepositIndex.h"    // the compiled segments -- the derived event masks + the ledger keys
#include "CvCascadeMMKernel.h"        // sumUnit (the per-item buildRate.self read)
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
	CascadeCityPackages& st = pCity->m_cascadeCityPackages;

	// ONE ctx serves every dirty package; the facts are the STANDING per-city cache.
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ec;
	ec.city = pCity; ec.plot = pCity->plot(); ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
	EnablerKernel::wireFacts(pCity, ec);

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
				const CascadeCityFacts& facts = EnablerKernel::cityFacts(pCity);
				for (std::set<int>::const_iterator it = facts.active.begin(); it != facts.active.end(); ++it)
					keyed += acc_mapGet(ps.cKeyedLedger[c], *it);
				st.cKeyed100[c] = keyed;
			}
		}
		if (iMask & CPK_CBASE) st.iCSrMatch = CommerceCalc::stateReligionMatch(pCity, ec);
	}
	if (iMask & CPK_WB)
	{
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
		if (iMask & CPK_SCFLAT)
		{
			st.scGpBaseBld = CascadeScalarChannels::gpBaseBuildings(pCity, ec);
			st.scTradeCity = CascadeScalarChannels::tradeRoutesCity(pCity, ec);
		}
		if (iMask & CPK_SCFLAT)
			st.scTradeCoastalCiv = CascadeScalarChannels::tradeRoutesCoastalCivCity(pCity, ec);
		if (iMask & CPK_SCPCT)
		{
			st.scGpModCity = CascadeScalarChannels::gpModifierCity(pCity, ec);
			st.scGpModSr = CascadeScalarChannels::gpModifierSrCity(pCity, ec);
			st.scDefense = CascadeScalarChannels::defenseAmount(pCity, ec);
			st.scMaintModCity = CascadeScalarChannels::maintenanceModifierCity(pCity, ec);
		}
	}
	if (iMask & CPK_SCSPEC)
	{
		++CascadePerf::scSpecRefresh;
		PerfAccumTimer perfSc(CascadePerf::scRefreshMs);
		st.scGpBaseSpec = CascadeScalarChannels::gpBaseSpecialists(pCity, ec);
	}
	if (iMask & CPK_BR)
		CascadeScalarChannels::fillBuildRateCity(pCity, ec, st.brCityKeyed, st.brCityMilitary, st.brCitySpace,
			st.brSrUnitProd, st.brSrBuildingProd);
	if (iMask & (CPK_FRONT_B | CPK_FRONT_U | CPK_FRONT_PP))
	{
		// the ENABLER frontier sets (#430 THE FLIP): the harness-proven calls VERBATIM at the city ctx --
		// SPLIT per domain (the perf surgery): a canConstruct read pays ONLY the BuildingCascade walk, etc.
		const CvTeam& kTeam = GET_TEAM(player.getTeam());
		if (iMask & CPK_FRONT_B)
		{
			st.enBuildable.clear();
			BuildingCascade::buildable(pCity, player, kTeam, st.enBuildable);
		}
		if (iMask & CPK_FRONT_U)
		{
			st.enTrainable.clear();
			UnitCascade::trainable(pCity, player, kTeam, st.enTrainable);
		}
		if (iMask & CPK_FRONT_PP)
		{
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
	CascadePlayerScope& ps = pPlayer->m_cascadePlayerScope;

	// the player-ctx (capital) for the empire walks; a cityless player gets fully-zeroed packages
	CvCascadeEvalCtx ec;
	const CvCity* pCap = pPlayer->getCapitalCity();
	if (pCap == NULL) { int iLoop; pCap = pPlayer->firstCity(&iLoop); }
	const bool bHasCtx = pCap != NULL;
	if (bHasCtx)
	{
		ec.city = pCap; ec.plot = pCap->plot(); ec.player = pPlayer; ec.team = &GET_TEAM(pPlayer->getTeam());
		EnablerKernel::wireFacts(pCap, ec);
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
		CascadeWellbeing::playerAreaEmpire(*pPlayer, ps.wbAreaByFam, ps.wbEmpireByFam, ps.wbBuildingKeyedByFam);
	if (iMask & PSC_SC)
		CascadeScalarChannels::fillPlayerScalars(*pPlayer, ps);
	if (iMask & PSC_BR)
		CascadeScalarChannels::fillBuildRatePlayer(*pPlayer, ps);
	if (iMask & PSC_FRONT_P)
	{
		// the ENABLER player frontier (#430 THE FLIP): the harness-proven BARE player ctx (no city/plot --
		// parity was proven with exactly this shape, never the capital ctx above)
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
// ENSURE-ON-READ (the FACTS idiom, deliberately NOT the rates' bare fetch): gate reads are decision-time,
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
	if (j == NULL || j->requiresBuild == NULL) return true;
	CvCascadeEvalCtx pec;
	pec.plot = pPlot; pec.player = pPlayer; pec.team = &kTeam;
	CvCascadeEvalFlags bflags; bflags.strictStateReligionForBuild = true;
	return cascadeEvalCondition(j->requiresBuild, pec, bflags);
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
	const CvPlayer& kPlayer = GET_PLAYER(pUnit->getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	kPlayer.m_cascadePlayerScope.set.ensure(PSC_FRONT_PROMO);
	const CascadePlayerScope& ps = kPlayer.m_cascadePlayerScope;

	// the per-UNIT halves MEMO (the promotion-TREE pattern: one unit × ~600 candidate promos per render;
	// rebuilding the held-promo folds per candidate was the measured UI sluggishness, owner 2026-07-04).
	// Signature = owner+id + held-set checksum + the unitcombat, refreshed by one cheap bool sweep; the
	// accumHave folds run ONLY on signature change. Game-thread statics (the s_enablerRooted precedent).
	static int s_uidOwner = -1, s_uidId = -1; static long s_sig = 0x7fffffff;
	static std::set<int> s_uCand, s_uRem;
	static std::map<int, bool> s_verdicts;   // the TURN-SCOPED per-(unit,promo) verdict cache -- a tree
	                                         // render after the first sweep is pure map hits (the UI
	                                         // re-sweeps candidates per frame; legacy recomputed each time)
	const int nPromo = GC.getNumPromotionInfos();
	long sig = 0;
	for (int pr = 0; pr < nPromo; ++pr)
		if (pUnit->isHasPromotion((PromotionTypes)pr)) sig += (pr + 1) * 3 + 1;
	const UnitCombatTypes eUC = pUnit->getUnitCombatType();
	sig = sig * 131 + (long)eUC * 7;
	sig = sig * 1009 + GC.getGame().getGameTurn();   // turn-scoped: any slow-changing bespoke input self-expires
	if ((int)pUnit->getOwner() != s_uidOwner || pUnit->getID() != s_uidId || sig != s_sig)
	{
		s_uidOwner = (int)pUnit->getOwner(); s_uidId = pUnit->getID(); s_sig = sig;
		s_verdicts.clear();
		EnBucketSets cand, rem;
		for (int pr = 0; pr < nPromo; ++pr)
			if (pUnit->isHasPromotion((PromotionTypes)pr)) EnablerKernel::accumHave(InfoRepo<CvPromotionInfo>::get().get(pr), cand, rem);
		if (eUC != NO_UNITCOMBAT) EnablerKernel::accumHave(InfoRepo<CvUnitCombatInfo>::get().get((int)eUC), cand, rem);
		s_uCand.clear(); s_uRem.clear();
		s_uCand.swap(cand["promotions"]);
		s_uRem.swap(rem["promotions"]);
	}
	{
		std::map<int, bool>::const_iterator vit = s_verdicts.find(ePromo);
		if (vit != s_verdicts.end()) return vit->second;
	}
	const std::set<int>& uCand = s_uCand;
	const std::set<int>& uRem = s_uRem;
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
	s_verdicts[ePromo] = bVerdict;
	return bVerdict;
}

// ---- buildRate: ledger lookups keyed by the head item (+ the item's own bonus-gated self mods, live) ----
// HOT-PATH RULE (generic code, STATIC storage): the AI's production planning calls these per (item × city)
// at ~5202×26 scale -- every segment id resolves ONCE into compiled per-type caches; the read path never
// touches a string or a string-map.

struct AccBrSegs
{
	int chan, self, percent, units, buildings, domains, unitCombats;
	AccBrSegs()
	{
		chan = DepositIndex::lookupSegment("buildRate");
		self = DepositIndex::lookupSegment("self");
		percent = DepositIndex::lookupSegment("percent");
		units = DepositIndex::lookupSegment("units");
		buildings = DepositIndex::lookupSegment("buildings");
		domains = DepositIndex::lookupSegment("domains");
		unitCombats = DepositIndex::lookupSegment("unitCombats");
	}
};
static const AccBrSegs& acc_brSegs()
{
	static AccBrSegs s;   // resolved once, after the index exists (first use is post-load)
	return s;
}

// The per-type key-segment caches: type id -> compiled segment id (-1 = never authored as a key).
static int acc_typeSeg(std::vector<int>& cache, int n, int i, const char* szType)
{
	if ((int)cache.size() != n) cache.assign(n, -2);
	if (i < 0 || i >= n) return -1;
	if (cache[i] == -2) cache[i] = DepositIndex::lookupSegment(szType);
	return cache[i];
}
static int acc_unitSeg(UnitTypes e)
{
	static std::vector<int> c;
	return acc_typeSeg(c, GC.getNumUnitInfos(), (int)e, e == NO_UNIT ? "" : GC.getUnitInfo(e).getType());
}
static int acc_buildingSeg(BuildingTypes e)
{
	static std::vector<int> c;
	return acc_typeSeg(c, GC.getNumBuildingInfos(), (int)e, e == NO_BUILDING ? "" : GC.getBuildingInfo(e).getType());
}
static int acc_domainSeg(DomainTypes e)
{
	static std::vector<int> c;
	return acc_typeSeg(c, NUM_DOMAIN_TYPES, (int)e, e == NO_DOMAIN ? "" : GC.getDomainInfo(e).getType());
}
static int acc_combatSeg(UnitCombatTypes e)
{
	static std::vector<int> c;
	return acc_typeSeg(c, GC.getNumUnitCombatInfos(), (int)e, e == NO_UNITCOMBAT ? "" : GC.getUnitCombatInfo(e).getType());
}

static int acc_brLookup(const CvCity* pCity, int memberSeg, int keySeg)
{
	if (memberSeg < 0 || keySeg < 0) return 0;
	const long key = ((long)memberSeg << 20) | keySeg;
	const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
	const CascadePlayerScope& ps = GET_PLAYER(pCity->getOwner()).m_cascadePlayerScope;
	int v = 0;
	std::map<long, int>::const_iterator it = st.brCityKeyed.find(key);
	if (it != st.brCityKeyed.end()) v += it->second;
	it = ps.brEmpKeyed.find(key);
	if (it != ps.brEmpKeyed.end()) v += it->second;
	return v;
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
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.nSeg != 2 || dep.unitId != sg.percent || dep.seg[0] != sg.chan || dep.seg[1] != sg.self) continue;
		if (!bCtx)   // build the eval ctx lazily -- most items carry no self mods at all
		{
			const CvPlayer& player = GET_PLAYER(pCity->getOwner());
			ec.city = pCity; ec.plot = pCity->plot(); ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
			EnablerKernel::wireFacts(pCity, ec);
			bCtx = true;
		}
		if (MMKernel::applies(dep.enabled, dep.disabled, ec)) iSum += dep.value100 / 100;
	}
	return iSum;
}

int CascadeAccumulator::buildRateUnit(const CvCity* pCity, UnitTypes eUnit)
{
	if (pCity == NULL || eUnit == NO_UNIT) return 0;
	const CvUnitInfo& unit = GC.getUnitInfo(eUnit);
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const CascadePlayerScope& ps = owner.m_cascadePlayerScope;
	const AccBrSegs& sg = acc_brSegs();
	int iMod = acc_brSelf(InfoRepo<CvUnitInfo>::get().get((int)eUnit), pCity)
	         + acc_brLookup(pCity, sg.units, acc_unitSeg(eUnit));
	if (!unit.isNoNonTypeProdMods())
	{
		iMod += acc_brLookup(pCity, sg.domains, acc_domainSeg(unit.getDomainType()));
		if (unit.getUnitCombatType() != NO_UNITCOMBAT)   // subs count only with a main combat
		{
			iMod += acc_brLookup(pCity, sg.unitCombats, acc_combatSeg((UnitCombatTypes)unit.getUnitCombatType()));
			foreach_(const UnitCombatTypes eSub, unit.getSubCombatTypes())
				iMod += acc_brLookup(pCity, sg.unitCombats, acc_combatSeg(eSub));
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
	         + acc_brLookup(pCity, acc_brSegs().buildings, acc_buildingSeg(eBuilding));
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
	pCity->m_cascadeCityPackages.set.markDirty(CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC));
	pCity->m_cascadeFacts.set.markAllDirty();

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
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
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
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	kPlayer.m_cascadePlayerScope.set.markAllDirty();
	int iLoop;
	for (const CvCity* pc = kPlayer.firstCity(&iLoop); pc != NULL; pc = kPlayer.nextCity(&iLoop))
	{
		// conditions on city-scope deposits reference techs/civics/GA -- the fan-out is the event's real
		// footprint (spec conditions), so the city packages + facts re-check
		pc->m_cascadeCityPackages.set.markAllDirty();
		pc->m_cascadeFacts.set.markAllDirty();
	}
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
		pc->m_cascadeFacts.set.markAllDirty();
		pc->m_cascadeFacts.set.ensure();      // facts first: the package fills read them
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
	pCity->m_cascadeFacts.set.ensure();      // facts first: the package fills read them
	pCity->m_cascadeCityPackages.set.ensure(CPK_EAGER);
}
