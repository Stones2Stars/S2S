//
//	CascadeAccumulator -- the #430 modifier machine over the SCOPE PACKAGES (see the header +
//	docs/plans/structural-cleanup/scope-packages.md). Fills ride the single-source calculator component
//	functions (the substrate law); combines are bare fetches + the channel's formula with live gates;
//	freshness is event marks + the slice/load boundaries. No epochs, no stamps, no read-side ensure.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeAccumulator.h"
#include "CvEventSpine.h"   // emitCacheInvalidate -- announce the warm-up / self-heal package marks (observability)
#include "CvCascadePerfCount.h"       // per-turn call counters (the [MODIFIER/perf] census)
#include "AI/BetterBTSAI.h"           // PerfAccumTimer -- the refresh stopwatches
#include "CvCascadeYieldBasePackages.h"
#include "CvCascadeBuildingPackage.h"
#include "CvCascadePercentStack.h"
#include "CvCascadeCommerceCalc.h"    // baseOwn100 + the CombineSplit kernel + the pools/ledgers
#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx
#include "CvEnablerKernel.h"   // EnablerKernel::wireOperatingBuildings / operatingBuildings -- the standing operating buildings cache
#include "CvCascadeWellbeing.h"       // the §2b gather + the ONE verdict assembly
#include "CvCascadeScalarChannels.h"  // the scalar city halves + the player fill + the buildRate ledgers
#include "CvCascadeOperatingBuildings.h"
#include "CvCascadeDepositIndex.h"    // the compiled segments -- the derived event masks + the ledger keys
#include "CvCascadeMMKernel.h"        // MMKernel::applies -- the deposit condition gate (acc_brSelf's buildRate.self read)
#include "CvInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"              // GC.getGame()
#include "Engine/CvCity.h"            // CITY_MAX_YIELD_RATE + m_cascadeCityPackages
#include "Engine/CvPlayer.h"          // m_cascadePlayerScope
#include "Engine/CvGame.h"            // m_cascadeWorldScope
#include "Engine/CvArea.h"
#include "Engine/CvMap.h"             // the coastal gate's ocean-min-size
#include "CvBuildingInfo.h"
#include "CvUnitInfo.h"
#include "CvProjectInfo.h"
#include "Infos/CvWorldInfo.h"
#include "CvTechInfo.h"          // the frontier fills (obsoletes.builds rem-set + promo tech halves)
#include "CvPromotionInfo.h"     // enPromotionValid
#include "CvUnitCombatInfo.h"    // enPromotionValid (the unitcombat HAVE leg)
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

static long acc_yieldCombine(const CvCity* pCity, YieldTypes eY);   // defined with the combines below; the CPK_YRATE fill stores it

void CascadeAccumulator::refreshCityPackages(const CvCity* pCity, int iMask)
{
	if (pCity == NULL || iMask == 0) return;
	emitCacheRebuilt(0, pCity->getOwner(), pCity->getID(), iMask);   // observability: this city's packages recomputed
	++CascadePerf::accRefresh;
	// (scope,channel) calc-count [DEC-calc-count-gate]: attribute this refresh's value-computes to (city, channel).
	// Mirrors EXACTLY the iMask-gated compute blocks below -- yields/commerce count per type (the loops), the
	// scalars per named channel. A quiet turn refreshes nothing -> zero; the blanket refreshes every bit for every
	// city -> it balloons and the histogram names the culprit.
	if (iMask & CPK_YPCT)     CascadePerf::calcN(CSCOPE_CITY, CCHAN_BASE_YIELDS, NUM_YIELD_TYPES);
	if (iMask & CPK_YSPEC)    CascadePerf::calcN(CSCOPE_CITY, CCHAN_BASE_YIELDS, NUM_YIELD_TYPES);
	if (iMask & CPK_YEXTRA)   CascadePerf::calcN(CSCOPE_CITY, CCHAN_BASE_YIELDS, NUM_YIELD_TYPES);
	if (iMask & CPK_CSPEC)    CascadePerf::calcN(CSCOPE_CITY, CCHAN_COMMERCE, NUM_COMMERCE_TYPES);
	if (iMask & CPK_CPCT)     CascadePerf::calcN(CSCOPE_CITY, CCHAN_COMMERCE, NUM_COMMERCE_TYPES);
	if (iMask & CPK_CBASE)    CascadePerf::calcN(CSCOPE_CITY, CCHAN_COMMERCE, NUM_COMMERCE_TYPES);
	if (iMask & CPK_WB)       CascadePerf::calc(CSCOPE_CITY, CCHAN_WELLBEING);
	if (iMask & CPK_SCFLAT) { CascadePerf::calc(CSCOPE_CITY, CCHAN_GP); CascadePerf::calc(CSCOPE_CITY, CCHAN_TRADE);
	                          CascadePerf::calc(CSCOPE_CITY, CCHAN_FREE_SPECIALISTS); }
	if (iMask & CPK_SCPCT) { CascadePerf::calc(CSCOPE_CITY, CCHAN_GP); CascadePerf::calc(CSCOPE_CITY, CCHAN_DEFENSE);
	                          CascadePerf::calc(CSCOPE_CITY, CCHAN_MAINTENANCE); }
	if (iMask & CPK_SCSPEC)   CascadePerf::calc(CSCOPE_CITY, CCHAN_GP);
	if (iMask & CPK_BR)       CascadePerf::calc(CSCOPE_CITY, CCHAN_BUILDRATE);
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
	// The REALIZED-rate cache (CPK_YRATE -- the "cache the sum" ruling): store the full §2a combine per yield
	// channel. LAST in the fill so a co-marked call's input fills above land first; the plot pull + trade input
	// + GA gate are baked at fill (their change sites mark this bit).
	if (iMask & CPK_YRATE)
	{
		CascadePerf::calcN(CSCOPE_CITY, CCHAN_BASE_YIELDS, NUM_YIELD_TYPES);
		for (int y = 0; y < NUM_YIELD_TYPES; ++y)
			st.yRate100[y] = acc_yieldCombine(pCity, (YieldTypes)y);
	}
	// (the buildable/trainable/creatable/maintainable frontiers all live on the standardized enabler domains --
	// CvCity::m_enabler / CvPlayer::m_enabler, event-maintained; enabler.md par.7/8)
}

void CascadeAccumulator::refreshPlayerScope(const CvPlayer* pPlayer, int iMask)
{
	if (pPlayer == NULL || iMask == 0) return;
	emitCacheRebuilt(1, pPlayer->getID(), pPlayer->getID(), iMask);   // observability: this empire's packages recomputed
	// (scope,channel) calc-count [DEC-calc-count-gate]: attribute this refresh's value-computes to (empire, channel)
	if (iMask & PSC_YFLAT)       CascadePerf::calcN(CSCOPE_EMPIRE, CCHAN_BASE_YIELDS, NUM_YIELD_TYPES);
	if (iMask & PSC_CFLAT)       CascadePerf::calcN(CSCOPE_EMPIRE, CCHAN_COMMERCE, NUM_COMMERCE_TYPES);
	if (iMask & PSC_WB)          CascadePerf::calc(CSCOPE_EMPIRE, CCHAN_WELLBEING);
	if (iMask & PSC_SC)        { CascadePerf::calc(CSCOPE_EMPIRE, CCHAN_GP); CascadePerf::calc(CSCOPE_EMPIRE, CCHAN_MAINTENANCE);
	                             CascadePerf::calc(CSCOPE_EMPIRE, CCHAN_TRADE); }
	if (iMask & PSC_BR)          CascadePerf::calc(CSCOPE_EMPIRE, CCHAN_BUILDRATE);
	if (iMask & PSC_FRONT_P)     CascadePerf::calc(CSCOPE_EMPIRE, CCHAN_FRONTIER);
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
		// (techs + civics left this fill: their lists are the STANDARDIZED enabler's maintained vectors --
		// CvPlayer::m_enabler.techs/.civics, seeded at load + event-delta'd; enabler.md par.7/7.1)
		ps.enHurryOk.clear();
		EnablerKernel::gateSet(EDGEB_HURRIES, candP, fec, *pPlayer, kTeam, false, ps.enHurryOk);
		// (the canBuild unlock left this fill: the builds domain is the STANDARDIZED enabler's maintained
		// vector -- CvPlayer::m_enabler.builds, event-built; enabler.md par.7.1)
	}
	// (the promotion frontier's player-wide tech halves left this fill: the promotions domain is the
	// STANDARDIZED enabler's maintained vector -- CvPlayer::m_enabler.promotions, event-built; the per-unit
	// composite overlays its planes at level-up. enabler.md par.7.1)
}

void CascadeAccumulator::refreshWorldScope(const CvGame* pGame, int iMask)
{
	if (pGame == NULL || iMask == 0) return;
	emitCacheRebuilt(2, -1, -1, iMask);   // observability: the world packages recomputed
	// (scope,channel) calc-count [DEC-calc-count-gate]: the world scope's tenant today is the tradeRoutes world term
	CascadePerf::calc(CSCOPE_WORLD, CCHAN_TRADE);
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

// #430 F4: the UNIT self-accumulator refresh (modifier.md §6; f4-unit-plane.md). GATHER-ON-DIRTY over the unit's
// small held-set -- each HELD promotion + each HELD unit-combat class (primary + subs, tracked exactly as the
// retired apply-loops fired, via isHasUnitCombat). These step-1 deposits are UNCONDITIONED, but the ctx carries
// unit/player/team (source == target) for the conditioned unit channels that land later.
//
// ⛔ DELTA-ONLY (DEC-mirror-then-redesign, owner 2026-07-19): the gather is a faithful MIRROR of the legacy
// m_iExtra* accumulators, which were fed ONLY by processPromotion / processUnitCombat -- NEVER the unit-type base.
// So the UNIT-TYPE intrinsic (CvUnitInfo base values) is DELIBERATELY NOT a source here: the consumer composite
// (e.g. withdrawalProbability() = m_pUnitInfo->getWithdrawalProbability() + getExtraWithdrawal()) already adds the
// type base ONCE. Summing jType would materialize that same base a SECOND time (the base scalar and the deposit
// share one address, e.g. withdrawal.unit.percent) -- a double-count. getExtra*() must reproduce the legacy DELTA
// (promotions + unit-combats), nothing more.
//
// NONE of these groups has a special-unit source: special units author zero deposits for these channels and
// SPECIALUNIT is not pushed into the DepositIndex (GC.getSpecialUnitInfo is the legacy CvInfoBase array, not a
// deposit-carrying CvInfo) -- so their legacy folds summed a perpetually-zero scalar; the cascade matches that zero.
void CascadeAccumulator::refreshUnitPackages(const CvUnit* pUnit, int iMask)
{
	if (pUnit == NULL || iMask == 0) return;
	CascadeUnitPackages& st = pUnit->m_cascadeUnitPackages;
	const CvPlayer& player = GET_PLAYER(pUnit->getOwner());
	CvCascadeEvalCtx ec;
	ec.unit = pUnit; ec.player = &player; ec.team = &GET_TEAM(player.getTeam());

	if (iMask & UPK_WITHDRAWAL)
	{
		// contract rule 2: fully define the output every call (zero-then-sum). withdrawal.unit.percent, HUMAN int.
		// DELTA-ONLY: promotions + unit-combats, NO unit-type base (the consumer composite adds the base once).
		int iWithdrawal = 0;
		// each HELD promotion
		for (int i = 0; i < GC.getNumPromotionInfos(); ++i)
		{
			if (!pUnit->isHasPromotion((PromotionTypes)i)) continue;
			const CvInfo* jP = InfoRepo<CvPromotionInfo>::get().get(i);
			if (jP != NULL) iWithdrawal += MMKernel::sumUnit(jP, "withdrawal.unit", "percent", ec);
		}
		// each HELD unit-combat class (primary + subs -- the exact set the apply-loop processUnitCombat folded)
		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)i)) continue;
			const CvInfo* jC = InfoRepo<CvUnitCombatInfo>::get().get(i);
			if (jC != NULL) iWithdrawal += MMKernel::sumUnit(jC, "withdrawal.unit", "percent", ec);
		}
		st.withdrawal = iWithdrawal;
	}

	if (iMask & UPK_FIRSTSTRIKE)
	{
		// contract rule 2: fully define the outputs every call (zero-then-sum). firstStrike COUNT + CHANCE, HUMAN int.
		// DELTA-ONLY: promotions + unit-combats, NO unit-type base (the consumer composite adds the base once).
		int iStrikes = 0, iChance = 0;
		// each HELD promotion
		for (int i = 0; i < GC.getNumPromotionInfos(); ++i)
		{
			if (!pUnit->isHasPromotion((PromotionTypes)i)) continue;
			const CvInfo* jP = InfoRepo<CvPromotionInfo>::get().get(i);
			if (jP == NULL) continue;
			iStrikes += MMKernel::sumUnit(jP, "firstStrike.unit.strikes", "flat", ec);
			iChance  += MMKernel::sumUnit(jP, "firstStrike.unit.chance", "flat", ec);
		}
		// each HELD unit-combat class (primary + subs -- the exact set the apply-loop processUnitCombat folded)
		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)i)) continue;
			const CvInfo* jC = InfoRepo<CvUnitCombatInfo>::get().get(i);
			if (jC == NULL) continue;
			iStrikes += MMKernel::sumUnit(jC, "firstStrike.unit.strikes", "flat", ec);
			iChance  += MMKernel::sumUnit(jC, "firstStrike.unit.chance", "flat", ec);
		}
		st.fsStrikes = iStrikes;
		st.fsChance = iChance;
	}

	if (iMask & UPK_HEAL)
	{
		// contract rule 2: fully define the outputs every call (zero-then-sum). heal territory family, HUMAN int.
		// DELTA-ONLY: promotions + unit-combats. (Heal's jType returned 0 already -- no unit type authors territory
		// heal, and the heal consumers read the cache directly with NO separate base add -- but drop jType for the
		// one uniform rule; if a unit type ever authors territory heal it would belong in the type-base consumer path,
		// not this delta accumulator.)
		int iEnemy = 0, iNeutral = 0, iFriendly = 0, iSameTile = 0, iAdjacent = 0;
		// each HELD promotion
		for (int i = 0; i < GC.getNumPromotionInfos(); ++i)
		{
			if (!pUnit->isHasPromotion((PromotionTypes)i)) continue;
			const CvInfo* jP = InfoRepo<CvPromotionInfo>::get().get(i);
			if (jP == NULL) continue;
			iEnemy    += MMKernel::sumUnit(jP, "heal.unit.enemy", "flat", ec);
			iNeutral  += MMKernel::sumUnit(jP, "heal.unit.neutral", "flat", ec);
			iFriendly += MMKernel::sumUnit(jP, "heal.unit.friendly", "flat", ec);
			iSameTile += MMKernel::sumUnit(jP, "heal.unit.sameTile", "flat", ec);
			iAdjacent += MMKernel::sumUnit(jP, "heal.unit.adjacentTile", "flat", ec);
		}
		// each HELD unit-combat class (primary + subs -- the exact set the apply-loop processUnitCombat folded)
		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)i)) continue;
			const CvInfo* jC = InfoRepo<CvUnitCombatInfo>::get().get(i);
			if (jC == NULL) continue;
			iEnemy    += MMKernel::sumUnit(jC, "heal.unit.enemy", "flat", ec);
			iNeutral  += MMKernel::sumUnit(jC, "heal.unit.neutral", "flat", ec);
			iFriendly += MMKernel::sumUnit(jC, "heal.unit.friendly", "flat", ec);
			iSameTile += MMKernel::sumUnit(jC, "heal.unit.sameTile", "flat", ec);
			iAdjacent += MMKernel::sumUnit(jC, "heal.unit.adjacentTile", "flat", ec);
		}
		st.healEnemy = iEnemy;
		st.healNeutral = iNeutral;
		st.healFriendly = iFriendly;
		st.healSameTile = iSameTile;
		st.healAdjacent = iAdjacent;
	}

	if (iMask & UPK_EVASION)
	{
		// contract rule 2: zero-then-sum. air.unit.evasion.percent, HUMAN int. DELTA-ONLY (promotions + unit-combats).
		int iEvasion = 0;
		for (int i = 0; i < GC.getNumPromotionInfos(); ++i)
		{
			if (!pUnit->isHasPromotion((PromotionTypes)i)) continue;
			const CvInfo* jP = InfoRepo<CvPromotionInfo>::get().get(i);
			if (jP != NULL) iEvasion += MMKernel::sumUnit(jP, "air.unit.evasion", "percent", ec);
		}
		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)i)) continue;
			const CvInfo* jC = InfoRepo<CvUnitCombatInfo>::get().get(i);
			if (jC != NULL) iEvasion += MMKernel::sumUnit(jC, "air.unit.evasion", "percent", ec);
		}
		st.evasion = iEvasion;
	}

	if (iMask & UPK_INTERCEPT)
	{
		// contract rule 2: zero-then-sum. air.unit.intercept.percent, HUMAN int. DELTA-ONLY (promotions + unit-combats).
		int iIntercept = 0;
		for (int i = 0; i < GC.getNumPromotionInfos(); ++i)
		{
			if (!pUnit->isHasPromotion((PromotionTypes)i)) continue;
			const CvInfo* jP = InfoRepo<CvPromotionInfo>::get().get(i);
			if (jP != NULL) iIntercept += MMKernel::sumUnit(jP, "air.unit.intercept", "percent", ec);
		}
		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)i)) continue;
			const CvInfo* jC = InfoRepo<CvUnitCombatInfo>::get().get(i);
			if (jC != NULL) iIntercept += MMKernel::sumUnit(jC, "air.unit.intercept", "percent", ec);
		}
		st.intercept = iIntercept;
	}

	if (iMask & UPK_COLLATERAL)
	{
		// contract rule 2: zero-then-sum. collateral.unit.damage.percent, HUMAN int. DELTA-ONLY (promotions + unit-combats).
		int iCollateral = 0;
		for (int i = 0; i < GC.getNumPromotionInfos(); ++i)
		{
			if (!pUnit->isHasPromotion((PromotionTypes)i)) continue;
			const CvInfo* jP = InfoRepo<CvPromotionInfo>::get().get(i);
			if (jP != NULL) iCollateral += MMKernel::sumUnit(jP, "collateral.unit.damage", "percent", ec);
		}
		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)i)) continue;
			const CvInfo* jC = InfoRepo<CvUnitCombatInfo>::get().get(i);
			if (jC != NULL) iCollateral += MMKernel::sumUnit(jC, "collateral.unit.damage", "percent", ec);
		}
		st.collateralDamage = iCollateral;
	}

	if (iMask & UPK_CAPTURE)
	{
		// contract rule 2: zero-then-sum. capture.unit.{probability,resistance}.flat, HUMAN int. DELTA-ONLY.
		int iProb = 0, iResist = 0;
		for (int i = 0; i < GC.getNumPromotionInfos(); ++i)
		{
			if (!pUnit->isHasPromotion((PromotionTypes)i)) continue;
			const CvInfo* jP = InfoRepo<CvPromotionInfo>::get().get(i);
			if (jP == NULL) continue;
			iProb   += MMKernel::sumUnit(jP, "capture.unit.probability", "flat", ec);
			iResist += MMKernel::sumUnit(jP, "capture.unit.resistance", "flat", ec);
		}
		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)i)) continue;
			const CvInfo* jC = InfoRepo<CvUnitCombatInfo>::get().get(i);
			if (jC == NULL) continue;
			iProb   += MMKernel::sumUnit(jC, "capture.unit.probability", "flat", ec);
			iResist += MMKernel::sumUnit(jC, "capture.unit.resistance", "flat", ec);
		}
		st.captureProb = iProb;
		st.captureResist = iResist;
	}
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
	// the CACHED realized sum (the "cache the sum" ruling): a stored int like legacy's m_aiBaseYieldRate --
	// the dirty-flagged lazy read is the CvDerivedCache doctrine's own shape.
	pCity->m_cascadeCityPackages.set.ensure(CPK_YRATE);
	return pCity->m_cascadeCityPackages.yRate100[eY];
}

long CascadeAccumulator::commerceRate100(const CvCity* pCity, CommerceTypes eC)
{
	if (pCity == NULL || eC < 0 || eC >= NUM_COMMERCE_TYPES) return 0;
	pCity->m_cascadeCityPackages.set.ensure(CPK_YRATE);
	const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const CascadePlayerScope& ps = owner.m_cascadePlayerScope;
	const long yc100 = st.yRate100[YIELD_COMMERCE];
	const long prate = st.yRate100[YIELD_PRODUCTION] / 100;
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

// The L6 fold's read: the derived trait national GP flat (replaces the m_iNationalGreatPeopleRate ride-in
// in the flipped getBaseGreatPeopleRate; the *Legacy bodies are demolition fodder, unread by the cascade).
// The max(0,·) clamp mirrors the legacy getter exactly.
int CascadeAccumulator::scGpNational(const CvPlayer* pPlayer)
{
	if (pPlayer == NULL) return 0;
	pPlayer->m_cascadePlayerScope.set.ensure(PSC_SC);
	return std::max(0, pPlayer->m_cascadePlayerScope.gpNationalFlat);
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


// The promotion COMPOSITE (the harness's cascade side verbatim, single source -- the harness now consumes
// this): the frontier half (tech halves cached player-wide + the unit's held promos + its unitcombat) +
// the event-injection-only mirror + requires, over the bespoke legacy half (isPromotionValidLegacy(...,
// bFree=true) -- exactly the tech-gate-skipping ride parity was proven with).
bool CascadeAccumulator::enPromotionValid(const CvUnit* pUnit, int ePromo)
{
	if (pUnit == NULL || ePromo < 0) return false;
	CascadeCondScope ccs(CC_PROMO);   // the composite's own evals
	const CvPlayer& kPlayer = GET_PLAYER(pUnit->getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());

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
		memo.uCand.swap(cand[EDGEB_PROMOTIONS]);
		memo.uRem.swap(rem[EDGEB_PROMOTIONS]);
	}
	{
		std::map<int, bool>::const_iterator vit = memo.verdicts.find(ePromo);
		if (vit != memo.verdicts.end()) return vit->second;
	}
	const std::set<int>& uCand = memo.uCand;
	const std::set<int>& uRem = memo.uRem;
	// the membership FORMULA with the per-unit planes OVERLAID on the player domain's maintained planes
	// (enabler.md par.7.1: the player holds the unlocked-promotions set; the unit's held-promo/unitcombat
	// halves overlay at level-up): Σenable > 0 && Σremove == 0. The old "no tech edge anywhere ==>
	// always-unlocked" whitelist is DEAD (superseded-ideas #14) -- start promotions ride the
	// TECH_GAME_START root's enables.promotions; a missing edge fails CLOSED, visibly.
	const EnablerDomain& d = kPlayer.m_enabler.promotions;
	const bool bInRem = d.removeCount(ePromo) > 0 || uRem.count(ePromo) != 0;
	const bool bInCand = d.enableCount(ePromo) > 0 || uCand.count(ePromo) != 0;
	const bool bUnlocked = !bInRem && bInCand;
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

static int acc_brSelf(const CvInfo* d, const CvCity* pCity)
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

// The WIDEN rule for the realized-rate cache: any mark touching a yield INPUT package invalidates the stored
// combine too (the "cache the sum" ruling's freshness half).
static int acc_widenCityBits(int iMask)
{
	return (iMask & (CPK_YPCT | CPK_YSPEC | CPK_YEXTRA)) ? (iMask | CPK_YRATE) : iMask;
}

void CascadeAccumulator::dirtyCity(const CvCity* pCity, int iMask)
{
	if (pCity == NULL) return;
	pCity->m_cascadeCityPackages.set.markDirty(acc_widenCityBits(iMask));
}

// The building event: the CITY mask stays CONSERVATIVE-ALL (a new/lost building can flip OTHER buildings'
// active state through the operate/provides fixpoint, so this building's own deposits do NOT bound the
// city-side effect); the CROSS-SCOPE masks are DERIVED from the building's compiled deposits (percent-vs-flat
// × scope), so a city-only building never touches the player/world packages, and only a genuine grantor
// (empire buildings-keyed commerce) fans out to the sibling cities' CBASE realizations.
void CascadeAccumulator::buildingProcessed(const CvCity* pCity, BuildingTypes eBuilding)
{
	if (pCity == NULL) return;
	// #430: the buildable/trainable frontiers live on the per-city ENABLER domains (CvCity::m_enabler --
	// event-maintained, enabler.md par.7), so a building change marks only the modifier packages here.
	pCity->m_cascadeCityPackages.set.markDirty(CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC));
	EnablerKernel::onBuildingChangedActive(pCity, (int)eBuilding);   // operating buildings: targeted ripple into the authoritative active set (was blanket markAllDirty)

	const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get((int)eBuilding);
	if (d == NULL) return;
	// #430 R2: the cross-scope masks are the COMPILED reverse route (DepositIndex::routeFor) -- the former inline
	// per-deposit loop, lifted verbatim into the index and computed once. So a city-only building routes to nothing
	// here; only a genuine grantor (empire/area buildings-keyed commerce, specialist perSpecialist, world flats)
	// fans out to the player / sibling-city / world packages.
	const SourceRoute& route = DepositIndex::routeFor(d);
	if (route.playerBits != 0 || route.cityBits != 0)
	{
		const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
		if (route.playerBits != 0) owner.m_cascadePlayerScope.set.markDirty(route.playerBits);
		if (route.cityBits != 0)
		{
			int iLoop;
			for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
				pc->m_cascadeCityPackages.set.markDirty(acc_widenCityBits(route.cityBits));   // the sibling loop includes this city
		}
	}
	if (route.world) GC.getGame().m_cascadeWorldScope.set.markAllDirty();
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
	EnablerKernel::buildActiveIndex();   // the operate reverse-index for the targeted active-set maintenance
}

// Part C: a city-local HAVE atom flipped -> re-check ONLY the frontier entities that reference it, for BOTH
// domains (buildable + trainable), in place. Each cascade skips its box if a full rebuild is already pending.
void CascadeAccumulator::cityHaveChanged(const CvCity* pCity, int eHaveKind)
{
	if (pCity == NULL) return;
	const CvPlayer& kPlayer = GET_PLAYER(pCity->getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	EnablerKernel::onHaveChangedActive(pCity, eHaveKind);   // operating buildings: targeted ripple for the HAVE-referencing operate
}

// #430 G3: a single bonus's access flipped -> the targeted operate ripple over that bonus's operate consumers.
void CascadeAccumulator::cityBonusAccessChanged(const CvCity* pCity, int eBonus)
{
	if (pCity == NULL || eBonus < 0) return;
	EnablerKernel::onBonusAccessChangedActive(pCity, eBonus);
}

// ===================== the BOUNDARIES =====================
//
// The per-turn / load SELF-HEAL blankets (playerSliceRebuild, worldRebuild -- markAll + eager ensure of ALL
// packages) are REMOVED ([DEC-no-self-heal]). They were the calc-ALL rollerskate: they recomputed every package
// every turn instead of only the ones a spine event marked. Correctness is now ONLY the eventspine-routed marks
// (the R3 cache-invalidation consumer -> the deposit-index route) + LAZY recalc of the dirty packages; a missed
// invalidation surfaces as a live divergence, never a silently rebuilt-away cost. The only ruled eager-ensure is
// cityCreated (a founded city's yields stand at once). Do NOT re-introduce a slice/world blanket.

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
