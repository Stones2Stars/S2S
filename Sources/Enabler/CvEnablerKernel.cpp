//
//	EnablerKernel -- the shared GENERATE->GATE primitive + gate helpers (see the header): the `enables` forward walk
//	and the `requires` gate, the ONE implementation every enabler reaches (the single-source law, patterns.md).
//

#include "CvGameCoreDLL.h"
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "Enabler/CvEnablerKernel.h"
#include "Enabler/CvEnabler.h"            // EnablerDomain -- the standardized domain the applyEdges deltas write
#include "CvInfo.h"
#include "CvTechInfo.h"        // cascadeStartNode -- the synthetic TECH_GAME_START root
#include "Repos/InfoRepo.h"
#include "Tally/CvTally.h"
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Spine/CvEventSpine.h"      // emitVicinityBonusChanged -- the ACTIVE half of the vicinity-supply fact
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "Conditions/CvConditionEval.h"   // cascadeEvalCondition -- the StoneBase-ported typed-condition evaluator
#include "CvCondition.h"       // CvCondition tree (CASC_COND_*/CASC_PRED_*) -- scanned for the operate reverse-index
#include "CvBuildingInfo.h"
#include "CvBonusInfo.h"       // the provided-bonus set's divergence naming
#include "CvUnitInfo.h"
#include "CvTechInfo.h"
#include "CvCivicInfo.h"
#include "CvProjectInfo.h"
#include "CvProcessInfo.h"
#include "CvPromotionInfo.h"
#include "CvBuildInfo.h"
#include "CvCorporationInfo.h"   // the CAN-I-EVER bar reads a corporation's own entity gate
#include "Engine/CvGame.h"

// The enables buckets this pass generates over (one HAVE traversal fills them all).
static const EnEdgeBucket EN_GEN_BUCKETS[] =
{
	EDGEB_BUILDINGS, EDGEB_UNITS, EDGEB_PROJECTS, EDGEB_PROCESSES, EDGEB_TECHS,
	EDGEB_CIVICS, EDGEB_PROMOTIONS, EDGEB_BUILDS, EDGEB_HURRIES, NO_EDGEB
};

// The per-(bucket) InfoRepo dispatch -- the entity's CvInfo by bucket + id.
const CvInfo* EnablerKernel::infoFor(EnEdgeBucket eBucket, int id)
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
	// Corporations are NOT an enabler domain (they spread, they are not offered from a queue -- the engine drives
	// that state, culture-religion-research.md), but the bucket must resolve so the CAN-I-EVER bar can read a
	// corporation's own entity gate.
	case EDGEB_CORPORATIONS: return InfoRepo<CvCorporationInfo>::get().get(id);
	// The BONUS axis is GATE-ONLY (enabler.md §8 resolved forks): a bonus never drives tree membership, but its
	// EDGEF_REQUIRED_BY dependents ARE what a bonus event re-gates, so the bucket must resolve to read them.
	case EDGEB_BONUSES:    return InfoRepo<CvBonusInfo>::get().get(id);
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

void EnablerKernel::supersededBy(EnEdgeBucket eBucket, int eId, std::vector<int>& superseded)
{
	superseded.clear();
	const CvInfo* pSuccessor = infoFor(eBucket, eId);
	if (pSuccessor == NULL)
	{
		return;
	}
	// The reverse pass landed every entity whose requires -- INCLUDING its dormant triggers -- names eId.
	const std::vector<int>* pDependents = pSuccessor->edge(EDGEF_REQUIRED_BY, eBucket);
	if (pDependents == NULL)
	{
		return;
	}
	for (size_t i = 0; i < pDependents->size(); ++i)
	{
		const CvInfo* pPredecessor = infoFor(eBucket, (*pDependents)[i]);
		if (pPredecessor == NULL)
		{
			continue;
		}
		// the EXACT predicate over the merged family: does this one go dormant TO eId, or merely reference it?
		const std::vector<int>& dormantSuccessors = pPredecessor->dormantTriggers();
		for (size_t iDormant = 0; iDormant < dormantSuccessors.size(); ++iDormant)
		{
			if (dormantSuccessors[iDormant] == eId)
			{
				superseded.push_back((*pDependents)[i]);
				break;
			}
		}
	}
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

// THE ONE PLAYER-DOMAIN HAVE APPLIER (see the header): apply the source's edges, then re-gate exactly the
// candidates it touches. Every player domain is this same mechanic with a different bucket.
void EnablerKernel::applyPlayerHave(const CvPlayer& kPlayer, EnablerDomain& d, EnEdgeBucket eBucket,
	const CvInfo* jSource, bool bHas)
{
	if (jSource == NULL || !d.isSeeded())
	{
		return;   // pre-init window: the object's own read/init emits replay its facts
	}
	// The touched set: the source's own enables/removal targets PLUS its EDGEF_REQUIRED_BY dependents -- the
	// par.7.1 step-2 re-gate, so a candidate whose `requires` merely REFERENCES this source re-evaluates too.
	std::set<int> touched;
	static const EnEdgeFamily FAMS[] = { EDGEF_ENABLES, EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, EDGEF_REQUIRED_BY, NUM_EDGEF };
	for (int f = 0; FAMS[f] != NUM_EDGEF; ++f)
	{
		const std::vector<int>* p = jSource->edge(FAMS[f], eBucket);
		if (p != NULL) touched.insert(p->begin(), p->end());
	}

	applyEdges(d, jSource, eBucket, bHas ? +1 : -1);

	CvCascadeEvalCtx ec;
	kPlayer.getEmpireContext().fillEvalCtx(ec);   // player+team -- the contexts ARE the eval state (contexts.md)
	CvCascadeEvalFlags gateFlags;
	for (std::set<int>::const_iterator it = touched.begin(); it != touched.end(); ++it)
	{
		if (!d.inTree(*it)) continue;
		const CvInfo* jCand = infoFor(eBucket, *it);
		d.setGateFailed(*it, (jCand != NULL && !cascadeGateOk(jCand->getGate(), ec, gateFlags))   // DEC-entity-gate
		                  || !requiresMet(jCand, ec)
		                  || !allowedOk(jCand, *it, kPlayer, /*bUnit*/ false, eBucket));
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

// The system-placement gate (see the header for the role it plays and why it is not the availability read).
bool EnablerKernel::everAvailable(EnEdgeBucket eBucket, int iId)
{
	if (iId < 0)
	{
		return false;
	}
	const CvInfo* j = infoFor(eBucket, iId);
	if (j == NULL)
	{
		return true;   // nothing to read a bar off -- an unresolvable bucket never bars
	}
	// A bare ctx is the POINT: every authored entity gate is a GAMEOPTION_ condition, which reads the live game
	// options directly and consults no scope context. Asking at game scope is what makes the answer the same for
	// every player and city, which is what "ever" means.
	CvCascadeEvalCtx ec;
	CvCascadeEvalFlags gateFlags;
	return cascadeGateOk(j->getGate(), ec, gateFlags);
}

bool EnablerKernel::requiresMetForPlayer(const CvPlayer& kPlayer, EnEdgeBucket eBucket, int iId)
{
	const CvInfo* j = infoFor(eBucket, iId);
	if (j == NULL) return false;
	CvCascadeEvalCtx ec;
	kPlayer.getEmpireContext().fillEvalCtx(ec);   // player+team -- the contexts fill the eval state (contexts.md)
	CvCascadeEvalFlags gateFlags;
	if (!cascadeGateOk(j->getGate(), ec, gateFlags)) return false;   // entity-level enabled/disabled ([DEC-entity-gate])
	return requiresMet(j, ec);
}

// The city twin (see the header). Same ONE evaluator, over a ctx the city + empire contexts fill.
bool EnablerKernel::requiresMetInCity(const CvCity& kCity, EnEdgeBucket eBucket, int iId, bool bVisible,
	const CvCascadeHypothetical* pHypothetical)
{
	const CvInfo* j = infoFor(eBucket, iId);
	if (j == NULL) return false;
	CvCascadeEvalCtx ec;
	kCity.getCityContext().fillEvalCtx(ec);                                  // city+plot
	GET_PLAYER(kCity.getOwner()).getEmpireContext().fillEvalCtx(ec);         // player+team
	wireOperatingBuildings(&kCity, ec);
	// The what-if rides the ctx the contexts filled -- it OVERRIDES individual have-atom answers and touches
	// nothing else, so every other read stays the real city's.
	ec.hypothetical = pHypothetical;
	CvCascadeEvalFlags gateFlags;
	gateFlags.strictStateReligionForBuild = true;
	if (!cascadeGateOk(j->getGate(), ec, gateFlags)) return false;
	return requiresMet(j, ec, bVisible);
}

bool EnablerKernel::allowedOk(const CvInfo* j, int iId, const CvPlayer& kPlayer, bool bUnit, EnEdgeBucket eBucket)
{
	if (j == NULL) return true;
	// PROJECTS (parity find 2026-07-02): the tally has NO project domain, so the cap check read buildingCount(projectId)
	// = 0 and every already-built world project stayed offered (canCreate casc=1 leg=0 on ENCYCLOPEDIA/IMF/EVOLUTION).
	// Projects read the engine-owned counts (the tally read-not-store philosophy): world = created-ever, team = held.
	const CvAllowed* a = j->getAllowed();
	if (a == NULL) return true;
	if (eBucket == EDGEB_PROJECTS)
	{
		for (int iKind = 0; iKind < NUM_ALLOWEDCAP; ++iKind)
		{
			const int iCap = a->cap((EnAllowedCap)iKind);
			if (iCap < 0) continue;
			int iCount = -1;
			if (iKind == ALLOWEDCAP_WORLD)
			{
				iCount = GC.getGame().getProjectCreatedCount((ProjectTypes)iId);
			}
			else if (iKind == ALLOWEDCAP_TEAM || iKind == ALLOWEDCAP_EMPIRE)
			{
				iCount = GET_TEAM(kPlayer.getTeam()).getProjectCount((ProjectTypes)iId);
			}
			if (iCount >= 0 && iCount >= iCap) return false;
		}
		return true;
	}
	if (eBucket == EDGEB_TECHS)
	{
		// TECHS go through the TALLY like every other count domain: counting is the tally's job, so the
		// world/team/empire resolution lives there ONCE instead of being re-derived here
		// ([DEC-single-implementation]; tally.md -- a bespoke engine count-loop IS an unwired tally domain).
		// The live authoring is the 29 world-unique founder techs (allowed:{world:1}).
		for (int iKind = 0; iKind < NUM_ALLOWEDCAP; ++iKind)
		{
			const int iCap = a->cap((EnAllowedCap)iKind);
			if (iCap < 0) continue;
			int iCount = -1;
			if (iKind == ALLOWEDCAP_WORLD)
			{
				iCount = cascadeTally().techCount((int)kPlayer.getID(), iId, CASCADE_COUNT_WORLD);
			}
			else if (iKind == ALLOWEDCAP_TEAM || iKind == ALLOWEDCAP_EMPIRE)
			{
				iCount = cascadeTally().techCount((int)kPlayer.getID(), iId, CASCADE_COUNT_EMPIRE);
			}
			if (iCount >= 0 && iCount >= iCap) return false;
		}
		return true;
	}
	for (int iKind = 0; iKind < NUM_ALLOWEDCAP; ++iKind)
	{
		const int iCap = a->cap((EnAllowedCap)iKind);
		if (iCap < 0) continue;
		CascadeCountScope eScope;
		int iEntity;
		if (iKind == ALLOWEDCAP_WORLD)
		{
			eScope = CASCADE_COUNT_WORLD;
			iEntity = 0;
		}
		else if (iKind == ALLOWEDCAP_TEAM)
		{
			eScope = CASCADE_COUNT_TEAM;
			iEntity = (int)kPlayer.getTeam();
		}
		else if (iKind == ALLOWEDCAP_EMPIRE)
		{
			eScope = CASCADE_COUNT_EMPIRE;
			iEntity = (int)kPlayer.getID();
		}
		else
		{
			continue;   // wonder-category caps -> the per-city category gate (enabler.md §8 open item)
		}
		if (!bUnit && iKind == ALLOWEDCAP_EMPIRE
		// identity.noInstanceLimit waives ONLY the empire (national-wonder) enforcement -- the cap stays
		// authored (it IS the wonder category); the PALACE relocate case (CvPlayer::isBuildingMaxedOut mirror)
		&& GC.getBuildingInfo((BuildingTypes)iId).isNoInstanceLimit())
		{
			continue;
		}
		const int iCount = bUnit ? cascadeTally().unitCount(iEntity, iId, eScope)
		                         : cascadeTally().buildingCount(iEntity, iId, eScope);
		if (iCount >= iCap) return false;
	}
	return true;
}

// (The empire-capability query lives on the PLAYER, as its own keyed union -- Engine/CapabilityContext.h;
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
		const CvInfo* j = infoFor(eBucket, *it);
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
	const CvProvides* pv = j->getProvides();
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
	//	The obsoleting TECH is asked of the city's own team -- the enabler's machinery reads team techs directly
	//	(its two other callers already hold a CvTeam&), which is exactly what the tech bridge is for. What the
	//	eval ctx must not carry is a team for a STATE question ([contexts.md]); this is neither the ctx's nor a
	//	state read.
	if (pCity != NULL && EnablerKernel::obsoletedByHeldTech(j, GET_TEAM(pCity->getTeam()))) return EK_OBSOLETE;
	if (j->requiresOperate() != NULL && !cascadeEvalCondition(j->requiresOperate(), ecOp, flags)) return EK_DORMANT_OPERATE;
	const std::vector<int>& dorm = j->dormantTriggers();
	for (size_t i = 0; i < dorm.size(); ++i)
		if (pCity->getCityContext().hasBuilding(dorm[i])) return EK_DORMANT_TRIGGER;   // the §7 has-list, through the context
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
	// The PURE fixpoint recompute: the LOAD SEED and the validation ORACLE, never the read path. It FULLY defines
	// its output every call, so a caller never sees a partially-filled set.
	activeOut.clear();
	providedOut.clear();
	obsoleteOut.clear();
	if (pCity == NULL) return;
	const CvPlayer& kOwner = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ecOp;
	pCity->getCityContext().fillEvalCtx(ecOp);      // city+plot -- the contexts fill the eval state (contexts.md)
	kOwner.getEmpireContext().fillEvalCtx(ecOp);    // player+team
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
	// ⛔ THE BOUND IS A RUNAWAY GUARD, NOT A TIER BUDGET. A manufactured chain lights TIER BY TIER (grain -> flour
	// -> bread -> deli, enabler.md § Load-end reconciliation), so each iteration admits exactly one more tier and a
	// low cap silently truncates the deep end of the chain -- every consumer above it dorms, and nothing re-derives
	// it ([DEC-no-self-heal]). The supply only ever GROWS (provides never retract inside the least fixpoint), so the
	// loop converges in at most one iteration per providing building; the bound only has to exceed the longest
	// possible chain. It mirrors the targeted ripple's own guard in this file, ASSERT included -- a truncated
	// fixpoint leaves the operating set WRONG and is a defect to fix at its cause, never a state to accept.
	const int iFixpointGuard = GC.getNumBuildingInfos() + 8;
	int iIterations = 0;
	std::set<int> prov;
	ecOp.vicinityProvidedBonuses = &prov;
	for (; iIterations < iFixpointGuard; ++iIterations)
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
	FAssertMsg(iIterations < iFixpointGuard, "the operate/provides fixpoint hit its runaway guard -- the operating set is left incomplete");
	providedOut = prov;
}

// ============================ the ACTIVE-SET targeted maintenance ==========================================
// The per-city active-building set (m_operatingBuildings.active/provided) is maintained by TARGETED
// PROPAGATION, not blanket-recomputed on every event: a HAVE-change ripples ONLY the affected buildings into the
// AUTHORITATIVE set (the recompute above stays the LOAD seed + the validation oracle). Mirrors the frontier's
// s_bc*/recheckHave (CvBuildingEnabler.cpp), extended to the operate<->provides fixpoint. enabler.md §7.
namespace {

static bool s_operateIndexBuilt = false;
static std::vector<int> s_operateNeedsPopulation, s_operateNeedsPower, s_operateNeedsReligion, s_operateNeedsCorporation, s_operateNeedsGoldenAge, s_operateNeedsStateReligion, s_operateNeedsCivic, s_operateNeedsTech, s_operateNeedsLiveState;
static std::map<int, std::vector<int> > s_operatePropertyBandConsumers;   // F5: PROPERTY_ id -> buildings whose requires.operate has a band on it
static std::map<int, std::set<int> > s_operatePropertyBandThresholds;         // F5: PROPERTY_ id -> the sorted union of its band thresholds (the watermark's window boundaries)
static std::vector<int> s_operateNeedsAnyBonus;                         // {buildings whose operate references ANY bonus} -- the whole-set re-check (plot-group membership change)
static std::map<int, std::vector<int> > s_operateBonusConsumers;   // BONUS_ id -> {buildings whose operate consumes it}
static std::map<int, std::vector<int> > s_operateBuildingDependents;     // BUILDING_ id -> {buildings whose operate references it}
static std::map<int, std::vector<int> > s_operateDormantTriggeredBy;           // trigger BUILDING_ id -> {buildings it dorms}
static std::vector<int> s_operateObsoletableBuildings;                      // buildings with any obsoletedBy.techs -> re-checked on a tech (player-scope) change (json §4.2)

// The work-list ripple: re-check `seeds` under the AUTHORITATIVE provided supply, updating active/provided/
// providedCount in place; on an active flip that crosses a bonus's provided-count 0<->1, push that bonus's operate
// consumers (the provides-ripple).
//
// ⚑ A CROSSING IS A VICINITY-SUPPLY FACT, AND IT IS ANNOUNCED (json.md §5a: a dormant building supplies nothing).
// The building-PRESENCE half already emits from processBuilding; this is the ACTIVE half, and without it the flip
// was visible to nothing outside this function -- vicinity-conditioned packages were never re-marked and the
// bonus's requires.BUILD dependents were never re-gated (this list walks only the OPERATE consumers). A missing
// emit is a silently wrong value ([DEC-no-self-heal]: a miss must surface, never be swept away).
//
// ⛔ The crossings are announced AFTER the fixpoint converges, never inside it, because `emit()` dispatches
// SYNCHRONOUSLY: a mid-loop emit hands every consumer a half-settled `provided` set to evaluate against. Today's
// consumers do not re-enter this function (the enabler's re-gates the BUILD tri-state, the modifier marks, the
// contexts refresh), so this is not unwinding a live recursion -- it is the invariant that keeps the fact honest
// as consumers are added. Banked-then-drained is the same discipline the load bracket uses.
static void ek_recheckActiveSet(const CvCity* pCity, const std::vector<int>& seeds)
{
	if (pCity == NULL || seeds.empty()) return;
	// ⚖ SET UP -> POPULATE -> ANNOUNCE (owner). The WORK runs during the load read and must: each building
	// deserializing does its own dormancy check, which is what lights a manufactured chain tier by tier (a
	// building that never checks itself never provides, so the next tier never lights). ⛔ What must NOT run
	// mid-read is the ANNOUNCE -- seedOperatingBuildings announces the settled set once at GAME_LOAD_FINISHED,
	// so an in-read emit would announce an activation a second time and a consumer APPLIES the moved source's
	// deposits on that fact.
	// ⚠ The set is idempotent and the deposits are NOT, so the enabler's own stored-vs-oracle tripwire is blind
	// to a double announce -- it is the modifier packages that carry the damage.
	const bool bAnnounce = !spineGameLoadInProgress();
	OperatingBuildings& f = pCity->m_operatingBuildings;   // AUTHORITATIVE
	const CvPlayer& kOwner = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ecOp;
	pCity->getCityContext().fillEvalCtx(ecOp);      // city+plot -- the contexts fill the eval state (contexts.md)
	kOwner.getEmpireContext().fillEvalCtx(ecOp);    // player+team
	ecOp.activeBuildings = NULL;                    // break recursion (operate BUILDING_ atoms -> raw presence)
	ecOp.vicinityProvidedBonuses = &f.provided;     // the LIVE authoritative supply (mutated as the ripple runs)
	CvCascadeEvalFlags flags;
	std::set<int> pending;
	std::vector<int> work;
	std::vector<std::pair<int, int> > supplyCrossings;   // (BONUS_ id, +1 gained / -1 lost) -- drained below
	for (size_t i = 0; i < seeds.size(); ++i) if (pending.insert(seeds[i]).second) work.push_back(seeds[i]);
	// A runaway guard, not a recovery: nothing repairs a truncated fixpoint, so tripping this leaves the
	// operating set WRONG and is a defect to fix at its cause ([DEC-no-self-heal]).
	int cap = GC.getNumBuildingInfos() + 512;
	while (!work.empty())
	{
		if (--cap < 0)
		{
			FAssertMsg(false, "operating-set fixpoint exceeded its runaway guard -- the set is left incomplete");
			break;
		}
		const int b = work.back(); work.pop_back();
		// The ONE shared verdict (ek_classifyBuilding) drives BOTH memberships. Obsolete-set maintenance (json
		// §4.2): present ∧ obsoleted-by-held-tech keeps the building in the parallel obsolete set (the whenObsolete
		// tree). An obsolete building classifies neither active nor dormant (checked FIRST, enabler.md §3.2), so an
		// active->obsolete move is an active DROP that runs the provides-ripple below.
		const bool wasObs = (f.obsolete.count(b) != 0);
		const bool was = (f.active.count(b) != 0);
		bool nowObs = false, now = false;
		if (pCity->getCityContext().hasBuilding(b))   // present at all -- the §7 has-list, through the context
		{
			const EkBuildingVerdict v = ek_classifyBuilding(b, pCity, ecOp, flags);
			nowObs = (v == EK_OBSOLETE);
			now = (v == EK_ACTIVE);
		}
		if (nowObs != wasObs)
		{
			if (nowObs) f.obsolete.insert(b); else f.obsolete.erase(b);
			// ⚖ OBSERVABILITY ONLY -- logging + the player NOTIFICATION (owner). The FATE is applied on the TECH
			// fact (a tech is the only thing that can obsolete), so nothing waits on this and no consumer may
			// route the apply through it ([enabler.md §3.2], [event-spine.md] player alerts).
			if (nowObs)
	{
		emitCityBuildingObsoletedAdded(pCity->getID(), (int)pCity->getOwner(), b);
	}
	else
	{
		emitCityBuildingObsoletedRemoved(pCity->getID(), (int)pCity->getOwner(), b);
	}
		}
		if (now == was) continue;
		// ⚖ ANNOUNCE THE OPERATE CROSSING. This is the play-time twin of the load seed's emit: the verdict is the
		// enabler's, so the enabler announces it, at the one place it changes. Consumers keyed on the operating
		// fact -- the city's amenity fold, the modifier's deposits, the free-promotion path -- see a dormancy flip
		// exactly as they see a construction, which is what makes a dormant building stop contributing.
		// ⛔ It is NOT a duplicate of the building-PRESENCE facts: presence cannot tell dormant from operating
		// (event-spine.md), and this fires only on a genuine active<->dormant change (`now == was` returned above).
		if (!bAnnounce)
		{
			// inside the load bracket: the set still MOVES (above), the seed announces it once at the end
		}
		else if (now)
		{
			emitCityBuildingActivated(pCity->getID(), pCity->getOwner(), b);
		}
		else
		{
			emitCityBuildingDormanted(pCity->getID(), pCity->getOwner(), b);
		}
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
					supplyCrossings.push_back(std::make_pair(bn, 1));   // GAINED -- announced after convergence
					std::map<int, std::vector<int> >::const_iterator cit = s_operateBonusConsumers.find(bn);
					if (cit != s_operateBonusConsumers.end())
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
					supplyCrossings.push_back(std::make_pair(bn, -1));   // LOST -- announced after convergence
					std::map<int, std::vector<int> >::const_iterator cit = s_operateBonusConsumers.find(bn);
					if (cit != s_operateBonusConsumers.end())
						for (size_t k = 0; k < cit->second.size(); ++k)
							if (pending.insert(cit->second[k]).second) work.push_back(cit->second[k]);
				}
			}
		}
	}

	// The fixpoint has settled -- announce every supply crossing it produced. Consumers (the modifier's
	// vicinity-conditioned packages, the enabler's own vicinity re-gate, the contexts) now see a coherent
	// `provided` set. A re-entrant flip lands as its own converged batch, so the chain terminates for the same
	// reason the fixpoint does: a no-op write crosses nothing and announces nothing.
	for (size_t i = 0; i < supplyCrossings.size(); ++i)
	{
		if (supplyCrossings[i].second > 0)
	{
		emitCityVicinityBonusAdded(pCity->getID(), pCity->getOwner(), supplyCrossings[i].first, supplyCrossings[i].second);
	}
	else
	{
		emitCityVicinityBonusRemoved(pCity->getID(), pCity->getOwner(), supplyCrossings[i].first, -supplyCrossings[i].second);
	}
	}
}

} // namespace

// The ONE requires-tree HAVE-atom scanner (see the header) -- was three near-identical file-static copies
// (bc_scanCond / uc_scanCond / ek_scanOp); the legs that differed are the two flags.
void EnablerKernel::scanCondDeps(const CvCondition* c, CascadeCondDeps& d, bool bTrackUnits, bool bMarkDynamic)
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
		else if (t.compare(0, 9, "RELIGION_") == 0) { d.religion = true; if (c->id >= 0) d.religions.insert(c->id); }
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
		// The in-city one sets BOTH and breaks on its own -- sharing the fallthrough would hand the precise
		// flag to its three siblings, which is exactly the widening it exists to avoid.
		case CASC_PRED_STATE_RELIGION_IN_CITY:      d.stateReligionInCity = true; d.stateReligion = true; break;
		case CASC_PRED_HAS_STATE_RELIGION:
		case CASC_PRED_STATE_RELIGION:
		case CASC_PRED_IS_STATE_RELIGION_HOLY_CITY: d.stateReligion = true; break;
		// COASTAL is a `requires` CONDITION in the JSON model, never a property of the entity -- a rebuilt info
		// carries no isWater(), because "needs a coast" is something the city must supply, not something the
		// building IS. A consumer weighting a coastal-only candidate by how much coastline the empire has reads
		// it from HERE rather than re-walking the tree ([DEC-single-implementation]).
		case CASC_PRED_HAS_COAST:               d.coastal = true; break;
		case CASC_PRED_HAS_BONUS:               if (c->id >= 0) d.bonuses.insert(c->id); if (bMarkDynamic) d.dynamic = true; break;
		default:                                if (bMarkDynamic) d.dynamic = true; break;   // IS_CAPITAL / counts / plot / connection -- read LIVE at eval
		}
		return;
	}
}

void EnablerKernel::buildActiveIndex()
{
	if (s_operateIndexBuilt) return;
	s_operateIndexBuilt = true;
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
		if (j == NULL) continue;
		CascadeCondDeps d;
		// OPERATE only -- active/dormant is governed by requires.operate; DYNAMIC marked (the per-turn re-check bucket).
		scanCondDeps(j->requiresOperate(), d, /*bTrackUnits*/ false, /*bMarkDynamic*/ true);
		if (d.pop)            s_operateNeedsPopulation.push_back(b);
		if (d.power)          s_operateNeedsPower.push_back(b);
		if (d.religion)       s_operateNeedsReligion.push_back(b);
		if (d.corp)           s_operateNeedsCorporation.push_back(b);
		if (d.goldenAge)      s_operateNeedsGoldenAge.push_back(b);
		if (d.stateReligion)  s_operateNeedsStateReligion.push_back(b);
		if (d.civicAny)       s_operateNeedsCivic.push_back(b);
		if (!d.techs.empty()) s_operateNeedsTech.push_back(b);
		if (d.dynamic)        s_operateNeedsLiveState.push_back(b);
		if (!d.bonuses.empty()) s_operateNeedsAnyBonus.push_back(b);   // #430 G3: the whole-set bucket (a plot-group membership shift may move any of them)
		for (std::set<int>::const_iterator it = d.bonuses.begin(); it != d.bonuses.end(); ++it) s_operateBonusConsumers[*it].push_back(b);
		for (std::set<int>::const_iterator it = d.buildings.begin(); it != d.buildings.end(); ++it) s_operateBuildingDependents[*it].push_back(b);
		for (std::map<int, std::pair<int, int> >::const_iterator it = d.propertyBands.begin(); it != d.propertyBands.end(); ++it)
		{
			s_operatePropertyBandConsumers[it->first].push_back(b);                       // F5: the property-band operate reverse-index
			if (it->second.first  != -1) s_operatePropertyBandThresholds[it->first].insert(it->second.first);   // min boundary
			if (it->second.second != -1) s_operatePropertyBandThresholds[it->first].insert(it->second.second);   // max boundary
		}
		const std::vector<int>& dorm = j->dormantTriggers();
		for (size_t i = 0; i < dorm.size(); ++i) s_operateDormantTriggeredBy[dorm[i]].push_back(b);
		// obsolescence is tech-driven (obsoletedBy.techs): index the obsoletable buildings for the player-scope re-check.
		if (j->edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS) != NULL) s_operateObsoletableBuildings.push_back(b);
	}
}

void EnablerKernel::onBuildingChangedActive(const CvCity* pCity, int eBuilding)
{
	buildActiveIndex();
	if (pCity == NULL || eBuilding < 0) return;
	std::vector<int> seeds;
	seeds.push_back(eBuilding);
	std::map<int, std::vector<int> >::const_iterator dep = s_operateBuildingDependents.find(eBuilding);
	if (dep != s_operateBuildingDependents.end()) seeds.insert(seeds.end(), dep->second.begin(), dep->second.end());
	std::map<int, std::vector<int> >::const_iterator dm = s_operateDormantTriggeredBy.find(eBuilding);
	if (dm != s_operateDormantTriggeredBy.end()) seeds.insert(seeds.end(), dm->second.begin(), dm->second.end());
	ek_recheckActiveSet(pCity, seeds);
}

void EnablerKernel::onHaveChangedActive(const CvCity* pCity, int eHaveKind)
{
	buildActiveIndex();
	if (pCity == NULL) return;
	switch (eHaveKind)
	{
	case CASC_HAVE_POP:   ek_recheckActiveSet(pCity, s_operateNeedsPopulation); break;
	case CASC_HAVE_POWER: ek_recheckActiveSet(pCity, s_operateNeedsPower); break;
	case CASC_HAVE_CORP:  ek_recheckActiveSet(pCity, s_operateNeedsCorporation); break;
	case CASC_HAVE_RELIGION:
	{
		std::vector<int> seeds(s_operateNeedsReligion);
		seeds.insert(seeds.end(), s_operateNeedsStateReligion.begin(), s_operateNeedsStateReligion.end());
		ek_recheckActiveSet(pCity, seeds);
		break;
	}
	// #430 G3: a plot-group MEMBERSHIP change (the city moved group) can shift its ENTIRE connected-resource set, so
	// every bonus-operate building re-checks. A single bonus's access flip is the targeted onBonusAccessChangedActive.
	case CASC_HAVE_BONUS: ek_recheckActiveSet(pCity, s_operateNeedsAnyBonus); break;
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
	std::map<int, std::vector<int> >::const_iterator it = s_operateBonusConsumers.find(eBonus);
	if (it != s_operateBonusConsumers.end()) ek_recheckActiveSet(pCity, it->second);
}

// A property crossed one of its operate-band boundaries in pCity (the enabler's own spine consumer detects it off
// SEVT_PROPERTY_ADDED / _REMOVED against propertyBandThresholds) -> re-check ONLY the buildings whose
// requires.operate consumes THAT property's band into the authoritative operating set. Direction-less by design:
// ek_classifyBuilding re-reads the live value against the band, so high/low is redundant. Mirrors
// onBonusAccessChangedActive; the operate fixpoint below is the authoritative verdict.
void EnablerKernel::onPropertyBandHitActive(const CvCity* pCity, int eProperty)
{
	buildActiveIndex();
	if (pCity == NULL || eProperty < 0) return;
	std::map<int, std::vector<int> >::const_iterator it = s_operatePropertyBandConsumers.find(eProperty);
	if (it != s_operatePropertyBandConsumers.end()) ek_recheckActiveSet(pCity, it->second);
}

const std::map<int, std::set<int> >& EnablerKernel::propertyBandThresholds()
{
	buildActiveIndex();
	return s_operatePropertyBandThresholds;
}

void EnablerKernel::onPlayerScopeChangedActive(const CvCity* pCity)
{
	buildActiveIndex();
	if (pCity == NULL) return;
	// tech/civic/golden-age player-scope change: re-check the buildings whose operate references any of them.
	// Over-inclusive (we don't get the specific tech/civic here), but these events are infrequent -- amortized cheap.
	std::vector<int> seeds(s_operateNeedsTech);
	seeds.insert(seeds.end(), s_operateNeedsCivic.begin(), s_operateNeedsCivic.end());
	seeds.insert(seeds.end(), s_operateNeedsGoldenAge.begin(), s_operateNeedsGoldenAge.end());
	seeds.insert(seeds.end(), s_operateObsoletableBuildings.begin(), s_operateObsoletableBuildings.end());   // tech research flips obsolescence (json §4.2)
	ek_recheckActiveSet(pCity, seeds);
}

// The WHOLE per-city operating set recomputed FROM SOURCE into a CALLER-OWNED buffer: the fixpoint's three
// sets plus the provider ref-count the ripple bookkeeps. Two callers, one implementation -- the LOAD/creation
// SEED points it at the city's own storage, and the ENDPOINT ORACLE points it at a scratch buffer it owns
// (state-repositories.md, the endpoint oracle). Handing it the destination is what makes "the oracle cannot
// repair the maintained set" structural rather than a discipline: it is never given the stored set at all.
void EnablerKernel::recomputeOperatingSetInto(const CvCity* pCity, OperatingBuildings& kOut)
{
	kOut.active.clear();
	kOut.obsolete.clear();
	kOut.provided.clear();
	kOut.providedCount.clear();
	if (pCity == NULL) return;
	recomputeOperatingBuildingsInto(pCity, kOut.active, kOut.provided, kOut.obsolete);
	for (std::set<int>::const_iterator it = kOut.active.begin(); it != kOut.active.end(); ++it)
	{
		const std::vector<int>* pProvidedBonuses = ek_provides(*it);
		if (pProvidedBonuses == NULL)
		{
			continue;
		}
		for (size_t iBonusIndex = 0; iBonusIndex < pProvidedBonuses->size(); ++iBonusIndex)
		{
			kOut.providedCount[(*pProvidedBonuses)[iBonusIndex]]++;
		}
	}
}

// The LOAD seed: the ONE full recompute into the city's own storage. It runs
// once per load/reset/city-creation; the on*Active hooks maintain the set in place thereafter (targeted
// propagation, never a blanket recompute -- enabler.md §3.2), so the full recompute never runs per-event again.
void EnablerKernel::seedOperatingBuildings(const CvCity* pCity)
{
	if (pCity == NULL) return;
	recomputeOperatingSetInto(pCity, pCity->m_operatingBuildings);
	// ANNOUNCE the computed verdict: every building this seed just resolved as OPERATING becomes processed-in.
	// Without it the operating axis is silent on a load -- processBuilding is a play-time path, so a consumer
	// keyed on the operating fact (the city's amenity fold) could never build. ⛔ This is NOT the banned
	// state-walking pseudo-emit (superseded-ideas #13): that fabricates PRESENCE facts by walking already-populated
	// objects, whereas the operate verdict is DERIVED and is being computed right here -- the fact is announced by
	// the derivation that produces it, which is the ordinary emit contract.
	const std::set<int>& kActive = pCity->m_operatingBuildings.active;
	for (std::set<int>::const_iterator it = kActive.begin(); it != kActive.end(); ++it)
	{
		emitCityBuildingActivated(pCity->getID(), pCity->getOwner(), *it);
	}
}

const OperatingBuildings& EnablerKernel::operatingBuildings(const CvCity* pCity)
{
	// A BARE FETCH of the AUTHORITATIVE per-city set, unconditionally. There is no recompute on this path and
	// there must not be one: the set is built ONCE by seedOperatingBuildings (city creation / the load seed) and
	// kept current in place by the targeted on*Active hooks above (state-repositories.md: the enabler's sets are
	// maintained by targeted PROPAGATION, never blanket-invalidated-and-recomputed -- blanket-recomputing the
	// fixpoint per event is DESPAIR_INDEX #2). A propagation that fails to fire therefore leaves the set visibly
	// wrong, which is how the missing hook gets found ([DEC-no-self-heal]); an external reader finds it by
	// diffing this served set against the endpoint oracle's fresh recompute.
	return pCity->m_operatingBuildings;
}

void EnablerKernel::dormedByBuilding(const CvCity* pCity, int eCandidate, std::vector<int>& kOut)
{
	kOut.clear();
	if (pCity == NULL || eCandidate < 0)
	{
		return;
	}
	buildActiveIndex();

	// The dormant-trigger index is the STATIC half -- "who names eCandidate as the successor that dorms them",
	// inverted once at load. Nothing here evaluates a condition or asks a what-if.
	const std::map<int, std::vector<int> >::const_iterator kDormed = s_operateDormantTriggeredBy.find(eCandidate);
	if (kDormed == s_operateDormantTriggeredBy.end())
	{
		return;
	}
	// ...and the standing ACTIVE set is the LIVE half: a building that is already dormant or obsolete here
	// contributes nothing today, so superseding it nets out nothing.
	const OperatingBuildings& kOperating = operatingBuildings(pCity);
	for (size_t iI = 0; iI < kDormed->second.size(); ++iI)
	{
		const int iBuilding = kDormed->second[iI];
		if (kOperating.active.count(iBuilding) > 0)
		{
			kOut.push_back(iBuilding);
		}
	}
}

bool EnablerKernel::cityHasVicinityBonus(const CvCity* pCity, int eBonus, CvCascVicinity eTier)
{
	if (pCity == NULL)
	{
		return false;
	}
	// Half one -- the ENABLER's: an ACTIVE building here that `provides` this bonus supplies it in-vicinity
	// (json §5a). Only the enabler can answer it, because an operating building's own operate clause may consume
	// a bonus another operating building provides -- that is the least fixpoint this set already resolved.
	if (operatingBuildings(pCity).provided.count(eBonus) != 0)
	{
		return true;
	}
	// Half two -- the CITY CONTEXT's: the MAP providers over the workable radius, at the asked ownership tier.
	return pCity->getCityContext().hasVicinityBonusAt(eBonus, eTier);
}

void EnablerKernel::wireOperatingBuildings(const CvCity* pCity, CvCascadeEvalCtx& ec)
{
	const OperatingBuildings& f = operatingBuildings(pCity);
	ec.activeBuildings = &f.active;
	ec.obsoleteBuildings = &f.obsolete;
	ec.vicinityProvidedBonuses = &f.provided;
}
