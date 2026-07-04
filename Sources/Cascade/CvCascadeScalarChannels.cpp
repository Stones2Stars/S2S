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
			const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
			if (d != NULL) iSum += MMKernel::sumUnit(d, addr, unit, ec);
		}
		return iSum;
	}
	const int nB = GC.getNumBuildingInfos();   // unwired-ctx fallback (correctness identical)
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

// ===================== the maintenance AREA split (one walk, fill + oracle) =====================
// Per-city area grouping of the area/otherArea building percents (the one player-wide sum
// sc_playerBuildings cannot express). Single-source: the CvPlayer package fill AND the oracle both call it.
static void sc_maintAreaSplit(const CvPlayer& owner, const CvTeam* pTeam,
	std::map<int, int>& areaPct, std::map<int, int>& otherAreaPct, int& iOtherTotal)
{
	areaPct.clear();
	otherAreaPct.clear();
	iOtherTotal = 0;
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
			areaPct[iArea] += MMKernel::sumUnit(d, "maintenance.area", "percent", pec);
			const int iOther = MMKernel::sumUnit(d, "maintenance.area.otherArea", "percent", pec);
			if (iOther != 0) { otherAreaPct[iArea] += iOther; iOtherTotal += iOther; }
		}
	}
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
	iMod += sc_playerBuildings("greatPeopleRate.empire", "percent", owner, ec.team);   // GLOBAL GP mods feed the player from ANY city
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
	iCivTrait = sc_playerBuildings("greatPeopleRate.empire", "percent", owner, ec.team);   // the player-wide building half
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
	// the player-wide global routes (any city's buildings' empire flats + civics/traits)
	iCount += sc_playerBuildings("tradeRoutes.empire", "flat", owner, ec.team);
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
	     + sc_civicsTraits("greatPeopleRate.city", "percent", owner, ec)
	     + sc_civicsTraits("greatPeopleRate.empire", "percent", owner, ec);
}

int CascadeScalarChannels::gpModifierSrCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	return sc_civicsTraits("stateReligion.empire.greatPeopleRate", "percent", *ec.player, ec);
}

int CascadeScalarChannels::maintenanceModifierCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = *ec.player;
	int iMod = sc_buildings("maintenance.city", "percent", pCity, ec)
	         + sc_civicsTraits("maintenance.city", "percent", owner, ec)
	         + sc_civicsTraits("maintenance.empire", "percent", owner, ec)
	         + sc_civicsTraits("maintenance.area", "percent", owner, ec);
	const CvTeam& team = GET_TEAM(owner.getTeam());
	for (int i = 0; i < GC.getNumTechInfos(); ++i)
	{
		if (!team.isHasTech((TechTypes)i)) continue;
		const CvJsonInfo* d = InfoRepo<CvTechInfo>::get().get(i);
		if (d != NULL) iMod += MMKernel::sumUnit(d, "maintenance.empire", "percent", ec);
	}
	return iMod;
}

int CascadeScalarChannels::tradeRoutesCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& owner = *ec.player;
	int iCount = sc_buildings("tradeRoutes.city", "flat", pCity, ec)
	           + sc_civicsTraits("tradeRoutes.empire", "flat", owner, ec);
	const CvTeam& team = GET_TEAM(owner.getTeam());
	for (int i = 0; i < GC.getNumTechInfos(); ++i)
	{
		if (!team.isHasTech((TechTypes)i)) continue;
		const CvJsonInfo* d = InfoRepo<CvTechInfo>::get().get(i);
		if (d != NULL) iCount += MMKernel::sumUnit(d, "tradeRoutes.empire", "flat", ec);
	}
	return iCount;
}

int CascadeScalarChannels::tradeRoutesCoastalCivCity(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	return sc_civicsTraits("tradeRoutes.empire.coastal", "flat", *ec.player, ec);
}

void CascadeScalarChannels::fillPlayerScalars(const CvPlayer& player, CascadePlayerScope& out)
{
	// PLAYER-BUILDING sums only (each walked per SOURCE city with that city's own ctx -- city-agnostic to
	// the READING city). The civic/trait/tech halves are CITY-REALIZED (the city fills above).
	out.gpModPlayer = 0; out.maintPlayerAll = 0; out.maintConnPct = 0;
	out.maintAreaPct.clear(); out.maintOtherAreaPct.clear(); out.maintOtherAreaTotal = 0;
	out.tradeEmpireAll = 0; out.tradeCoastalAll = 0; out.tradeWorldMine = 0;
	const CvTeam* pTeam = &GET_TEAM(player.getTeam());
	out.gpModPlayer = sc_playerBuildings("greatPeopleRate.empire", "percent", player, pTeam);
	out.maintPlayerAll = sc_playerBuildings("maintenance.empire", "percent", player, pTeam);
	out.maintConnPct = sc_playerBuildings("maintenance.empire.connectedCity", "percent", player, pTeam);
	sc_maintAreaSplit(player, pTeam, out.maintAreaPct, out.maintOtherAreaPct, out.maintOtherAreaTotal);
	out.tradeEmpireAll = sc_playerBuildings("tradeRoutes.empire", "flat", player, pTeam);
	out.tradeCoastalAll = sc_playerBuildings("tradeRoutes.empire.coastal", "flat", player, pTeam);
	out.tradeWorldMine = sc_playerBuildings("tradeRoutes.world", "flat", player, pTeam);
}

// The keyed buildRate LEDGER accumulate over one source's deposits at one scope: (memberSeg<<20)|keySeg -> Σ.
static void sc_brLedgerFold(const CvJsonInfo* d, int chanId, int scopeId, int pctId,
	const CvCascadeEvalCtx& ec, int iPureSign, std::map<long, int>& out)
{
	if (d == NULL) return;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.nSeg != 4 || dep.unitId != pctId) continue;
		if (dep.seg[0] != chanId || dep.seg[1] != scopeId) continue;
		const int v = dep.value100 / 100;
		if (iPureSign > 0 && v < 0) continue;   // PURE_TRAITS: a positive trait keeps only v>=0
		if (iPureSign < 0 && v > 0) continue;   //              a negative trait keeps only v<=0
		if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
		out[((long)dep.seg[2] << 20) | dep.seg[3]] += v;
	}
}

void CascadeScalarChannels::fillBuildRateCity(const CvCity* pCity, const CvCascadeEvalCtx& ec,
	std::map<long, int>& outKeyed, int& outMilitary, int& outSpace, int& outSrUnit, int& outSrBuilding)
{
	outKeyed.clear();
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
			const CvJsonTraitInfo* d = MMKernel::traitData(i);
			if (d == NULL) continue;
			sc_brLedgerFold(d, chanId, empireId, pctId, ec, bPure ? (d->negativeTrait ? -1 : 1) : 0, outKeyed);
		}
	}
	outMilitary = sc_buildings("buildRate.city.military", "percent", pCity, ec)
	            + sc_civicsTraits("buildRate.empire.military", "percent", *ec.player, ec);
	outSpace = sc_buildings("buildRate.city.space", "percent", pCity, ec)
	         + sc_civicsTraits("buildRate.empire.space", "percent", *ec.player, ec);
	outSrUnit = sc_civicsTraits("stateReligion.empire.unitProduction", "percent", *ec.player, ec);
	outSrBuilding = sc_civicsTraits("stateReligion.empire.buildingProduction", "percent", *ec.player, ec);
}

void CascadeScalarChannels::fillBuildRatePlayer(const CvPlayer& player, CascadePlayerScope& out)
{
	// BUILDING-sourced halves only (walked per SOURCE city with that city's own ctx); the civic/trait keyed
	// folds + members + the SR fields are CITY-REALIZED (fillBuildRateCity / the city package).
	out.brEmpKeyed.clear();
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
			const CascadeCityFacts& facts = EnablerKernel::cityFacts(pc);
			CvCascadeEvalCtx pec;
			pec.city = pc; pec.plot = pc->plot(); pec.player = &player; pec.team = pTeam;
			pec.activeBuildings = &facts.active; pec.vicinityProvidedBonuses = &facts.provided;
			for (std::set<int>::const_iterator it = facts.active.begin(); it != facts.active.end(); ++it)
				sc_brLedgerFold(InfoRepo<CvBuildingInfo>::get().get(*it), chanId, empireId, pctId, pec, 0, out.brEmpKeyed);
		}
	}
	out.brEmpMilitary = sc_playerBuildings("buildRate.empire.military", "percent", player, pTeam);
	out.brEmpSpace = sc_playerBuildings("buildRate.empire.space", "percent", player, pTeam);
}
