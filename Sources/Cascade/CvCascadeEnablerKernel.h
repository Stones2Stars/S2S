#pragma once
#ifndef CV_CASCADE_ENABLER_KERNEL_H
#define CV_CASCADE_ENABLER_KERNEL_H

//
//	EnablerKernel -- the shared GENERATE->GATE primitive + gate helpers of the #430 "can I?" machine (enabler.md §1-3):
//	ONE GENERATE->GATE over the InfoRepo `enables` edges, applied per gate. The per-domain cascades (TechCascade /
//	BuildingCascade / UnitCascade) and the generic civics/builds/projects/processes gates FEED themselves through these;
//	they are the single-implementation enabler primitives. See docs/architecture/patterns.md (the single-source law) +
//	docs/plans/structural-cleanup/cascade-engine-430.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx / CvCascadeEvalFlags -- the eval target for requires conditions
#include "CvCascadeCityFacts.h"       // CascadeCityFacts -- the standing per-city building-facts cache
#include <map>
#include <set>
#include <string>

class CvJsonInfo;
class CvPlayer;
class CvCity;
class CvTeam;

// The buckets keyed on the JSON `enables`/`obsoletes`/`replaces`/`disables` edge families -- one HAVE traversal fills
// them all. Shared by the kernel + the per-domain cascades + the shadow harness (promotions).
typedef std::map<std::string, std::set<int> > EnBucketSets;

class EnablerKernel
{
public:
	// The per-(bucket) InfoRepo dispatch -- the entity's CvJsonInfo by bucket name + id.
	static const CvJsonInfo* jsonFor(const std::string& b, int id);

	// Insert edge `key`'s targets (if present) into out.
	static void addEdge(const CvJsonInfo* j, const std::string& key, std::set<int>& out);

	// Accumulate one HAVE entity's source-side edges across every bucket (enables ADD to cand; obsoletes/replaces/
	// disables collected into rem for the post-gather set-difference).
	static void accumHave(const CvJsonInfo* j, EnBucketSets& cand, EnBucketSets& rem);

	// GENERATE (enabler.md §2). HAVE = team techs + adopted civics (+ the city's buildings if pCity != NULL).
	static void generate(const CvPlayer& kPlayer, const CvCity* pCity, EnBucketSets& cand);

	// Target-side obsoletedBy.techs: any held team tech obsoletes j.
	static bool obsoletedByHeldTech(const CvJsonInfo* j, const CvTeam& kTeam);

	// requires gate: build ∧ operate, through the typed-condition evaluator (STRICT state religion for build).
	static bool requiresMet(const CvJsonInfo* j, const CvCascadeEvalCtx& ec);

	// allowed cap gate: current tally count vs each scope cap (world/team/empire).
	static bool allowedOk(const CvJsonInfo* j, int iId, const CvPlayer& kPlayer, bool bUnit, const std::string& bucket = "");

	// canFoundReligion -- a PLAYER-WIDE state predicate reproduced from game state (CvPlayer::canFoundReligion).
	static bool canFoundReligion(const CvPlayer& kPlayer);

	// GATE: candidates[bucket] -> the available set (requires + allowed + obsoletedBy).
	static void gateSet(const std::string& bucket, const EnBucketSets& cand, const CvCascadeEvalCtx& ec,
		const CvPlayer& kPlayer, const CvTeam& kTeam, bool bUnit, std::set<int>& avail);

	// The PURE facts recompute: the two per-city building facts in ONE fixpoint pass. `activeOut` = the ACTIVE
	// (present ∧ operate-holds ∧ ¬dormant-trigger) building ids for pCity. DORMANCY is DERIVED from
	// `requires.operate` + its dormant triggers (the successor buildings whose presence dorms this) -- never the
	// engine active-building/`/state` (DEC-calc-zero-ride-in; dormancy is 100% governed by operate enablers).
	// `providedOut` = the union of every ACTIVE building's `provides.bonuses` -- the BONUS ids building supply
	// makes present IN-VICINITY (json §5a). This is CvCity::cascadeRefreshFacts' recompute target -- consumers
	// never call it directly; they read the STANDING cache via cityFacts()/wireFacts() below.
	static void recomputeCityFactsInto(const CvCity* pCity, std::set<int>& activeOut, std::set<int>& providedOut);

	// The STANDING per-city facts (CvCity::m_cascadeFacts, CvCascadeCityFacts.h) -- ensures freshness (event
	// dirty + the shared accumulator epoch + the turn-roll self-heal) and returns the cache. Replaces the
	// turn-scoped memo: the facts are EVENT-CORRECT, so the old "shadow-phase-only" caveat is closed.
	static const CascadeCityFacts& cityFacts(const CvCity* pCity);
	// Convenience: ensure + point ec.activeBuildings / ec.vicinityProvidedBonuses at the standing sets
	// (feeds cascadeIsBuildingActive + ev_vicinityHas; no per-call set copies).
	static void wireFacts(const CvCity* pCity, CvCascadeEvalCtx& ec);
};

#endif // CV_CASCADE_ENABLER_KERNEL_H
