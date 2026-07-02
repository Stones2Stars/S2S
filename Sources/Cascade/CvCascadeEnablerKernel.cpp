//
//	EnablerKernel -- the shared GENERATE->GATE primitive + gate helpers (see the header). Ported VERBATIM from
//	CvCascadeEnabler.cpp's file-static en_* helpers; promoted to a declared surface so every cascade + the shadow
//	harness reach the ONE implementation (the single-source law, patterns.md). LOGIC unchanged: only the signatures +
//	internal call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadePerfCount.h"   // per-turn call counters + stopwatches (owner 2026-07-02: repeat-calc hunt)
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "CvCascadeEnablerKernel.h"
#include "CvJsonInfo.h"
#include "Repos/InfoRepo.h"
#include "CvCascadeTally.h"
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "CvCascadeConditionEval.h"   // cascadeEvalCondition -- the StoneBase-ported typed-condition evaluator (was BoolExpr)
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvTechInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvProjectInfo.h"
#include "Infos/CvProcessInfo.h"
#include "Infos/CvPromotionInfo.h"
#include "Infos/CvBuildInfo.h"
#include "Engine/CvGame.h"

// The enables buckets this pass generates over (one HAVE traversal fills them all).
static const char* EN_BUCKETS[] = { "buildings", "units", "projects", "processes", "techs", "civics", "promotions", "builds", "hurries", NULL };

// The per-(bucket) InfoRepo dispatch -- the entity's CvJsonInfo by bucket name + id.
const CvJsonInfo* EnablerKernel::jsonFor(const std::string& b, int id)
{
	if (b == "buildings") return InfoRepo<CvBuildingInfo>::get().get(id);
	if (b == "units")     return InfoRepo<CvUnitInfo>::get().get(id);
	if (b == "projects")  return InfoRepo<CvProjectInfo>::get().get(id);
	if (b == "processes") return InfoRepo<CvProcessInfo>::get().get(id);
	if (b == "techs")     return InfoRepo<CvTechInfo>::get().get(id);
	if (b == "civics")    return InfoRepo<CvCivicInfo>::get().get(id);
	if (b == "promotions") return InfoRepo<CvPromotionInfo>::get().get(id);
	if (b == "builds")    return InfoRepo<CvBuildInfo>::get().get(id);
	// "hurries" has no InfoRepo (HURRY_ not in RJ_REPO_TYPES) -> NULL; canHurry needs only the enables.hurries edges
	// (on the civics/techs), and a NULL json passes requires/allowed -> the gate IS "the hurry type is generated".
	return NULL;
}

void EnablerKernel::addEdge(const CvJsonInfo* j, const std::string& key, std::set<int>& out)
{
	if (j == NULL) return;
	std::map<std::string, std::vector<int> >::const_iterator it = j->edges.find(key);
	if (it == j->edges.end()) return;
	for (size_t i = 0; i < it->second.size(); ++i) out.insert(it->second[i]);
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
		if (kTeam.isHasTech((TechTypes)iT)) accumHave(InfoRepo<CvTechInfo>::get().get(iT), cand, rem);
	for (int iCO = 0; iCO < GC.getNumCivicOptionInfos(); ++iCO)
	{
		const CivicTypes eCivic = kPlayer.getCivics((CivicOptionTypes)iCO);
		if (eCivic != NO_CIVIC) accumHave(InfoRepo<CvCivicInfo>::get().get((int)eCivic), cand, rem);
	}
	if (pCity != NULL)
	{
		const std::vector<BuildingTypes> aHas = pCity->getHasBuildings();
		for (size_t i = 0; i < aHas.size(); ++i) accumHave(InfoRepo<CvBuildingInfo>::get().get((int)aHas[i]), cand, rem);
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
	std::map<std::string, std::vector<int> >::const_iterator it = j->edges.find("obsoletedBy.techs");
	if (it == j->edges.end()) return false;
	for (size_t i = 0; i < it->second.size(); ++i)
		if (kTeam.isHasTech((TechTypes)it->second[i])) return true;
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
	if (j->requiresBuild != NULL && !cascadeEvalCondition(j->requiresBuild, ec, flags)) return false;
	if (j->requiresOperate != NULL && !cascadeEvalCondition(j->requiresOperate, ec, flags)) return false;
	return true;
}

bool EnablerKernel::allowedOk(const CvJsonInfo* j, int iId, const CvPlayer& kPlayer, bool bUnit, const std::string& bucket)
{
	if (j == NULL) return true;
	// PROJECTS (parity find 2026-07-02): the tally has NO project domain, so the cap check read buildingCount(projectId)
	// = 0 and every already-built world project stayed offered (canCreate casc=1 leg=0 on ENCYCLOPEDIA/IMF/EVOLUTION).
	// Projects read the engine-owned counts (the tally read-not-store philosophy): world = created-ever, team = held.
	if (bucket == "projects")
	{
		for (std::map<std::string, int>::const_iterator it = j->allowed.begin(); it != j->allowed.end(); ++it)
		{
			int iCount = -1;
			if (it->first == "world")      iCount = GC.getGame().getProjectCreatedCount((ProjectTypes)iId);
			else if (it->first == "team")  iCount = GET_TEAM(kPlayer.getTeam()).getProjectCount((ProjectTypes)iId);
			else if (it->first == "empire") iCount = GET_TEAM(kPlayer.getTeam()).getProjectCount((ProjectTypes)iId);
			if (iCount >= 0 && iCount >= it->second) return false;
		}
		return true;
	}
	for (std::map<std::string, int>::const_iterator it = j->allowed.begin(); it != j->allowed.end(); ++it)
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
// LIMITED_RELIGIONS_EXCEPTIONS carve-out). Reads raw state only -- a faithful mirror, shadowed vs the engine.
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

// COMPUTE the two per-city building facts in ONE pass. `activeOut` = the ACTIVE buildings for pCity (present ∧
// operate-holds ∧ ¬dormant-trigger). DORMANCY is DERIVED from `requires.operate` + its dormant triggers (the successor
// buildings whose presence dorms this) -- NEVER the engine active-building/`/state` read (DEC-calc-zero-ride-in; owner:
// dormancy is 100% governed by operate enablers). Present is the raw input hasBuilding (the un-dormancy-gated presence).
// `providedOut` = every ACTIVE building's `provides.bonuses` unioned -- the in-vicinity bonus supply (json §5a),
// computed from JSON, not the engine. `ecOp` = a COPY of ec with activeBuildings=NULL so a BUILDING_ predicate INSIDE an
// operate condition resolves via raw presence -- this breaks any recursion (operate conditions reference resources/
// civics in practice, not building-active).
typedef std::pair<std::set<int>, std::set<int> > FactsPair;
static std::map<int, FactsPair> s_memo;   // the turn-scoped facts memo (key: owner*100000 + city id)
static int s_iMemoTurn = -1;

void EnablerKernel::factsMemoClear() { s_memo.clear(); }

void EnablerKernel::computeCityBuildingFacts(const CvCity* pCity, const CvCascadeEvalCtx& ec, std::set<int>& activeOut, std::set<int>& providedOut)
{
	if (pCity == NULL) return;
	++CascadePerf::facts;
	PerfAccumTimer perfT(CascadePerf::factsMs);
	// TURN-SCOPED MEMO (perf, 2026-07-02): this is a full O(nBuildings) scan with a real operate-condition eval per
	// present building, and every cascade compute (percent stack, commerce ctxs, the getter instrument, the gates)
	// rebuilds it -- once the repos actually populated (the InfoRepo singleton fix) that latent cost made a logged
	// turn drag hard. The facts are stable within a turn for shadow/diagnostic purposes, so memo per (city, turn).
	// ⛔ SHADOW-PHASE ONLY correctness: a mid-turn building change goes stale until the next turn -- fine while the
	// legacy engine stays authoritative; MUST become event-invalidated (building-count DOMAIN events) before any
	// consumer cut relies on these facts live.
	const int iTurn = GC.getGame().getGameTurn();
	if (iTurn != s_iMemoTurn) { s_memo.clear(); s_iMemoTurn = iTurn; }
	const int iKey = ((int)pCity->getOwner()) * 100000 + pCity->getID();
	std::map<int, FactsPair>::const_iterator mit = s_memo.find(iKey);
	if (mit != s_memo.end()) { ++CascadePerf::factsMemoHit; activeOut = mit->second.first; providedOut = mit->second.second; return; }
	CvCascadeEvalCtx ecOp = ec;
	ecOp.activeBuildings = NULL;   // break recursion: operate's own BUILDING_ atoms resolve via raw presence
	CvCascadeEvalFlags flags;      // default flags
	const int nB = GC.getNumBuildingInfos();
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
		for (int b = 0; b < nB; ++b)
		{
			if (!pCity->hasBuilding((BuildingTypes)b)) continue;   // not present (raw input -- the un-dormancy-gated presence)
			const CvJsonInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
			// operate fails -> dormant.
			if (j != NULL && j->requiresOperate != NULL && !cascadeEvalCondition(j->requiresOperate, ecOp, flags)) continue;
			// a dormant-trigger successor is present -> dormant.
			bool dormant = false;
			if (j != NULL)
				for (size_t i = 0; i < j->dormantTriggers.size(); ++i)
					if (pCity->hasBuilding((BuildingTypes)j->dormantTriggers[i])) { dormant = true; break; }
			if (dormant) continue;
			activeOut.insert(b);   // active (under the CURRENT supply estimate)
			// This ACTIVE building's `provides.bonuses` supply those bonuses IN-VICINITY (json §5a).
			if (j != NULL)
			{
				std::map<std::string, std::vector<int> >::const_iterator pit = j->edges.find("provides.bonuses");
				if (pit != j->edges.end())
					for (size_t i = 0; i < pit->second.size(); ++i) provNext.insert(pit->second[i]);
			}
		}
		if (provNext == prov) break;   // supply stable -> the active set is the fixpoint
		prov.swap(provNext);
	}
	providedOut = prov;
	s_memo[iKey] = FactsPair(activeOut, providedOut);   // store for this turn's remaining computes on this city
}
