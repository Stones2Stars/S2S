//
//	EnablerKernel -- the shared GENERATE->GATE primitive + gate helpers (see the header): the `enables` forward walk
//	and the `requires` gate, the ONE implementation every enabler reaches (the single-source law, patterns.md).
//

#include "CvGameCoreDLL.h"
#include "CvCascadePerfCount.h"   // per-turn call counters + stopwatches (owner 2026-07-02: repeat-calc hunt)
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "CvEnablerKernel.h"
#include "CvEnabler.h"            // EnablerDomain -- the standardized domain the applyEdges deltas write
#include "CvInfo.h"
#include "CvTechInfo.h"        // cascadeStartNode -- the synthetic TECH_GAME_START root
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
#include "CvBuildingInfo.h"
#include "CvUnitInfo.h"
#include "CvTechInfo.h"
#include "CvCivicInfo.h"
#include "CvProjectInfo.h"
#include "CvProcessInfo.h"
#include "CvPromotionInfo.h"
#include "CvBuildInfo.h"
#include "Engine/CvGame.h"

// The enables buckets this pass generates over (one HAVE traversal fills them all).
static const EnEdgeBucket EN_GEN_BUCKETS[] =
{
	EDGEB_BUILDINGS, EDGEB_UNITS, EDGEB_PROJECTS, EDGEB_PROCESSES, EDGEB_TECHS,
	EDGEB_CIVICS, EDGEB_PROMOTIONS, EDGEB_BUILDS, EDGEB_HURRIES, NO_EDGEB
};

// The per-(bucket) InfoRepo dispatch -- the entity's CvInfo by bucket + id.
const CvInfo* EnablerKernel::jsonFor(EnEdgeBucket eBucket, int id)
{
	switch (eBucket)
	{
	case EDGEB_BUILDINGS:  return InfoRepo<CvBuildingInfo>::get().get(id);
	case EDGEB_UNITS:      return InfoRepo<CvUnitInfo>::get().get(id);
	case EDGEB_PROJECTS:   return InfoRepo<CvProjectInfo>::get().get(id);
	case EDGEB_PROCESSES:  return InfoRepo<CvProcessInfo>::get().get(id);
	case EDGEB_TECHS:      return InfoRepo<CvTechInfo>::get().get(id);
	case EDGEB_CIVICS:     return InfoRepo<CvCivicInfo>::get().get(id);
	case EDGEB_PROMOTIONS: return InfoRepo<CvPromotionInfo>::get().get(id);
	case EDGEB_BUILDS:     return InfoRepo<CvBuildInfo>::get().get(id);
	// EDGEB_HURRIES has no InfoRepo (HURRY_ not in RJ_REPO_TYPES) -> NULL; canHurry needs only the enables.hurries
	// edges (on the civics/techs), and a NULL json passes requires/allowed -> the gate IS "the hurry type is generated".
	default:               return NULL;
	}
}

void EnablerKernel::addEdge(const CvInfo* j, EnEdgeFamily eFamily, EnEdgeBucket eBucket, std::set<int>& out)
{
	if (j == NULL) return;
	const std::vector<int>* p = j->edge(eFamily, eBucket);
	if (p == NULL) return;
	for (size_t i = 0; i < p->size(); ++i) out.insert((*p)[i]);
}

// Accumulate one HAVE entity's source-side edges across every bucket: enables ADD to candidates; obsoletes/replaces/
// disables collected into rem (subtracted after the whole HAVE set is gathered -- enabler.md §2 set-difference).
void EnablerKernel::accumHave(const CvInfo* j, EnBucketSets& cand, EnBucketSets& rem)
{
	if (j == NULL) return;
	for (int i = 0; EN_GEN_BUCKETS[i] != NO_EDGEB; ++i)
	{
		const EnEdgeBucket b = EN_GEN_BUCKETS[i];
		addEdge(j, EDGEF_ENABLES,   b, cand[b]);
		addEdge(j, EDGEF_OBSOLETES, b, rem[b]);
		addEdge(j, EDGEF_REPLACES,  b, rem[b]);
		addEdge(j, EDGEF_DISABLES,  b, rem[b]);
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
		if (kTeam.isHasTech((TechTypes)iT)) accumHave(InfoRepo<CvTechInfo>::get().get(iT), cand, rem);
	for (int iCO = 0; iCO < GC.getNumCivicOptionInfos(); ++iCO)
	{
		const CivicTypes eCivic = kPlayer.getCivics((CivicOptionTypes)iCO);
		if (eCivic != NO_CIVIC) accumHave(InfoRepo<CvCivicInfo>::get().get((int)eCivic), cand, rem);
	}
	if (pCity != NULL)
	{
		const std::vector<BuildingTypes> aHas = pCity->getHasBuildings();
		for (size_t i = 0; i < aHas.size(); ++i)
		{
			const CvInfo* jb = InfoRepo<CvBuildingInfo>::get().get((int)aHas[i]);
			// #430 obsoletion FLIP (owner 2026-07-07): a now-present obsolete building no longer UNLOCKS -- its
			// `enables` are SUPERSEDED (the same frontier outcome as the pre-flip world where legacy removed it),
			// so it is skipped from GENERATE. This keeps the frontier input INVARIANT to the flip -- the enabler
			// parity no longer leans on legacy obsoletion. (Its whenObsolete MODIFIER still delivers via the
			// operating-buildings obsolete set; only its enabler/frontier contribution stops here.)
			if (jb != NULL && obsoletedByHeldTech(jb, kTeam)) continue;
			accumHave(jb, cand, rem);
		}
	}
	for (int i = 0; i < NUM_EDGEB; ++i)
	{
		const std::set<int>& r = rem.a[i];
		for (std::set<int>::const_iterator jt = r.begin(); jt != r.end(); ++jt) cand.a[i].erase(*jt);
	}
}

bool EnablerKernel::obsoletedByOtherHeldTech(const CvInfo* j, const CvTeam& kTeam, TechTypes eExclude)
{
	if (j == NULL) return false;
	const std::vector<int>* p = j->edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS);
	if (p == NULL) return false;
	for (size_t i = 0; i < p->size(); ++i)
		if ((TechTypes)(*p)[i] != eExclude && kTeam.isHasTech((TechTypes)(*p)[i])) return true;
	return false;
}

void EnablerKernel::applyEdges(EnablerDomain& d, const CvInfo* j, EnEdgeBucket eBucket, int iDelta)
{
	if (j == NULL) return;
	const std::vector<int>* p = j->edge(EDGEF_ENABLES, eBucket);
	if (p != NULL) for (size_t i = 0; i < p->size(); ++i) d.addEnable((*p)[i], iDelta);
	static const EnEdgeFamily REMOVE_FAMILIES[] = { EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, NUM_EDGEF };
	for (int e = 0; REMOVE_FAMILIES[e] != NUM_EDGEF; ++e)
	{
		p = j->edge(REMOVE_FAMILIES[e], eBucket);
		if (p != NULL) for (size_t i = 0; i < p->size(); ++i) d.addRemove((*p)[i], iDelta);
	}
}

bool EnablerKernel::obsoletedByHeldTech(const CvInfo* j, const CvTeam& kTeam)
{
	if (j == NULL) return false;
	const std::vector<int>* p = j->edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS);
	if (p == NULL) return false;
	for (size_t i = 0; i < p->size(); ++i)
		if (kTeam.isHasTech((TechTypes)(*p)[i])) return true;
	return false;
}

// requires gate: build ∧ operate, evaluated through the typed-condition evaluator (StoneBase port) against the live
// engine ctx. The ENABLER reads with STRICT flags -- a {STATE_RELIGION:X} in requires.build must MATCH the player's
// state religion (the modifier's lenient compound is the modifier-side reading; json §3.5 / enabler §3).
bool EnablerKernel::requiresMet(const CvInfo* j, const CvCascadeEvalCtx& ec, bool bVisible)
{
	if (j == NULL) return true;
	CvCascadeEvalCtx gateEc = ec;
	gateEc.buildingAtomsPresence = true;   // the GATE reads the §7 has-list (presence), never the operate-derived active set
	CvCascadeEvalFlags flags;
	flags.strictStateReligionForBuild = true;
	flags.testVisible = bVisible;   // VISIBLE frontier: relax the greyable clauses (connectable resource / unadopted civic -> GREYED, enabler.md §6)
	if (j->requiresBuild() != NULL && !cascadeEvalCondition(j->requiresBuild(), gateEc, flags)) return false;
	if (j->requiresOperate() != NULL && !cascadeEvalCondition(j->requiresOperate(), gateEc, flags)) return false;
	return true;
}

bool EnablerKernel::allowedOk(const CvInfo* j, int iId, const CvPlayer& kPlayer, bool bUnit, EnEdgeBucket eBucket)
{
	if (j == NULL) return true;
	// PROJECTS (parity find 2026-07-02): the tally has NO project domain, so the cap check read buildingCount(projectId)
	// = 0 and every already-built world project stayed offered (canCreate casc=1 leg=0 on ENCYCLOPEDIA/IMF/EVOLUTION).
	// Projects read the engine-owned counts (the tally read-not-store philosophy): world = created-ever, team = held.
	const CvJsonAllowed* a = j->getAllowed();
	if (a == NULL) return true;
	if (eBucket == EDGEB_PROJECTS)
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
	if (eBucket == EDGEB_TECHS)
	{
		// TECHS read the engine-owned counts too (the tally read-not-store philosophy; the PROJECT precedent
		// above): world = ever-alive teams holding it (countKnownTechNumTeams -- techs are monotonic, so held
		// == ever-held); team/empire = the own team's held flag. The live authoring is the 29 world-unique
		// founder techs (allowed:{world:1}).
		for (std::map<std::string, int>::const_iterator it = a->all().begin(); it != a->all().end(); ++it)
		{
			int iCount = -1;
			if (it->first == "world")       iCount = GC.getGame().countKnownTechNumTeams((TechTypes)iId);
			else if (it->first == "team"
			      || it->first == "empire")  iCount = GET_TEAM(kPlayer.getTeam()).isHasTech((TechTypes)iId) ? 1 : 0;
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
		if (!bUnit && k == "empire"
		// identity.noInstanceLimit waives ONLY the empire (national-wonder) enforcement -- the cap stays
		// authored (it IS the wonder category); the PALACE relocate case (CvPlayer::isBuildingMaxedOut mirror)
		&& GC.getBuildingInfo((BuildingTypes)iId).isNoLimit())
		{
			continue;
		}
		const int iCount = bUnit ? cascadeTally().unitCount(iEntity, iId, eScope)
		                         : cascadeTally().buildingCount(iEntity, iId, eScope);
		if (iCount >= it->second) return false;
	}
	return true;
}

// (The empire-capability query lives in CascadeCapabilities -- the ONE derived-on-query union, cached per team;
// the kernel's former techs-only duplicate was folded into it 2026-07-02, capabilities.md.)

// canFoundReligion -- a PLAYER-WIDE state predicate (CvPlayer::canFoundReligion): NOT a JSON frontier, reproduced from
// game state so the enabler owns the gate (it is what enables/AI-reads the religion-founding action). >=1 city, not
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
void EnablerKernel::gateSet(EnEdgeBucket eBucket, const EnBucketSets& cand, const CvCascadeEvalCtx& ec,
	const CvPlayer& kPlayer, const CvTeam& kTeam, bool bUnit, std::set<int>& avail, bool bVisible)
{
	const std::set<int>& b = cand[eBucket];
	for (std::set<int>::const_iterator it = b.begin(); it != b.end(); ++it)
	{
		const CvInfo* j = jsonFor(eBucket, *it);
		if (obsoletedByHeldTech(j, kTeam)) continue;
		if (requiresMet(j, ec, bVisible) && allowedOk(j, *it, kPlayer, bUnit, eBucket)) avail.insert(*it);
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
	const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
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
	const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
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
	// dormancy" the owner flagged); the self-contained enabler must solve the fixpoint itself. LEAST fixpoint:
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
// The per-city active-building set (m_operatingBuildings.active/provided) is maintained by TARGETED
// PROPAGATION, not blanket-recomputed on every event: a HAVE-change ripples ONLY the affected buildings into the
// AUTHORITATIVE set (the recompute above stays the LOAD seed + the validation oracle). Mirrors the frontier's
// s_bc*/recheckHave (CvBuildingEnabler.cpp), extended to the operate<->provides fixpoint. enabler.md §7.
namespace {

static bool s_opIdxBuilt = false;
static std::vector<int> s_opPop, s_opPower, s_opReligion, s_opCorp, s_opGolden, s_opStateRel, s_opCivic, s_opTechAny, s_opDynamic;
static std::map<int, std::vector<int> > s_opPropBandConsumers;   // F5: PROPERTY_ id -> buildings whose requires.operate has a band on it
static std::map<int, std::set<int> > s_opPropThresholds;         // F5: PROPERTY_ id -> the sorted union of its band thresholds (the watermark's window boundaries)
static std::vector<int> s_opAnyBonus;                         // {buildings whose operate references ANY bonus} -- the whole-set re-check (plot-group membership change)
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
		else if (t.compare(0, 9, "PROPERTY_") == 0) { if (c->id >= 0) d.propertyBands[c->id] = std::make_pair(c->min, c->max); }  // F5: a property OPERATE band -- NOT dynamic; the watermark emits a targeted band-hit on a threshold crossing
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
		const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
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
		if (!d.bonuses.empty()) s_opAnyBonus.push_back(b);   // #430 G3: the whole-set bucket (a plot-group membership shift may move any of them)
		for (std::set<int>::const_iterator it = d.bonuses.begin(); it != d.bonuses.end(); ++it) s_opBonusConsumers[*it].push_back(b);
		for (std::set<int>::const_iterator it = d.buildings.begin(); it != d.buildings.end(); ++it) s_opBuildingDeps[*it].push_back(b);
		for (std::map<int, std::pair<int, int> >::const_iterator it = d.propertyBands.begin(); it != d.propertyBands.end(); ++it)
		{
			s_opPropBandConsumers[it->first].push_back(b);                       // F5: the property-band operate reverse-index
			if (it->second.first  != -1) s_opPropThresholds[it->first].insert(it->second.first);   // min boundary
			if (it->second.second != -1) s_opPropThresholds[it->first].insert(it->second.second);   // max boundary
		}
		const std::vector<int>& dorm = j->dormantTriggers();
		for (size_t i = 0; i < dorm.size(); ++i) s_opDormBy[dorm[i]].push_back(b);
		// obsolescence is tech-driven (obsoletedBy.techs): index the obsoletable buildings for the player-scope re-check.
		if (j->edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS) != NULL) s_opObsoletable.push_back(b);
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
	// #430 G3: a plot-group MEMBERSHIP change (the city moved group) can shift its ENTIRE connected-resource set, so
	// every bonus-operate building re-checks. A single bonus's access flip is the targeted onBonusAccessChangedActive.
	case CascadeAccumulator::CASC_HAVE_BONUS: ek_recheckActiveSet(pCity, s_opAnyBonus); break;
	default: break;
	}
}

// #430 G3: a SINGLE bonus's ACCESS (connection:trade network count, or connection:vicinity presence) flipped in
// pCity -> re-check ONLY the buildings whose requires.operate consumes THAT bonus (the reverse-FK bucket) into the
// authoritative operating set. The provides-ripple inside ek_recheckActiveSet carries any cascading operate flips.
void EnablerKernel::onBonusAccessChangedActive(const CvCity* pCity, int eBonus)
{
	buildActiveIndex();
	if (pCity == NULL || eBonus < 0) return;
	std::map<int, std::vector<int> >::const_iterator it = s_opBonusConsumers.find(eBonus);
	if (it != s_opBonusConsumers.end()) ek_recheckActiveSet(pCity, it->second);
}

// F5: a property crossed one of its operate-band thresholds in pCity (the property-engine watermark detected it) ->
// re-check ONLY the buildings whose requires.operate consumes THAT property's band into the authoritative operating
// set. Direction-less by design: ek_classifyBuilding re-reads the live value against the band, so high/low is
// redundant. Mirrors onBonusAccessChangedActive; the per-turn checkBuildings then applies the flip via setDisabledBuilding.
void EnablerKernel::onPropertyBandHitActive(const CvCity* pCity, int eProperty)
{
	buildActiveIndex();
	if (pCity == NULL || eProperty < 0) return;
	std::map<int, std::vector<int> >::const_iterator it = s_opPropBandConsumers.find(eProperty);
	if (it != s_opPropBandConsumers.end()) ek_recheckActiveSet(pCity, it->second);
}

const std::map<int, std::set<int> >& EnablerKernel::propertyBandThresholds()
{
	buildActiveIndex();
	return s_opPropThresholds;
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
