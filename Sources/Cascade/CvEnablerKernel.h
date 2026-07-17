#pragma once
#ifndef CV_ENABLER_KERNEL_H
#define CV_ENABLER_KERNEL_H

//
//	EnablerKernel -- the shared GENERATE->GATE primitive + gate helpers of the #430 "can I?" machine (enabler.md §1-3):
//	ONE GENERATE->GATE over the InfoRepo `enables` edges, applied per gate. The per-domain enablers (TechEnabler /
//	BuildingEnabler / UnitEnabler) and the generic civics/builds/projects/processes gates FEED themselves through these;
//	they are the single-implementation enabler primitives. See docs/architecture/patterns.md (the single-source law) +
//	docs/plans/structural-cleanup/cascade-engine-430.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx / CvCascadeEvalFlags -- the eval target for requires conditions
#include "CvCascadeOperatingBuildings.h"       // OperatingBuildings -- the standing per-city operating-buildings cache
#include "CvJsonEdges.h"              // EnEdgeFamily/EnEdgeBucket -- the interned edge vocabulary (no strings at runtime)
#include <map>
#include <set>
#include <string>
#include <vector>

class CvInfo;
class CvJsonCondition;
class CvPlayer;
class CvCity;
class CvTeam;

// The per-bucket candidate/removal sets one HAVE traversal fills -- indexed by the INTERNED bucket enum
// (CvJsonEdges.h; no strings at runtime). Shared by the kernel + the per-domain enablers + the accumulator's
// frontier fills (promotions included).
struct EnBucketSets
{
	std::set<int> a[NUM_EDGEB];
	std::set<int>& operator[](EnEdgeBucket e) { return a[e]; }
	const std::set<int>& operator[](EnEdgeBucket e) const { return a[e]; }
};

// The requires-tree HAVE-atom dependency signature: which HAVE-classes gate an entity's requires. The ONE shape the
// three reverse-index builders (the building/unit frontier boxes + the kernel's operate index) collect into via
// EnablerKernel::scanCondDeps below; each consumer reads only the fields its buckets need.
struct CascadeCondDeps
{
	bool pop, power, religion, corp, goldenAge, stateReligion, civicAny;
	bool dynamic;             // a non-HAVE atom (live state no event carries) -- only set under bMarkDynamic
	std::set<int> techs;      // specific TECH_ ids referenced
	std::set<int> bonuses;    // specific BONUS_ ids referenced (presence or HAS_BONUS predicate)
	std::set<int> buildings;  // specific BUILDING_ ids referenced
	std::set<int> units;      // specific UNIT_ ids referenced -- only collected under bTrackUnits
	CascadeCondDeps() : pop(false), power(false), religion(false), corp(false), goldenAge(false),
		stateReligion(false), civicAny(false), dynamic(false) {}
};

class EnablerDomain;

class EnablerKernel
{
public:
	// The per-(bucket) InfoRepo dispatch -- the entity's CvInfo by bucket + id.
	static const CvInfo* jsonFor(EnEdgeBucket eBucket, int id);

	// Insert the (family, bucket) edge's targets (if present) into out.
	static void addEdge(const CvInfo* j, EnEdgeFamily eFamily, EnEdgeBucket eBucket, std::set<int>& out);

	// Accumulate one HAVE entity's source-side edges across every bucket (enables ADD to cand; obsoletes/replaces/
	// disables collected into rem for the post-gather set-difference).
	static void accumHave(const CvInfo* j, EnBucketSets& cand, EnBucketSets& rem);

	// GENERATE (enabler.md §2). HAVE = team techs + adopted civics (+ the city's buildings if pCity != NULL).
	static void generate(const CvPlayer& kPlayer, const CvCity* pCity, EnBucketSets& cand);

	// Target-side obsoletedBy.techs: any held team tech obsoletes j.
	static bool obsoletedByHeldTech(const CvInfo* j, const CvTeam& kTeam);
	// ... by a held tech OTHER than eExclude (the tech-delta's obsolete-present ripple counterfactual).
	static bool obsoletedByOtherHeldTech(const CvInfo* j, const CvTeam& kTeam, TechTypes eExclude);

	// The ONE domain-refcount edge applier (enabler.md par.7.1; DEC-single-implementation): apply/withdraw a
	// HAVE-source's edges into a domain -- enables.<bucket> -> the enable plane, obsoletes/replaces/
	// disables.<bucket> -> the remove plane. Every per-domain enabler's seed + event deltas route through this.
	static void applyEdges(EnablerDomain& d, const CvInfo* j, EnEdgeBucket eBucket, int iDelta);

	// requires gate: build ∧ operate, through the typed-condition evaluator (STRICT state religion for build).
	// bVisible=true relaxes the GREYABLE clauses (connectable resource / unadopted civic) for the visible frontier (enabler.md §6).
	static bool requiresMet(const CvInfo* j, const CvCascadeEvalCtx& ec, bool bVisible = false);

	// allowed cap gate: current tally count vs each scope cap (world/team/empire).
	static bool allowedOk(const CvInfo* j, int iId, const CvPlayer& kPlayer, bool bUnit, EnEdgeBucket eBucket = NO_EDGEB);

	// canFoundReligion -- a PLAYER-WIDE state predicate reproduced from game state (CvPlayer::canFoundReligion).
	static bool canFoundReligion(const CvPlayer& kPlayer);

	// GATE: candidates[bucket] -> the available set (requires + allowed + obsoletedBy). bVisible=true yields the
	// VISIBLE frontier (greyable clauses relaxed, enabler.md §6) for the build-list (bTestVisible) read.
	static void gateSet(EnEdgeBucket eBucket, const EnBucketSets& cand, const CvCascadeEvalCtx& ec,
		const CvPlayer& kPlayer, const CvTeam& kTeam, bool bUnit, std::set<int>& avail, bool bVisible = false);

	// The ONE requires-tree HAVE-atom scanner (recursing GROUP children + enabled/disabled): classifies PRESENCE
	// atoms by type prefix/token and PREDICATEs by predKind into `d`. The two legs that differ between the three
	// reverse-index builders are parameters: bTrackUnits collects UNIT_ presence atoms (the unit index's
	// requires-another-unit's-count leg); bMarkDynamic marks every untracked atom -- plus every BONUS_ reference
	// (trade/map/vicinity shifts aren't a discrete event) -- DYNAMIC, routing its entity to the bounded per-turn
	// re-check (the operate index). Over-inclusion is safe (a few extra re-checks).
	static void scanCondDeps(const CvJsonCondition* c, CascadeCondDeps& d, bool bTrackUnits, bool bMarkDynamic);

	// The PURE operating buildings recompute: the two per-city operating buildings in ONE fixpoint pass. `activeOut` = the ACTIVE
	// (present ∧ operate-holds ∧ ¬dormant-trigger) building ids for pCity. DORMANCY is DERIVED from
	// `requires.operate` + its dormant triggers (the successor buildings whose presence dorms this) -- never the
	// engine active-building/`/state` (DEC-calc-zero-ride-in; dormancy is 100% governed by operate enablers).
	// `providedOut` = the union of every ACTIVE building's `provides.bonuses` -- the BONUS ids building supply
	// makes present IN-VICINITY (json §5a). `obsoleteOut` = the PRESENT ∧ obsoleted-by-held-tech buildings (json §4.2):
	// a THIRD outcome collected in the SAME pass -- excluded from active/provides, it deposits its `whenObsolete` tree.
	// This is CvCity::refreshOperatingBuildings' recompute target -- consumers never call it directly; they read the
	// STANDING cache via operatingBuildings()/wireOperatingBuildings() below.
	static void recomputeOperatingBuildingsInto(const CvCity* pCity, std::set<int>& activeOut, std::set<int>& providedOut, std::set<int>& obsoleteOut);

	// --- ACTIVE-SET targeted maintenance (state-repositories.md: the active-building set is maintained by
	// targeted PROPAGATION, not blanket-recomputed). buildActiveIndex() inverts every building's `requires.operate`
	// into an operate-only reverse index at LOAD; the on*Active hooks ripple ONLY the affected buildings into the
	// AUTHORITATIVE m_operatingBuildings (active/provided/providedCount) in place -- the recompute above stays the load
	// SEED + the validation oracle. Mirrors the frontier's s_bc*/recheckHave, extended to the operate/provides fixpoint.
	static void buildActiveIndex();
	static void onBuildingChangedActive(const CvCity* pCity, int eBuilding);   // a building built/lost in pCity
	static void onHaveChangedActive(const CvCity* pCity, int eHaveKind);       // pop/religion/corp/power/bonus-whole-set (CASC_HAVE_*)
	static void onBonusAccessChangedActive(const CvCity* pCity, int eBonus);   // #430 G3: a SINGLE bonus's access (trade/vicinity) flipped -> re-check its operate consumers (reverse-FK targeted)
	static void onPlayerScopeChangedActive(const CvCity* pCity);              // tech/civic/golden-age (player scope)
	static void seedOperatingBuildings(const CvCity* pCity);                          // the LOAD seed: full recompute + the provider ref-count

	// The STANDING per-city operating buildings (CvCity::m_operatingBuildings, CvCascadeOperatingBuildings.h) -- ensures freshness (event
	// dirty + the shared accumulator epoch + the turn-roll self-heal) and returns the cache. Replaces the
	// turn-scoped memo: the operating buildings are EVENT-CORRECT, so the old "shadow-phase-only" caveat is closed.
	static const OperatingBuildings& operatingBuildings(const CvCity* pCity);
	// Convenience: ensure + point ec.activeBuildings / ec.vicinityProvidedBonuses at the standing sets
	// (feeds cascadeIsBuildingActive + ev_vicinityHas; no per-call set copies).
	static void wireOperatingBuildings(const CvCity* pCity, CvCascadeEvalCtx& ec);
};

#endif // CV_ENABLER_KERNEL_H
