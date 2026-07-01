//
//	CommerceCalc -- StoneBase CommerceSplit.cs + CommercePackages.cs (see the header). Ported VERBATIM from
//	CvCascadeModifierMath.cpp's file-static cvCommerce* functions; promoted to a declared surface (the single-source law,
//	patterns.md). LOGIC unchanged: only the signatures + the package-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeCommerceCalc.h"
#include "CvCascadeMMKernel.h"
#include "CvCascadePercentStack.h"     // MMBreak + PercentStack
#include "CvCascadeYieldBasePackages.h"  // specialist / goldenAge (reused §1 packages)
#include "CvCascadeBuildingPackage.h"    // buildingFlat (reused §1 package)
#include "CvJsonInfo.h"                // CvJsonInfo + CvCascadeDeposit + the cascade Json* identity structs
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
#include "AI/CvPlayerAI.h"             // GET_PLAYER
#include "AI/CvTeamAI.h"              // GET_TEAM
#include "CvCascadeEnablerKernel.h"    // EnablerKernel::computeCityBuildingFacts (shrine/stateReligion build their own ctx)
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
// grants commerce to OTHER building TYPES B empire-wide: Σ over the city's ACTIVE buildings B of (Σ over granting
// buildings G of count(G) × G's {ch}.empire.buildings.{B}.flat). Pure deposits (no readJson gap). ×100.
long CommerceCalc::buildingKeyed(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const CvPlayer& player = *ec.player;
	const std::string prefix = channel + ".empire.buildings.";
	const int nB = GC.getNumBuildingInfos();
	// Ledger keyed by TARGET building type-string: Σ_G count(G) × G's {ch}.empire.buildings.{B}.flat (×100).
	std::map<std::string, long> ledger;
	for (int g = 0; g < nB; ++g)
	{
		const int cnt = player.getBuildingCount((BuildingTypes)g);
		if (cnt <= 0) continue;
		const CvJsonInfo* dg = InfoRepo<CvBuildingInfo>::get().get(g);
		if (dg == NULL) continue;
		for (size_t i = 0; i < dg->deposits.size(); ++i)
		{
			const CvCascadeDeposit& dep = dg->deposits[i];
			if (dep.unit != "flat" || dep.address.size() <= prefix.size()) continue;
			if (dep.address.compare(0, prefix.size(), prefix) != 0) continue;
			if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
			ledger[dep.address.substr(prefix.size())] += (long)cnt * dep.value100;   // target = BUILDING_X after the prefix
		}
	}
	if (ledger.empty()) return 0;
	long sum = 0;
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;
		const std::map<std::string, long>::const_iterator it = ledger.find(GC.getBuildingInfo((BuildingTypes)b).getType());
		if (it != ledger.end()) sum += it->second;
	}
	return sum;
}

// §2 BASE: shrine commerce ×100 -- Σ active SHRINE buildings (getGlobalReligionCommerce FK) of religion.shrine.{c} ×
// world religion-levels (ShrinePackage; engine CvCity:12278). ⏳ INTERIM: reads the Info CONFIG (getGlobalReligionCommerce
// + countReligionLevels = config + raw world-count, NOT a computed output); proper home is readJson mapping the religion
// `shrine` + building `identity.shrine` intrinsic blocks (a cleanliness follow-up, not a correctness gap).
long CommerceCalc::shrine(const std::string& channel, const CvCity* pCity)
{
	CvCascadeEvalCtx ec;   // local eval ctx (this package takes no ec) -- compute the active set for the presence test
	ec.city = pCity; ec.player = &GET_PLAYER(pCity->getOwner()); ec.team = &GET_TEAM(GET_PLAYER(pCity->getOwner()).getTeam());
	std::set<int> activeB, provB; EnablerKernel::computeCityBuildingFacts(pCity, ec, activeB, provB);
	ec.activeBuildings = &activeB; ec.vicinityProvidedBonuses = &provB;
	long sum = 0;
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;
		const CvJsonInfo* bd = InfoRepo<CvBuildingInfo>::get().get(b);
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
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;
		const CvJsonInfo* bd = InfoRepo<CvBuildingInfo>::get().get(b);
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
long CommerceCalc::stateReligion(const std::string& channel, const CvCity* pCity)
{
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	const ReligionTypes eState = player.getStateReligion();
	if (eState == NO_RELIGION) return 0;
	CvCascadeEvalCtx ec;   // local eval ctx (this package takes no ec) -- compute the active set for the match test
	ec.city = pCity; ec.player = &player; ec.team = &GET_TEAM(player.getTeam());
	std::set<int> activeB, provB; EnablerKernel::computeCityBuildingFacts(pCity, ec, activeB, provB);
	ec.activeBuildings = &activeB; ec.vicinityProvidedBonuses = &provB;
	const int nB = GC.getNumBuildingInfos();
	int pool = 0;
	for (int b = 0; b < nB; ++b)
	{
		const int cnt = player.getBuildingCount((BuildingTypes)b);
		if (cnt <= 0) continue;
		const CvJsonInfo* bd = InfoRepo<CvBuildingInfo>::get().get(b);
		if (bd == NULL) continue;
		const std::map<std::string, int>& m = static_cast<const CvJsonBuildingInfo*>(bd)->stateReligionCommerce;   // cascade identity (self-contained)
		std::map<std::string, int>::const_iterator it = m.find(channel);
		if (it != m.end() && it->second != 0) pool += cnt * it->second;
	}
	if (pool == 0) return 0;
	int match = 0;
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;
		const CvJsonInfo* bd = InfoRepo<CvBuildingInfo>::get().get(b);
		if (bd != NULL && static_cast<const CvJsonBuildingInfo*>(bd)->religion == (int)eState) ++match;   // cascade identity.religion FK
	}
	return 100L * pool * match;
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
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;
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
	const int revenueMod = GET_TEAM(player.getTeam()).getCorporationRevenueModifier();   // team tech accumulator (raw state)
	const int maintPct = GC.getWorldInfo(GC.getMap().getWorldSize()).getCorporationMaintenancePercent();   // world config
	const std::string wantCity = channel + ".city";
	int total = 0;
	const int nC = GC.getNumCorporationInfos();
	for (int c = 0; c < nC; ++c)
	{
		if (!pCity->isActiveCorporation((CorporationTypes)c)) continue;
		const CvJsonInfo* cd = InfoRepo<CvCorporationInfo>::get().get(c);   // the corp's cascade deposits (self-contained, no engine config)
		if (cd == NULL) continue;
		int iC100 = 0;
		for (size_t i = 0; i < cd->deposits.size(); ++i)
		{
			const CvCascadeDeposit& dep = cd->deposits[i];
			if (dep.unit != "flat" || dep.address != wantCity) continue;
			if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
			if (!dep.perAnyOf.empty())   // CommercesProduced: scaled per prereq-bonus × maintPct (the mapped per:{anyOf} list)
			{
				for (size_t b = 0; b < dep.perAnyOf.size(); ++b)
				{
					const int n = pCity->getNumBonuses((BonusTypes)dep.perAnyOf[b]);
					if (n > 0) iC100 += dep.value100 * n * maintPct / 100;
				}
			}
			else iC100 += dep.value100;   // getCommerceChange (unscaled flat, already ×100)
		}
		total += (MMKernel::modifiedInt(iC100, revenueMod) + 99) / 100;   // revenue mod + engine ceil ÷100, per corp
	}
	return total;
}

// The §2 COMMERCE-SPLIT ASSEMBLER + CombineSplit kernel (CommerceSplit.cs). channel = the commerce-type string (eC index).
long CommerceCalc::commerceRate100(const std::string& channel, CommerceTypes eC, const CvCity* pCity, const CvCascadeEvalCtx& ec,
	long yieldCommerce100, long prodRate)
{
	const CvPlayer& player = *ec.player;
	const long CAP = CITY_MAX_YIELD_RATE;        // the engine #defines (CvCity.h:25-26), NOT GlobalDefines -- getDefineINT returns 0!
	const long CAP100 = CITY_MAX_YIELD_RATE100;

	// HALF 1: the base commerce yield (§1 commerce-yield WITH modifiers) split by the channel slider. yieldCommerce100 +
	// prodRate are precomputed ONCE per city by the caller (the 4 commerce types share them -- avoids 8× redundant §1 computes).
	const int slider = player.getCommercePercent(eC);
	const long splitBase = std::min(CAP100, yieldCommerce100) * slider / 100;

	// HALF 2: the baseExtra100 free-additions (all BASE, × the commerce percent stack). The reused §1 packages are
	// channel-agnostic (called with the commerce-type channel string).
	const int specialist = YieldBasePackages::specialist(channel, pCity, ec);
	const int religion = CommerceCalc::religion(channel, pCity, ec);
	const int goldenAge = YieldBasePackages::goldenAge(channel, player, ec);
	const long buildingOwn100 = BuildingPackage::buildingFlat(channel, pCity, ec);
	const long playerExtra100 = CommerceCalc::playerExtra(channel, player, ec);
	const long buildingKeyed100 = CommerceCalc::buildingKeyed(channel, pCity, ec);
	const long shrine100 = CommerceCalc::shrine(channel, pCity);
	const long corpHQ100 = CommerceCalc::corpHQ(channel, pCity, ec);
	const long stateRel100 = CommerceCalc::stateReligion(channel, pCity);
	const long double100 = CommerceCalc::doubleExtra(channel, pCity, ec);
	const long buildingCommerce100 = buildingOwn100 + buildingKeyed100 + shrine100 + corpHQ100 + stateRel100 + double100;
	const int corporation = CommerceCalc::corporation(channel, pCity, ec);   // separate base bucket (its own ceil ÷100, ×100 here)
	const long baseExtra100 = 100L * specialist + 100L * religion + 100L * corporation + 100L * goldenAge + buildingCommerce100 + playerExtra100;

	MMBreak bk;
	const int totalModifier = PercentStack::percentStack(channel, pCity, bk);   // the commerce percent stack (getTotalCommerceRateModifier)
	const int prodToCommerce = 0;                                    // Process (the lone AFTER) -- pluggable static slot (TODO)

	if (pCity->isDisorder()) return 0;   // civil disorder forces realized commerce to 0 before any combine
	// CombineSplit kernel (CvCity:11969-11996), bit-exact integer math.
	long iRate = splitBase + std::min(CAP100, baseExtra100);
	if (iRate < CAP)
	{
		if (totalModifier != 0) iRate = (iRate > 0) ? iRate * (long)totalModifier / 100 : iRate * 100 / totalModifier;
		iRate += prodRate * prodToCommerce;
	}
	if (iRate < 0 && (channel == "culture" || channel == "research")) return 0;
	if (iRate < MIN_TOL_FALSE_ACCUMULATE) return CAP;   // the very-negative sentinel (CvPlayer.h:49)
	return std::min(CAP, iRate);
}
