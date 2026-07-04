//
//	CascadeScalarChannels -- the #430 city scalar channels (see the header). Per channel: Σ the curated
//	deposits over the live source sets (ACTIVE buildings via the facts cache; adopted civics; held traits with
//	the PURE_TRAITS filter), conditions evaluated against the live ctx.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeScalarChannels.h"
#include "CvCascadeMMKernel.h"
#include "CvCascadeDepositIndex.h"    // the compiled segment ids the keyed walks match on
#include "CvCascadeAccumulator.h"     // CvCascadePlayerStamp -- the shared per-player rollup freshness stamp
#include "CvCascadeEnablerKernel.h"   // cityFacts -- the player-wide maintenance walk
#include "CvCascadeCityFacts.h"
#include "CvJsonInfo.h"
#include "CvJsonTraitInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "Engine/CvCity.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvTechInfo.h"
#include "Infos/CvSpecialistInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvUnitCombatInfo.h"
#include "Infos/CvProjectInfo.h"
#include <map>

// Σ a unit over the city's ACTIVE buildings at an address (the shared building-source walk).
static int sc_buildings(const std::string& addr, const char* unit, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const int nB = GC.getNumBuildingInfos();
	int iSum = 0;
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;
		const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, ec);
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
		const CascadeCityFacts& facts = EnablerKernel::cityFacts(pc);
		CvCascadeEvalCtx pec;
		pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = pTeam;
		pec.activeBuildings = &facts.active; pec.vicinityProvidedBonuses = &facts.provided;
		for (std::set<int>::const_iterator it = facts.active.begin(); it != facts.active.end(); ++it)
		{
			const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
			if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, pec);
		}
	}
	return iSum;
}

// Σ a unit over the player's adopted civics + held traits (pure-filtered) at an address.
static int sc_civicsTraits(const std::string& addr, const char* unit, const CvPlayer& owner, const CvCascadeEvalCtx& ec)
{
	int iSum = 0;
	for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
	{
		const CivicTypes eCivic = owner.getCivics((CivicOptionTypes)i);
		if (eCivic == NO_CIVIC) continue;
		const CvJsonInfo* d = InfoRepo<CvCivicInfo>::get().get(eCivic);
		if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, ec);
	}
	for (int i = 0; i < GC.getNumTraitInfos(); ++i)
	{
		if (!owner.hasTrait((TraitTypes)i)) continue;
		const CvJsonTraitInfo* d = MMKernel::traitData(i);
		if (d != NULL) iSum += MMKernel::sumTrait(d, addr, unit, ec);
	}
	return iSum;
}

// ===================== the per-player SCALAR ROLLUP (increment F) =====================
// The player-wide building walks (gp empire percent, maintenance empire/area/connected, tradeRoutes
// empire/coastal/world flats) are IDENTICAL for every city of the player, so they are CACHED per player on
// the shared CvCascadePlayerStamp (the WbPlayerRollup mechanism, converged per state-repositories.md "one
// pattern everywhere"). The simple sums are cached CALLS to the existing sc_playerBuildings walk (the rollup
// caches results, it never re-implements the walk); only the maintenance AREA split -- which needs per-city
// area grouping sc_playerBuildings cannot express -- walks itself (RELOCATED from maintenanceModifier).
// Tech sums stay in-calculator (cheap, and their eval ctx stays the calling city's).
struct ScPlayerRollup
{
	CvCascadePlayerStamp stamp;
	int iGpEmpirePct;                     // greatPeopleRate.empire percent (player buildings)
	int iTradeEmpireFlat;                 // tradeRoutes.empire flat (player buildings)
	int iTradeCoastalFlat;                // tradeRoutes.empire.coastal flat (player buildings)
	int iTradeWorldFlat;                  // tradeRoutes.world flat (THIS player's buildings; the world term sums the living players')
	int iMaintEmpirePct;                  // maintenance.empire percent (player buildings)
	int iMaintConnPct;                    // maintenance.empire.connectedCity percent (player buildings)
	std::map<int, int> maintAreaPct;      // areaId -> Σ maintenance.area from cities IN that area
	std::map<int, int> maintOtherAreaPct; // areaId -> Σ maintenance.area.otherArea from cities IN that area
	int iMaintOtherAreaTotal;
	ScPlayerRollup() : iGpEmpirePct(0), iTradeEmpireFlat(0), iTradeCoastalFlat(0), iTradeWorldFlat(0),
		iMaintEmpirePct(0), iMaintConnPct(0), iMaintOtherAreaTotal(0) {}
};
static ScPlayerRollup s_scRollup[MAX_PLAYERS];

static const ScPlayerRollup& sc_rollup(const CvPlayer& owner, const CvTeam* pTeam)
{
	static ScPlayerRollup emptyRollup;   // out-of-range owners read zeros
	const PlayerTypes ePlayer = owner.getID();
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS) return emptyRollup;
	ScPlayerRollup& r = s_scRollup[ePlayer];
	if (r.stamp.freshen(ePlayer)) return r;   // payload current for this (player, epoch, turn)
	// STALE -- rebuild the payload (the stamp is already current; a reentrant read sees the fresh sums build)
	r.iGpEmpirePct = sc_playerBuildings("greatPeopleRate.empire", "percent", owner, pTeam);
	r.iTradeEmpireFlat = sc_playerBuildings("tradeRoutes.empire", "flat", owner, pTeam);
	r.iTradeCoastalFlat = sc_playerBuildings("tradeRoutes.empire.coastal", "flat", owner, pTeam);
	r.iTradeWorldFlat = sc_playerBuildings("tradeRoutes.world", "flat", owner, pTeam);
	r.iMaintEmpirePct = sc_playerBuildings("maintenance.empire", "percent", owner, pTeam);
	r.iMaintConnPct = sc_playerBuildings("maintenance.empire.connectedCity", "percent", owner, pTeam);
	// the AREA split (relocated from maintenanceModifier): per-city area grouping of the area/otherArea sums
	r.maintAreaPct.clear();
	r.maintOtherAreaPct.clear();
	r.iMaintOtherAreaTotal = 0;
	{
		int iLoop;
		for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
		{
			const CascadeCityFacts& facts = EnablerKernel::cityFacts(pc);
			CvCascadeEvalCtx pec;
			pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = pTeam;
			pec.activeBuildings = &facts.active; pec.vicinityProvidedBonuses = &facts.provided;
			const int iArea = pc->area()->getID();
			for (std::set<int>::const_iterator it = facts.active.begin(); it != facts.active.end(); ++it)
			{
				const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
				if (d == NULL) continue;
				r.maintAreaPct[iArea] += MMKernel::sumUnit(d, "maintenance.area", "percent", pec);
				const int iOther = MMKernel::sumUnit(d, "maintenance.area.otherArea", "percent", pec);
				if (iOther != 0) { r.maintOtherAreaPct[iArea] += iOther; r.iMaintOtherAreaTotal += iOther; }
			}
		}
	}
	return r;
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
		const CvJsonInfo* d = InfoRepo<CvSpecialistInfo>::get().get(i);
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
	iMod += sc_rollup(owner, ec.team).iGpEmpirePct;   // GLOBAL GP mods feed the player from ANY city (rollup-cached)
	iMod += sc_civicsTraits("greatPeopleRate.city", "percent", owner, ec);
	iMod += sc_civicsTraits("greatPeopleRate.empire", "percent", owner, ec);
	// ...+ the STATE-RELIGION grouped family (civic stateReligion.empire.greatPeopleRate) while the state
	// religion is PRESENT IN THIS CITY (:7160)...
	{
		const ReligionTypes eState = owner.getStateReligion();
		if (eState != NO_RELIGION && pCity->isHasReligion(eState))
			iMod += sc_civicsTraits("stateReligion.empire.greatPeopleRate", "percent", owner, ec);
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
	iCivTrait = sc_rollup(owner, ec.team).iGpEmpirePct;   // seeded with the player-wide building half (rollup-cached)
	iCivTrait += sc_civicsTraits("greatPeopleRate.city", "percent", owner, ec)
	           + sc_civicsTraits("greatPeopleRate.empire", "percent", owner, ec);
	iSr = 0;
	const ReligionTypes eState = owner.getStateReligion();
	if (eState != NO_RELIGION && pCity->isHasReligion(eState))
		iSr = sc_civicsTraits("stateReligion.empire.greatPeopleRate", "percent", owner, ec);
}

int CascadeScalarChannels::defenseAmount(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	// the building city defense stack (legacy m_iBuildingDefense; natural/bonus/player defense are their
	// own legacy terms at the combine -- netted separately)
	return sc_buildings("defense.city.amount", "percent", pCity, ec);
}

int CascadeScalarChannels::tradeRouteCount(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	int iCount = 0;
	// this city's extra routes (building city flats)
	iCount += sc_buildings("tradeRoutes.city", "flat", pCity, ec);
	// the player-wide global routes (any city's buildings' empire flats [rollup-cached] + civics/traits)
	iCount += sc_rollup(owner, ec.team).iTradeEmpireFlat;
	iCount += sc_civicsTraits("tradeRoutes.empire", "flat", owner, ec);
	// TECHS feed the player routes too (processTech :30911)
	{
		const CvTeam& team = GET_TEAM(owner.getTeam());
		for (int i = 0; i < GC.getNumTechInfos(); ++i)
		{
			if (!team.isHasTech((TechTypes)i)) continue;
			const CvJsonInfo* d = InfoRepo<CvTechInfo>::get().get(i);
			if (d != NULL) iCount += MMKernel::sumUnit(d, "tradeRoutes.empire", "flat", ec);
		}
	}
	// WORLD routes: ANY player's active world-wonder grants EVERY player (:7410) -- tradeRoutes.world flats
	// summed over all living players' active sets (each player's own rollup)
	for (int p = 0; p < MAX_PC_PLAYERS; ++p)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)p);
		if (!kP.isAlive()) continue;
		iCount += sc_rollup(kP, &GET_TEAM(kP.getTeam())).iTradeWorldFlat;
	}
	// the coastal half pays only in coastal cities
	if (pCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
	{
		iCount += sc_rollup(owner, ec.team).iTradeCoastalFlat;
		iCount += sc_civicsTraits("tradeRoutes.empire.coastal", "flat", owner, ec);
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
		const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (d != NULL) iSum += MMKernel::sumKeyed4U(d, chanId, cityId, memberId, keyId, pctId, ec, false);
	}
	// the player-wide EMPIRE-scope keyed member (any city's building feeds the player accumulator)
	{
		int iLoop;
		for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
		{
			const CascadeCityFacts& facts = EnablerKernel::cityFacts(pc);
			CvCascadeEvalCtx pec;
			pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = ec.team;
			pec.activeBuildings = &facts.active; pec.vicinityProvidedBonuses = &facts.provided;
			for (std::set<int>::const_iterator it = facts.active.begin(); it != facts.active.end(); ++it)
			{
				const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
				if (d != NULL) iSum += MMKernel::sumKeyed4U(d, chanId, empireId, memberId, keyId, pctId, pec, false);
			}
		}
	}
	// civics + traits (empire scope); traits carry the PURE_TRAITS alignment filter
	for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
	{
		const CivicTypes eCivic = owner.getCivics((CivicOptionTypes)i);
		if (eCivic == NO_CIVIC) continue;
		const CvJsonInfo* d = InfoRepo<CvCivicInfo>::get().get(eCivic);
		if (d != NULL) iSum += MMKernel::sumKeyed4U(d, chanId, empireId, memberId, keyId, pctId, ec, false);
	}
	const bool bPure = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);
	for (int i = 0; i < GC.getNumTraitInfos(); ++i)
	{
		if (!owner.hasTrait((TraitTypes)i)) continue;
		const CvJsonTraitInfo* d = MMKernel::traitData(i);
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
	iSum += sc_civicsTraits(empAddr, "percent", owner, ec);
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
		const CvJsonInfo* d = InfoRepo<CvUnitInfo>::get().get(eUnit);
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
				parts.iStateReligion = sc_civicsTraits("stateReligion.empire.unitProduction", "percent", owner, ec);
		}
	}
	else if (eB != NO_BUILDING)
	{
		const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(eB);
		if (d != NULL) parts.iSelf = MMKernel::sumUnit(d, "buildRate.self", "percent", ec);
		parts.iKeyed = sc_buildRateKeyed("buildings", GC.getBuildingInfo(eB).getType(), pCity, ec);
		if (bSrInCity)
			parts.iStateReligion = sc_civicsTraits("stateReligion.empire.buildingProduction", "percent", owner, ec);
	}
	else if (ePr != NO_PROJECT)
	{
		const CvJsonInfo* d = InfoRepo<CvProjectInfo>::get().get(ePr);
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
	int iMod = 0;
	iMod += sc_buildings("maintenance.city", "percent", pCity, ec);
	iMod += sc_civicsTraits("maintenance.city", "percent", owner, ec);
	iMod += sc_civicsTraits("maintenance.empire", "percent", owner, ec);
	iMod += sc_civicsTraits("maintenance.area", "percent", owner, ec);
	// TECHS feed the player maintenance modifier too (processTech CvPlayer:30916)
	{
		const CvTeam& team = GET_TEAM(owner.getTeam());
		for (int i = 0; i < GC.getNumTechInfos(); ++i)
		{
			if (!team.isHasTech((TechTypes)i)) continue;
			const CvJsonInfo* d = InfoRepo<CvTechInfo>::get().get(i);
			if (d != NULL) iMod += MMKernel::sumUnit(d, "maintenance.empire", "percent", ec);
		}
	}
	// the player-wide building halves, rollup-cached (the walk RELOCATED into sc_rollup): the empire percent;
	// the AREA split -- a city in area A collects A's own area sums + every OTHER area's otherArea sums; and
	// the connected-to-capital percent when this city is connected (and not the capital)
	{
		const ScPlayerRollup& r = sc_rollup(owner, ec.team);
		iMod += r.iMaintEmpirePct;
		const int iArea = pCity->area()->getID();
		std::map<int, int>::const_iterator ait = r.maintAreaPct.find(iArea);
		if (ait != r.maintAreaPct.end()) iMod += ait->second;
		std::map<int, int>::const_iterator oit = r.maintOtherAreaPct.find(iArea);
		iMod += r.iMaintOtherAreaTotal - (oit != r.maintOtherAreaPct.end() ? oit->second : 0);
		if (pCity->isConnectedToCapital() && !pCity->isCapital()) iMod += r.iMaintConnPct;
	}
	return iMod;
}
