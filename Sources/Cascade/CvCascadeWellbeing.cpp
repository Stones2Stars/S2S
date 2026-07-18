//
//	CascadeWellbeing -- the #430 modifier machine WELLBEING channel (modifier.md §2b). The four realized
//	health/happiness verdicts from the curated deposits + raw-state inputs, transcribed from the StoneBase
//	assembler (WellbeingLevels.cs) that reached attributed parity against the legacy engine. The verdict
//	assembly mirrors CvCity::happyLevel (:5709) / unhappyLevel (:5626) / goodHealth (:5851) / badHealth (:5878)
//	term-for-term, substituting each STORED accumulator with the cascade's current-state computed term; the
//	raw-state inputs (anger percents, timers, gate flags, the extra/religion accumulators) stay the same live
//	engine reads the legacy bodies make.
//
//	KNOWN legacy-divergence classes, documented and never chased (modifier.md §2b): the improvement health
//	BALANCE-CUT (the cascade term is deliberately 0) and the STORED-ACCUMULATOR DRIFT (the legacy side's
//	serialized accumulators carry history pollution; the cascade recompute is the correct side).
//

#include "CvGameCoreDLL.h"
#include "Infos/CvWorldInfo.h"
#include "Infos/CvCommerceInfo.h"
#include "CvCascadeWellbeing.h"
#include "CvCascadeMMKernel.h"
#include "CvCascadeDepositIndex.h"
#include "CvEnablerKernel.h"
#include "CvCascadeAccumulator.h"   // the scope-package types ride its ScopePackages include
#include "CvCascadePerfCount.h"      // the wbCompute counter/timer (automation-cost attribution)
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "CvJsonCondition.h"
#include "CvInfo.h"
#include "CvTraitInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvGame.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlot.h"
#include "Engine/CvArea.h"
#include "Engine/CvTeam.h"
#include "AI/CvTeamAI.h"              // GET_TEAM (unity-batch include exposure: own your includes)
#include "Engine/CvGameCoreUtils.h"   // plotCity / range / PUF_canDefend
#include "Infos/CvHandicapInfo.h"
#include "CvTraitInfo.h"
#include "CvTechInfo.h"
#include "CvBuildingInfo.h"
#include "CvBonusInfo.h"
#include "CvCorporationInfo.h"
#include "CvProjectInfo.h"
#include "CvSpecialistInfo.h"
#include "CvFeatureInfo.h"

// (WbSplit + CascadeWbTerms -- the shared §2b term types -- live in CvCascadeScopePackages.h: the same
// structs ARE the city wb packages; this calculator fills fresh instances, the accumulator fills standing
// ones. The area/empire building splits are PLAYER-scope and thread through as assembly parameters.)

// ===================== the condition-shape classifier (the classified fold) =====================
// Each curated entry folds at ITS legacy accumulator's granularity: BONUS-gated -> the bonus accumulators
// (per-contribution split); {TECH_X}-gated -> the tech accumulator (term net); {STATE_RELIGION:X} -> the
// state-religion accumulator (term net); everything else -> the source's own bucket.
enum WbEntryClass { WB_BASE, WB_BONUS_GATED, WB_TECH_GATED, WB_STATE_RELIGION };

static WbEntryClass wb_classify(const CvJsonCondition* c)
{
	if (c == NULL) return WB_BASE;
	switch (c->kind)
	{
	case CASC_COND_PRESENCE:
		if (c->type.compare(0, 6, "BONUS_") == 0) return WB_BONUS_GATED;
		if (c->type.compare(0, 5, "TECH_") == 0) return WB_TECH_GATED;
		return WB_BASE;
	case CASC_COND_PREDICATE:
		return c->predKind == CASC_PRED_STATE_RELIGION ? WB_STATE_RELIGION : WB_BASE;
	case CASC_COND_GROUP:
		if (c->all.size() == 1 && c->anyOf.empty() && c->noneOf.empty()) return wb_classify(c->all[0]);
		return WB_BASE;
	default:
		return WB_BASE;
	}
}

// PURE_TRAITS (StoneBase PureFilter): under the option a negative trait keeps only v<=0, a positive only v>=0.
static bool wb_pureKeep(bool bPure, bool bNegativeTrait, int v)
{
	if (!bPure) return true;
	return bNegativeTrait ? v <= 0 : v >= 0;
}

// ===================== the walks =====================

// ONE building's compiled-record vector folded into both wb families + the commerce-happiness pools (the shared
// inner body): per deposit, dispatch on the family segment. Classified flat folds + the perPopulation pools +
// commerce pers. active buildings pass DepositIndex::depositsFor; obsolete buildings pass whenObsoleteFor
// (json §4.2, part-1 delivery) -- IDENTICAL classification, into the SAME terms.
static void wb_foldBuildingDeposits(const std::vector<CascadeDeposit>& deps,
	int famHappy, int famHealth, int famCH, int scopeCity, int unitFlat, int unitPerPop,
	const int aiChSeg[NUM_COMMERCE_TYPES], const CvCascadeEvalCtx& ec,
	CascadeWbTerms& tHap, CascadeWbTerms& tHea, int aiCommercePer[NUM_COMMERCE_TYPES],
	int& iBaseNetHap, int& iBaseNetHea)
{
	for (size_t i = 0; i < deps.size(); ++i)
	{
		const CascadeDeposit& dep = deps[i];
		if (dep.seg[1] != scopeCity) continue;
		// commerce-happiness: "commerceHappiness.city.<channel>" flat -> the per-channel pool
		if (dep.seg[0] == famCH && dep.nSeg == 3 && dep.unitId == unitFlat)
		{
			for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
				if (dep.seg[2] == aiChSeg[c] && MMKernel::applies(dep.enabled, dep.disabled, ec))
					{ aiCommercePer[c] += (int)(MMKernel::perScale(dep, ec, dep.value100) / 100); break; }   // §3.7 per (identity when hasPer==false)
			continue;
		}
		const bool bHap = dep.seg[0] == famHappy;
		if ((!bHap && dep.seg[0] != famHealth) || dep.nSeg != 2) continue;
		CascadeWbTerms& t = bHap ? tHap : tHea;
		if (dep.unitId == unitPerPop)
		{
			if (MMKernel::applies(dep.enabled, dep.disabled, ec)) t.iPpPct += (int)(MMKernel::perScale(dep, ec, dep.value100) / 100);   // §3.7 per (identity when hasPer==false)
			continue;
		}
		if (dep.unitId != unitFlat) continue;
		if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
		const int v = (int)(MMKernel::perScale(dep, ec, dep.value100) / 100);   // §3.7 per (identity when hasPer==false)
		switch (wb_classify(dep.enabled))
		{
		case WB_BONUS_GATED:    t.bonus.fold(v); break;
		case WB_TECH_GATED:     t.iTechGatedNet += v; break;
		case WB_STATE_RELIGION: t.iSrNet += v; break;
		default:                (bHap ? iBaseNetHap : iBaseNetHea) += v; break;
		}
	}
}

// ONE building pass for BOTH wellbeing families + the commerce-happiness pools (was three separate
// 5202-info loops -- the wbCompute attribution's biggest cut). Active buildings deliver `deposits`; obsolete
// buildings deliver `whenObsolete` (json §4.2) into the SAME terms -- part-1 delivery, the verdict combine unchanged.
static void wb_buildingsAll(int famHappy, int famHealth, int famCH, const CvCity* pCity,
	const CvCascadeEvalCtx& ec, CascadeWbTerms& tHap, CascadeWbTerms& tHea, int aiCommercePer[NUM_COMMERCE_TYPES])
{
	const int scopeCity = DepositIndex::lookupSegment("city");
	const int unitFlat = DepositIndex::lookupSegment("flat");
	const int unitPerPop = DepositIndex::lookupSegment("perPopulation");
	int aiChSeg[NUM_COMMERCE_TYPES];
	{
		static const char* aszC[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c) aiChSeg[c] = DepositIndex::lookupSegment(aszC[c]);
	}
	if (ec.activeBuildings == NULL) return;   // the standing active set IS the walk domain (never all infos)
	for (std::set<int>::const_iterator abIt = ec.activeBuildings->begin(); abIt != ec.activeBuildings->end(); ++abIt)
	{
		const int b = *abIt;
		const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (d == NULL) continue;
		int iBaseNetHap = 0, iBaseNetHea = 0;
		wb_foldBuildingDeposits(DepositIndex::depositsFor(d), famHappy, famHealth, famCH, scopeCity, unitFlat, unitPerPop,
			aiChSeg, ec, tHap, tHea, aiCommercePer, iBaseNetHap, iBaseNetHea);
		tHap.bld.fold(iBaseNetHap);
		tHea.bld.fold(iBaseNetHea);
		// the EVENT-granted per-building ledgers ride the same accumulators (measured zero save-wide)
		const int iLedgerHap = pCity->getBuildingHappyChange((BuildingTypes)b);
		if (iLedgerHap != 0) tHap.bld.fold(iLedgerHap);
		const int iLedgerHea = pCity->getBuildingHealthChange((BuildingTypes)b);
		if (iLedgerHea != 0) tHea.bld.fold(iLedgerHea);
	}
	// obsolete buildings deliver their whenObsolete wb numbers into the SAME terms (json §4.2). No event ledgers --
	// an obsolete building's whenObsolete tree IS its cascade contribution. Inert until the swap cut.
	if (ec.obsoleteBuildings != NULL)
		for (std::set<int>::const_iterator obIt = ec.obsoleteBuildings->begin(); obIt != ec.obsoleteBuildings->end(); ++obIt)
		{
			const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(*obIt);
			if (d == NULL) continue;
			int iBaseNetHap = 0, iBaseNetHea = 0;
			wb_foldBuildingDeposits(DepositIndex::whenObsoleteFor(d), famHappy, famHealth, famCH, scopeCity, unitFlat, unitPerPop,
				aiChSeg, ec, tHap, tHea, aiCommercePer, iBaseNetHap, iBaseNetHea);
			tHap.bld.fold(iBaseNetHap);
			tHea.bld.fold(iBaseNetHea);
		}
}

// ===================== the player-wide AREA/EMPIRE building walk =====================
// The player-wide building fold maps, BOTH families in one player-city walk: area-scope deposits land on
// same-area cities, empire-scope everywhere (the engine's area()/player getBuildingHappiness/Health
// accumulators). This IS the CvPlayer wb package fill; the calculator's compute() calls it fresh.
void CascadeWellbeing::playerAreaEmpire(const CvPlayer& player,
	std::map<int, std::map<int, WbSplit> >& areaByFam, std::map<int, WbSplit>& empireByFam,
	std::map<int, std::map<int, int> >& keyedByFam)
{
	areaByFam.clear();
	empireByFam.clear();
	keyedByFam.clear();
	const int famHappy = DepositIndex::lookupSegment("happiness");
	const int famHealth = DepositIndex::lookupSegment("health");
	const int scopeArea = DepositIndex::lookupSegment("area");
	const int scopeEmpire = DepositIndex::lookupSegment("empire");
	const int unitFlat = DepositIndex::lookupSegment("flat");
	const int segBuildings = DepositIndex::lookupSegment("buildings");
	int iLoop;
	for (const CvCity* pc = player.firstCity(&iLoop); pc != NULL; pc = player.nextCity(&iLoop))
	{
		const OperatingBuildings& operatingBuildings = EnablerKernel::operatingBuildings(pc);
		CvCascadeEvalCtx pec;
		pec.city = pc; pec.plot = pc->plot(); pec.player = &player; pec.team = &GET_TEAM(player.getTeam());
		pec.activeBuildings = &operatingBuildings.active; pec.vicinityProvidedBonuses = &operatingBuildings.provided;
		const int iArea = pc->area()->getID();
		for (std::set<int>::const_iterator it = operatingBuildings.active.begin(); it != operatingBuildings.active.end(); ++it)
		{
			const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
			if (d == NULL) continue;
			const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(d);
			for (size_t i = 0; i < deps.size(); ++i)
			{
				const CascadeDeposit& dep = deps[i];
				if ((dep.seg[0] != famHappy && dep.seg[0] != famHealth) || dep.unitId != unitFlat) continue;
				if (dep.nSeg == 4 && dep.seg[1] == scopeEmpire && dep.seg[2] == segBuildings)
				{
					// keyed by BUILDING (the Royal-Tomb class): +N onto EVERY city holding the keyed
					// building -- ledgered here in the GRANTOR's ctx (the commerce-twin precedent),
					// realized per city by foldBuildingKeyed
					if (dep.targetFk >= 0 && MMKernel::applies(dep.enabled, dep.disabled, pec))
						keyedByFam[dep.seg[0]][dep.targetFk] += (int)(MMKernel::perScale(dep, pec, dep.value100) / 100);   // §3.7 per, resolved at the GRANTOR's ctx (the ledger realization folds plain ints)
					continue;
				}
				if (dep.nSeg != 2) continue;
				if (dep.seg[1] == scopeEmpire)
				{
					if (MMKernel::applies(dep.enabled, dep.disabled, pec)) empireByFam[dep.seg[0]].fold((int)(MMKernel::perScale(dep, pec, dep.value100) / 100));   // §3.7 per (identity when hasPer==false)
				}
				else if (dep.seg[1] == scopeArea)
				{
					if (MMKernel::applies(dep.enabled, dep.disabled, pec)) areaByFam[dep.seg[0]][iArea].fold((int)(MMKernel::perScale(dep, pec, dep.value100) / 100));   // §3.7 per (identity when hasPer==false)
				}
			}
		}
	}
}

void CascadeWellbeing::foldBuildingKeyed(const std::map<int, std::map<int, int> >& keyedByFam,
	const CvCascadeEvalCtx& ec, CascadeWbTerms& hap, CascadeWbTerms& hea)
{
	const int famHappy = DepositIndex::lookupSegment("happiness");
	const int famHealth = DepositIndex::lookupSegment("health");
	for (std::map<int, std::map<int, int> >::const_iterator fit = keyedByFam.begin(); fit != keyedByFam.end(); ++fit)
	{
		if (fit->first != famHappy && fit->first != famHealth) continue;
		CascadeWbTerms& t = (fit->first == famHappy) ? hap : hea;
		for (std::map<int, int>::const_iterator it = fit->second.begin(); it != fit->second.end(); ++it)
		{
			if (cascadeIsBuildingActive(it->first, ec)) t.extraB.fold(it->second);
		}
	}
}

// One civic/trait source's empire members (the MemberSources walk): classified plain flats + buildings.{B} +
// features.{F} + the `unit: IS_MILITARY`-qualified `cities` entries (json §3.7 -- the retired perMilitaryUnit
// member's spec form) + the ranked `cities` member. Trait sources carry the PURE_TRAITS filter.
static void wb_memberSource(int famId, const CvInfo* d, bool bTrait, bool bPure, bool bNegative,
	const CvCity* pCity, const CvCascadeEvalCtx& ec, const std::map<int, int>& featureCounts,
	bool bInTopCities, CascadeWbTerms& t)
{
	const int scopeEmpire = DepositIndex::lookupSegment("empire");
	const int unitFlat = DepositIndex::lookupSegment("flat");
	const int segBuildings = DepositIndex::lookupSegment("buildings");
	const int segFeatures = DepositIndex::lookupSegment("features");
	const int segCities = DepositIndex::lookupSegment("cities");
	int iFlatNet = 0;
	const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(d);
	for (size_t i = 0; i < deps.size(); ++i)
	{
		const CascadeDeposit& dep = deps[i];
		if (dep.seg[0] != famId || dep.seg[1] != scopeEmpire) continue;
		const int v = dep.value100 / 100;   // the AUTHORED value: the pure filter (sign) + the military per-unit fold read it raw
		if (!wb_pureKeep(bPure, bNegative, v)) continue;
		if (dep.nSeg == 2 && dep.unitId == unitFlat)
		{
			if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
			const int vs = (int)(MMKernel::perScale(dep, ec, dep.value100) / 100);   // §3.7 per (identity when hasPer==false)
			if (wb_classify(dep.enabled) == WB_BONUS_GATED) t.bonus.fold(vs);
			else iFlatNet += vs;
		}
		else if (dep.nSeg == 4 && dep.seg[2] == segBuildings)
		{
			// keyed by BUILDING: pays while the keyed building is ACTIVE in this city (the engine's
			// player extraBuildingHappiness/Health per-building accumulators)
			if (dep.targetFk >= 0 && cascadeIsBuildingActive(dep.targetFk, ec)
				&& MMKernel::applies(dep.enabled, dep.disabled, ec))
				t.extraB.fold((int)(MMKernel::perScale(dep, ec, dep.value100) / 100));   // §3.7 per (identity when hasPer==false)
		}
		else if (dep.nSeg == 4 && dep.seg[2] == segFeatures)
		{
			std::map<int, int>::const_iterator fit = dep.targetFk >= 0 ? featureCounts.find(dep.targetFk) : featureCounts.end();
			if (fit != featureCounts.end() && MMKernel::applies(dep.enabled, dep.disabled, ec))
				t.featMember.fold(fit->second * (int)(MMKernel::perScale(dep, ec, dep.value100) / 100));   // §3.7 per (identity when hasPer==false)
		}
		else if (dep.nSeg == 3 && dep.seg[2] == segCities && dep.unitId == unitFlat && dep.unitQual != NULL)
		{
			// the `unit: IS_MILITARY`-qualified `cities` entry (json §3.7): the per-unit VALUE only -- the ×count
			// fold happens LIVE at read, outside every cache ([DEC-unit-modifiers-on-top])
			if (MMKernel::applies(dep.enabled, dep.disabled, ec)) t.iMilitary += v;
		}
		else if (dep.nSeg == 3 && dep.seg[2] == segCities && dep.unitId == unitFlat)
		{
			// the ranked `cities` scaler (unqualified): pays while this city ranks <= the target city count
			if (bInTopCities && MMKernel::applies(dep.enabled, dep.disabled, ec))
				t.iLargest += (int)(MMKernel::perScale(dep, ec, dep.value100) / 100);   // §3.7 per (identity when hasPer==false)
		}
	}
	if (bTrait) t.iTraitNet += iFlatNet; else t.iCivicNet += iFlatNet;
}

// Entity-set empire/city flats, per-entity split fold (bonus/corp/project). Plain sumUnit per entity.
static void wb_entity(const CvInfo* d, const std::string& addr, const CvCascadeEvalCtx& ec, WbSplit& out)
{
	if (d == NULL) return;
	out.fold(MMKernel::sumUnit(d, addr, "flat", ec));
}

// ===================== the per-family gather =====================

static void wb_gather(const char* szFam, const CvCity* pCity, const CvCascadeEvalCtx& ec, CascadeWbTerms& t)
{
	const int famId = DepositIndex::lookupSegment(szFam);
	if (famId < 0) return;   // family never authored
	const std::string fam(szFam);
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const CvTeam& team = GET_TEAM(owner.getTeam());

	// (the player-wide area/empire building splits are PLAYER-scope -- playerAreaEmpire; threaded into the
	// verdict assembly as parameters, never gathered here)

	// -- the shared member-walk inputs --
	std::map<int, int> featureCounts;
	for (int i = 0; i < pCity->getNumCityPlots(); ++i)
	{
		const CvPlot* p = plotCity(pCity->getX(), pCity->getY(), i);
		if (p != NULL && p->getFeatureType() != NO_FEATURE)
		{
			const int fkey = DepositIndex::segIdForFeature(p->getFeatureType());
			if (fkey >= 0) ++featureCounts[fkey];
		}
	}
	const bool bInTopCities = pCity->findPopulationRank() <= GC.getWorldInfo(GC.getMap().getWorldSize()).getTargetNumCities();
	const bool bPure = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);

	// -- civics --
	for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
	{
		const CivicTypes eCivic = owner.getCivics((CivicOptionTypes)i);
		if (eCivic == NO_CIVIC) continue;
		const CvInfo* d = InfoRepo<CvCivicInfo>::get().get(eCivic);
		if (d != NULL) wb_memberSource(famId, d, false, false, false, pCity, ec, featureCounts, bInTopCities, t);
	}
	// -- traits (the option-selected curated set + the PURE_TRAITS filter; NEVER the engine CvTraitInfo) --
	for (int i = 0; i < GC.getNumTraitInfos(); ++i)
	{
		if (!owner.hasTrait((TraitTypes)i)) continue;
		const CvTraitInfo* d = MMKernel::traitData(i);
		if (d != NULL) wb_memberSource(famId, d, true, bPure, d->negativeTrait, pCity, ec, featureCounts, bInTopCities, t);
	}
	// -- bonuses (presence ×1: processBonus is transition-gated -- the falsified ×count is documented) --
	const std::string empAddr = fam + ".empire";
	for (int i = 0; i < GC.getNumBonusInfos(); ++i)
		if (pCity->hasBonus((BonusTypes)i))
			wb_entity(InfoRepo<CvBonusInfo>::get().get(i), empAddr, ec, t.bonus);
	// -- corporations (city scope; the HAS_CORPORATION conditions gate inside) --
	const std::string cityAddr = fam + ".city";
	for (int i = 0; i < GC.getNumCorporationInfos(); ++i)
		if (pCity->isHasCorporation((CorporationTypes)i))
			wb_entity(InfoRepo<CvCorporationInfo>::get().get(i), cityAddr, ec, t.corp);
	// -- techs (team-held empire flats, NET -- they feed the engine's player EXTRA accumulator) --
	for (int i = 0; i < GC.getNumTechInfos(); ++i)
	{
		if (!team.isHasTech((TechTypes)i)) continue;
		const CvInfo* d = InfoRepo<CvTechInfo>::get().get(i);
		if (d != NULL) t.iTechNet += MMKernel::sumUnit(d, empAddr, "flat", ec);
	}
	// -- projects (empire per completed count ×1 presence + the lone world scope) --
	const std::string worldAddr = fam + ".world";
	for (int i = 0; i < GC.getNumProjectInfos(); ++i)
	{
		if (team.getProjectCount((ProjectTypes)i) > 0)
			wb_entity(InfoRepo<CvProjectInfo>::get().get(i), empAddr, ec, t.project);
		if (GC.getGame().getProjectCreatedCount((ProjectTypes)i) > 0)
			wb_entity(InfoRepo<CvProjectInfo>::get().get(i), worldAddr, ec, t.project);
	}
	// -- specialists (city flats ×count, ×100 pools folded /100 per type -- the engine's /100-at-use) --
	for (int i = 0; i < GC.getNumSpecialistInfos(); ++i)
	{
		const int iCount = pCity->getSpecialistCount((SpecialistTypes)i) + pCity->getFreeSpecialistCount((SpecialistTypes)i);
		if (iCount == 0) continue;
		const CvInfo* d = InfoRepo<CvSpecialistInfo>::get().get(i);
		if (d == NULL) continue;
		const long v100 = MMKernel::sumUnit100(d, cityAddr, "flat", ec) * iCount;
		if (v100 >= 0) t.spec.iGood += (int)(v100 / 100); else t.spec.iBad -= (int)(-v100 / 100);
	}
	// -- the feature SUBSTRATE percent (health's fallout class): per radius feature, plot.percent summed
	// -- per feature TYPE then /100 folded (the engine featureGood/Bad split per feature)
	{
		const std::string plotAddr = fam + ".plot";
		std::map<int, int> perFeature;   // featureType -> Σ percent over its radius plots
		for (int i = 0; i < pCity->getNumCityPlots(); ++i)
		{
			const CvPlot* p = plotCity(pCity->getX(), pCity->getY(), i);
			if (p == NULL || p->getFeatureType() == NO_FEATURE) continue;
			const CvInfo* d = InfoRepo<CvFeatureInfo>::get().get(p->getFeatureType());
			if (d == NULL) continue;
			const int pct = MMKernel::sumUnit(d, plotAddr, "percent", ec);
			if (pct != 0) perFeature[(int)p->getFeatureType()] += pct;
		}
		for (std::map<int, int>::const_iterator it = perFeature.begin(); it != perFeature.end(); ++it)
			t.featSubstrate.fold(it->second / 100);
	}
}

// The ENGINE-info trait/tech parts of the player EXTRA accumulators (the exact processTrait :28459 /
// processTech :30912-13 feeds) -- subtracted from the stored inputs so only the event/Python remainder folds.
static void wb_extraParts(const CvPlayer& owner, int& iTraitHappy, int& iTechHappy, int& iTechHealth)
{
	iTraitHappy = 0; iTechHappy = 0; iTechHealth = 0;
	for (int i = 0; i < GC.getNumTraitInfos(); ++i)
		if (owner.hasTrait((TraitTypes)i))
			iTraitHappy += GC.getTraitInfo((TraitTypes)i).getHappiness();
	const CvTeam& team = GET_TEAM(owner.getTeam());
	for (int i = 0; i < GC.getNumTechInfos(); ++i)
		if (team.isHasTech((TechTypes)i))
		{
			iTechHappy += GC.getTechInfo((TechTypes)i).getHappiness();
			iTechHealth += GC.getTechInfo((TechTypes)i).getHealth();
		}
}

// ===================== the city gather (the scope-package fill + the calculator's city half) =====================

void CascadeWellbeing::gatherCityTerms(const CvCity* pCity, const CvCascadeEvalCtx& ec,
	CascadeWbTerms& hap, CascadeWbTerms& hea, int aiCommercePer[])
{
	hap.reset();
	hea.reset();
	// The per-commerce pool SEEDS at the CommerceInfo iInitialHappiness constant ("the base we start from",
	// owner 2026-07-04) -- the culture-slider happiness base (culture=10 -> +1 happy per 10% slider; legacy
	// CvCity::init seeds m_aiCommerceHappinessPer with it). Static system-Info config, not legacy state
	// (the sanctioned config-read class); the building commerceHappiness deposits then add on top.
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		aiCommercePer[c] = GC.getCommerceInfo((CommerceTypes)c).getInitialHappiness();
	// ONE building pass serves both families + the commerce-happiness pools (the wbCompute cost cut)
	wb_buildingsAll(DepositIndex::lookupSegment("happiness"), DepositIndex::lookupSegment("health"),
		DepositIndex::lookupSegment("commerceHappiness"), pCity, ec, hap, hea, aiCommercePer);
	wb_gather("happiness", pCity, ec, hap);
	wb_gather("health", pCity, ec, hea);
}

// ===================== the verdict assembly (the four engine bodies, term-substituted) =====================
// PURE over its inputs + the live raw-state reads -- ONE implementation (patterns.md): the package combine
// feeds standing terms, compute() feeds fresh ones.

CascadeWellbeingVerdicts CascadeWellbeing::assemble(const CvCity* pCity,
	const CascadeWbTerms& hap, const CascadeWbTerms& hea, const int aiCommercePer[],
	const WbSplit& hapArea, const WbSplit& hapEmp, const WbSplit& heaArea, const WbSplit& heaEmp)
{
	CascadeWellbeingVerdicts out;
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const int iPop = pCity->getPopulation();

	int iTraitHappyPart, iTechHappyPart, iTechHealthPart;
	wb_extraParts(owner, iTraitHappyPart, iTechHappyPart, iTechHealthPart);

	// -- shared derived terms --
	const int iPopExtraHappy = iPop * hap.iPpPct / 100;   // truncating (calculatePopulationHappiness)
	const int iPopHealth = iPop * hea.iPpPct / 100;       // truncating (calculatePopulationHealth)
	// RELIGION happiness: per present religion the state/non-state per-religion value, DERIVED (the
	// self-containment extraction 2026-07-05): the legacy player accumulators are exactly INITIAL define +
	// Σ adopted civics' values (writer census: init CvPlayer:391-392 + processCivics :18282-83 + the
	// recalc re-seed :28764; no other feeder) -- never the m_i{State,NonState}ReligionHappiness
	// accumulators, which are demolition fodder, unread by the cascade. Sign-split per religion (updateReligionHappiness).
	WbSplit religion;
	{
		const ReligionTypes eState = owner.getStateReligion();
		int iStateAcc = GC.getINITIAL_STATE_RELIGION_HAPPINESS();
		int iNonStateAcc = GC.getINITIAL_NON_STATE_RELIGION_HAPPINESS();
		CvCascadeEvalCtx rec;   // the civic deposits are unconditioned flats; assemble is pure over inputs
		rec.city = pCity; rec.plot = pCity->plot(); rec.player = &owner; rec.team = &GET_TEAM(owner.getTeam());
		for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
		{
			const CivicTypes eCivic = owner.getCivics((CivicOptionTypes)i);
			if (eCivic == NO_CIVIC) continue;
			const CvInfo* d = InfoRepo<CvCivicInfo>::get().get(eCivic);
			if (d == NULL) continue;
			iStateAcc += MMKernel::sumUnit(d, "stateReligion.empire.happiness", "flat", rec);
			iNonStateAcc += MMKernel::sumUnit(d, "happiness.empire.nonStateReligion", "flat", rec);
		}
		for (int i = 0; i < GC.getNumReligionInfos(); ++i)
			if (pCity->isHasReligion((ReligionTypes)i))
				religion.fold((ReligionTypes)i == eState ? iStateAcc : iNonStateAcc);
	}
	// COMMERCE happiness: per commerce type the building-fed per (pooled by the ONE building pass) × the
	// slider% /100, truncating per type, NET
	int iCommerceHappy = 0;
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		iCommerceHappy += aiCommercePer[c] * owner.getCommercePercent((CommerceTypes)c) / 100;
	// the EXTRA nets: stored inputs − the engine trait/tech parts + the cascade's computed nets
	const int iExtraHappy = pCity->getExtraHappiness() + owner.getExtraHappiness()
		- iTraitHappyPart - iTechHappyPart + hap.iTraitNet + hap.iTechNet;
	const int iExtraHealthCity = pCity->getExtraHealth();
	const int iExtraHealthPlayer = owner.getExtraHealth() - iTechHealthPart + hea.iTechNet;

	// ---------------- happyLevel (:5709), term-substituted ----------------
	{
		int iH = 0;
		iH += std::max(0, pCity->getRevSuccessHappiness());
		iH += std::max(0, hap.iLargest);
		iH += std::max(0, hap.iSrNet);
		iH += hap.bld.iGood;
		iH += hap.extraB.iGood;
		iH += hap.featMember.iGood + hap.featSubstrate.iGood;
		iH += hap.bonus.iGood;
		iH += religion.iGood;
		iH += std::max(0, iCommerceHappy);
		iH += hapArea.iGood;
		iH += hapEmp.iGood;
		iH += std::max(0, iExtraHappy);
		iH += std::max(0, GC.getHandicapInfo(pCity->getHandicapType()).getHappyBonus());
		iH += std::max(0, pCity->getVassalHappiness());
		iH += std::max(0, hap.iCivicNet);
		iH += std::max(0, iPopExtraHappy);
		iH += hap.spec.iGood;
		iH += hap.project.iGood;   // engine world+project split; the cascade folds both project scopes here
		iH += hap.corp.iGood;
		iH += std::max(0, pCity->getCelebrityHappiness());
		iH += std::max(0, hap.iTechGatedNet);
		if (GC.getGame().isOption(GAMEOPTION_MAP_PERSONALIZED))
			iH += std::max(0, owner.getLandmarkHappiness());
		if (pCity->getHappinessTimer() > 0)
			iH += GC.getTEMP_HAPPY();
		out.iHappy = std::max(0, iH);
	}
	// ---------------- unhappyLevel (:5626), term-substituted ----------------
	if (pCity->isNoUnhappiness() || (pCity->isCapital() && owner.isNoCapitalUnhappiness()))
	{
		out.iUnhappy = 0;
	}
	else
	{
		int iAngerPercent = 0;
		iAngerPercent += pCity->getOvercrowdingPercentAnger();
		iAngerPercent += pCity->getNoMilitaryPercentAnger();
		iAngerPercent += pCity->getCulturePercentAnger();
		iAngerPercent += pCity->getReligionPercentAnger();
		iAngerPercent += pCity->getHurryPercentAnger();
		iAngerPercent += pCity->getConscriptPercentAnger();
		iAngerPercent += pCity->getDefyResolutionPercentAnger();
		iAngerPercent += pCity->getWarWearinessPercentAnger();
		iAngerPercent += pCity->getRevRequestPercentAnger();
		iAngerPercent += pCity->getRevIndexPercentAnger();
		for (int i = 0; i < GC.getNumCivicInfos(); ++i)
			iAngerPercent += owner.getCivicPercentAnger((CivicTypes)i);
		int iU = iAngerPercent * iPop / GC.getPERCENT_ANGER_DIVISOR();

		iU -= std::min(0, hap.iLargest);
		iU -= std::min(0, hap.iSrNet);
		iU -= hap.bld.iBad;
		iU -= hap.extraB.iBad;
		iU -= hap.featMember.iBad + hap.featSubstrate.iBad;
		iU -= hap.bonus.iBad;
		iU -= religion.iBad;
		iU -= std::min(0, iCommerceHappy);
		iU -= hapArea.iBad;
		iU -= hapEmp.iBad;
		iU -= std::min(0, iExtraHappy);
		iU -= std::min(0, GC.getHandicapInfo(pCity->getHandicapType()).getHappyBonus());
		iU += std::max(0, pCity->getVassalUnhappiness());
		iU += std::max(0, pCity->getEspionageHappinessCounter());
		iU -= std::min(0, hap.iCivicNet);
		iU -= std::min(0, iPopExtraHappy);
		iU -= hap.spec.iBad;
		iU -= hap.project.iBad;
		iU += std::max(0, owner.calculateTaxRateUnhappiness());
		iU -= hap.corp.iBad;
		iU += std::max(0, pCity->getEventAnger());
		iU -= std::min(0, hap.iTechGatedNet);

		int iForeignAnger = owner.getForeignUnhappyPercent();
		if (iForeignAnger != 0)
		{
			iForeignAnger = 100 / iForeignAnger;
			iForeignAnger = (100 - pCity->plot()->calculateCulturePercent(pCity->getOwner())) * iForeignAnger / 100;
			iU += std::max(0, iForeignAnger);
		}
		if (GC.getGame().isOption(GAMEOPTION_MAP_PERSONALIZED))
		{
			if (!owner.isNoLandmarkAnger()) iU += std::max(0, pCity->getLandmarkAnger());
			iU -= std::min(0, owner.getLandmarkHappiness());
		}
		if (owner.getCityLimit() != 0 && owner.getCityOverLimitUnhappy() != 0)
		{
			const int iOver = owner.getNumCities() - owner.getCityLimit();
			if (iOver > 0) iU += owner.getCityOverLimitUnhappy() * iOver;
		}
		out.iUnhappy = std::max(0, iU);
	}
	out.iMilPerUnit = hap.iMilitary;
	// the angry-pop input to health folds the LIVE military term (the same combine the reads apply on top)
	const int iMilLive = out.iMilPerUnit * pCity->getMilitaryHappinessUnits();
	const int iAngry = range((out.iUnhappy - std::min(0, iMilLive)) - (out.iHappy + std::max(0, iMilLive)), 0, iPop);

	// -- the building-health composites (totalGood/BadBuildingHealth :5827/:5837) --
	const int iTotalGoodBld = hea.bld.iGood + heaArea.iGood + heaEmp.iGood
		+ hea.extraB.iGood + std::max(0, iPopHealth);
	const int iTotalBadBld = pCity->isBuildingOnlyHealthy() ? 0
		: hea.bld.iBad + heaArea.iBad + heaEmp.iBad + hea.extraB.iBad + std::min(0, iPopHealth);

	// ---------------- goodHealth (:5851), term-substituted ----------------
	{
		int iG = 0;
		iG += std::max(0, pCity->getFreshWaterGoodHealth());
		iG += hea.featMember.iGood + hea.featSubstrate.iGood;
		iG += hea.bonus.iGood;
		iG += std::max(0, iTotalGoodBld);
		iG += std::max(0, iExtraHealthCity);
		iG += std::max(0, GC.getHandicapInfo(pCity->getHandicapType()).getHealthBonus());
		// improvement health: the deliberate curator BALANCE-CUT -- the cascade term is 0 by design (§2b)
		iG += hea.spec.iGood;
		iG += hea.corp.iGood;
		iG += std::max(0, hea.iTechGatedNet);
		iG += std::max(0, iExtraHealthPlayer);
		iG += std::max(0, hea.iCivicNet);
		iG += std::max(0, hea.iTraitNet);   // the engine's "civilizationHealth" IS the trait health (:28458)
		iG += hea.project.iGood;            // world+project health
		out.iGood = iG;
	}
	// ---------------- badHealth (:5878), term-substituted ----------------
	{
		int iT = 0;
		iT -= std::max(0, pCity->getEspionageHealthCounter());
		iT += hea.featMember.iBad + hea.featSubstrate.iBad;
		iT += hea.bonus.iBad;
		iT += std::min(0, iTotalBadBld);
		iT += std::min(0, iExtraHealthCity);
		iT += std::min(0, GC.getHandicapInfo(pCity->getHandicapType()).getHealthBonus());
		iT += hea.extraB.iBad;   // the engine's DOUBLE-ADD of min(0, extraBuildingBadHealth) -- mirrored verbatim
		// improvement bad health: BALANCE-CUT (0 by design)
		iT += hea.spec.iBad;
		iT += hea.corp.iBad;
		iT += std::min(0, hea.iTechGatedNet);
		iT += std::min(0, iExtraHealthPlayer);
		iT += std::min(0, hea.iCivicNet);
		iT += std::min(0, hea.iTraitNet);
		iT += hea.project.iBad;
		const int iUnhealthyPop = pCity->isNoUnhealthyPopulation() ? 0 : std::max(0, iPop - iAngry);
		out.iBad = iUnhealthyPop - iT;
	}
	return out;
}

// Per-source terms for the legacy sub-getters -- a live city gather (the verdict is the cached path). Perf is not a
// gate now; UI/AI decomposition reads are cold. The bonus term maps 1:1 to the retired m_iBonus* accumulators
// (sign convention identical: iGood = positives, iBad = negatives).
void CascadeWellbeing::bonusWellbeing(const CvCity* pCity, int& iHapGood, int& iHapBad, int& iHeaGood, int& iHeaBad)
{
	CvCascadeEvalCtx ec;
	CascadeWbTerms hap, hea;
	int aiPer[NUM_COMMERCE_TYPES];
	gatherCityTerms(pCity, ec, hap, hea, aiPer);
	iHapGood = hap.bonus.iGood;
	iHapBad  = hap.bonus.iBad;
	iHeaGood = hea.bonus.iGood;
	iHeaBad  = hea.bonus.iBad;
}

// The CITY building term (hap.bld/hea.bld) for the retired m_iBuildingGood/Bad{Happiness,Health} accumulators.
// Same live-gather shape as bonusWellbeing; the term already folds the event ledger (getBuildingHappy/HealthChange).
void CascadeWellbeing::buildingWellbeing(const CvCity* pCity, int& iHapGood, int& iHapBad, int& iHeaGood, int& iHeaBad)
{
	CvCascadeEvalCtx ec;
	CascadeWbTerms hap, hea;
	int aiPer[NUM_COMMERCE_TYPES];
	gatherCityTerms(pCity, ec, hap, hea, aiPer);
	iHapGood = hap.bld.iGood;
	iHapBad  = hap.bld.iBad;
	iHeaGood = hea.bld.iGood;
	iHeaBad  = hea.bld.iBad;
}

// The /computed decomposition recompute: fresh city gather + fresh player area/empire walk + the ONE assembly.
CascadeWellbeingVerdicts CascadeWellbeing::compute(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	PROFILE_FUNC();
	++CascadePerf::wbCompute;
	PerfAccumTimer perfT(CascadePerf::wbComputeMs);
	CascadeCondScope ccs(CC_WB);   // the condEval caller split
	CascadeWbTerms hap, hea;
	int aiCommercePer[NUM_COMMERCE_TYPES];
	gatherCityTerms(pCity, ec, hap, hea, aiCommercePer);
	std::map<int, std::map<int, WbSplit> > areaByFam;
	std::map<int, WbSplit> empireByFam;
	std::map<int, std::map<int, int> > keyedByFam;
	playerAreaEmpire(GET_PLAYER(pCity->getOwner()), areaByFam, empireByFam, keyedByFam);
	foldBuildingKeyed(keyedByFam, ec, hap, hea);
	const int famHappy = DepositIndex::lookupSegment("happiness");
	const int famHealth = DepositIndex::lookupSegment("health");
	const int iArea = pCity->area()->getID();
	WbSplit hapArea = areaByFam[famHappy][iArea], hapEmp = empireByFam[famHappy];
	WbSplit heaArea = areaByFam[famHealth][iArea], heaEmp = empireByFam[famHealth];
	return assemble(pCity, hap, hea, aiCommercePer, hapArea, hapEmp, heaArea, heaEmp);
}

// compute() is the /computed/cities/wellbeing decomposition path -- the fresh recompute the endpoint emits so a
// wellbeing value attributes to NAMED terms (the live reads ride the accumulator's standing packages).
