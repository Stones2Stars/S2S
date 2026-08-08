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

#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx / CvCascadeEvalFlags -- the eval target for requires conditions
#include "Enabler/CvOperatingBuildings.h"       // OperatingBuildings -- the standing per-city operating-buildings cache
#include "CvEdges.h"              // EnEdgeFamily/EnEdgeBucket -- the interned edge vocabulary (no strings at runtime)
#include <map>
#include <set>
#include <string>
#include <vector>

class CvInfo;
class CvCondition;
class CvPlayer;
class CvCity;
class CvTeam;

// The per-bucket candidate/removal sets one HAVE traversal fills -- indexed by the INTERNED bucket enum
// (CvEdges.h; no strings at runtime). Shared by the kernel + the per-domain enablers + the accumulator's
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
	// The PRECISE state-religion predicate, beside the collapsed `stateReligion` above. The collapsed flag
	// unions all four spellings because RE-GATING wants breadth (over-inclusion is safe, enabler.md §5); a
	// consumer asking what an entity actually NEEDS wants this one, which is the in-city requirement alone.
	bool stateReligionInCity;
	bool coastal;             // the entity needs a COAST (CASC_PRED_HAS_COAST) -- a requires CONDITION in the
	                          // JSON model, never an entity property, so a rebuilt info carries no isWater()
	bool dynamic;             // a non-HAVE atom (live state no event carries) -- only set under bMarkDynamic
	std::set<int> techs;      // specific TECH_ ids referenced
	std::set<int> bonuses;    // specific BONUS_ ids referenced (presence or HAS_BONUS predicate)
	std::set<int> buildings;  // specific BUILDING_ ids referenced
	std::set<int> units;      // specific UNIT_ ids referenced -- only collected under bTrackUnits
	std::set<int> religions;  // specific RELIGION_ ids referenced (the id half of the `religion` flag above)
	std::map<int, std::pair<int, int> > propertyBands;  // PROPERTY_ id -> the operate band [min,max] (F5: -1 = unset)
	// ==== THE PLOT-SUBSTRATE AXES -- the ids a plot fact carries ====
	// ⛔ These do NOT ride EDGEF_REQUIRED_BY, and that is a DELIBERATE exclusion, not a hole to route around:
	// CvReversePass's rp_requiredByRefInfo returns NULL for every plot-substrate prefix, so no terrain / feature
	// / improvement / route / mapcategory info ever carries a REQUIRED_BY edge. enabler.md par.8 states the
	// shape that replaces it -- "a coarse list matches a coarse event" -- so the ids are collected HERE and each
	// enabler compiles its own id -> dependents list from them.
	// ⚠ Reading the empty reverse edge instead is silent: the walk succeeds, finds nothing, and re-gates nobody,
	// which is indistinguishable from "no candidate needed re-gating" at every observation point.
	std::set<int> terrains;        // TERRAIN_ ids (presence, or a parameterized HAS_TERRAIN)
	std::set<int> features;        // FEATURE_ ids (presence, or a parameterized HAS_FEATURE)
	std::set<int> improvements;    // IMPROVEMENT_ ids (presence, or a parameterized HAS_IMPROVEMENT)
	std::set<int> routes;          // ROUTE_ ids
	std::set<int> mapCategories;   // MAPCATEGORY_ ids -- a plot's categories are DERIVED from its terrain
	                               // (CvPlot::getMapCategories forwards to the terrain info), so a TERRAIN fact
	                               // is what moves them and there is no mapcategory fact of its own
	std::set<int> plotPredicates;  // the bare CASC_PRED_* plot bits (HAS_RIVER / IS_WATER / HAS_COAST / ...),
	                               // which carry no id and ride SEVT_PLOT_PREDICATE_ADDED / _REMOVED
	CascadeCondDeps() : pop(false), power(false), religion(false), corp(false), goldenAge(false),
		stateReligion(false), stateReligionInCity(false), civicAny(false), coastal(false), dynamic(false) {}
};

// The plot atom a re-gate keys on. The id spaces overlap across kinds, so a candidate index keys on the PAIR
// (kind, id) -- never on the id alone.
enum PlotAtomKind
{
	PLOTATOM_TERRAIN = 0,
	PLOTATOM_FEATURE,
	PLOTATOM_IMPROVEMENT,
	PLOTATOM_ROUTE,
	PLOTATOM_MAPCATEGORY,
	PLOTATOM_PREDICATE,
	NUM_PLOTATOM_KINDS
};

class EnablerDomain;


// The HAVE-axis kinds an operate re-check keys on. These name ENABLER axes (which dependents re-gate when a
// city gains/loses a have), so they live with the enabler -- they were previously declared inside the modifier
// accumulator, which is why the enabler carried an include of the cascade it is not part of.
enum CascadeHaveKind { CASC_HAVE_POP = 0, CASC_HAVE_RELIGION, CASC_HAVE_CORP, CASC_HAVE_POWER, CASC_HAVE_BONUS };

class EnablerKernel
{
public:
	// The per-(bucket) InfoRepo dispatch -- the entity's CvInfo by bucket + id.
	static const CvInfo* infoFor(EnEdgeBucket eBucket, int id);

	// THE PLOT-ATOM SEEDS a fact resolves to -- the ONE place the terrain -> mapcategory hop lives, so both
	// domains ask rather than each carrying a copy ([DEC-single-implementation]).
	// ⚑ A TERRAIN fact seeds its own (PLOTATOM_TERRAIN, id) AND one (PLOTATOM_MAPCATEGORY, id) per category that
	// terrain carries, because a plot's map categories are DERIVED from its terrain and have no fact of their
	// own; every other kind seeds itself alone.
	static void plotAtomSeeds(int eKind, int iId, std::vector<std::pair<int, int> >& atomsOut);

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

	// THE SUPERSEDED SET -- "which entities of this kind does eId push into dormancy?", the REVERSE of the
	// dormant edge. The legacy `ReplacementBuildings` read the relation from the successor's side; the curated
	// model authors it target-side, as each PREDECESSOR's `requires.operate.dormant` naming its successor
	// (enabler.md §2: a building replacement is reversible DORMANCY, never a `replaces` removal). So the
	// forward question the enabler asks every turn -- "is a successor of mine present?" -- and this one are the
	// two directions of one edge, and this is the only place the reverse is walked
	// ([DEC-single-implementation]).
	//
	// ⚠ EDGEF_REQUIRED_BY is a MERGED bucket: it also carries every entity whose requires.build/operate merely
	// REFERENCES eId (enabler.md's edge-family caution -- a consumer with ALL semantics cannot read it raw).
	// So each candidate is confirmed against its OWN dormantTriggers, which is the exact predicate the caution
	// prescribes; without it a building that simply REQUIRES eId would read as superseded by it.
	static void supersededBy(EnEdgeBucket eBucket, int eId, std::vector<int>& superseded);

	// The ONE domain-refcount edge applier (enabler.md par.7.1; DEC-single-implementation): apply/withdraw a
	// HAVE-source's edges into a domain -- enables.<bucket> -> the enable plane, obsoletes/replaces/
	// disables.<bucket> -> the remove plane. Every per-domain enabler's seed + event deltas route through this.
	static void applyEdges(EnablerDomain& d, const CvInfo* j, EnEdgeBucket eBucket, int iDelta);

	// THE ONE PLAYER-DOMAIN HAVE APPLIER -- the whole per-domain mechanic in one place, parameterized by the
	// BUCKET exactly as everAvailable is. Acquiring or losing a HAVE source applies its edges into the domain
	// (the refcounted membership formula) and re-gates the candidates that source touches -- its own
	// enables/removal targets plus its EDGEF_REQUIRED_BY dependents (enabler.md par.7.1 steps 1+2).
	// ⛔ There is nothing domain-specific left to write: EnablerDomain is generic, applyEdges is generic and the
	// gate is generic, so a per-domain CLASS varies only by (bucket, repo, event) -- parameters, not code. Minting
	// one per info is the duplication this collapses ([DEC-single-implementation]).
	static void applyPlayerHave(const CvPlayer& kPlayer, EnablerDomain& d, EnEdgeBucket eBucket,
		const CvInfo* jSource, bool bHas);

	// requires gate: build ∧ operate, through the typed-condition evaluator (STRICT state religion for build).
	// bVisible=true relaxes the GREYABLE clauses (connectable resource / unadopted civic) for the visible frontier (enabler.md §6).
	static bool requiresMet(const CvInfo* j, const CvCascadeEvalCtx& ec, bool bVisible = false);

	// THE CAN-I-EVER BAR (enabler.md par.8) -- "is this entity barred for the WHOLE GAME, whoever asks, whatever
	// they hold?" It is the PERMANENT half the tri-state deliberately does not model: HIDDEN conflates "nothing
	// enables it YET" with "it can never be offered", and a valuation asking what an unlock is WORTH needs the
	// second. The picking-side twin of CvPlayer::canEverResearch, which carries the same shape for techs.
	//
	// The bar IS the entity-level gate ([DEC-entity-gate]) and nothing else: a whole-entity game-option gate
	// authors as `enabled`/`disabled`, so the option read lives HERE, once, for every domain the enabler deals
	// with -- never as a per-entity point getter on the info (an info never reads game state, json.md par.9).
	// Evaluated against a bare ctx BY DESIGN: every authored gate is a GAMEOPTION_ leaf, which reads the live
	// options and needs no scope context. A game option is fixed at setup, so the verdict is stable for the game.
	//
	// TOTAL over every bucket: CvInfo::getGate() is declared on the BASE returning NULL and cascadeGateOk(NULL)
	// is true, so an entity carrying no gate answers "never barred" and a newly-authored gate lights up as pure
	// DATA. ⛔ Do not add a per-domain variant -- the bucket parameter IS the domain axis.
	static bool everAvailable(EnEdgeBucket eBucket, int iId);

	// THE SYSTEM-PLACEMENT GATE -- `requires` for a caller that PLACES an entity rather than offering it through a
	// city's production queue: the trigger plane's spawns, the barbarian fielding filter, event/WB placements.
	//
	// It is a different question from availability, not a variant of it. The tri-state answers what the QUEUE
	// offers -- membership AND `requires` AND the `allowed` cap -- and a placing system is none of those: it is not
	// choosing from a queue, so the cap it would compete for does not bind it (enabler.md §4 keeps `allowed` a
	// SEPARATE gate: "how many of ME may exist", not "what I need"). Nor can it read the tri-state at all, since a
	// system-placed entity is statically excluded and therefore permanently HIDDEN (`identity.spawnOnly` /
	// `identity.notConstructible`, json.md §7) -- which is exactly why this exists.
	//
	// PLAYER scope: it answers "may this player have this at all", which is what a spawn asks. A caller needing
	// city-local supply (vicinity / plot group) is asking an availability question and must ask the CITY.
	// It routes to the ONE evaluator over a ctx the contexts fill ([DEC-single-implementation]); this is the single
	// home of that fill for placement callers, so no site assembles its own.
	static bool requiresMetForPlayer(const CvPlayer& kPlayer, EnEdgeBucket eBucket, int iId);

	// The CITY twin: `requires` (plus the entity gate) for a candidate in ONE city, evaluated regardless of whether
	// the candidate is currently in that city's tree. That independence is the point -- it answers "COULD this city
	// host it", which is what a valuation of a not-yet-held unlock needs, and which the tri-state cannot say because
	// the candidate is HIDDEN until the unlock lands.
	// bVisible relaxes the GREYABLE clauses (a connectable resource, an unadopted civic -- enabler.md §6), so a
	// caller can tell "satisfied here" from "satisfiable here" without inventing a heuristic for the difference.
	// pHypothetical (optional) is the AS-IF-HELD gate twin (CvConditionEval.h): the caller's "…and suppose I also
	// held X, and no longer held Y" applied to this one evaluation. It is the GATE half of a what-if; the
	// MEMBERSHIP half is EnablerOverlay. A caller asking both must ask both — a candidate can be gate-satisfiable
	// under a hypothetical and still not be in the tree, and vice versa (enabler.md par.1: `requires` never
	// changes membership).
	static bool requiresMetInCity(const CvCity& kCity, EnEdgeBucket eBucket, int iId, bool bVisible = false,
		const CvCascadeHypothetical* pHypothetical = NULL);

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
	static void scanCondDeps(const CvCondition* c, CascadeCondDeps& d, bool bTrackUnits, bool bMarkDynamic);

	// The PURE operating buildings recompute: the two per-city operating buildings in ONE fixpoint pass. `activeOut` = the ACTIVE
	// (present ∧ operate-holds ∧ ¬dormant-trigger) building ids for pCity. DORMANCY is DERIVED from
	// `requires.operate` + its dormant triggers (the successor buildings whose presence dorms this) -- never the
	// engine active-building/`/state` (DEC-calc-zero-ride-in; dormancy is 100% governed by operate enablers).
	// `providedOut` = the union of every ACTIVE building's `provides.bonuses` -- the BONUS ids building supply
	// makes present IN-VICINITY (json §5a). `obsoleteOut` = the PRESENT ∧ obsoleted-by-held-tech buildings (json §4.2):
	// a THIRD outcome collected in the SAME pass -- excluded from active/provides, it deposits its `whenObsolete` tree.
	// Consumers never call it directly; they read the STANDING set via operatingBuildings()/
	// wireOperatingBuildings() below. It is reached through recomputeOperatingSetInto().
	static void recomputeOperatingBuildingsInto(const CvCity* pCity, std::set<int>& activeOut, std::set<int>& providedOut, std::set<int>& obsoleteOut);

	// The WHOLE set (the three fixpoint sets + the provider ref-count) recomputed from source into a
	// CALLER-OWNED buffer -- the seed points it at the city's storage, the ENDPOINT ORACLE at its own scratch.
	// Never given the maintained set, so serving the oracle structurally cannot repair it.
	static void recomputeOperatingSetInto(const CvCity* pCity, OperatingBuildings& kOut);

	// --- ACTIVE-SET targeted maintenance (state-repositories.md: the active-building set is maintained by
	// targeted PROPAGATION, not blanket-recomputed). buildActiveIndex() inverts every building's `requires.operate`
	// into an operate-only reverse index at LOAD; the on*Active hooks ripple ONLY the affected buildings into the
	// AUTHORITATIVE m_operatingBuildings (active/provided/providedCount) in place -- the recompute above stays the load
	// SEED + the validation oracle. Mirrors the frontier's s_bc*/recheckHave, extended to the operate/provides fixpoint.
	static void buildActiveIndex();
	static void onBuildingChangedActive(const CvCity* pCity, int eBuilding);   // a building built/lost in pCity
	static void onHaveChangedActive(const CvCity* pCity, int eHaveKind);       // pop/religion/corp/power/bonus-whole-set (CASC_HAVE_*)
	static void onBonusAccessChangedActive(const CvCity* pCity, int eBonus);   // #430 G3: a SINGLE bonus's access (trade/vicinity) flipped -> re-check its operate consumers (reverse-FK targeted)
	static void onPropertyBandHitActive(const CvCity* pCity, int eProperty);   // F5: a property crossed a band threshold in pCity -> re-check its operate-band consumers (reverse-FK targeted)
	static const std::vector<int>& propertyBandBuildings();                   // the PROPERTY-BAND population -- what the property system places in every city (CvCity::placeSystemBuildings)
	static const std::map<int, std::set<int> >& propertyBandThresholds();      // PROPERTY_ id -> the sorted union of its operate-band boundaries; the enabler's own consumer reads it to tell a value MOVE from a band CROSSING
	static void onPlayerScopeChangedActive(const CvCity* pCity);              // tech/civic/golden-age (player scope)
	static void seedOperatingBuildings(const CvCity* pCity);                          // the LOAD seed: full recompute + the provider ref-count

	// The STANDING per-city operating buildings (CvCity::m_operatingBuildings, CvOperatingBuildings.h) -- a BARE
	// FETCH, unconditionally. Nothing is recomputed on this path: the set is seeded once and kept current in
	// place by the targeted on*Active hooks above, so a missed propagation stays visibly wrong
	// ([DEC-no-self-heal]) until an external reader diffs this set against the endpoint oracle's recompute.
	static const OperatingBuildings& operatingBuildings(const CvCity* pCity);

	// "If eCandidate stood here, which of this city's ACTIVE buildings would it send DORMANT?" -- the WHAT half
	// of a supersession-aware what-if; the caller then asks the VALUATION what each is worth and nets it out
	// (two calls, one per system: availability is the enabler's, magnitude the cascade's --
	// [DEC-enabler-not-cascade]). No hypothetical evaluation is involved and none is needed: "B dorms while X is
	// present" is STATIC authored data (B's requires.operate dormant trigger), so this is the load-built
	// dormant-trigger index intersected with the standing ACTIVE set -- both already maintained, nothing scanned.
	static void dormedByBuilding(const CvCity* pCity, int eCandidate, std::vector<int>& kOut);
	// Convenience: point ec.activeBuildings / ec.vicinityProvidedBonuses at the standing sets
	// (feeds cascadeIsBuildingActive + ev_vicinityHas; no per-call set copies).
	static void wireOperatingBuildings(const CvCity* pCity, CvCascadeEvalCtx& ec);

	// THE §5a VICINITY UNION for a bound city -- the ONE home for "is this bonus in vicinity here?"
	// ([DEC-single-implementation]). The supply has two independently-owned halves and a reader must union both
	// (contexts.md § THE VICINITY SPLIT): the ENABLER owns the active-building `provides` half (the operate/
	// provides least fixpoint, which only it can resolve), the CITY CONTEXT owns the MAP half (the tiered
	// radius presence). Both are stored, event-maintained sets, so this is two O(1) fetches and never a scan.
	// ⚠ Consult it INSTEAD of the engine's own vicinity getter, which re-derives both halves per call -- a
	// radius walk plus a sweep of every building testing isActiveBuilding.
	static bool cityHasVicinityBonus(const CvCity* pCity, int eBonus, CvCascVicinity eTier);
};

#endif // CV_ENABLER_KERNEL_H
