//
//	EnablerKernel -- the shared GENERATE->GATE primitive + gate helpers (see the header). Ported VERBATIM from
//	CvCascadeEnabler.cpp's file-static en_* helpers; promoted to a declared surface so every cascade reaches the ONE
//	implementation (the single-source law, patterns.md). LOGIC unchanged: only the signatures + internal call sites
//	were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadePerfCount.h"   // per-turn call counters + stopwatches (owner 2026-07-02: repeat-calc hunt)
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "CvCascadeEnablerKernel.h"
#include "CvJsonInfo.h"
#include "CvJsonTechInfo.h"        // cascadeStartNode -- the synthetic TECH_GAME_START root
#include "Repos/InfoRepo.h"
#include "CvCascadeTally.h"
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "CvCascadeConditionEval.h"   // cascadeEvalCondition -- the StoneBase-ported typed-condition evaluator
#include "CvJsonCondition.h"       // CvJsonCondition tree (CASC_COND_*/CASC_PRED_*) -- scanned for the operate reverse-index
#include "CvCascadeAccumulator.h"     // the accumulator package surface (the epochs are DELETED -- scope-packages.md phase 3)
#include "CvJsonBuildingInfo.h"
#include "CvJsonUnitInfo.h"
#include "CvJsonTechInfo.h"
#include "CvJsonCivicInfo.h"
#include "CvJsonProjectInfo.h"
#include "CvJsonProcessInfo.h"
#include "CvJsonPromotionInfo.h"
#include "Infos/CvBuildInfo.h"
#include "Engine/CvGame.h"

// The enables buckets this pass generates over (one HAVE traversal fills them all).
static const char* EN_BUCKETS[] = { "buildings", "units", "projects", "processes", "techs", "civics", "promotions", "builds", "hurries", NULL };

// The per-(bucket) InfoRepo dispatch -- the entity's CvJsonInfo by bucket name + id.
const CvJsonInfo* EnablerKernel::jsonFor(const std::string& b, int id)
{
	if (b == "buildings") return InfoRepo<CvJsonBuildingInfo>::get().get(id);
	if (b == "units")     return InfoRepo<CvJsonUnitInfo>::get().get(id);
	if (b == "projects")  return InfoRepo<CvJsonProjectInfo>::get().get(id);
	if (b == "processes") return InfoRepo<CvJsonProcessInfo>::get().get(id);
	if (b == "techs")     return InfoRepo<CvJsonTechInfo>::get().get(id);
	if (b == "civics")    return InfoRepo<CvJsonCivicInfo>::get().get(id);
	if (b == "promotions") return InfoRepo<CvJsonPromotionInfo>::get().get(id);
	if (b == "builds")    return InfoRepo<CvBuildInfo>::get().get(id);
	// "hurries" has no InfoRepo (HURRY_ not in RJ_REPO_TYPES) -> NULL; canHurry needs only the enables.hurries edges
	// (on the civics/techs), and a NULL json passes requires/allowed -> the gate IS "the hurry type is generated".
	return NULL;
}

void EnablerKernel::addEdge(const CvJsonInfo* j, const std::string& key, std::set<int>& out)
{
	if (j == NULL) return;
	const std::vector<int>* p = j->edge(key);
	if (p == NULL) return;
	for (size_t i = 0; i < p->size(); ++i) out.insert((*p)[i]);
}

// Accumulate one HAVE entity's source-side edges across every bucket: enables ADD to candidates; obsoletes/replaces/
// disables collected into rem (subtracted after the whole HAVE set is gathered -- enabler.md §2 set-difference).
void EnablerKernel::accumHave(const CvJsonInfo* j, EnBucketSets& cand, EnBucketSets& rem)
{
	if (j == NULL) return;
	for (int i = 0; EN_BUCKETS[i] != NULL; ++i)
	{
		const std::string b = EN_BUCKETS[i];
		addEdge(j, "enables." + b, cand[b]);
		addEdge(j, "obsoletes." + b, rem[b]);
		addEdge(j, "replaces." + b, rem[b]);
		addEdge(j, "disables." + b, rem[b]);
	}
}

// GENERATE (enabler.md §2). HAVE = team techs + adopted civics (+ the city's buildings if pCity != NULL).
void EnablerKernel::generate(const CvPlayer& kPlayer, const CvCity* pCity, EnBucketSets& cand)
{
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	EnBucketSets rem;
	// The synthetic TECH_GAME_START root: every civ grants it, so it is universally HELD -- seed its `enables` (the
	// no-tech-prereq starting set: CAVE_DWELLING/NOMADISM/LANGUAGE, UNIT_BRUTE, …) into GENERATE for every player.
	accumHave(&cascadeStartNode(), cand, rem);
	for (int iT = 0; iT < GC.getNumTechInfos(); ++iT)
		if (kTeam.isHasTech((TechTypes)iT)) accumHave(InfoRepo<CvJsonTechInfo>::get().get(iT), cand, rem);
	for (int iCO = 0; iCO < GC.getNumCivicOptionInfos(); ++iCO)
	{
		const CivicTypes eCivic = kPlayer.getCivics((CivicOptionTypes)iCO);
		if (eCivic != NO_CIVIC) accumHave(InfoRepo<CvJsonCivicInfo>::get().get((int)eCivic), cand, rem);
	}
	if (pCity != NULL)
	{
		const std::vector<BuildingTypes> aHas = pCity->getHasBuildings();
		for (size_t i = 0; i < aHas.size(); ++i)
		{
			const CvJsonInfo* jb = InfoRepo<CvJsonBuildingInfo>::get().get((int)aHas[i]);
			// #430 obsoletion FLIP (owner 2026-07-07): a now-present obsolete building no longer UNLOCKS -- its
			// `enables` are SUPERSEDED (the same frontier outcome as the pre-flip world where legacy removed it),
			// so it is skipped from GENERATE. This keeps the frontier input INVARIANT to the flip -- the enabler
			// parity no longer leans on legacy obsoletion. (Its whenObsolete MODIFIER still delivers via the
			// operating-buildings obsolete set; only its enabler/frontier contribution stops here.)
			if (jb != NULL && obsoletedByHeldTech(jb, kTeam)) continue;
			accumHave(jb, cand, rem);
		}
	}
	for (EnBucketSets::iterator it = cand.begin(); it != cand.end(); ++it)
	{
		const std::set<int>& r = rem[it->first];
		for (std::set<int>::const_iterator jt = r.begin(); jt != r.end(); ++jt) it->second.erase(*jt);
	}
}

bool EnablerKernel::obsoletedByHeldTech(const CvJsonInfo* j, const CvTeam& kTeam)
{
	if (j == NULL) return false;
	const std::vector<int>* p = j->edge("obsoletedBy.techs");
	if (p == NULL) return false;
	for (size_t i = 0; i < p->size(); ++i)
		if (kTeam.isHasTech((TechTypes)(*p)[i])) return true;
	return false;
}

// requires gate: build ∧ operate, evaluated through the typed-condition evaluator (StoneBase port) against the live
// engine ctx. The ENABLER reads with STRICT flags -- a {STATE_RELIGION:X} in requires.build must MATCH the player's
// state religion (the modifier's lenient compound is the modifier-side reading; json §3.5 / enabler §3).
bool EnablerKernel::requiresMet(const CvJsonInfo* j, const CvCascadeEvalCtx& ec)
{
	if (j == NULL) return true;
	CvCascadeEvalFlags flags;
	flags.strictStateReligionForBuild = true;
	if (j->requiresBuild() != NULL && !cascadeEvalCondition(j->requiresBuild(), ec, flags)) return false;
	if (j->requiresOperate() != NULL && !cascadeEvalCondition(j->requiresOperate(), ec, flags)) return false;
	return true;
}

bool EnablerKernel::allowedOk(const CvJsonInfo* j, int iId, const CvPlayer& kPlayer, bool bUnit, const std::string& bucket)
{
	if (j == NULL) return true;
	// PROJECTS (parity find 2026-07-02): the tally has NO project domain, so the cap check read buildingCount(projectId)
	// = 0 and every already-built world project stayed offered (canCreate casc=1 leg=0 on ENCYCLOPEDIA/IMF/EVOLUTION).
	// Projects read the engine-owned counts (the tally read-not-store philosophy): world = created-ever, team = held.
	const CvJsonAllowed* a = j->getAllowed();
	if (a == NULL) return true;
	if (bucket == "projects")
	{
		for (std::map<std::string, int>::const_iterator it = a->all().begin(); it != a->all().end(); ++it)
		{
			int iCount = -1;
			if (it->first == "world")      iCount = GC.getGame().getProjectCreatedCount((ProjectTypes)iId);
			else if (it->first == "team")  iCount = GET_TEAM(kPlayer.getTeam()).getProjectCount((ProjectTypes)iId);
			else if (it->first == "empire") iCount = GET_TEAM(kPlayer.getTeam()).getProjectCount((ProjectTypes)iId);
			if (iCount >= 0 && iCount >= it->second) return false;
		}
		return true;
	}
	for (std::map<std::string, int>::const_iterator it = a->all().begin(); it != a->all().end(); ++it)
	{
		const std::string& k = it->first;
		CascadeCountScope eScope; int iEntity;
		if (k == "world")       { eScope = CASCADE_COUNT_WORLD;  iEntity = 0; }
		else if (k == "team")   { eScope = CASCADE_COUNT_TEAM;   iEntity = (int)kPlayer.getTeam(); }
		else if (k == "empire") { eScope = CASCADE_COUNT_EMPIRE; iEntity = (int)kPlayer.getID(); }
		else continue;   // category cap -> first-cut TODO
		const int iCount = bUnit ? cascadeTally().unitCount(iEntity, iId, eScope)
		                         : cascadeTally().buildingCount(iEntity, iId, eScope);
		if (iCount >= it->second) return false;
	}
	return true;
}

// (The empire-capability query lives in CascadeCapabilities -- the ONE derived-on-query union, cached per team;
// the kernel's former techs-only duplicate was folded into it 2026-07-02, capabilities.md.)

// canFoundReligion -- a PLAYER-WIDE state predicate (CvPlayer::canFoundReligion): NOT a JSON frontier, reproduced from
// game state so the cascade owns the gate (it is what enables/AI-reads the religion-founding action). >=1 city, not
// NPC, not the first 3 turns; under RELIGION_LIMITED a holy-city owner cannot found another (minus the rebel /
// LIMITED_RELIGIONS_EXCEPTIONS carve-out). Reads raw state only -- a faithful mirror of the engine predicate.
bool EnablerKernel::canFoundReligion(const CvPlayer& kPlayer)
{
	if (kPlayer.getNumCities() < 1 || kPlayer.isNPC()
	|| (GC.getGame().isGameStart() && GC.getGame().getElapsedGameTurns() < 3))
		return false;
	if (GC.getGame().isOption(GAMEOPTION_RELIGION_LIMITED))
		if (((kPlayer.getNumCities() > 1) && !kPlayer.isRebel()) || !GC.isLIMITED_RELIGIONS_EXCEPTIONS())
			if (kPlayer.hasHolyCity())
				return false;
	return true;
}

// GATE: candidates[bucket] -> the available set (requires + allowed + obsoletedBy).
void EnablerKernel::gateSet(const std::string& bucket, const EnBucketSets& cand, const CvCascadeEvalCtx& ec,
	const CvPlayer& kPlayer, const CvTeam& kTeam, bool bUnit, std::set<int>& avail)
{
	EnBucketSets::const_iterator b = cand.find(bucket);
	if (b == cand.end()) return;
	for (std::set<int>::const_iterator it = b->second.begin(); it != b->second.end(); ++it)
	{
		const CvJsonInfo* j = jsonFor(bucket, *it);
		if (obsoletedByHeldTech(j, kTeam)) continue;
		if (requiresMet(j, ec) && allowedOk(j, *it, kPlayer, bUnit, bucket)) avail.insert(*it);
	}
}

// ============================ the ONE per-building verdict ================================================
namespace {

// The per-building outcomes of the operate pass for a PRESENT building (enabler.md §3.2).
enum EkBuildingVerdict
{
	EK_ACTIVE,           // operating: not obsolete ∧ operate holds ∧ no dormant-trigger successor present
	EK_OBSOLETE,         // obsoletedBy tech held -- the THIRD outcome: neither active nor dormant, provides nothing (json §4.2)
	EK_DORMANT_OPERATE,  // requires.operate fails under the current vicinity supply
	EK_DORMANT_TRIGGER   // a dormant-trigger successor building is present
};

// b's `provides.bonuses` (json §5a), or NULL.
static const std::vector<int>* ek_provides(int b)
{
	const CvJsonInfo* j = InfoRepo<CvJsonBuildingInfo>::get().get(b);
	if (j == NULL) return NULL;
	const CvJsonProvides* pv = j->getProvides();
	return (pv != NULL && !pv->bonuses.empty()) ? &pv->bonuses : NULL;
}

// The ONE per-building classification for a PRESENT building b, under the supply `ecOp.vicinityProvidedBonuses`
// points at -- shared by the full seed recompute (recomputeOperatingBuildingsInto) and the targeted ripple
// (ek_recheckActiveSet), so the two can never diverge in per-building logic. ORDER (enabler.md §3.2: obsolete is
// the third outcome, checked before operate): obsolescence is INDEPENDENT of operate/dormancy -- a present
// building whose obsoletedBy tech is held is obsolete regardless of operate, so it is checked FIRST.
static EkBuildingVerdict ek_classifyBuilding(int b, const CvCity* pCity, CvCascadeEvalCtx& ecOp, const CvCascadeEvalFlags& flags)
{
	const CvJsonInfo* j = InfoRepo<CvJsonBuildingInfo>::get().get(b);
	if (j == NULL) return EK_ACTIVE;
	if (ecOp.team != NULL && EnablerKernel::obsoletedByHeldTech(j, *ecOp.team)) return EK_OBSOLETE;
	if (j->requiresOperate() != NULL && !cascadeEvalCondition(j->requiresOperate(), ecOp, flags)) return EK_DORMANT_OPERATE;
	const std::vector<int>& dorm = j->dormantTriggers();
	for (size_t i = 0; i < dorm.size(); ++i)
		if (pCity->hasBuilding((BuildingTypes)dorm[i])) return EK_DORMANT_TRIGGER;
	return EK_ACTIVE;
}

} // namespace

// COMPUTE the two per-city operating buildings in ONE pass. `activeOut` = the ACTIVE buildings for pCity (present ∧
// operate-holds ∧ ¬dormant-trigger). DORMANCY is DERIVED from `requires.operate` + its dormant triggers (the successor
// buildings whose presence dorms this) -- NEVER the engine active-building/`/state` read (DEC-calc-zero-ride-in; owner:
// dormancy is 100% governed by operate enablers). Present is the raw input hasBuilding (the un-dormancy-gated presence).
// `providedOut` = every ACTIVE building's `provides.bonuses` unioned -- the in-vicinity bonus supply (json §5a),
// computed from JSON, not the engine. `ecOp` = a COPY of ec with activeBuildings=NULL so a BUILDING_ predicate INSIDE an
// operate condition resolves via raw presence -- this breaks any recursion (operate conditions reference resources/
// civics in practice, not building-active).
void EnablerKernel::recomputeOperatingBuildingsInto(const CvCity* pCity, std::set<int>& activeOut, std::set<int>& providedOut, std::set<int>& obsoleteOut)
{
	// The PURE fixpoint recompute -- the standing cache's refresh target (CvCity::refreshOperatingBuildings). Contract
	// rule 2 (CvDerivedCache.h): FULLY define the output every call.
	activeOut.clear();
	providedOut.clear();
	obsoleteOut.clear();
	if (pCity == NULL) return;
	++CascadePerf::operatingBuildingsRecomputed;
	PerfAccumTimer perfT(CascadePerf::operatingBuildingsRecomputeMs);
	CascadeCondScope ccs(CC_OPERATING_BUILDINGS);   // the condEval caller split (attributes correctly even nested in a fill)
	const CvPlayer& kOwner = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ecOp;
	ecOp.city = pCity; ecOp.plot = pCity->plot(); ecOp.player = &kOwner; ecOp.team = &GET_TEAM(kOwner.getTeam());
	ecOp.activeBuildings = NULL;   // break recursion: operate's own BUILDING_ atoms resolve via raw presence
	CvCascadeEvalFlags flags;      // default flags
	// enabler-frontier-perf.md Part D (safe subset): iterate the city's PRESENT buildings, not a 0..NumBuildingInfos
	// scan (~9400 hasBuilding probes × the fixpoint iterations, per recompute, at recompute frequency). m_hasBuildings
	// is maintained in lockstep with hasBuilding()/m_bHasBuildings (CvCity::alterBuildingLedger), so this is the SAME
	// present set -- and the fixpoint is order-independent (each building's active status reads the PRIOR iteration's
	// `prov`; provNext is a set union), so the active/provided output is bit-identical. (The deeper affected-subset
	// recompute + the conditional operating buildings-skip are left as full-dirty: a completed building always joins the active set,
	// and the mutually-dependent fixpoint resists a correct partial recompute -- rule 3; A-C already collapse the
	// recompute FREQUENCY, which is where the ceFacts win comes from.)
	const std::vector<BuildingTypes> aHas = pCity->getHasBuildings();
	// ⛔ FIXPOINT (2026-07-02, the Athens −15 stack find): the active set and the vicinity supply are MUTUALLY
	// dependent — an operate condition may consume a bonus another ACTIVE building `provides` (json §5a), so a
	// single pass evaluates those consumers against an incomplete supply and dorms them wrongly (per-building
	// dry-calc proved the data exact; the whole bC under-count was ~14 buildings dormed by a NULL/partial provB).
	// StoneBase never faced this: its AugmentState reads the ENGINE's dormant set (the "StoneBase cheated on
	// dormancy" the owner flagged); the self-contained cascade must solve the fixpoint itself. LEAST fixpoint:
	// start with an EMPTY supply, iterate active→provides until stable (bounded; provides only ever ADD, so
	// convergence is fast — typically 2 scans).
	std::set<int> prov;
	ecOp.vicinityProvidedBonuses = &prov;
	for (int iter = 0; iter < 5; ++iter)
	{
		activeOut.clear();
		std::set<int> provNext;
		for (size_t iB = 0; iB < aHas.size(); ++iB)
		{
			const int b = (int)aHas[iB];   // a PRESENT building (raw input -- the un-dormancy-gated presence)
			// The ONE shared verdict (ek_classifyBuilding) -- the SAME classification the targeted ripple applies,
			// so the seed and the ripple can never diverge. Obsolescence is checked FIRST (enabler.md §3.2: obsolete
			// is the THIRD outcome, checked before operate -- a present building whose obsoletedBy tech is held is
			// neither active nor dormant regardless of operate; excluded from active, provides nothing). The #430
			// obsoletion flip landed: an obsolete building STAYS present, and its whenObsolete tree deposits off
			// this set (json §4.2, modifier-side).
			const EkBuildingVerdict v = ek_classifyBuilding(b, pCity, ecOp, flags);
			if (v == EK_OBSOLETE) { obsoleteOut.insert(b); continue; }   // -> the parallel obsolete set (json §4.2)
			if (v != EK_ACTIVE) continue;                                // dormant (operate fails / trigger present)
			activeOut.insert(b);   // active (under the CURRENT supply estimate)
			// This ACTIVE building's `provides.bonuses` supply those bonuses IN-VICINITY (json §5a).
			const std::vector<int>* pv = ek_provides(b);
			if (pv != NULL)
				for (size_t i = 0; i < pv->size(); ++i) provNext.insert((*pv)[i]);
		}
		if (provNext == prov) break;   // supply stable -> the active set is the fixpoint
		prov.swap(provNext);
	}
	providedOut = prov;
}

// ============================ the ACTIVE-SET targeted maintenance ==========================================
// The per-city active-building set (m_operatingBuildings.active/provided) is a CASCADE maintained by TARGETED
// PROPAGATION, not blanket-recomputed on every event: a HAVE-change ripples ONLY the affected buildings into the
// AUTHORITATIVE set (the recompute above stays the LOAD seed + the validation oracle). Mirrors the frontier's
// s_bc*/recheckHave (CvCascadeBuildingCascade.cpp), extended to the operate<->provides fixpoint. enabler.md §7.
namespace {

static bool s_opIdxBuilt = false;
static std::vector<int> s_opPop, s_opPower, s_opReligion, s_opCorp, s_opGolden, s_opStateRel, s_opCivic, s_opTechAny, s_opDynamic;
static std::map<int, std::vector<int> > s_opBonusConsumers;   // BONUS_ id -> {buildings whose operate consumes it}
static std::map<int, std::vector<int> > s_opBuildingDeps;     // BUILDING_ id -> {buildings whose operate references it}
static std::map<int, std::vector<int> > s_opDormBy;           // trigger BUILDING_ id -> {buildings it dorms}
static std::vector<int> s_opObsoletable;                      // buildings with any obsoletedBy.techs -> re-checked on a tech (player-scope) change (json §4.2)

// The work-list ripple: re-check `seeds` under the AUTHORITATIVE provided supply, updating active/provided/
// providedCount in place; on an active flip that crosses a bonus's provided-count 0<->1, push that bonus's operate
// consumers (the provides-ripple). Bounded by the fixpoint; a runaway cap self-heals at the slice boundary.
static void ek_recheckActiveSet(const CvCity* pCity, const std::vector<int>& seeds)
{
	if (pCity == NULL || seeds.empty()) return;
	OperatingBuildings& f = pCity->m_operatingBuildings;   // AUTHORITATIVE
	const CvPlayer& kOwner = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ecOp;
	ecOp.city = pCity; ecOp.plot = pCity->plot(); ecOp.player = &kOwner; ecOp.team = &GET_TEAM(kOwner.getTeam());
	ecOp.activeBuildings = NULL;                    // break recursion (operate BUILDING_ atoms -> raw presence)
	ecOp.vicinityProvidedBonuses = &f.provided;     // the LIVE authoritative supply (mutated as the ripple runs)
	CvCascadeEvalFlags flags;
	std::set<int> pending;
	std::vector<int> work;
	for (size_t i = 0; i < seeds.size(); ++i) if (pending.insert(seeds[i]).second) work.push_back(seeds[i]);
	int cap = GC.getNumBuildingInfos() + 512;
	while (!work.empty())
	{
		if (--cap < 0) break;
		const int b = work.back(); work.pop_back();
		// The ONE shared verdict (ek_classifyBuilding) drives BOTH memberships. Obsolete-set maintenance (json
		// §4.2): present ∧ obsoleted-by-held-tech keeps the building in the parallel obsolete set (the whenObsolete
		// tree). An obsolete building classifies neither active nor dormant (checked FIRST, enabler.md §3.2), so an
		// active->obsolete move is an active DROP that runs the provides-ripple below.
		const bool wasObs = (f.obsolete.count(b) != 0);
		const bool was = (f.active.count(b) != 0);
		bool nowObs = false, now = false;
		if (pCity->hasBuilding((BuildingTypes)b))
		{
			const EkBuildingVerdict v = ek_classifyBuilding(b, pCity, ecOp, flags);
			nowObs = (v == EK_OBSOLETE);
			now = (v == EK_ACTIVE);
		}
		if (nowObs != wasObs) { if (nowObs) f.obsolete.insert(b); else f.obsolete.erase(b); }
		if (now == was) continue;
		const std::vector<int>* prov = ek_provides(b);
		if (now)
		{
			f.active.insert(b);
			if (prov) for (size_t i = 0; i < prov->size(); ++i)
			{
				const int bn = (*prov)[i];
				if (++f.providedCount[bn] == 1)
				{
					f.provided.insert(bn);
					std::map<int, std::vector<int> >::const_iterator cit = s_opBonusConsumers.find(bn);
					if (cit != s_opBonusConsumers.end())
						for (size_t k = 0; k < cit->second.size(); ++k)
							if (pending.insert(cit->second[k]).second) work.push_back(cit->second[k]);
				}
			}
		}
		else
		{
			f.active.erase(b);
			if (prov) for (size_t i = 0; i < prov->size(); ++i)
			{
				const int bn = (*prov)[i];
				std::map<int, int>::iterator pc = f.providedCount.find(bn);
				if (pc != f.providedCount.end() && --(pc->second) <= 0)
				{
					f.providedCount.erase(pc);
					f.provided.erase(bn);
					std::map<int, std::vector<int> >::const_iterator cit = s_opBonusConsumers.find(bn);
					if (cit != s_opBonusConsumers.end())
						for (size_t k = 0; k < cit->second.size(); ++k)
							if (pending.insert(cit->second[k]).second) work.push_back(cit->second[k]);
				}
			}
		}
	}
}

} // namespace

// The ONE requires-tree HAVE-atom scanner (see the header) -- was three near-identical file-static copies
// (bc_scanCond / uc_scanCond / ek_scanOp); the legs that differed are the two flags.
void EnablerKernel::scanCondDeps(const CvJsonCondition* c, CascadeCondDeps& d, bool bTrackUnits, bool bMarkDynamic)
{
	if (c == NULL) return;
	if (c->kind == CASC_COND_GROUP)
	{
		for (size_t i = 0; i < c->all.size(); ++i)    scanCondDeps(c->all[i], d, bTrackUnits, bMarkDynamic);
		for (size_t i = 0; i < c->anyOf.size(); ++i)  scanCondDeps(c->anyOf[i], d, bTrackUnits, bMarkDynamic);
		for (size_t i = 0; i < c->noneOf.size(); ++i) scanCondDeps(c->noneOf[i], d, bTrackUnits, bMarkDynamic);
		scanCondDeps(c->enabled, d, bTrackUnits, bMarkDynamic);
		scanCondDeps(c->disabled, d, bTrackUnits, bMarkDynamic);
		return;
	}
	if (c->kind == CASC_COND_PRESENCE)
	{
		const std::string& t = c->type;
		if (t == "POPULATION") d.pop = true;
		else if (t.compare(0, 5, "TECH_") == 0)  { if (c->id >= 0) d.techs.insert(c->id); }
		else if (t.compare(0, 6, "BONUS_") == 0) { if (c->id >= 0) d.bonuses.insert(c->id); if (bMarkDynamic) d.dynamic = true; }  // trade/map/vicinity shifts aren't a discrete event
		else if (t.compare(0, 6, "CIVIC_") == 0) d.civicAny = true;
		else if (t.compare(0, 9, "RELIGION_") == 0) d.religion = true;
		else if (t.compare(0, 12, "CORPORATION_") == 0) d.corp = true;
		else if (t.compare(0, 9, "BUILDING_") == 0) { if (c->id >= 0) d.buildings.insert(c->id); }
		else if (bTrackUnits && t.compare(0, 5, "UNIT_") == 0) { if (c->id >= 0) d.units.insert(c->id); }
		else if (bMarkDynamic) d.dynamic = true;
		return;
	}
	if (c->kind == CASC_COND_PREDICATE)
	{
		switch (c->predKind)
		{
		case CASC_PRED_HAS_POWER:               d.power = true; break;
		case CASC_PRED_IS_GOLDEN_AGE:           d.goldenAge = true; break;
		case CASC_PRED_HAS_CORPORATION:         d.corp = true; break;
		case CASC_PRED_HAS_RELIGION:
		case CASC_PRED_IS_HOLY_CITY:            d.religion = true; break;
		case CASC_PRED_HAS_STATE_RELIGION:
		case CASC_PRED_STATE_RELIGION_IN_CITY:
		case CASC_PRED_STATE_RELIGION:
		case CASC_PRED_IS_STATE_RELIGION_HOLY_CITY: d.stateReligion = true; break;
		case CASC_PRED_HAS_BONUS:               if (c->id >= 0) d.bonuses.insert(c->id); if (bMarkDynamic) d.dynamic = true; break;
		default:                                if (bMarkDynamic) d.dynamic = true; break;   // IS_CAPITAL / counts / plot / connection -- read LIVE at eval
		}
		return;
	}
}

void EnablerKernel::buildActiveIndex()
{
	if (s_opIdxBuilt) return;
	s_opIdxBuilt = true;
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		const CvJsonInfo* j = InfoRepo<CvJsonBuildingInfo>::get().get(b);
		if (j == NULL) continue;
		CascadeCondDeps d;
		// OPERATE only -- active/dormant is governed by requires.operate; DYNAMIC marked (the per-turn re-check bucket).
		scanCondDeps(j->requiresOperate(), d, /*bTrackUnits*/ false, /*bMarkDynamic*/ true);
		if (d.pop)            s_opPop.push_back(b);
		if (d.power)          s_opPower.push_back(b);
		if (d.religion)       s_opReligion.push_back(b);
		if (d.corp)           s_opCorp.push_back(b);
		if (d.goldenAge)      s_opGolden.push_back(b);
		if (d.stateReligion)  s_opStateRel.push_back(b);
		if (d.civicAny)       s_opCivic.push_back(b);
		if (!d.techs.empty()) s_opTechAny.push_back(b);
		if (d.dynamic)        s_opDynamic.push_back(b);
		for (std::set<int>::const_iterator it = d.bonuses.begin(); it != d.bonuses.end(); ++it) s_opBonusConsumers[*it].push_back(b);
		for (std::set<int>::const_iterator it = d.buildings.begin(); it != d.buildings.end(); ++it) s_opBuildingDeps[*it].push_back(b);
		const std::vector<int>& dorm = j->dormantTriggers();
		for (size_t i = 0; i < dorm.size(); ++i) s_opDormBy[dorm[i]].push_back(b);
		// obsolescence is tech-driven (obsoletedBy.techs): index the obsoletable buildings for the player-scope re-check.
		if (j->edge("obsoletedBy.techs") != NULL) s_opObsoletable.push_back(b);
	}
}

void EnablerKernel::onBuildingChangedActive(const CvCity* pCity, int eBuilding)
{
	buildActiveIndex();
	if (pCity == NULL || eBuilding < 0) return;
	std::vector<int> seeds;
	seeds.push_back(eBuilding);
	std::map<int, std::vector<int> >::const_iterator dep = s_opBuildingDeps.find(eBuilding);
	if (dep != s_opBuildingDeps.end()) seeds.insert(seeds.end(), dep->second.begin(), dep->second.end());
	std::map<int, std::vector<int> >::const_iterator dm = s_opDormBy.find(eBuilding);
	if (dm != s_opDormBy.end()) seeds.insert(seeds.end(), dm->second.begin(), dm->second.end());
	ek_recheckActiveSet(pCity, seeds);
}

void EnablerKernel::onHaveChangedActive(const CvCity* pCity, int eHaveKind)
{
	buildActiveIndex();
	if (pCity == NULL) return;
	switch (eHaveKind)
	{
	case CascadeAccumulator::CASC_HAVE_POP:   ek_recheckActiveSet(pCity, s_opPop); break;
	case CascadeAccumulator::CASC_HAVE_POWER: ek_recheckActiveSet(pCity, s_opPower); break;
	case CascadeAccumulator::CASC_HAVE_CORP:  ek_recheckActiveSet(pCity, s_opCorp); break;
	case CascadeAccumulator::CASC_HAVE_RELIGION:
	{
		std::vector<int> seeds(s_opReligion);
		seeds.insert(seeds.end(), s_opStateRel.begin(), s_opStateRel.end());
		ek_recheckActiveSet(pCity, seeds);
		break;
	}
	default: break;
	}
}

void EnablerKernel::onPlayerScopeChangedActive(const CvCity* pCity)
{
	buildActiveIndex();
	if (pCity == NULL) return;
	// tech/civic/golden-age player-scope change: re-check the buildings whose operate references any of them.
	// Over-inclusive (we don't get the specific tech/civic here), but these events are infrequent -- amortized cheap.
	std::vector<int> seeds(s_opTechAny);
	seeds.insert(seeds.end(), s_opCivic.begin(), s_opCivic.end());
	seeds.insert(seeds.end(), s_opGolden.begin(), s_opGolden.end());
	seeds.insert(seeds.end(), s_opObsoletable.begin(), s_opObsoletable.end());   // tech research flips obsolescence (json §4.2)
	ek_recheckActiveSet(pCity, seeds);
}

void EnablerKernel::onSliceRebuildActive(const CvCity* pCity)
{
	buildActiveIndex();
	if (pCity == NULL) return;
	// the bounded per-turn self-heal: re-check ONLY the DYNAMIC-operate buildings (operate reads live non-HAVE
	// state no event carries -- IS_CAPITAL, counts, connection, bonus trade/vicinity shifts). Everything else is
	// event-hooked. Replaces the whole-city full recompute every slice.
	ek_recheckActiveSet(pCity, s_opDynamic);
}

// The LOAD seed (CvCity::refreshOperatingBuildings): the ONE full recompute of active/provided + the provider ref-count
// the ripple maintains. Called on the first ensure() after load/reset/city-creation; the on*Active hooks keep it
// current thereafter, so the full recompute never runs per-event again.
void EnablerKernel::seedOperatingBuildings(const CvCity* pCity)
{
	if (pCity == NULL) return;
	recomputeOperatingBuildingsInto(pCity, pCity->m_operatingBuildings.active, pCity->m_operatingBuildings.provided, pCity->m_operatingBuildings.obsolete);
	std::map<int, int>& pcnt = pCity->m_operatingBuildings.providedCount;
	pcnt.clear();
	const std::set<int>& act = pCity->m_operatingBuildings.active;
	for (std::set<int>::const_iterator it = act.begin(); it != act.end(); ++it)
	{
		const std::vector<int>* prov = ek_provides(*it);
		if (prov) for (size_t k = 0; k < prov->size(); ++k) pcnt[(*prov)[k]]++;
	}
}

const OperatingBuildings& EnablerKernel::operatingBuildings(const CvCity* pCity)
{
	// The standing per-city operating buildings, PURE Set protocol (scope-packages.md): events mark the Set directly
	// (building/religion/corp flips in CvCity; tech/civic/GA via markPlayerScopeAndCities -- operate
	// conditions read those; the slice boundary is the self-heal for the unhooked classes) -- no polling.
	OperatingBuildings& f = pCity->m_operatingBuildings;
	f.set.ensure();
	++CascadePerf::operatingBuildingsCacheHits;   // a standing-cache read (the census' "served without a recompute" counter)
	return f;
}

void EnablerKernel::wireOperatingBuildings(const CvCity* pCity, CvCascadeEvalCtx& ec)
{
	const OperatingBuildings& f = operatingBuildings(pCity);
	ec.activeBuildings = &f.active;
	ec.obsoleteBuildings = &f.obsolete;
	ec.vicinityProvidedBonuses = &f.provided;
}
