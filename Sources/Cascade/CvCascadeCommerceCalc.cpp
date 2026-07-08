//
//	CommerceCalc -- StoneBase CommerceSplit.cs + CommercePackages.cs (see the header). Ported VERBATIM from
//	CvCascadeModifierMath.cpp's file-static cvCommerce* functions; promoted to a declared surface (the single-source law,
//	patterns.md). LOGIC unchanged: only the signatures + the package-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeCommerceCalc.h"
#include "CvCascadeMMKernel.h"
#include "CvCascadeBuildingPackage.h"    // buildingFlat (reused §1 package)
#include "CvJsonInfo.h"                // CvJsonInfo + the cascade Json* identity structs
#include "CvJsonCorporationInfo.h"     // the corp typed members (change/produced/prereqBonuses -- the collapsed corp families)
#include "Repos/InfoRepo.h"            // InfoRepo<CvXInfo>::get().get(id)
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvGame.h"            // GC.getGame().countReligionLevels / countCorporationLevels / getGameTurnYear (§2)
#include "Engine/CvMap.h"             // GC.getMap().getWorldSize (the §2 corp maintenance percent)
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvReligionInfo.h"     // InfoRepo<CvReligionInfo> + getGlobalReligionCommerce (the §2 religion/shrine packages)
#include "Infos/CvCorporationInfo.h"  // getHeadquarterCommerce (the §2 corp-HQ package)
#include "Infos/CvHeritageInfo.h"     // InfoRepo<CvHeritageInfo> (the §2 player-extra heritage commerce)
#include "Infos/CvWorldInfo.h"        // getCorporationMaintenancePercent (the §2 corporation package)
#include "Infos/CvProcessInfo.h"      // InfoRepo<CvProcessInfo> -- the L2 process production->commerce fold
#include "AI/CvPlayerAI.h"             // GET_PLAYER
#include "AI/CvTeamAI.h"              // GET_TEAM
#include "CvCascadeCapabilities.h"     // corporationRevenueModifier -- the derived-from-tech read (never the legacy accumulator)
#include "CvCascadeEnablerKernel.h"    // EnablerKernel::wireOperatingBuildings (shrine/stateReligion build their own ctx over the standing operating buildings)
#include "CvCascadeDepositIndex.h"     // DepositIndex -- the compiled deposit index (buildingKeyed matches ints)
#include <map>
#include <set>

static const char* CMC_CHANNELS[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

const char* CommerceCalc::channel(int eC) { return CMC_CHANNELS[eC]; }

// §2 BASE: religion commerce -- Σ the city's PRESENT religions' {ch}.city.flat (StateReligion/HolyCity tables, gated). x1.
int CommerceCalc::religion(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const std::string wantCity = channel + ".city";
	int sum = 0;
	for (int r = 0; r < GC.getNumReligionInfos(); ++r)
	{
		if (!pCity->isHasReligion((ReligionTypes)r)) continue;
		const CvJsonInfo* d = InfoRepo<CvReligionInfo>::get().get(r);
		if (d != NULL) sum += MMKernel::sumUnit(d, wantCity, "flat", ec);
	}
	return sum;
}

// §2 BASE: player-extra commerce ×100 -- trait CommerceChanges + heritage EraCommerceChanges, both {ch}.empire.flat (the
// heritage era-counter gate `enabled:{ERA,min}` is evaluated by MMKernel::applies). (Traits: option-gated active set +
// PURE_TRAITS via sumTrait100/traitData.)
long CommerceCalc::playerExtra(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec)
{
	const std::string wantEmpire = channel + ".empire";
	long sum = 0;
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!player.hasTrait((TraitTypes)t)) continue;
		sum += MMKernel::sumTrait100(MMKernel::traitData(t), wantEmpire, "flat", ec);
	}
	for (int h = 0; h < GC.getNumHeritageInfos(); ++h)
	{
		if (!player.hasHeritage((HeritageTypes)h)) continue;
		const CvJsonInfo* d = InfoRepo<CvHeritageInfo>::get().get(h);
		if (d != NULL) sum += MMKernel::sumUnit100(d, wantEmpire, "flat", ec);
	}
	return sum;
}

// §2 BASE: building-keyed commerce ×100 (GlobalBuildingExtraCommerces, BuildingKeyedCommercePackage) -- a building G
// grants commerce to OTHER building TYPES B empire-wide. This is the GRANTOR LEDGER (player-scope): targetFk -> Σ over
// granting buildings G of count(G) × G's {ch}.empire.buildings.{B}.flat (×100); the accumulator realizes it against the
// city's ACTIVE targets at read. Pure deposits (no readJson gap).
void CommerceCalc::buildingKeyedLedger(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec,
	std::map<int, long>& out)
{
	// Compiled-index matching (the deposit-index increment): the grantor scan matches ints, and the ledger keys
	// on the deposit's FK-resolved target id -- no string build/compare anywhere on this path.
	out.clear();
	const int chanId = DepositIndex::lookupSegment(channel);
	if (chanId < 0) return;
	const int segEmpire = DepositIndex::lookupSegment("empire");
	const int segBuildings = DepositIndex::lookupSegment("buildings");
	const int segFlat = DepositIndex::lookupSegment("flat");
	if (segEmpire < 0 || segBuildings < 0 || segFlat < 0) return;
	const int nB = GC.getNumBuildingInfos();
	for (int g = 0; g < nB; ++g)
	{
		const int cnt = player.getBuildingCount((BuildingTypes)g);
		if (cnt <= 0) continue;
		const CvJsonInfo* dg = InfoRepo<CvBuildingInfo>::get().get(g);
		if (dg == NULL) continue;
		const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(dg);
		for (size_t i = 0; i < deps.size(); ++i)
		{
			const CascadeDeposit& dep = deps[i];
			if (dep.unitId != segFlat || dep.nSeg != 4) continue;
			if (dep.seg[0] != chanId || dep.seg[1] != segEmpire || dep.seg[2] != segBuildings) continue;
			if (dep.targetFk < 0) continue;
			if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
			// §3.7 per (identity when hasPer==false), resolved at FILL: the realization (the accumulator's
			// CBASE keyed sum) folds plain ledgered longs -- no deposit exists there -- and this player-scope
			// fill's ctx is the SAME one the enabled/disabled gates already evaluate against.
			out[dep.targetFk] += (long)cnt * MMKernel::perScale(dep, ec, dep.value100);
		}
	}
}

// §2 BASE: shrine commerce ×100 -- Σ active SHRINE buildings (getGlobalReligionCommerce FK) of religion.shrine.{c} ×
// world religion-levels (ShrinePackage; engine CvCity:12278). ⏳ INTERIM: reads the Info CONFIG (getGlobalReligionCommerce
// + countReligionLevels = config + raw world-count, NOT a computed output); proper home is readJson mapping the religion
// `shrine` + building `identity.shrine` intrinsic blocks (a cleanliness follow-up, not a correctness gap).
long CommerceCalc::shrine(const std::string& channel, const CvCity* pCity)
{
	CvCascadeEvalCtx ec;   // local eval ctx (this package takes no ec) -- the standing operating buildings serve the presence test
	ec.city = pCity; ec.plot = pCity->plot(); ec.player = &GET_PLAYER(pCity->getOwner()); ec.team = &GET_TEAM(GET_PLAYER(pCity->getOwner()).getTeam());
	EnablerKernel::wireOperatingBuildings(pCity, ec);
	long sum = 0;
	if (ec.activeBuildings == NULL) return 0;   // the standing active set IS the walk domain (never all infos)
	for (std::set<int>::const_iterator abIt = ec.activeBuildings->begin(); abIt != ec.activeBuildings->end(); ++abIt)
	{
		const CvJsonInfo* bd = InfoRepo<CvBuildingInfo>::get().get(*abIt);
		if (bd == NULL) continue;
		const int rel = static_cast<const CvJsonBuildingInfo*>(bd)->shrineReligion;   // cascade identity.shrine FK (self-contained)
		if (rel < 0) continue;
		const CvJsonInfo* rd = InfoRepo<CvReligionInfo>::get().get(rel);
		if (rd == NULL) continue;
		const std::map<std::string, int>& sc = static_cast<const CvJsonReligionInfo*>(rd)->shrineCommerce;
		std::map<std::string, int>::const_iterator it = sc.find(channel);
		if (it != sc.end() && it->second != 0) sum += (long)it->second * GC.getGame().countReligionLevels((ReligionTypes)rel) * 100;
	}
	return sum;
}

// §2 BASE: corp-HQ commerce ×100 -- Σ active corp-HQ buildings (getGlobalCorporationCommerce FK) of corp.headquarters.{c}
// × world corp-levels (CorpHQPackage; engine CvCity:12286). ⏳ INTERIM config read (see shrine).
long CommerceCalc::corpHQ(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const std::string wantHQ = channel + ".empire.headquarters";
	long sum = 0;
	if (ec.activeBuildings == NULL) return 0;
	for (std::set<int>::const_iterator abIt = ec.activeBuildings->begin(); abIt != ec.activeBuildings->end(); ++abIt)
	{
		const CvJsonInfo* bd = InfoRepo<CvBuildingInfo>::get().get(*abIt);
		if (bd == NULL) continue;
		const int corp = static_cast<const CvJsonBuildingInfo*>(bd)->corpHQ;   // cascade identity.corporationHQ FK (self-contained)
		if (corp < 0) continue;
		const CvJsonInfo* cd = InfoRepo<CvCorporationInfo>::get().get(corp);
		if (cd == NULL) continue;
		const int per = MMKernel::sumUnit(cd, wantHQ, "perCorporationLevel", ec);      // the corp's headquarters deposit (cascade)
		if (per != 0) sum += (long)per * GC.getGame().countCorporationLevels((CorporationTypes)corp) * 100;
	}
	return sum;
}

// §2 BASE: state-religion commerce ×100 -- 100 × POOL × matchCount (StateReligionPackage; engine CvCity:12266-73). POOL =
// Σ player building TYPES of count × building.getStateReligionCommerce(c); matchCount = the city's active buildings whose
// religion == the owner's state religion. COMPUTED from building counts + config (NOT the engine pool accumulator).
long CommerceCalc::stateReligionPool(const std::string& channel, const CvPlayer& player)
{
	if (player.getStateReligion() == NO_RELIGION) return 0;
	const int nB = GC.getNumBuildingInfos();
	long pool = 0;
	for (int b = 0; b < nB; ++b)
	{
		const int cnt = player.getBuildingCount((BuildingTypes)b);
		if (cnt <= 0) continue;
		const CvJsonInfo* bd = InfoRepo<CvBuildingInfo>::get().get(b);
		if (bd == NULL) continue;
		const std::map<std::string, int>& m = static_cast<const CvJsonBuildingInfo*>(bd)->stateReligionCommerce;   // cascade identity (self-contained)
		std::map<std::string, int>::const_iterator it = m.find(channel);
		if (it != m.end() && it->second != 0) pool += (long)cnt * it->second;
	}
	return pool;
}

int CommerceCalc::stateReligionMatch(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const ReligionTypes eState = ec.player->getStateReligion();
	if (eState == NO_RELIGION || ec.activeBuildings == NULL) return 0;
	int match = 0;
	for (std::set<int>::const_iterator abIt = ec.activeBuildings->begin(); abIt != ec.activeBuildings->end(); ++abIt)
	{
		const CvJsonInfo* bd = InfoRepo<CvBuildingInfo>::get().get(*abIt);
		if (bd != NULL && static_cast<const CvJsonBuildingInfo*>(bd)->religion == (int)eState) ++match;   // cascade identity.religion FK
	}
	return match;
}

long CommerceCalc::stateReligion(const std::string& channel, const CvCity* pCity)
{
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	if (player.getStateReligion() == NO_RELIGION) return 0;
	CvCascadeEvalCtx ec;   // local eval ctx (this package takes no ec) -- the standing operating buildings serve the match test
	ec.city = pCity; ec.plot = pCity->plot(); ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
	EnablerKernel::wireOperatingBuildings(pCity, ec);
	const long pool = stateReligionPool(channel, player);
	if (pool == 0) return 0;
	return 100L * pool * stateReligionMatch(pCity, ec);
}

// §2 BASE: CommerceChangeDoubleTime whole-building doubling ×100 -- for each active building older than its double-time
// threshold (game-years), ANOTHER copy of its WHOLE per-building commerce (own un-conditioned city.flat + shrine + corpHQ)
// (DoubleExtraPackage; engine CvCity:12290). ⏳ INTERIM config read; own-flat via the un-conditioned deposit (×100, integer).
long CommerceCalc::doubleExtra(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const long year = GC.getGame().getGameTurnYear();
	const std::string wantCity = channel + ".city";
	const std::string wantHQ = channel + ".empire.headquarters";
	long extra = 0;
	if (ec.activeBuildings == NULL) return 0;
	for (std::set<int>::const_iterator abIt = ec.activeBuildings->begin(); abIt != ec.activeBuildings->end(); ++abIt)
	{
		const int b = *abIt;
		const CvJsonInfo* bd = InfoRepo<CvBuildingInfo>::get().get(b);
		if (bd == NULL) continue;
		const CvJsonBuildingInfo* bj = static_cast<const CvJsonBuildingInfo*>(bd);
		std::map<std::string, int>::const_iterator dt = bj->commerceDoubleTime.find(channel);   // cascade identity.commerceDoubleTime
		if (dt == bj->commerceDoubleTime.end() || dt->second <= 0) continue;
		const int built = pCity->getBuildingData((BuildingTypes)b).iTimeBuilt;
		if (built == MIN_INT || (year - (long)built) < dt->second) continue;   // unbuilt/unknown never doubles
		extra += (long)MMKernel::sumUnconditioned(bd, wantCity, "flat") * 100;        // own un-conditioned (×100; integer)
		const int rel = bj->shrineReligion;                                    // its shrine (cascade)
		if (rel >= 0)
		{
			const CvJsonInfo* rd = InfoRepo<CvReligionInfo>::get().get(rel);
			if (rd != NULL)
			{
				const std::map<std::string, int>& sc = static_cast<const CvJsonReligionInfo*>(rd)->shrineCommerce;
				std::map<std::string, int>::const_iterator it = sc.find(channel);
				if (it != sc.end()) extra += (long)it->second * GC.getGame().countReligionLevels((ReligionTypes)rel) * 100;
			}
		}
		const int corp = bj->corpHQ;                                           // its corp-HQ (cascade)
		if (corp >= 0)
		{
			const CvJsonInfo* cd = InfoRepo<CvCorporationInfo>::get().get(corp);
			if (cd != NULL) extra += (long)MMKernel::sumUnit(cd, wantHQ, "perCorporationLevel", ec) * GC.getGame().countCorporationLevels((CorporationTypes)corp) * 100;
		}
	}
	return extra;
}

// §2 BASE: corporation commerce -- Σ active corps' getCorporationCommerceByCorporation (engine CvCity:12752): per corp,
// iC = getCommerceChange(c)×100 + Σ prereq-bonus(getCommerceProduced(c) × city bonus-count × worldCorpMaintPct/100),
// team-revenue-modified, ceil(÷100). Returns human (the §2 bucket ×100s it). ⏳ INTERIM config read (corp config + bonus
// counts + isActiveCorporation -- config + raw state, not a computed output).
int CommerceCalc::corporation(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	if (pCity->isDisorder()) return 0;
	const CvPlayer& player = *ec.player;
	// DERIVED from held techs (the ruled self-containment fix, cutover.md Rulings #4) -- the legacy team
	// ACCUMULATOR is a computed output and dies at the cut; the derived sum rides CascadeTeamCaps' freshness.
	const int revenueMod = CascadeCapabilities::corporationRevenueModifier(player.getTeam());
	const int maintPct = GC.getWorldInfo(GC.getMap().getWorldSize()).getCorporationMaintenancePercent();   // world config
	// channel -> the CommerceTypes index (the collapsed corp members are commerce-enum-keyed)
	int eC = -1;
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
		if (channel == CMC_CHANNELS[i]) { eC = i; break; }
	if (eC < 0) return 0;
	int total = 0;
	const int nC = GC.getNumCorporationInfos();
	for (int c = 0; c < nC; ++c)
	{
		if (!pCity->isActiveCorporation((CorporationTypes)c)) continue;
		const CvJsonInfo* cd = InfoRepo<CvCorporationInfo>::get().get(c);   // the corp's cascade data (self-contained, no engine config)
		if (cd == NULL) continue;
		// The corp families are COLLAPSED to typed members (CvJsonCorporationInfo: the uniform HAS_CORPORATION gate --
		// carried by the isActiveCorporation walk above -- + the ONE shared per:{anyOf} prereq set; CvJsonModEntry.h:
		// "do NOT wrap those"): CHANGE = the un-per'd flat (×1, so ×100 here = the old entry's value100);
		// PRODUCED = the per-scaled base (already ×100), applied per prereq-bonus × city count × maintPct --
		// the identical (value, bonus) terms as the retired per-deposit walk.
		const CvJsonCorporationInfo* corp = static_cast<const CvJsonCorporationInfo*>(cd);
		int iC100 = 100 * corp->getCommerceChange(eC);
		const int produced = corp->getCommerceProduced(eC);
		if (produced != 0)
		{
			for (int b = 0; b < corp->getNumPrereqBonuses(); ++b)
			{
				const int n = pCity->getNumBonuses((BonusTypes)corp->getPrereqBonus(b));
				if (n > 0) iC100 += produced * n * maintPct / 100;
			}
		}
		total += (MMKernel::modifiedInt(iC100, revenueMod) + 99) / 100;   // revenue mod + engine ceil ÷100, per corp
	}
	return total;
}

// The CITY-ONLY base terms ×100 (the scope-package fill): religion + corporation + building-own + shrine +
// corpHQ + doubleTime -- everything of the HALF-2 base-extra sum that is THIS city's own scope (the
// player-scope goldenAge/playerExtra and the keyed/SR realizations are separate scope packages).
long CommerceCalc::baseOwn100(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const int religion = CommerceCalc::religion(channel, pCity, ec);
	const long buildingOwn100 = BuildingPackage::buildingFlat(channel, pCity, ec);
	const long shrine100 = CommerceCalc::shrine(channel, pCity);
	const long corpHQ100 = CommerceCalc::corpHQ(channel, pCity, ec);
	const long double100 = CommerceCalc::doubleExtra(channel, pCity, ec);
	const int corporation = CommerceCalc::corporation(channel, pCity, ec);   // separate base bucket (its own ceil ÷100, ×100 here)
	return 100L * religion + 100L * corporation + buildingOwn100 + shrine100 + corpHQ100 + double100;
}

// The CombineSplit KERNEL (CvCity:11969-11996), bit-exact integer math. Slider + disorder read LIVE -- they
// need no invalidation anywhere (the accumulator's read-time combine relies on that).
long CommerceCalc::combineSplit(CommerceTypes eC, const CvCity* pCity, long yieldCommerce100, long prodRate,
	long lBaseExtra100, int iTotalModifier)
{
	const long CAP = CITY_MAX_YIELD_RATE;        // the engine #defines (CvCity.h:25-26), NOT GlobalDefines -- getDefineINT returns 0!
	const long CAP100 = CITY_MAX_YIELD_RATE100;
	if (pCity->isDisorder()) return 0;   // civil disorder forces realized commerce to 0 before any combine
	const int slider = GET_PLAYER(pCity->getOwner()).getCommercePercent(eC);
	const long splitBase = std::min(CAP100, yieldCommerce100) * slider / 100;
	// The L2 census fold (2026-07-05): the PROCESS production->commerce conversion, LIVE at combine (the
	// slider class -- the head order is volatile raw state, the value is static process config; zero
	// invalidation; NEVER the legacy m_aiProductionToCommerceModifier accumulator, whose sole feeder is
	// processProcess on head-order changes). Curated shape: {ch}.city.percent ON the process entity --
	// only this process-aware read consumes process deposits (the percent stack never walks processes).
	int prodToCommerce = 0;
	{
		const ProcessTypes eProcess = pCity->getProductionProcess();
		if (eProcess != NO_PROCESS)
		{
			const CvJsonInfo* pd = InfoRepo<CvProcessInfo>::get().get((int)eProcess);
			if (pd != NULL)
			{
				const CvPlayer& kOwner = GET_PLAYER(pCity->getOwner());
				CvCascadeEvalCtx pec;
				pec.city = pCity; pec.plot = pCity->plot(); pec.player = &kOwner; pec.team = &GET_TEAM(kOwner.getTeam());
				prodToCommerce = MMKernel::sumUnit(pd, std::string(CommerceCalc::channel(eC)) + ".city", "percent", pec);
			}
		}
	}
	long iRate = splitBase + std::min(CAP100, lBaseExtra100);
	if (iRate < CAP)
	{
		if (iTotalModifier != 0) iRate = (iRate > 0) ? iRate * (long)iTotalModifier / 100 : iRate * 100 / iTotalModifier;
		iRate += prodRate * prodToCommerce;
	}
	if (iRate < 0 && (eC == COMMERCE_CULTURE || eC == COMMERCE_RESEARCH)) return 0;
	if (iRate < MIN_TOL_FALSE_ACCUMULATE) return CAP;   // the very-negative sentinel (CvPlayer.h:49)
	return std::min(CAP, iRate);
}
