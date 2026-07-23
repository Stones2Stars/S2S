//
//	CascadeScalarChannels -- the #430 city scalar channels (see the header). Per channel: Σ the curated
//	deposits over the live source sets (ACTIVE buildings via the operating buildings cache; adopted civics; held traits with
//	the PURE_TRAITS filter), conditions evaluated against the live ctx.
//

#include "CvGameCoreDLL.h"
#include "Infos/CvWorldInfo.h"
#include "Infos/CvCommerceInfo.h"
#include "CvCascadeScalarChannels.h"
#include "Data/CvDepositRead.h"
#include "Data/CvDepositIndex.h"    // the compiled segment ids the keyed walks match on
#include "CvCascadeAccumulator.h"     // the accumulator package surface (the stamps are DELETED -- scope-packages.md phase 3)
#include "Enabler/CvEnablerKernel.h"   // operatingBuildings -- the player-wide maintenance walk
#include "Enabler/CvOperatingBuildings.h"
#include "CvInfo.h"
#include "CvTraitInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "AI/CvPlayerAI.h"            // GET_PLAYER (was riding a preceding unity-batch file -- self-sufficient now)
#include "AI/CvTeamAI.h"             // GET_TEAM (ditto)
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "Engine/CvCity.h"
#include "CvBuildingInfo.h"
#include "CvCivicInfo.h"
#include "CvTechInfo.h"
#include "CvSpecialistInfo.h"
#include "CvUnitInfo.h"
#include "CvUnitCombatInfo.h"
#include "CvProjectInfo.h"      // InfoRepo<CvProjectInfo> -- the L9 project maintenance fold
#include "CvCorporationInfo.h"  // InfoRepo<CvCorporationInfo> -- the L10 corp military buildRate fold
#include "Engine/CvArea.h"            // isHomeArea -- the L8 civic home/other-area overlay gate
#include "CvProjectInfo.h"
#include <map>

// Σ a unit over the city's ACTIVE buildings at an address (the shared building-source walk). Iterates the
// STANDING active set (~dozens) -- never all ~5202 infos with a per-info active check (the all-infos shape,
// run eagerly city×player×turn, was the measured 222s turn + the MAF-inducing churn).
static int sc_buildings(const std::string& addr, const char* unit, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	int iSum = 0;
	if (ec.activeBuildings != NULL)
	{
		for (std::set<int>::const_iterator it = ec.activeBuildings->begin(); it != ec.activeBuildings->end(); ++it)
		{
			const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
			if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, ec);
		}
		// obsolete buildings deliver their whenObsolete numbers into the SAME sum (json §4.2; part-1 delivery, no
		// combine change). 0 for any address the whenObsolete tree doesn't author -- inert until the swap cut.
		if (ec.obsoleteBuildings != NULL)
			for (std::set<int>::const_iterator it = ec.obsoleteBuildings->begin(); it != ec.obsoleteBuildings->end(); ++it)
			{
				const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
				if (d != NULL) iSum += MMKernel::sumUnitFrom(DepositIndex::whenObsoleteFor(d), addr, unit, ec);
			}
		return iSum;
	}
	const int nB = GC.getNumBuildingInfos();   // unwired-ctx fallback (correctness identical)
	for (int b = 0; b < nB; ++b)
	{
		const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (d == NULL) continue;
		if (cascadeIsBuildingActive(b, ec)) iSum += MMKernel::sumUnit(d, addr, unit, ec);
		else if (cascadeIsBuildingObsolete(b, ec)) iSum += MMKernel::sumUnitFrom(DepositIndex::whenObsoleteFor(d), addr, unit, ec);
	}
	return iSum;
}

// Σ a unit over ALL the player's cities' ACTIVE buildings at an address (the player-accumulator semantic:
// an empire-scope building deposit feeds the player from ANY city).
static int sc_playerBuildings(const std::string& addr, const char* unit, const CvPlayer& owner, const CvTeam* pTeam)
{
	int iSum = 0, iLoop;
	for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
	{
		const OperatingBuildings& operatingBuildings = EnablerKernel::operatingBuildings(pc);
		CvCascadeEvalCtx pec;
		pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = pTeam;
		pec.activeBuildings = &operatingBuildings.active; pec.vicinityProvidedBonuses = &operatingBuildings.provided;
		for (std::set<int>::const_iterator it = operatingBuildings.active.begin(); it != operatingBuildings.active.end(); ++it)
		{
			const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
			if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, pec);
		}
	}
	return iSum;
}

// Σ a unit over the player's adopted civics + held traits (pure-filtered) at an address.
// ⛔ CIVIC and TRAIT are walked SEPARATELY, never as one bundled sum: each is its own deposit SOURCE, and the
// source IS the slot position ([DEC-uniform-cache-shape]) -- so the slots a consumer sums are the slots an
// endpoint decomposes. Bundling them would re-bake an attribution the package is required to carry.
static int sc_civics(const std::string& addr, const char* unit, const CvPlayer& owner, const CvCascadeEvalCtx& ec)
{
	int iSum = 0;
	for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
	{
		const CivicTypes eCivic = owner.getCivics((CivicOptionTypes)i);
		if (eCivic == NO_CIVIC) continue;
		const CvInfo* d = InfoRepo<CvCivicInfo>::get().get(eCivic);
		if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, ec);
	}
	return iSum;
}

static int sc_traits(const std::string& addr, const char* unit, const CvPlayer& owner, const CvCascadeEvalCtx& ec)
{
	int iSum = 0;
	for (int i = 0; i < GC.getNumTraitInfos(); ++i)
	{
		if (!owner.hasTrait((TraitTypes)i)) continue;
		const CvTraitInfo* d = MMKernel::traitData(i);
		if (d != NULL) iSum += MMKernel::sumTrait(d, addr, unit, ec);
	}
	return iSum;
}

// ===================== the maintenance AREA split (one walk, fill + decomposition) =====================
// Per-city area grouping of the area/otherArea building percents (the one player-wide sum
// sc_playerBuildings cannot express). Single-source: the CvPlayer package fill AND the /computed
// decomposition recompute both call it.
static void sc_maintAreaSplit(const CvPlayer& owner, const CvTeam* pTeam,
	std::map<int, int>& areaPct, std::map<int, int>& otherAreaPct, int& iOtherTotal)
{
	areaPct.clear();
	otherAreaPct.clear();
	iOtherTotal = 0;
	int iLoop;
	for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
	{
		const OperatingBuildings& operatingBuildings = EnablerKernel::operatingBuildings(pc);
		CvCascadeEvalCtx pec;
		pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = pTeam;
		pec.activeBuildings = &operatingBuildings.active; pec.vicinityProvidedBonuses = &operatingBuildings.provided;
		const int iArea = pc->area()->getID();
		for (std::set<int>::const_iterator it = operatingBuildings.active.begin(); it != operatingBuildings.active.end(); ++it)
		{
			const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
			if (d == NULL) continue;
			areaPct[iArea] += MMKernel::sumUnit(d, "maintenance.area", "percent", pec);
			const int iOther = MMKernel::sumUnit(d, "maintenance.area.otherArea", "percent", pec);
			if (iOther != 0) { otherAreaPct[iArea] += iOther; iOtherTotal += iOther; }
		}
	}
}

// The player's CROSS-AREA aggregate only ("all my OTHER areas"): a genuine player-scope roll-up, so it stays
// player-side. The PER-AREA halves moved to the area's own package (CascadeAreaPackages, fillAreaScalars).
static void sc_maintOtherAreaTotal(const CvPlayer& owner, const CvTeam* pTeam, int& iOtherTotal)
{
	std::map<int, int> areaPctUnused, otherAreaPctUnused;
	sc_maintAreaSplit(owner, pTeam, areaPctUnused, otherAreaPctUnused, iOtherTotal);
}

int CascadeScalarChannels::gpBaseBuildings(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	// building flats (the player national rate is a live input at the combine)
	return sc_buildings("greatPeopleRate.city", "flat", pCity, ec);
}

int CascadeScalarChannels::gpBaseSpecialists(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	int iSum = 0;
	for (int i = 0; i < GC.getNumSpecialistInfos(); ++i)
	{
		const int iCount = pCity->getSpecialistCount((SpecialistTypes)i) + pCity->getFreeSpecialistCount((SpecialistTypes)i);
		if (iCount == 0) continue;
		const CvInfo* d = InfoRepo<CvSpecialistInfo>::get().get(i);
		if (d != NULL) iSum += iCount * MMKernel::sumUnit(d, "greatPeopleRate.city", "flat", ec);
	}
	return iSum;
}

int CascadeScalarChannels::gpRateBase(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	// the two increment-F components, summed (single-source: this IS their only combine)
	return gpBaseBuildings(pCity, ec) + gpBaseSpecialists(pCity, ec);
}

int CascadeScalarChannels::gpRateModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	// the §9.5 / :7153 stack: 100 + city + player percents...
	int iMod = 100;
	iMod += sc_buildings("greatPeopleRate.city", "percent", pCity, ec);
	iMod += sc_playerBuildings("greatPeopleRate.empire", "percent", owner, ec.team);   // GLOBAL GP mods feed the player from ANY city
	iMod += (sc_civics("greatPeopleRate.city", "percent", owner, ec) + sc_traits("greatPeopleRate.city", "percent", owner, ec));
	iMod += (sc_civics("greatPeopleRate.empire", "percent", owner, ec) + sc_traits("greatPeopleRate.empire", "percent", owner, ec));
	// ...+ the STATE-RELIGION grouped family (civic stateReligion.empire.greatPeopleRate) while the state
	// religion is PRESENT IN THIS CITY (:7160)...
	{
		const ReligionTypes eState = owner.getStateReligion();
		if (eState != NO_RELIGION && pCity->isHasReligion(eState))
			iMod += (sc_civics("stateReligion.empire.greatPeopleRate", "percent", owner, ec) + sc_traits("stateReligion.empire.greatPeopleRate", "percent", owner, ec));
	}
	// ...+ the golden-age modifier -- a GLOBAL DEFINE, a config input (not data)
	if (owner.isGoldenAge())
		iMod += GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER();
	return std::max(0, iMod);
}

void CascadeScalarChannels::gpModParts(const CvCity* pCity, const CvCascadeEvalCtx& ec, int& iBld, int& iCivTrait, int& iSr)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	iBld = sc_buildings("greatPeopleRate.city", "percent", pCity, ec);
	iCivTrait = sc_playerBuildings("greatPeopleRate.empire", "percent", owner, ec.team);   // the player-wide building half
	iCivTrait += (sc_civics("greatPeopleRate.city", "percent", owner, ec) + sc_traits("greatPeopleRate.city", "percent", owner, ec))
	           + (sc_civics("greatPeopleRate.empire", "percent", owner, ec) + sc_traits("greatPeopleRate.empire", "percent", owner, ec));
	iSr = 0;
	const ReligionTypes eState = owner.getStateReligion();
	if (eState != NO_RELIGION && pCity->isHasReligion(eState))
		iSr = (sc_civics("stateReligion.empire.greatPeopleRate", "percent", owner, ec) + sc_traits("stateReligion.empire.greatPeopleRate", "percent", owner, ec));
}

int CascadeScalarChannels::defenseAmount(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	// the building city defense stack (legacy m_iBuildingDefense; natural/bonus/player defense are their
	// own terms at the combine -- decomposed separately)
	return sc_buildings("defense.city.amount", "percent", pCity, ec);
}

// The L13 defense wiring (owner-ruled shape 2026-07-05: "all additive percentages, or things that raise
// the FLOOR of defense -- wire it properly so cities don't lose defense on flip"):
// ===== the freeSpecialists AMOUNT computer (the ruled two-part seam, 2026-07-05) =====
// The cascade owns the AMOUNTS (summed alive-with-source freeSpecialists deposits: `any` + count-by-type,
// parsed as unit-"count" deposits with resolved targetFk); the engine owns PLACEMENT; consumers read the
// placement OUTPUT. These sums FEED the engine placement at the demolition (replacing the
// changeFreeSpecialist(Count) process-applies); until then they serve the /computed decomposition. The
// NON-derivable legacy remainders (events, settled GPs, era-advance pulses) are engine state by class.
struct ScFsSegs { int fs, city, empire, area, any, count; };
static const ScFsSegs& sc_fsSegs()
{
	static ScFsSegs s = { -1, -1, -1, -1, -1, -1 };
	if (s.fs < 0) s.fs = DepositIndex::lookupSegment("freeSpecialists");
	if (s.city < 0) s.city = DepositIndex::lookupSegment("city");
	if (s.empire < 0) s.empire = DepositIndex::lookupSegment("empire");
	if (s.area < 0) s.area = DepositIndex::lookupSegment("area");
	if (s.any < 0) s.any = DepositIndex::lookupSegment("any");
	if (s.count < 0) s.count = DepositIndex::lookupSegment("count");
	return s;
}
static void sc_fsFold(const CvInfo* d, int scopeId, const CvCascadeEvalCtx& ec, int iPureSign,
	int& outAny, std::vector<int>* pByType)
{
	if (d == NULL) return;
	const ScFsSegs& sg = sc_fsSegs();
	const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(d);
	for (size_t i = 0; i < deps.size(); ++i)
	{
		const CascadeDeposit& dep = deps[i];
		if (dep.nSeg != 3 || dep.unitId != sg.count) continue;
		if (dep.seg[0] != sg.fs || dep.seg[1] != scopeId) continue;
		const int v = dep.value100 / 100;   // the AUTHORED value (the pure filter's sign gate)
		if (iPureSign > 0 && v < 0) continue;
		if (iPureSign < 0 && v > 0) continue;
		if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
		const int vs = (int)(MMKernel::perScale(dep, ec, dep.value100) / 100);   // §3.7 per (identity when hasPer==false)
		if (dep.seg[2] == sg.any) outAny += vs;
		else if (pByType != NULL && dep.targetFk >= 0 && dep.targetFk < (int)pByType->size())
			(*pByType)[dep.targetFk] += vs;
	}
}

void CascadeScalarChannels::fillFreeSpecialistsCity(const CvCity* pCity, const CvCascadeEvalCtx& ec,
	int& outAny, std::vector<int>& outByType)
{
	outAny = 0;
	outByType.assign(GC.getNumSpecialistInfos(), 0);
	const OperatingBuildings& operatingBuildings = EnablerKernel::operatingBuildings(pCity);
	for (std::set<int>::const_iterator it = operatingBuildings.active.begin(); it != operatingBuildings.active.end(); ++it)
		sc_fsFold(InfoRepo<CvBuildingInfo>::get().get(*it), sc_fsSegs().city, ec, 0, outAny, &outByType);
}

int CascadeScalarChannels::defenseBombardCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	return sc_buildings("defense.city.bombardDefense", "percent", pCity, ec);   // legacy m_iBuildingBombardDefense
}
int CascadeScalarChannels::defenseMinCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	return sc_buildings("defense.city.min", "flat", pCity, ec);                 // legacy m_iExtraMinDefense (additive floor)
}

int CascadeScalarChannels::tradeRouteCount(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	int iCount = 0;
	// this city's extra routes (building city flats)
	iCount += sc_buildings("tradeRoutes.city", "flat", pCity, ec);
	// the player-wide global routes (any city's buildings' empire flats + civics/traits)
	iCount += sc_playerBuildings("tradeRoutes.empire", "flat", owner, ec.team);
	iCount += (sc_civics("tradeRoutes.empire", "flat", owner, ec) + sc_traits("tradeRoutes.empire", "flat", owner, ec));
	// TECHS feed the player routes too (processTech :30911)
	{
		const CvTeam& team = GET_TEAM(owner.getTeam());
		for (int i = 0; i < GC.getNumTechInfos(); ++i)
		{
			if (!team.isHasTech((TechTypes)i)) continue;
			const CvInfo* d = InfoRepo<CvTechInfo>::get().get(i);
			if (d != NULL) iCount += MMKernel::sumUnit(d, "tradeRoutes.empire", "flat", ec);
		}
	}
	// WORLD routes: ANY player's active world-wonder grants EVERY player (:7410) -- tradeRoutes.world flats
	// summed over all living players' active sets
	for (int p = 0; p < MAX_PC_PLAYERS; ++p)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)p);
		if (!kP.isAlive()) continue;
		iCount += sc_playerBuildings("tradeRoutes.world", "flat", kP, &GET_TEAM(kP.getTeam()));
	}
	// the coastal half pays only in coastal cities
	if (pCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
	{
		iCount += sc_playerBuildings("tradeRoutes.empire.coastal", "flat", owner, ec.team);
		iCount += (sc_civics("tradeRoutes.empire.coastal", "flat", owner, ec) + sc_traits("tradeRoutes.empire.coastal", "flat", owner, ec));
	}
	// the precipice-review completions (2026-07-04): the project world grants + the live raw inputs
	iCount += tradeRoutesWorldProjects();
	iCount += tradeRouteLiveInputs(pCity);
	return iCount;
}

// The LIVE raw inputs of the trade-route count (never packaged): the GAME-scope store (vote resolutions
// CvGame:7980 + WorldBuilder -- a clean persisted store, the E-class ride-in ruling 2026-07-04) and the
// INITIAL_TRADE_ROUTES config define (CvPlayer:391/:28722). The CITY WB-poke residue (m_iExtraTradeRoutes
// minus its building feed) stays a NAMED divergence until the demolition's store split -- attributed via
// the endpoint's tradeRoutesCityExtraLeg field, never silently folded (a MIXED accumulator today).
int CascadeScalarChannels::tradeRouteLiveInputs(const CvCity* /*pCity*/)
{
	return GC.getGame().getTradeRoutes() + GC.getINITIAL_TRADE_ROUTES();
}

// PROJECT world routes (the precipice-review Internet-class find): CvProjectInfo world flats granted to
// EVERY player on completion (CvTeam::processProjectChange:4341; PROJECT_THE_INTERNET authors
// tradeRoutes.world.flat:1). Derived from the RAW team project counts × the compiled deposits -- the
// recompute also pays late-born players (the drift-repair class, cascade-right). The live data authors
// these UNCONDITIONED (a conditioned world deposit would need a ctx home -- extend then, not now).
int CascadeScalarChannels::tradeRoutesWorldProjects()
{
	int iCount = 0;
	CvCascadeEvalCtx ec;
	for (int t = 0; t < MAX_TEAMS; ++t)
	{
		const CvTeam& kT = GET_TEAM((TeamTypes)t);
		if (!kT.isAlive()) continue;
		for (int p = 0; p < GC.getNumProjectInfos(); ++p)
		{
			const int n = kT.getProjectCount((ProjectTypes)p);
			if (n <= 0) continue;
			const CvInfo* d = InfoRepo<CvProjectInfo>::get().get(p);
			if (d != NULL) iCount += n * MMKernel::sumUnit(d, "tradeRoutes.world", "flat", ec);
		}
	}
	return iCount;
}

// Σ a KEYED buildRate member over this city's active buildings (city scope) + all the player's cities'
// active buildings (empire scope) + civics + traits: buildRate.{city|empire}.{member}.{KEY}.percent.
// Rides the COMPILED deposit index (the parser layer -- the event->cache routing derives from these
// segments, state-repositories.md): the keyed buildRate deposits are PERCENT-unit, so the walk matches
// through sumKeyed4U with the percent segment (the flat-hardwired sumKeyed4F was the P10 buildRate bug).
// The trait leg threads the PURE_TRAITS sign exactly as sumTrait does (a negative trait keeps only v<=0).
static int sc_buildRateKeyed(const char* szMember, const char* szKey, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const int chanId = DepositIndex::lookupSegment("buildRate");
	const int memberId = DepositIndex::lookupSegment(szMember);
	const int keyId = DepositIndex::lookupSegment(szKey);
	const int pctId = DepositIndex::lookupSegment("percent");
	if (chanId < 0 || memberId < 0 || keyId < 0 || pctId < 0) return 0;   // never authored anywhere => 0
	const int cityId = DepositIndex::lookupSegment("city");
	const int empireId = DepositIndex::lookupSegment("empire");
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	int iSum = 0;
	// this city's buildings: the CITY-scope keyed member
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;
		const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (d != NULL) iSum += MMKernel::sumKeyed4U(d, chanId, cityId, memberId, keyId, pctId, ec, false);
	}
	// the player-wide EMPIRE-scope keyed member (any city's building feeds the player accumulator)
	{
		int iLoop;
		for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
		{
			const OperatingBuildings& operatingBuildings = EnablerKernel::operatingBuildings(pc);
			CvCascadeEvalCtx pec;
			pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = ec.team;
			pec.activeBuildings = &operatingBuildings.active; pec.vicinityProvidedBonuses = &operatingBuildings.provided;
			for (std::set<int>::const_iterator it = operatingBuildings.active.begin(); it != operatingBuildings.active.end(); ++it)
			{
				const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
				if (d != NULL) iSum += MMKernel::sumKeyed4U(d, chanId, empireId, memberId, keyId, pctId, pec, false);
			}
		}
	}
	// civics + traits (empire scope); traits carry the PURE_TRAITS alignment filter
	for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
	{
		const CivicTypes eCivic = owner.getCivics((CivicOptionTypes)i);
		if (eCivic == NO_CIVIC) continue;
		const CvInfo* d = InfoRepo<CvCivicInfo>::get().get(eCivic);
		if (d != NULL) iSum += MMKernel::sumKeyed4U(d, chanId, empireId, memberId, keyId, pctId, ec, false);
	}
	const bool bPure = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);
	for (int i = 0; i < GC.getNumTraitInfos(); ++i)
	{
		if (!owner.hasTrait((TraitTypes)i)) continue;
		const CvTraitInfo* d = MMKernel::traitData(i);
		if (d == NULL) continue;
		const int iPureSign = bPure ? (d->negativeTrait ? -1 : 1) : 0;
		iSum += MMKernel::sumKeyed4U(d, chanId, empireId, memberId, keyId, pctId, ec, false, iPureSign);
	}
	return iSum;
}

// A flat buildRate MEMBER (military/space) over city buildings + player-wide + civics/traits.
static int sc_buildRateMember(const char* szMember, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const std::string cityAddr = std::string("buildRate.city.") + szMember;
	const std::string empAddr = std::string("buildRate.empire.") + szMember;
	int iSum = sc_buildings(cityAddr, "percent", pCity, ec);
	iSum += sc_playerBuildings(empAddr, "percent", owner, ec.team);
	iSum += (sc_civics(empAddr, "percent", owner, ec) + sc_traits(empAddr, "percent", owner, ec));
	return iSum;
}

int CascadeScalarChannels::productionModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec, bool& bHasOrder, BuildRateParts* pParts)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const bool bSrInCity = owner.getStateReligion() != NO_RELIGION && pCity->isHasReligion(owner.getStateReligion());
	const UnitTypes eUnit = pCity->getProductionUnit();
	const BuildingTypes eB = pCity->getProductionBuilding();
	const ProjectTypes ePr = pCity->getProductionProject();
	bHasOrder = true;
	BuildRateParts parts;
	if (eUnit != NO_UNIT)
	{
		const CvUnitInfo& unit = GC.getUnitInfo(eUnit);
		// the target's OWN bonus-gated mods (BonusProductionModifiers -> buildRate.self, conditions gate on hasBonus)
		const CvInfo* d = InfoRepo<CvUnitInfo>::get().get(eUnit);
		if (d != NULL) parts.iSelf = MMKernel::sumUnit(d, "buildRate.self", "percent", ec);
		// the keyed source mods (the engine skips non-type mods under isNoNonTypeProdMods)
		parts.iKeyed = sc_buildRateKeyed("units", unit.getType(), pCity, ec);
		if (!unit.isNoNonTypeProdMods())
		{
			parts.iDomain = sc_buildRateKeyed("domains", GC.getDomainInfo(unit.getDomainType()).getType(), pCity, ec);
			if (unit.getUnitCombatType() != NO_UNITCOMBAT)   // subs count only with a main combat (:3912 nesting)
			{
				parts.iCombatMain = sc_buildRateKeyed("unitCombats", GC.getUnitCombatInfo((UnitCombatTypes)unit.getUnitCombatType()).getType(), pCity, ec);
				foreach_(const UnitCombatTypes eSub, unit.getSubCombatTypes())
					parts.iCombatSubs += sc_buildRateKeyed("unitCombats", GC.getUnitCombatInfo(eSub).getType(), pCity, ec);
			}
			if (unit.isMilitaryProduction())
				parts.iMember = sc_buildRateMember("military", pCity, ec);
			if (bSrInCity)
				parts.iStateReligion = (sc_civics("stateReligion.empire.unitProduction", "percent", owner, ec) + sc_traits("stateReligion.empire.unitProduction", "percent", owner, ec));
		}
	}
	else if (eB != NO_BUILDING)
	{
		const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(eB);
		if (d != NULL) parts.iSelf = MMKernel::sumUnit(d, "buildRate.self", "percent", ec);
		parts.iKeyed = sc_buildRateKeyed("buildings", GC.getBuildingInfo(eB).getType(), pCity, ec);
		if (bSrInCity)
			parts.iStateReligion = (sc_civics("stateReligion.empire.buildingProduction", "percent", owner, ec) + sc_traits("stateReligion.empire.buildingProduction", "percent", owner, ec));
	}
	else if (ePr != NO_PROJECT)
	{
		const CvInfo* d = InfoRepo<CvProjectInfo>::get().get(ePr);
		if (d != NULL) parts.iSelf = MMKernel::sumUnit(d, "buildRate.self", "percent", ec);
		if (GC.getProjectInfo(ePr).isSpaceship())
			parts.iMember = sc_buildRateMember("space", pCity, ec);
	}
	else
	{
		bHasOrder = false;
	}
	if (pParts != NULL) *pParts = parts;
	return parts.total();
}

int CascadeScalarChannels::maintenanceModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	// the effective-modifier stack (:getEffectiveMaintenanceModifier): THIS city's + the player's + the AREA
	// total + connected-to-capital. The player/area/connectedCity halves are PLAYER-WIDE building accumulators
	// (any city's active building feeds them), so those walk ALL the player's cities' active sets.
	// The CITY-REALIZED half is maintenanceModifierCity -- DELEGATE, never re-list its terms
	// ([DEC-single-implementation]). This function used to duplicate that walk (city buildings + civic/trait
	// city/empire/area + techs), and the two drifted the moment a term landed in only one: the L8 civic
	// home/otherArea overlay went into the cached fill and not into this decomposition, so the /computed
	// `maintModCasc` under-reported against the value the game actually serves (`maintModSlot`). Delegating
	// makes that class of drift unrepresentable.
	int iMod = maintenanceModifierCity(pCity, ec);
	// the player-wide building halves: the empire percent; the AREA split -- a city in area A collects A's
	// own area sums + every OTHER area's otherArea sums; and the connected-to-capital percent when this city
	// is connected (and not the capital)
	{
		iMod += sc_playerBuildings("maintenance.empire", "percent", owner, ec.team);
		std::map<int, int> areaPct, otherAreaPct;
		int iOtherTotal;
		sc_maintAreaSplit(owner, ec.team, areaPct, otherAreaPct, iOtherTotal);
		const int iArea = pCity->area()->getID();
		std::map<int, int>::const_iterator ait = areaPct.find(iArea);
		if (ait != areaPct.end()) iMod += ait->second;
		std::map<int, int>::const_iterator oit = otherAreaPct.find(iArea);
		iMod += iOtherTotal - (oit != otherAreaPct.end() ? oit->second : 0);
		if (pCity->isConnectedToCapital() && !pCity->isCapital())
			iMod += sc_playerBuildings("maintenance.empire.connectedCity", "percent", owner, ec.team);
	}
	return iMod;
}

// ===================== the SCOPE-PACKAGE FILLS (scope-packages.md) =====================
// The CITY-REALIZED halves: buildings + civics + traits + techs, every condition evaluated against THIS
// city's ctx (the Burdigala lesson: conditioned sums are city-realized joins). Only the per-source-city
// building walks stay player-side.

int CascadeScalarChannels::gpModifierCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = *ec.player;
	return sc_buildings("greatPeopleRate.city", "percent", pCity, ec)
	     + (sc_civics("greatPeopleRate.city", "percent", owner, ec) + sc_traits("greatPeopleRate.city", "percent", owner, ec))
	     + (sc_civics("greatPeopleRate.empire", "percent", owner, ec) + sc_traits("greatPeopleRate.empire", "percent", owner, ec));
}

int CascadeScalarChannels::gpModifierSrCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	return (sc_civics("stateReligion.empire.greatPeopleRate", "percent", *ec.player, ec) + sc_traits("stateReligion.empire.greatPeopleRate", "percent", *ec.player, ec));
}

int CascadeScalarChannels::maintenanceModifierCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = *ec.player;
	int iMod = sc_buildings("maintenance.city", "percent", pCity, ec)
	         + (sc_civics("maintenance.city", "percent", owner, ec) + sc_traits("maintenance.city", "percent", owner, ec))
	         + (sc_civics("maintenance.empire", "percent", owner, ec) + sc_traits("maintenance.empire", "percent", owner, ec))
	         + (sc_civics("maintenance.area", "percent", owner, ec) + sc_traits("maintenance.area", "percent", owner, ec));
	// The L8 census fold (2026-07-05): the civic HOME/OTHER-AREA maintenance overlays -- curated
	// maintenance.empire.{homeArea|otherArea}.percent (legacy: the player scalar half of
	// CvArea::getTotalAreaMaintenanceModifier, picked by isHomeArea). CITY-REALIZED: the home-area gate is
	// a city fact, so the matching member folds here with the city's own area.
	iMod += (sc_civics(pCity->area()->isHomeArea(owner.getID()) ? "maintenance.empire.homeArea"
	                                                                 : "maintenance.empire.otherArea", "percent", owner, ec) + sc_traits(pCity->area()->isHomeArea(owner.getID()) ? "maintenance.empire.homeArea"
	                                                                 : "maintenance.empire.otherArea", "percent", owner, ec));
	const CvTeam& team = GET_TEAM(owner.getTeam());
	for (int i = 0; i < GC.getNumTechInfos(); ++i)
	{
		if (!team.isHasTech((TechTypes)i)) continue;
		const CvInfo* d = InfoRepo<CvTechInfo>::get().get(i);
		if (d != NULL) iMod += MMKernel::sumUnit(d, "maintenance.empire", "percent", ec);
	}
	return iMod;
}

int CascadeScalarChannels::tradeRoutesCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = *ec.player;
	int iCount = sc_buildings("tradeRoutes.city", "flat", pCity, ec)
	           + (sc_civics("tradeRoutes.empire", "flat", owner, ec) + sc_traits("tradeRoutes.empire", "flat", owner, ec));
	const CvTeam& team = GET_TEAM(owner.getTeam());
	for (int i = 0; i < GC.getNumTechInfos(); ++i)
	{
		if (!team.isHasTech((TechTypes)i)) continue;
		const CvInfo* d = InfoRepo<CvTechInfo>::get().get(i);
		if (d != NULL) iCount += MMKernel::sumUnit(d, "tradeRoutes.empire", "flat", ec);
	}
	return iCount;
}

int CascadeScalarChannels::tradeRoutesCoastalCivCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	return (sc_civics("tradeRoutes.empire.coastal", "flat", *ec.player, ec) + sc_traits("tradeRoutes.empire.coastal", "flat", *ec.player, ec));
}

// ===================== the AREA scope fill (the scope owns its own sums) =====================
// modifier.md 1: deposits accumulate at their OWN scope. These previously sat as area-KEYED maps on
// CascadePlayerScope -- an upper scope's sums parked on a neighbour because AREA had no package at all.
// CvArea is ONE shared map object, so the package is indexed by player; that is the data's shape, not a
// reason to keep it player-side. AREA carries MODIFIERS only (owner ruling) -- no yields here, ever.
void CascadeScalarChannels::fillAreaScalars(const CvArea& area, CascadeAreaPackages& out)
{
	PROFILE_EXTRA_FUNC();
	const int iAreaId = area.getID();
	out.maintOwnPct.assign(MAX_PLAYERS, 0);     // contract rule 2: fully define the output every call
	out.maintOtherPct.assign(MAX_PLAYERS, 0);
	out.fsAny.assign(MAX_PLAYERS, 0);

	for (int iP = 0; iP < MAX_PLAYERS; ++iP)
	{
		const CvPlayer& owner = GET_PLAYER((PlayerTypes)iP);
		if (!owner.isAlive()) continue;
		const CvTeam* pTeam = &GET_TEAM(owner.getTeam());
		int iLoop;
		for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
		{
			if (pc->area() == NULL || pc->area()->getID() != iAreaId) continue;   // only THIS area's cities
			const OperatingBuildings& operatingBuildings = EnablerKernel::operatingBuildings(pc);
			CvCascadeEvalCtx pec;
			pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = pTeam;
			pec.activeBuildings = &operatingBuildings.active;
			pec.vicinityProvidedBonuses = &operatingBuildings.provided;
			for (std::set<int>::const_iterator it = operatingBuildings.active.begin(); it != operatingBuildings.active.end(); ++it)
			{
				const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
				if (d == NULL) continue;
				out.maintOwnPct[iP]   += MMKernel::sumUnit(d, "maintenance.area", "percent", pec);
				out.maintOtherPct[iP] += MMKernel::sumUnit(d, "maintenance.area.otherArea", "percent", pec);
				sc_fsFold(d, sc_fsSegs().area, pec, 0, out.fsAny[iP], NULL);   // area BY-TYPE has no authorings
			}
		}
	}
}

void CascadeScalarChannels::fillPlayerScalars(const CvPlayer& player, CascadePlayerScope& out)
{
	// PLAYER-BUILDING sums only (each walked per SOURCE city with that city's own ctx -- city-agnostic to
	// the READING city). The civic/trait/tech halves are CITY-REALIZED (the city fills above).
	out.gpModPlayer = 0; out.maintPlayerAll = 0; out.maintConnPct = 0;
	out.maintOtherAreaTotal = 0;   // the per-area halves live on CvArea now (its own package)
	out.tradeEmpireAll = 0; out.tradeCoastalAll = 0; out.tradeWorldMine = 0;
	const CvTeam* pTeam = &GET_TEAM(player.getTeam());
	out.gpModPlayer = sc_playerBuildings("greatPeopleRate.empire", "percent", player, pTeam);
	out.maintPlayerAll = sc_playerBuildings("maintenance.empire", "percent", player, pTeam);
	out.maintConnPct = sc_playerBuildings("maintenance.empire.connectedCity", "percent", player, pTeam);
	sc_maintOtherAreaTotal(player, pTeam, out.maintOtherAreaTotal);
	out.tradeEmpireAll = sc_playerBuildings("tradeRoutes.empire", "flat", player, pTeam);
	out.tradeCoastalAll = sc_playerBuildings("tradeRoutes.empire.coastal", "flat", player, pTeam);
	out.tradeWorldMine = sc_playerBuildings("tradeRoutes.world", "flat", player, pTeam);

	// The L13 defense wiring (2026-07-05): the PLAYER defense percents -- the legacy getCityDefenseModifier
	// three-accumulator class (building allCityDefense + civic extraCityDefense + trait cityDefenseBonus,
	// all curated defense.empire.amount.percent after the same-day re-home from the reader-less
	// combat.empire.cityDefense address). Bare player ctx (unconditioned percents).
	out.defPlayerAll = sc_playerBuildings("defense.empire.amount", "percent", player, pTeam);
	{
		CvCascadeEvalCtx pec;
		pec.player = &player; pec.team = pTeam;
		out.defPlayerAll += (sc_civics("defense.empire.amount", "percent", player, pec) + sc_traits("defense.empire.amount", "percent", player, pec));
		// L13 national BOMBARD modifier (2026-07-05): legacy m_iNationalBombardDefenseModifier -- sole feeder
		// is processTrait (CvPlayer.cpp:28613, trait iBombardDefense), curated defense.empire.bombardDefense
		// .percent (the same-day re-home from the reader-less combat.empire.bombardDefense). Civics author none,
		// so the civicsTraits fold == the trait sum. Composes into the flipped getBuildingBombardDefense.
		out.defPlayerBombard = (sc_civics("defense.empire.bombardDefense", "percent", player, pec) + sc_traits("defense.empire.bombardDefense", "percent", player, pec));
	}

	// The L9 census fold (2026-07-05): PROJECT maintenance -- curated maintenance.empire.{all|connectedCity}
	// .percent (legacy CvTeam::processProjectChange -> player.changeMaintenanceModifier /
	// changeConnectedCityMaintenanceModifier, count-scaled by construction; the distance/numCities members
	// feed the OTHER maintenance pipelines -- their own rows, not this getter). Bare player ctx.
	{
		CvCascadeEvalCtx pec;
		pec.player = &player; pec.team = pTeam;
		for (int i = 0; i < GC.getNumProjectInfos(); ++i)
		{
			const int n = pTeam->getProjectCount((ProjectTypes)i);
			if (n <= 0) continue;
			const CvInfo* d = InfoRepo<CvProjectInfo>::get().get(i);
			if (d == NULL) continue;
			out.maintPlayerAll += n * MMKernel::sumUnit(d, "maintenance.empire.all", "percent", pec);
			out.maintConnPct += n * MMKernel::sumUnit(d, "maintenance.empire.connectedCity", "percent", pec);
		}
	}

	// The freeSpecialists AMOUNT empire/area halves (the ruled seam, 2026-07-05): civics + traits (PURE) +
	// empire-scope buildings; the area.any buildings fold per SOURCE-city area (the maintAreaPct precedent).
	out.fsEmpireAny = 0;
	out.fsEmpireByType.assign(GC.getNumSpecialistInfos(), 0);
	{
		CvCascadeEvalCtx fsec;
		fsec.player = &player; fsec.team = pTeam;
		for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
		{
			const CivicTypes eCivic = player.getCivics((CivicOptionTypes)i);
			if (eCivic == NO_CIVIC) continue;
			sc_fsFold(InfoRepo<CvCivicInfo>::get().get(eCivic), sc_fsSegs().empire, fsec, 0,
				out.fsEmpireAny, &out.fsEmpireByType);
		}
		const bool bPure = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);
		for (int t = 0; t < GC.getNumTraitInfos(); ++t)
		{
			if (!player.hasTrait((TraitTypes)t)) continue;
			const CvTraitInfo* d = MMKernel::traitData(t);
			if (d == NULL) continue;
			sc_fsFold(d, sc_fsSegs().empire, fsec, bPure ? (d->negativeTrait ? -1 : 1) : 0,
				out.fsEmpireAny, &out.fsEmpireByType);
		}
		// empire + area buildings: each source city's active buildings, that city's own ctx
		int iLoop;
		for (const CvCity* pc = player.firstCity(&iLoop); pc != NULL; pc = player.nextCity(&iLoop))
		{
			const OperatingBuildings& operatingBuildings = EnablerKernel::operatingBuildings(pc);
			CvCascadeEvalCtx pec;
			pec.city = pc; pec.plot = pc->plot(); pec.player = &player; pec.team = pTeam;
			pec.activeBuildings = &operatingBuildings.active; pec.vicinityProvidedBonuses = &operatingBuildings.provided;
			for (std::set<int>::const_iterator it = operatingBuildings.active.begin(); it != operatingBuildings.active.end(); ++it)
			{
				const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
				sc_fsFold(d, sc_fsSegs().empire, pec, 0, out.fsEmpireAny, &out.fsEmpireByType);
			}
		}
	}

	// The L6 census fold (2026-07-05): the trait NATIONAL GP flats -- greatPeopleRate.empire.units.{GP}.flat,
	// summed across ALL unit keys (the key only routes which GP pool; the national scalar adds whole). The
	// m_iNationalGreatPeopleRate class: sole legacy feeder is processTrait (writer census 2026-07-05), so the
	// derived sum replaces the accumulator ride-in in the flipped getBaseGreatPeopleRate. City-agnostic ->
	// player scope; bare player ctx; PURE_TRAITS-gated exactly like every trait leg.
	out.gpNationalFlat = 0;
	{
		const int chanId = DepositIndex::lookupSegment("greatPeopleRate");
		const int empireId = DepositIndex::lookupSegment("empire");
		const int unitsId = DepositIndex::lookupSegment("units");
		const int flatId = DepositIndex::lookupSegment("flat");
		if (chanId >= 0 && empireId >= 0 && unitsId >= 0 && flatId >= 0)
		{
			CvCascadeEvalCtx pec;
			pec.player = &player; pec.team = pTeam;
			const bool bPure = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);
			for (int t = 0; t < GC.getNumTraitInfos(); ++t)
			{
				if (!player.hasTrait((TraitTypes)t)) continue;
				const CvTraitInfo* d = MMKernel::traitData(t);
				if (d == NULL) continue;
				const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(d);
				for (size_t i = 0; i < deps.size(); ++i)
				{
					const CascadeDeposit& dep = deps[i];
					if (dep.unitId != flatId) continue;
					if (dep.seg[0] != chanId || dep.seg[1] != empireId) continue;
					// the national rate = trait GP-rate changes: the UNIT-KEYED form
					// (greatPeopleRate.empire.units.{U}.flat, nSeg==4) + the UNTYPED NO_UNIT form
					// (greatPeopleRate.empire.flat, nSeg==3 -- the GP engine pools it; owner 2026-07-05)
					const bool bUnitKeyed = (dep.nSeg == 4 && dep.seg[2] == unitsId);
					const bool bUntyped   = (dep.nSeg == 2);   // address="greatPeopleRate.empire" (unit=flat is separate) -- the NO_UNIT national pool
					if (!bUnitKeyed && !bUntyped) continue;
					const int v = dep.value100 / 100;
					if (v <= 0) continue;   // legacy mirror: the national rate adds ONLY positive GP-rate changes
					if (bPure && (d->negativeTrait ? v > 0 : v < 0)) continue;   // the PureFilter sign rule
					if (!MMKernel::applies(dep.enabled, dep.disabled, pec)) continue;
					out.gpNationalFlat += (int)(MMKernel::perScale(dep, pec, dep.value100) / 100);   // §3.7 per (identity when hasPer==false)
				}
			}
		}
	}
}

// The member-kind segment ids for the ledger routing (append-only interner: a miss re-looks-up; a hit never changes).
struct ScBrMemberSegs { int units, buildings, domains, unitCombats, specialBuildings; };
static const ScBrMemberSegs& sc_brMemberSegs()
{
	static ScBrMemberSegs s = { -1, -1, -1, -1, -1 };
	if (s.units < 0) s.units = DepositIndex::lookupSegment("units");
	if (s.buildings < 0) s.buildings = DepositIndex::lookupSegment("buildings");
	if (s.domains < 0) s.domains = DepositIndex::lookupSegment("domains");
	if (s.unitCombats < 0) s.unitCombats = DepositIndex::lookupSegment("unitCombats");
	if (s.specialBuildings < 0) s.specialBuildings = DepositIndex::lookupSegment("specialBuildings");
	return s;
}

// Size the dense ledger to the live info counts + zero-fill (contract rule 2: fully define every fill).
static void sc_brLedgerReset(CascadeBrLedger& out)
{
	out.units.assign(GC.getNumUnitInfos(), 0);
	out.buildings.assign(GC.getNumBuildingInfos(), 0);
	out.domains.assign(NUM_DOMAIN_TYPES, 0);
	out.unitCombats.assign(GC.getNumUnitCombatInfos(), 0);
	out.specialBuildings.assign(GC.getNumSpecialBuildingInfos(), 0);
}

// The keyed buildRate LEDGER accumulate over one source's deposits at one scope: routed by member kind
// (seg[2]) into the dense per-kind table, indexed by the FK-resolved target id (match-equivalent to the
// former (memberSeg<<20)|keySeg map key -- both derive from the same INFOTYPE string; the precipice-§5
// dense-form fix, generic-code-static-storage on the read path).
static void sc_brLedgerFold(const CvInfo* d, int chanId, int scopeId, int pctId,
	const CvCascadeEvalCtx& ec, int iPureSign, CascadeBrLedger& out)
{
	if (d == NULL) return;
	const ScBrMemberSegs& ms = sc_brMemberSegs();
	const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(d);
	for (size_t i = 0; i < deps.size(); ++i)
	{
		const CascadeDeposit& dep = deps[i];
		if (dep.nSeg != 4 || dep.unitId != pctId) continue;
		if (dep.seg[0] != chanId || dep.seg[1] != scopeId) continue;
		const int v = dep.value100 / 100;
		if (iPureSign > 0 && v < 0) continue;   // PURE_TRAITS: a positive trait keeps only v>=0
		if (iPureSign < 0 && v > 0) continue;   //              a negative trait keeps only v<=0
		if (dep.targetFk < 0) continue;         // a non-resolvable key can never match a live enum's type
		std::vector<int>* pv = NULL;
		if      (dep.seg[2] == ms.units)            pv = &out.units;
		else if (dep.seg[2] == ms.buildings)        pv = &out.buildings;
		else if (dep.seg[2] == ms.domains)          pv = &out.domains;
		else if (dep.seg[2] == ms.unitCombats)      pv = &out.unitCombats;
		else if (dep.seg[2] == ms.specialBuildings) pv = &out.specialBuildings;   // the L11 trait keyed leg
		else continue;
		if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
		if (dep.targetFk < (int)pv->size())
			(*pv)[dep.targetFk] += (int)(MMKernel::perScale(dep, ec, dep.value100) / 100);   // §3.7 per (identity when hasPer==false)
	}
}

void CascadeScalarChannels::fillBuildRateCity(const CvCity* pCity, const CvCascadeEvalCtx& ec,
	CascadeBrLedger& outKeyed, int& outMilitary, int& outSpace, int& outSrUnit, int& outSrBuilding,
	int& outWorldWonder, int& outTeamWonder, int& outNationalWonder)
{
	sc_brLedgerReset(outKeyed);
	const int chanId = DepositIndex::lookupSegment("buildRate");
	const int cityId = DepositIndex::lookupSegment("city");
	const int pctId = DepositIndex::lookupSegment("percent");
	const int empireId = DepositIndex::lookupSegment("empire");
	if (chanId >= 0 && cityId >= 0 && pctId >= 0 && ec.activeBuildings != NULL)
	{
		for (std::set<int>::const_iterator it = ec.activeBuildings->begin(); it != ec.activeBuildings->end(); ++it)
			sc_brLedgerFold(InfoRepo<CvBuildingInfo>::get().get(*it), chanId, cityId, pctId, ec, 0, outKeyed);
	}
	// civics + traits keyed (empire scope, CITY-REALIZED -- conditions evaluate against THIS city)
	if (chanId >= 0 && empireId >= 0 && pctId >= 0)
	{
		const CvPlayer& owner = *ec.player;
		for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
		{
			const CivicTypes eCivic = owner.getCivics((CivicOptionTypes)i);
			if (eCivic == NO_CIVIC) continue;
			sc_brLedgerFold(InfoRepo<CvCivicInfo>::get().get(eCivic), chanId, empireId, pctId, ec, 0, outKeyed);
		}
		const bool bPure = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);
		for (int i = 0; i < GC.getNumTraitInfos(); ++i)
		{
			if (!owner.hasTrait((TraitTypes)i)) continue;
			const CvTraitInfo* d = MMKernel::traitData(i);
			if (d == NULL) continue;
			sc_brLedgerFold(d, chanId, empireId, pctId, ec, bPure ? (d->negativeTrait ? -1 : 1) : 0, outKeyed);
		}
	}
	outMilitary = sc_buildings("buildRate.city.military", "percent", pCity, ec)
	            + (sc_civics("buildRate.empire.military", "percent", *ec.player, ec) + sc_traits("buildRate.empire.military", "percent", *ec.player, ec));
	// The L10 census fold (2026-07-05): CORPORATION military production -- buildRate.city.military.percent
	// on the corp (own-output; the HAS_CORPORATION gate carries the ACTIVE-in-city semantic, matching the
	// legacy applyCorporationModifiers apply; the curator's production.city.military mis-address -- the
	// "Versailles bug" class, consumed by no reader -- was fixed + regenerated the same day).
	for (int c = 0; c < GC.getNumCorporationInfos(); ++c)
	{
		const CvInfo* d = InfoRepo<CvCorporationInfo>::get().get(c);
		if (d != NULL) outMilitary += MMKernel::sumUnit(d, "buildRate.city.military", "percent", ec);
	}
	outSpace = sc_buildings("buildRate.city.space", "percent", pCity, ec)
	         + (sc_civics("buildRate.empire.space", "percent", *ec.player, ec) + sc_traits("buildRate.empire.space", "percent", *ec.player, ec));
	outSrUnit = (sc_civics("stateReligion.empire.unitProduction", "percent", *ec.player, ec) + sc_traits("stateReligion.empire.unitProduction", "percent", *ec.player, ec));
	outSrBuilding = (sc_civics("stateReligion.empire.buildingProduction", "percent", *ec.player, ec) + sc_traits("stateReligion.empire.buildingProduction", "percent", *ec.player, ec));
	// The L11 census fold (2026-07-05): the WONDER-CATEGORY members (trait-authored only in data --
	// worldWonder 77 / teamWonder 53 / nationalWonder 82 curated entries; the legacy maxGlobal/maxTeam/
	// maxPlayer accumulator class). Civic/trait city-realized walks, the military/space idiom.
	outWorldWonder = (sc_civics("buildRate.empire.worldWonder", "percent", *ec.player, ec) + sc_traits("buildRate.empire.worldWonder", "percent", *ec.player, ec));
	outTeamWonder = (sc_civics("buildRate.empire.teamWonder", "percent", *ec.player, ec) + sc_traits("buildRate.empire.teamWonder", "percent", *ec.player, ec));
	outNationalWonder = (sc_civics("buildRate.empire.nationalWonder", "percent", *ec.player, ec) + sc_traits("buildRate.empire.nationalWonder", "percent", *ec.player, ec));
}

void CascadeScalarChannels::fillBuildRatePlayer(const CvPlayer& player, CascadePlayerScope& out)
{
	// BUILDING-sourced halves only (walked per SOURCE city with that city's own ctx); the civic/trait keyed
	// folds + members + the SR fields are CITY-REALIZED (fillBuildRateCity / the city package).
	sc_brLedgerReset(out.brEmpKeyed);
	out.brEmpMilitary = 0; out.brEmpSpace = 0;
	const CvTeam* pTeam = &GET_TEAM(player.getTeam());
	const int chanId = DepositIndex::lookupSegment("buildRate");
	const int empireId = DepositIndex::lookupSegment("empire");
	const int pctId = DepositIndex::lookupSegment("percent");
	if (chanId >= 0 && empireId >= 0 && pctId >= 0)
	{
		// empire-scope keyed: any city's active building feeds the player accumulator
		int iLoop;
		for (const CvCity* pc = player.firstCity(&iLoop); pc != NULL; pc = player.nextCity(&iLoop))
		{
			const OperatingBuildings& operatingBuildings = EnablerKernel::operatingBuildings(pc);
			CvCascadeEvalCtx pec;
			pec.city = pc; pec.plot = pc->plot(); pec.player = &player; pec.team = pTeam;
			pec.activeBuildings = &operatingBuildings.active; pec.vicinityProvidedBonuses = &operatingBuildings.provided;
			for (std::set<int>::const_iterator it = operatingBuildings.active.begin(); it != operatingBuildings.active.end(); ++it)
				sc_brLedgerFold(InfoRepo<CvBuildingInfo>::get().get(*it), chanId, empireId, pctId, pec, 0, out.brEmpKeyed);
		}
	}
	out.brEmpMilitary = sc_playerBuildings("buildRate.empire.military", "percent", player, pTeam);
	out.brEmpSpace = sc_playerBuildings("buildRate.empire.space", "percent", player, pTeam);
}
