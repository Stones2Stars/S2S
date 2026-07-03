//
//	CascadeWellbeing -- the #430 modifier machine WELLBEING channel (modifier.md §2b). The four realized
//	health/happiness verdicts from the curated deposits + raw-state inputs, transcribed from the StoneBase
//	assembler (WellbeingLevels.cs) that reached attributed parity against the legacy engine. The verdict
//	assembly mirrors CvCity::happyLevel (:5709) / unhappyLevel (:5626) / goodHealth (:5851) / badHealth (:5878)
//	term-for-term, substituting each STORED accumulator with the cascade's current-state computed term; the
//	raw-state inputs (anger percents, timers, gate flags, the extra/religion accumulators) stay the same live
//	engine reads the legacy bodies make.
//
//	KNOWN legacy-divergence classes the shadow shows and never chases (modifier.md §2b): the improvement health
//	BALANCE-CUT (the cascade term is deliberately 0) and the STORED-ACCUMULATOR DRIFT (the legacy side's
//	serialized accumulators carry history pollution; the cascade recompute is the correct side).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeWellbeing.h"
#include "CvCascadeMMKernel.h"
#include "CvCascadeDepositIndex.h"
#include "CvCascadeEnablerKernel.h"
#include "CvCascadeAccumulator.h"   // epochFor -- the per-player rollup cache stamp
#include "CvCascadePerfCount.h"      // the wbCompute counter/timer (automation-cost attribution)
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "CvCascadeCondition.h"
#include "CvJsonInfo.h"
#include "CvJsonTraitInfo.h"
#include "Repos/InfoRepo.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvGame.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlot.h"
#include "Engine/CvArea.h"
#include "Engine/CvTeam.h"
#include "Engine/CvGameCoreUtils.h"   // plotCity / range / PUF_canDefend
#include "Infos/CvHandicapInfo.h"
#include "Infos/CvTraitInfo.h"
#include "Infos/CvTechInfo.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvBonusInfo.h"
#include "Infos/CvCorporationInfo.h"
#include "Infos/CvProjectInfo.h"
#include "Infos/CvSpecialistInfo.h"
#include "Infos/CvFeatureInfo.h"

// ===================== the signed-split pair (StoneBase Split) =====================
// Good = Σ max(0, instance), Bad = Σ min(0, instance) -- the §2b per-source-instance sign split.
struct WbSplit
{
	int iGood, iBad;
	WbSplit() : iGood(0), iBad(0) {}
	void fold(int v) { if (v >= 0) iGood += v; else iBad += v; }
};

// The per-family term set one compute pass gathers (happiness and health run the same walks).
struct WbTerms
{
	WbSplit bld;          // building city base (per-building NET fold) + the event ledger
	WbSplit areaBld;      // player buildings' area-scope deposits (this area)
	WbSplit empBld;       // player buildings' empire-scope deposits
	WbSplit bonus;        // bonus own empire flats + building/civic/trait BONUS-gated entries (per-contribution)
	WbSplit extraB;       // civic/trait buildings.{B} keyed (per-entry fold)
	WbSplit featMember;   // civic/trait features.{F} keyed × radius feature count
	WbSplit featSubstrate;// feature-info plot.percent per radius feature, per-feature /100 fold (health only in data)
	WbSplit corp;         // corporation city flats (per-corp fold)
	WbSplit project;      // project empire + world flats (per-project fold)
	WbSplit spec;         // specialist city flats ×count (×100 pool folded /100 per type)
	int iCivicNet;        // civic plain empire flats, NET (the engine's getCivicHappiness/CivicHealth term shape)
	int iTraitNet;        // trait plain empire flats, NET (feeds extra [happiness] / civilization [health])
	int iTechNet;         // tech empire flats, NET (feeds extra)
	int iSrNet;           // building {STATE_RELIGION:X}-gated, NET across buildings (m_paiStateReligionHappiness)
	int iTechGatedNet;    // building {TECH_X}-gated, NET (m_iExtraBuilding*FromTech)
	int iMilitary;        // civic/trait perMilitaryUnit × military units, NET term
	int iLargest;         // civic/trait ranked `cities` member (rank <= target), NET term
	int iPpPct;           // perPopulation percent pool (raw percent-of-pop; ×pop/100 at use)
	WbTerms() : iCivicNet(0), iTraitNet(0), iTechNet(0), iSrNet(0), iTechGatedNet(0),
		iMilitary(0), iLargest(0), iPpPct(0) {}
};

// ===================== the condition-shape classifier (the classified fold) =====================
// Each curated entry folds at ITS legacy accumulator's granularity: BONUS-gated -> the bonus accumulators
// (per-contribution split); {TECH_X}-gated -> the tech accumulator (term net); {STATE_RELIGION:X} -> the
// state-religion accumulator (term net); everything else -> the source's own bucket.
enum WbEntryClass { WB_BASE, WB_BONUS_GATED, WB_TECH_GATED, WB_STATE_RELIGION };

static WbEntryClass wb_classify(const CvCascadeCondition* c)
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

// ONE building pass for BOTH wellbeing families + the commerce-happiness pools (was three separate
// 5202-info loops -- the wbCompute attribution's biggest cut): per ACTIVE building, per deposit, dispatch on
// the family segment. Classified flat folds + the perPopulation pools + the event ledgers + commerce pers.
static void wb_buildingsAll(int famHappy, int famHealth, int famCH, const CvCity* pCity,
	const CvCascadeEvalCtx& ec, WbTerms& tHap, WbTerms& tHea, int aiCommercePer[NUM_COMMERCE_TYPES])
{
	const int scopeCity = DepositIndex::lookupSegment("city");
	const int unitFlat = DepositIndex::lookupSegment("flat");
	const int unitPerPop = DepositIndex::lookupSegment("perPopulation");
	int aiChSeg[NUM_COMMERCE_TYPES];
	{
		static const char* aszC[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c) aiChSeg[c] = DepositIndex::lookupSegment(aszC[c]);
	}
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		if (!cascadeIsBuildingActive(b, ec)) continue;
		const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
		if (d == NULL) continue;
		int iBaseNetHap = 0, iBaseNetHea = 0;
		for (size_t i = 0; i < d->deposits.size(); ++i)
		{
			const CvCascadeDeposit& dep = d->deposits[i];
			if (dep.seg[1] != scopeCity) continue;
			// commerce-happiness: "commerceHappiness.city.<channel>" flat -> the per-channel pool
			if (dep.seg[0] == famCH && dep.nSeg == 3 && dep.unitId == unitFlat)
			{
				for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
					if (dep.seg[2] == aiChSeg[c] && MMKernel::applies(dep.enabled, dep.disabled, ec))
						{ aiCommercePer[c] += dep.value100 / 100; break; }
				continue;
			}
			const bool bHap = dep.seg[0] == famHappy;
			if ((!bHap && dep.seg[0] != famHealth) || dep.nSeg != 2) continue;
			WbTerms& t = bHap ? tHap : tHea;
			if (dep.unitId == unitPerPop)
			{
				if (MMKernel::applies(dep.enabled, dep.disabled, ec)) t.iPpPct += dep.value100 / 100;
				continue;
			}
			if (dep.unitId != unitFlat) continue;
			if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
			const int v = dep.value100 / 100;
			switch (wb_classify(dep.enabled))
			{
			case WB_BONUS_GATED:    t.bonus.fold(v); break;
			case WB_TECH_GATED:     t.iTechGatedNet += v; break;
			case WB_STATE_RELIGION: t.iSrNet += v; break;
			default:                (bHap ? iBaseNetHap : iBaseNetHea) += v; break;
			}
		}
		tHap.bld.fold(iBaseNetHap);
		tHea.bld.fold(iBaseNetHea);
		// the EVENT-granted per-building ledgers ride the same accumulators (measured zero save-wide)
		const int iLedgerHap = pCity->getBuildingHappyChange((BuildingTypes)b);
		if (iLedgerHap != 0) tHap.bld.fold(iLedgerHap);
		const int iLedgerHea = pCity->getBuildingHealthChange((BuildingTypes)b);
		if (iLedgerHea != 0) tHea.bld.fold(iLedgerHea);
	}
}

// ===================== the per-player AREA/EMPIRE rollup cache =====================
// The player-wide building rollup is IDENTICAL for every city of the player (per area) -- computing it per
// city made the WB compute O(player-cities × buildings) per call (the measured automation collapse). It is
// cached PER PLAYER at the END-TURN cadence (the owner's wellbeing ruling): stamped by (epoch, turn),
// rebuilt once, shared by all the player's cities; mid-turn building completions self-heal next turn.
struct WbPlayerRollup
{
	int iEpoch, iTurn;
	// famId -> (areaId -> area Split; empire Split) for the two wellbeing families
	std::map<int, std::map<int, WbSplit> > areaByFam;
	std::map<int, WbSplit> empireByFam;
	WbPlayerRollup() : iEpoch(-1), iTurn(-1) {}
};
static WbPlayerRollup s_wbRollup[MAX_PLAYERS];

static void wb_rollupRebuild(int famId, PlayerTypes ePlayer, WbPlayerRollup& r);

// The player-wide building walk, CACHED: area-scope deposits land on same-area cities, empire-scope everywhere
// (the engine's area()/player getBuildingHappiness/Health accumulators). Per-building split fold.
static void wb_playerBuildings(int famId, const CvCity* pCity, WbTerms& t)
{
	const PlayerTypes eOwner = pCity->getOwner();
	if (eOwner >= 0 && eOwner < MAX_PLAYERS)
	{
		WbPlayerRollup& r = s_wbRollup[eOwner];
		const int iEpoch = CascadeAccumulator::epochFor(eOwner);
		const int iTurn = GC.getGame().getGameTurn();
		if (r.iEpoch != iEpoch || r.iTurn != iTurn)
		{
			r.iEpoch = iEpoch; r.iTurn = iTurn;
			r.areaByFam.clear(); r.empireByFam.clear();
		}
		if (r.areaByFam.find(famId) == r.areaByFam.end())
			wb_rollupRebuild(famId, eOwner, r);
		const std::map<int, WbSplit>& areas = r.areaByFam[famId];
		std::map<int, WbSplit>::const_iterator ait = areas.find(pCity->area()->getID());
		if (ait != areas.end()) { t.areaBld.iGood += ait->second.iGood; t.areaBld.iBad += ait->second.iBad; }
		const WbSplit& emp = r.empireByFam[famId];
		t.empBld.iGood += emp.iGood; t.empBld.iBad += emp.iBad;
		return;
	}
	// (no valid owner -- nothing to fold)
}

// One rebuild: loop the player's cities ONCE, folding each active building's area/empire deposits.
static void wb_rollupRebuild(int famId, PlayerTypes ePlayer, WbPlayerRollup& r)
{
	const int scopeArea = DepositIndex::lookupSegment("area");
	const int scopeEmpire = DepositIndex::lookupSegment("empire");
	const int unitFlat = DepositIndex::lookupSegment("flat");
	const CvPlayer& owner = GET_PLAYER(ePlayer);
	std::map<int, WbSplit>& areas = r.areaByFam[famId];   // creates the fam entry (the rebuilt marker)
	WbSplit& emp = r.empireByFam[famId];
	int iLoop;
	for (const CvCity* pc = owner.firstCity(&iLoop); pc != NULL; pc = owner.nextCity(&iLoop))
	{
		const CascadeCityFacts& facts = EnablerKernel::cityFacts(pc);
		CvCascadeEvalCtx pec;
		pec.city = pc; pec.plot = pc->plot(); pec.player = &owner; pec.team = &GET_TEAM(owner.getTeam());
		pec.activeBuildings = &facts.active; pec.vicinityProvidedBonuses = &facts.provided;
		WbSplit& area = areas[pc->area()->getID()];
		for (std::set<int>::const_iterator it = facts.active.begin(); it != facts.active.end(); ++it)
		{
			const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(*it);
			if (d == NULL) continue;
			for (size_t i = 0; i < d->deposits.size(); ++i)
			{
				const CvCascadeDeposit& dep = d->deposits[i];
				if (dep.seg[0] != famId || dep.nSeg != 2 || dep.unitId != unitFlat) continue;
				if (dep.seg[1] == scopeEmpire)
				{
					if (MMKernel::applies(dep.enabled, dep.disabled, pec)) emp.fold(dep.value100 / 100);
				}
				else if (dep.seg[1] == scopeArea)
				{
					if (MMKernel::applies(dep.enabled, dep.disabled, pec)) area.fold(dep.value100 / 100);
				}
			}
		}
	}
}

// One civic/trait source's empire members (the MemberSources walk): classified plain flats + buildings.{B} +
// features.{F} + perMilitaryUnit + the ranked `cities` member. Trait sources carry the PURE_TRAITS filter.
static void wb_memberSource(int famId, const CvJsonInfo* d, bool bTrait, bool bPure, bool bNegative,
	const CvCity* pCity, const CvCascadeEvalCtx& ec, const std::map<int, int>& featureCounts,
	bool bInTopCities, WbTerms& t)
{
	const int scopeEmpire = DepositIndex::lookupSegment("empire");
	const int unitFlat = DepositIndex::lookupSegment("flat");
	const int unitPerMil = DepositIndex::lookupSegment("perMilitaryUnit");
	const int segBuildings = DepositIndex::lookupSegment("buildings");
	const int segFeatures = DepositIndex::lookupSegment("features");
	const int segPerMil = DepositIndex::lookupSegment("perMilitaryUnit");
	const int segCities = DepositIndex::lookupSegment("cities");
	int iFlatNet = 0;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.seg[0] != famId || dep.seg[1] != scopeEmpire) continue;
		const int v = dep.value100 / 100;
		if (!wb_pureKeep(bPure, bNegative, v)) continue;
		if (dep.nSeg == 2 && dep.unitId == unitFlat)
		{
			if (!MMKernel::applies(dep.enabled, dep.disabled, ec)) continue;
			if (wb_classify(dep.enabled) == WB_BONUS_GATED) t.bonus.fold(v);
			else iFlatNet += v;
		}
		else if (dep.nSeg == 4 && dep.seg[2] == segBuildings)
		{
			// keyed by BUILDING: pays while the keyed building is ACTIVE in this city (the engine's
			// player extraBuildingHappiness/Health per-building accumulators)
			if (dep.targetFk >= 0 && cascadeIsBuildingActive(dep.targetFk, ec)
				&& MMKernel::applies(dep.enabled, dep.disabled, ec))
				t.extraB.fold(v);
		}
		else if (dep.nSeg == 4 && dep.seg[2] == segFeatures)
		{
			std::map<int, int>::const_iterator fit = dep.targetFk >= 0 ? featureCounts.find(dep.targetFk) : featureCounts.end();
			if (fit != featureCounts.end() && MMKernel::applies(dep.enabled, dep.disabled, ec))
				t.featMember.fold(fit->second * v);
		}
		else if (dep.seg[2] == segPerMil || dep.unitId == unitPerMil)
		{
			// authored doubly nested (happiness.empire.perMilitaryUnit, unit perMilitaryUnit) -- the per-unit
			// VALUE only (owner ruling: the ×count fold happens LIVE at read, outside every cache)
			if (MMKernel::applies(dep.enabled, dep.disabled, ec)) t.iMilitary += v;
		}
		else if (dep.nSeg == 3 && dep.seg[2] == segCities && dep.unitId == unitFlat)
		{
			// the ranked `cities` scaler: pays while this city ranks <= the target city count
			if (bInTopCities && MMKernel::applies(dep.enabled, dep.disabled, ec)) t.iLargest += v;
		}
	}
	if (bTrait) t.iTraitNet += iFlatNet; else t.iCivicNet += iFlatNet;
}

// Entity-set empire/city flats, per-entity split fold (bonus/corp/project). Plain sumUnit per entity.
static void wb_entity(const CvJsonInfo* d, const std::string& addr, const CvCascadeEvalCtx& ec, WbSplit& out)
{
	if (d == NULL) return;
	out.fold(MMKernel::sumUnit(d, addr, "flat", ec));
}

// ===================== the per-family gather =====================

static void wb_gather(const char* szFam, const CvCity* pCity, const CvCascadeEvalCtx& ec, WbTerms& t)
{
	const int famId = DepositIndex::lookupSegment(szFam);
	if (famId < 0) return;   // family never authored
	const std::string fam(szFam);
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const CvTeam& team = GET_TEAM(owner.getTeam());

	// -- the player-wide area/empire rollups (the CITY building pass runs ONCE for both families in
	// -- compute() via wb_buildingsAll -- not here) --
	wb_playerBuildings(famId, pCity, t);

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
		const CvJsonInfo* d = InfoRepo<CvCivicInfo>::get().get(eCivic);
		if (d != NULL) wb_memberSource(famId, d, false, false, false, pCity, ec, featureCounts, bInTopCities, t);
	}
	// -- traits (the option-selected curated set + the PURE_TRAITS filter; NEVER the engine CvTraitInfo) --
	for (int i = 0; i < GC.getNumTraitInfos(); ++i)
	{
		if (!owner.hasTrait((TraitTypes)i)) continue;
		const CvJsonTraitInfo* d = MMKernel::traitData(i);
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
		const CvJsonInfo* d = InfoRepo<CvTechInfo>::get().get(i);
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
		const CvJsonInfo* d = InfoRepo<CvSpecialistInfo>::get().get(i);
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
			const CvJsonInfo* d = InfoRepo<CvFeatureInfo>::get().get(p->getFeatureType());
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

// ===================== the verdict assembly (the four engine bodies, term-substituted) =====================

CascadeWellbeingVerdicts CascadeWellbeing::compute(const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	PROFILE_FUNC();
	++CascadePerf::wbCompute;
	PerfAccumTimer perfT(CascadePerf::wbComputeMs);
	CascadeWellbeingVerdicts out;
	const CvPlayer& owner = GET_PLAYER(pCity->getOwner());
	const int iPop = pCity->getPopulation();

	WbTerms hap, hea;
	int aiCommercePer[NUM_COMMERCE_TYPES] = { 0, 0, 0, 0 };
	// ONE building pass serves both families + the commerce-happiness pools (the wbCompute cost cut)
	wb_buildingsAll(DepositIndex::lookupSegment("happiness"), DepositIndex::lookupSegment("health"),
		DepositIndex::lookupSegment("commerceHappiness"), pCity, ec, hap, hea, aiCommercePer);
	wb_gather("happiness", pCity, ec, hap);
	wb_gather("health", pCity, ec, hea);

	int iTraitHappyPart, iTechHappyPart, iTechHealthPart;
	wb_extraParts(owner, iTraitHappyPart, iTechHappyPart, iTechHealthPart);

	// -- shared derived terms --
	const int iPopExtraHappy = iPop * hap.iPpPct / 100;   // truncating (calculatePopulationHappiness)
	const int iPopHealth = iPop * hea.iPpPct / 100;       // truncating (calculatePopulationHealth)
	// RELIGION happiness: per present religion the player's state/non-state accumulator (INPUTS -- the
	// mixed-accumulator precedent), sign-split per religion (updateReligionHappiness)
	WbSplit religion;
	{
		const ReligionTypes eState = owner.getStateReligion();
		const int iStateAcc = owner.getStateReligionHappiness();
		const int iNonStateAcc = owner.getNonStateReligionHappiness();
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
		iH += hap.areaBld.iGood;
		iH += hap.empBld.iGood;
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
		iU -= hap.areaBld.iBad;
		iU -= hap.empBld.iBad;
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
	const int iTotalGoodBld = hea.bld.iGood + hea.areaBld.iGood + hea.empBld.iGood
		+ hea.extraB.iGood + std::max(0, iPopHealth);
	const int iTotalBadBld = pCity->isBuildingOnlyHealthy() ? 0
		: hea.bld.iBad + hea.areaBld.iBad + hea.empBld.iBad + hea.extraB.iBad + std::min(0, iPopHealth);

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

// The [MODIFIER/wellbeing] shadow lives in CvCascadeModifierMath.cpp (the shadow harness) -- it rides the
// registered [MODIFIER] spine domain and the harness's per-city wired eval ctx.
