#pragma once
#ifndef CV_INFO_VALUATION_H
#define CV_INFO_VALUATION_H

//
//	InfoValuation -- the ONE per-GROUP what-if valuation walk + the §2a fold seams (patterns.md § THE GETTER
//	SETUP category 3; contexts.md § The read). The rebuilt infos' `expected*` endpoints DELEGATE here
//	([DEC-single-implementation] -- no per-type reimplementation): each endpoint answers *"what do I gain from
//	this entity HERE?"* as the compiled unconditioned ×100 sums fetched straight PLUS the group's conditioned
//	tail through the ONE evaluator (MMKernel::applies over a CvCascadeEvalCtx the CONTEXTS fill), `plots`-target
//	deposits scaled by `cityContext.plotAttrs` counts, per-scalers resolved through the ONE §3.7 resolver
//	(MMKernel::perScale, entry carrier), the SCOPE AXIS FOLDED into the experienced-here answer (the §1 scope
//	principle: a city experiences the sum of the world/team/empire/area/city packages), and the AUDIENCE
//	resolved from the asking player (json §3.9 `ai`).
//
//	⛔ A what-if NEVER evaluates `requires`: the entity-level active/dormant verdict is the ENABLER's and is FED
//	IN via the precomputed operating set (EnablerKernel::wireOperatingBuildings fills the eval ctx's
//	activeBuildings/vicinityProvidedBonuses) -- the walk evaluates only the deposits' own §3.9 conditions.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state
//	(patterns.md static-class law; a namespace risks VC7.1/Boost name-mangling).
//

#include "Engine/TraitContext.h"   // TraitContext::HeldTrait -- the id+record pair the keyed trait legs walk
#include "CvInfoKinds.h"                  // ModifierFamily / kind enums / WellbeingChannel / CvCascUnit / CvCascScope
#include <vector>                         // the collectors' out-lists
#include <utility>                        // std::pair -- the keyed-combat (target, percent) rows
#include "Defines/CvEnums.h"              // NUM_YIELD_TYPES / NUM_COMMERCE_TYPES -- the group out-array extents

class CvModifiers;
class CvTraitInfo;
class CityContext;
class EmpireContext;
class CvPlotGroup;
struct CvCascadeEvalCtx;
struct CvCascadeHypothetical;   // the AS-IF-HELD gate twin expectedSum optionally evaluates the conditioned tail under
// the scope objects the cross-scope roll-up reads packages off (declarations only -- the engine headers stay
// out of this header; the roll-up bodies include them)
class CvPlot;
class CvCity;
class CvPlayer;
class CvTeam;
class CvArea;

//	One keyed heal deposit: `heal.unit.unitCombat.{UNITCOMBAT_X}.{heal|adjacentHeal}`. Both amounts are ×100
//	([DEC-fixedpoint-x100]); the reader reduces at its point of use.
struct HealByUnitCombat
{
	int iUnitCombat;
	int iHeal;
	int iAdjacentHeal;

	HealByUnitCombat() : iUnitCombat(-1), iHeal(0), iAdjacentHeal(0) {}
};

// ⚖ THE COMMERCE SPLIT, TERM BY TERM -- the CityRateTerms treatment for the commerce half. A channel's realized
// value is FOUR independent quantities collapsed into one int (the slider's share of the commerce yield, the
// channel's own percent stack, the channel's deposits, and the process conversion added after all of it), so
// "research is too low" is unanswerable against the total and immediately answerable against the terms
// ([http-endpoints.md]: a value served term by term attributes a divergence to a NAMED source).
// ⛔ NOT a second combine ([DEC-single-implementation]): commerceSplit IS this with the terms discarded, so a
// census can never describe arithmetic the split does not actually do.
// Global scope on purpose -- `CvCity`'s census read forward-declares it instead of pulling the whole calc
// surface into a core game-object header. Every magnitude is ×100 EXCEPT `sliderPercent` and `percentSum`,
// which are plain counters ([DEC-fixedpoint-x100]: a percentage carries no decimals).
struct CvCommerceSplitTerms
{
	int64_t commerceYield;      // the city's realized COMMERCE yield -- what the slider divides
	int     sliderPercent;      // the player's plain 0..100 counter for this channel
	int64_t share;              // the slider's share of that yield, BEFORE the channel's own stack
	int     percentSum;         // Σ percent for this channel across the chain (the stack is 100 + this)
	int64_t deposits;           // the channel's own deposits (a CONSUMED channel's maintained receiver sum)
	int64_t processConversion;  // TIER 2 -- production converted by an active process, never multiplied
	int64_t rate;               // what the split produced

	CvCommerceSplitTerms()
		: commerceYield(0), sliderPercent(0), share(0), percentSum(0), deposits(0), processConversion(0), rate(0) {}
};

class InfoValuation
{
public:
	//	The KEYED heal read, collected ONCE for every consumer ([DEC-single-implementation]). It walks the
	//	entity's own COMPILED ENTRY LIST -- the handful of combat classes it actually authored -- rather than
	//	enumerating the whole UnitCombat registry, which is the keyed-container inversion the rebuild deletes
	//	(pedia-read-map finding 2). Entries for one combat class are merged into a single row.
	static void collectHealByUnitCombat(const CvModifiers* modifiers, std::vector<HealByUnitCombat>& healRows);

	//	The KEYED COMBAT axes -- `combat.unit.<axis>.{TARGET}.<kind>` (json §6.1's named-entity key). The axis is
	//	an enum rather than a segment string so no call site interns one; the segment ids resolve once, here.
	enum CombatTargetAxis
	{
		COMBAT_TARGET_TERRAIN = 0,   // combat.unit.terrain.{TERRAIN_X}
		COMBAT_TARGET_FEATURE,       // combat.unit.feature.{FEATURE_X}
		COMBAT_TARGET_UNITCOMBAT,    // combat.unit.unitCombat.{UNITCOMBAT_X}
		COMBAT_TARGET_UNIT,          // combat.unit.vsUnit.{UNIT_X}
		COMBAT_TARGET_DOMAIN,        // combat.unit.domain.{DOMAIN_X}
		COMBAT_TARGET_FLANKING       // combat.unit.flanking.{UNITCOMBAT_X} -- keyed by CLASS, never by unit
	};

	//	The POINT read: this entity's keyed combat percent against ONE target. Walks the entity's own compiled
	//	entries -- the handful it authored -- never the target registry ([DEC-single-implementation]; the
	//	keyed-container inversion is what pedia-read-map finding 2 names as the shape to delete).
	//	⚠ A PERCENT, so it is NOT scaled ([DEC-fixedpoint-x100]): use it as returned.
	static int keyedCombat(const CvModifiers* modifiers, CombatTargetAxis eAxis, int iTargetFk, int iKind);

	//	The COLLECT form: every (target, percent) this entity authored on one axis+kind. For a consumer that
	//	iterates what is there rather than asking about a specific target.
	//	⛔ THE KIND IS MATCHED EXACTLY HERE -- unlike the point reads, this form has NO `iKind < 0` wildcard, so a
	//	kind that does not match reads NOTHING and the consumer silently scores zero. Pass the kind the address
	//	actually compiles to (below): a MEMBER-LESS address is kind 0, never a negative.
	static void collectKeyedCombat(const CvModifiers* modifiers, CombatTargetAxis eAxis, int iKind,
		std::vector<std::pair<int, int> >& targetPercents);

	//	The GENERAL keyed point read that `keyedCombat` above is one specialization of: sum this entity's own
	//	compiled deposits onto ONE named target, for ANY family whose §6.1 address ends in a named-entity key
	//	(`buildRate.<scope>.{units|buildings|unitCombats|specialBuildings}.{TARGET}`, and its siblings).
	//	`iKind < 0` matches ANY kind -- a wildcard for a caller that does not care which member it hit.
	//	⛔ It is NOT the kind a member-less address carries: `<family>.<scope>.{TARGET}.<unit>` with no member
	//	segment compiles to KIND 0, the scope-wide amount (`CvModifiers.cpp` mod_decodeLeaf). Reading the wildcard
	//	as "what a keyless address compiles to" is the trap -- it happens to work here, and reads nothing at all
	//	on the exact-match COLLECT forms above.
	//	⚠ Returns the value AS COMPILED -- a percent is NOT scaled ([DEC-fixedpoint-x100]).
	//	⚖ iTargetSegment carries the SAME -1 semantics as the COLLECT form below, and the two must never drift:
	//	-1 is the DIRECT-KEYED address shape (`allowedSpecialists.city.{SPECIALIST_X}` -- a named-entity key
	//	sitting straight under the scope with no plural container token), not a failure code. Rejecting it here
	//	made every direct-keyed point read answer 0, which is silent: the caller passes the right family, kind
	//	and target, and simply never sees the deposit.
	//	⛔ UNCONDITIONED ENTRIES ONLY, exactly as `CvModifiers::targetedSum` -- a conditioned entry is the
	//	VALUATION's to evaluate through the ONE evaluator against the contexts, never something a point read
	//	folds in as though its condition held ([patterns.md] THE GETTER SETUP: the compiled sum, the conditioned
	//	list and the `expected*` what-if are three distinct reads). Summing the tail here applies every
	//	tech-gated and age-gated deposit from turn 0.
	static int keyedTarget(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
		int iTargetSegment, int iTargetFk);

	//	The COLLECT form of the general keyed read -- every (targetFk, value) this entity authored on one
	//	family+segment(+kind), for a consumer that iterates WHAT IS THERE rather than asking about one target.
	//	Stands to `keyedTarget` exactly as `collectKeyedCombat` stands to `keyedCombat`.
	//	⛔ This is what a "for every domain / every unitcombat, does this entity deposit?" loop becomes: it walks
	//	the handful the entity AUTHORED instead of the whole target registry, which is the own-data inversion
	//	pedia-read-map finding 2 names as the shape to delete.
	//	⚠ Values come back AS COMPILED (a flat is ×100, a percent is not -- [DEC-fixedpoint-x100]).
	//	⛔ UNCONDITIONED ENTRIES ONLY -- the same condition semantics as the point form above and as
	//	`CvModifiers::targetedSums`; the conditioned tail belongs to the valuation, not to this walk.
	//	⚖ iTargetSegment is matched EXACTLY, and -1 is a REAL ADDRESS SHAPE, not a failure: a named-entity key
	//	may sit directly under the scope with no plural container token (`religion.city.{RELIGION_X}` -- the
	//	address decode routes an underscored segment to targetFk and leaves targetSeg unset), so -1 selects
	//	precisely those. Serving both shapes here is what keeps this the ONE keyed read
	//	([DEC-single-implementation]) instead of every direct-keyed consumer hand-rolling an entry walk.
	//	⚑ A FAILED `keyedTargetSegment` lookup is STRUCTURALLY distinct from -1, so it cannot be mistaken for the
	//	direct-keyed shape: it answers TARGET_SEGMENT_NONE and both keyed reads match nothing against it. That is
	//	deliberate rather than a caution to remember -- the two meanings ("this address carries no container
	//	token" and "that token was never authored anywhere") are opposite intents, and collapsing them onto one
	//	value is what would silently hand a caller the direct-keyed entries it did not ask for.
	//	iScope filters to entries AUTHORED at one scope (-1 = any), the same shape as iKind's filter. It matters
	//	wherever the same family+target is authored at two scopes with two different consumers -- `buildRate`
	//	keyed by buildings is the live case (a city-scope row speeds the build HERE, an empire-scope row speeds
	//	it in every city), so an unfiltered read would hand each consumer the other's rows as well.
	static void collectKeyedTarget(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
		int iTargetSegment, std::vector<std::pair<int, int> >& targetValues, int iScope = -1);

	//	THE AUTHORS-ANY PREDICATE -- "does this entity author a deposit of this SIGN anywhere in this family?"
	//	It answers the RELEVANCE question an AI focus probe asks ("is this building worth considering for
	//	production?"), which the point reads structurally cannot: a point read serves ONE compiled
	//	(family, kind, scope, unit) slot and deliberately excludes the conditioned tail, so a probe built from
	//	point getters has to ask one getter per shape -- scope-wide, power-gated, plots-target, keyed-by-domain,
	//	keyed-by-unitcombat -- and still misses every shape nobody thought to add a getter for.
	//	⚑ This walks the handful the entity AUTHORED, so it is the own-data inversion's cure
	//	(pedia-read-map finding 2): a "for every domain, does this deposit?" loop over the whole target registry
	//	becomes one read. It also SUBSUMES the sentinel probe (`getDomainProductionModifier(NO_DOMAIN)`) -- asking
	//	a keyed getter with a NO_* key to mean "any" is a read of a slot that cannot exist.
	//	⚠ SIGN IS PRESERVED ON PURPOSE. The probes it replaces test `> 0` / `< 0`, and a building whose only
	//	production entry is NEGATIVE is not a production building; collapsing to a bare "authors anything" would
	//	quietly change which buildings the AI offers. iSign is +1 (any positive) or -1 (any negative).
	//	⚠ Values are AS COMPILED, so this compares against 0 only -- never a magnitude ([DEC-fixedpoint-x100]).
	//	iKind / iScope filter exactly as they do on collectKeyedTarget (-1 = any).
	static bool authorsAnySigned(const CvModifiers* modifiers, ModifierFamily eFamily, int iSign,
		int iKind = -1, int iScope = -1);

	// The OVER-LIMIT penalty a source authors: the magnitude of its wellbeing entry scaled per unit ABOVE a
	// threshold token (json §3.7 `above` -- the civic over-expansion class, `{value:-6, per:{CITY, above:
	// CITY_LIMIT}}`). Returned POSITIVE as a penalty-per-extra-item, so a caller weighs it without re-deriving
	// the sign. 0 = the source authors none. An ENTRY-LIST read by necessity: the entry is conditioned and
	// per-scaled, so no point sum can answer it ([modifier.md] §5).
	static int overThresholdPenalty(const CvModifiers* modifiers, ModifierFamily eFamily,
		const char* szAboveToken);

	//	The interner lookup for a keyed target token, resolved ONCE per call site rather than per entry.
	//	⚑ A token that was never authored anywhere answers TARGET_SEGMENT_NONE, NOT -1: -1 is the live
	//	direct-keyed address shape, so returning it here would make an unauthored token select precisely the
	//	entries the caller did not ask for. Both keyed reads match nothing against TARGET_SEGMENT_NONE, so a
	//	caller may forward this result straight into them and a missing token answers 0 by construction.
	static int keyedTargetSegment(const char* szTargetSegment);

	//	The "that token is not in the data" answer from keyedTargetSegment -- distinct from -1 (direct-keyed).
	enum { TARGET_SEGMENT_NONE = -2 };

	//	Materialize the doubleMove FK lists from an entity's OWN compiled MOVEMENT entries. doubleMove is half
	//	movement cost on that ground ([skills.md] par.1) -- a keyed movement modifier, never a skill -- and the
	//	consumers want the plain target list, so it materializes once at mapFrom
	//	([DEC-materialize-at-mapfrom]). ONE implementation for every carrier ([DEC-single-implementation]).
	static void fillDoubleMoveTargets(const CvModifiers* modifiers,
		std::vector<int>& terrainTargets, std::vector<int>& featureTargets);

	// THE EVAL-CTX FILL SEAM (contexts.md: "the contexts ARE the eval state"): CityContext::fillEvalCtx
	// (city/plot) + EmpireContext::fillEvalCtx (player/team) + the plotGroup pass-in (the reserved TRADED-bonus
	// source -- evalCtx.plotGroup; NULL = the city's own plot-group-backed reads answer trade) + the enabler's
	// standing operating set wired in (EnablerKernel::wireOperatingBuildings -- the FED-IN active/dormant
	// verdict). Every expected* endpoint builds its ctx through this one seam; no caller assembles a
	// raw-pointer ctx beside the contexts.
	// The PLOT-ANCHORED fill: the ctx for a walk whose target is THIS plot rather than a city centre. The city
	// fill would overwrite the plot silo with the centre's, so a per-plot ask (the package rebuild, and every
	// what-if that names a plot) fills through here instead. Binds the plot's own silo, its owner's empire silo
	// (which answers every team fact) and the working city's, wiring in the enabler's operating set with it.
	static void fillEvalCtxAtPlot(const CvPlot& plot, CvCascadeEvalCtx& evalCtx);

	static void fillEvalCtx(const CityContext& cityContext, const EmpireContext& empireContext,
		const CvPlotGroup* plotGroup, CvCascadeEvalCtx& evalCtx);

	// ---- the per-GROUP what-if endpoints (contexts.md: one endpoint per GROUP of channels, never per single
	// ---- channel; each fills its group's ×100 out-array -- contexts in, expected values out). The per-info
	// ---- endpoints (CvBuildingInfo::expectedFlatYields et al.) are one-line delegations onto these.

	// The three yield channels' FLAT group ({food,production,commerce}.<scope>.flat).
	static void expectedFlatYields(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&flatYields)[NUM_YIELD_TYPES],
		const CvCascadeHypothetical* pHypothetical = NULL);
	// The three yield channels' PERCENT group ({food,production,commerce}.<scope>.percent -- the §2a additive stack's slice).
	static void expectedYieldModifiers(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&yieldModifiers)[NUM_YIELD_TYPES],
		const CvCascadeHypothetical* pHypothetical = NULL);
	// The PLOT yield group, two legs summed per channel: (1) the `plots`-target deposits ({channel}.<scope>.plots
	// -- json §6.1), each value × the count of the city's radius plots matching its predicate, read from
	// cityContext.plotAttrs (COUNTS, never objects); (2) the info's OWN untargeted plot-scope output
	// (plotOwnYield -- the substrate leg: what this terrain/feature/improvement/route itself yields on the
	// ctx's target plot, the city centre under the city fill; a specific-plot ask calls plotOwnYield /
	// plotBaseYields with a plot-bound ctx directly).
	static void expectedPlotYields(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&plotYields)[NUM_YIELD_TYPES],
		const CvCascadeHypothetical* pHypothetical = NULL);
	// The four commerce channels' FLAT group ({gold,research,culture,espionage}.<scope>.flat).
	static void expectedFlatCommerce(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&flatCommerce)[NUM_COMMERCE_TYPES],
		const CvCascadeHypothetical* pHypothetical = NULL);
	// Their PERCENT twin -- the commerce sibling of expectedYieldModifiers.
	static void expectedCommerceModifiers(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&commerceModifiers)[NUM_COMMERCE_TYPES],
		const CvCascadeHypothetical* pHypothetical = NULL);
	// The DEFENSE family's whole kind group. ⚑ It exists for the same reason every group read here does: a
	// caller wanting the group must NOT loop expectedModifier per kind, because each of those calls rebuilds
	// the eval ctx -- the group read builds it ONCE and walks the kinds inside it.
	static void expectedDefenseKinds(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&defenseKinds)[NUM_DEFENSE_KINDS],
		const CvCascadeHypothetical* pHypothetical = NULL);
	// The four wellbeing channels (modifier.md §2b): the authored happiness/health deposits, each contribution
	// SIGN-ROUTED to its channel (a negative happiness value lands as positive anger -- a routing rule, never a
	// storage shape). Every channel of the out-array carries a POSITIVE magnitude.
	static void expectedWellbeing(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&wellbeing)[NUM_WELLBEING_CHANNELS],
		const CvCascadeHypothetical* pHypothetical = NULL);

	// The GROUPED/SCALAR-family analogue (the same walk for one (family, kind, unit) slot): the experienced-here
	// ×100 answer for a defense kind, a maintenance modifier, an InfoScalar slot, ... -- the per-info
	// expectedScalar/expectedModifier endpoints delegate here with their vocabulary axes spelled out.
	//
	// pHypothetical (optional) is the AS-IF-HELD gate twin (CvConditionEval.h): the conditioned tail is evaluated
	// as though the caller also held / no longer held the named ids. ⚑ It is what makes "what would this be
	// WORTH if I had X" answerable in the ONE valuation, so a caller asks the DELTA between two calls instead of
	// re-deriving which entries a bonus gates ([patterns.md] THE VALUATION PROTOCOL -- contexts in, delta out).
	// ⚠ It reaches ONLY the conditioned entries; the unconditioned compiled sums are what they are.
	static int expectedSum(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		const CvCascadeHypothetical* pHypothetical = NULL);

	//	The SCOPE-RESTRICTED twin, for the off-spine `self` scope the fold set cannot carry (see the .cpp).
	static int expectedSumAt(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
		CvCascScope eScope, const CityContext& cityContext, const EmpireContext& empireContext,
		const CvPlotGroup* plotGroup, const CvCascadeHypothetical* pHypothetical = NULL);

	//	THE KEYED TWIN of expectedSum -- the what-if for a deposit that is BOTH keyed and conditioned.
	//	⚑ Why it must exist as its own read: `expectedSum` answers a family's POINT slot and deliberately skips
	//	every targeted entry (a keyed entry never folds scope-wide -- folding it would hand a melee-only bonus to
	//	every unit, [modifier.md §5]), while `keyedTarget` answers the keyed axis but only its UNCONDITIONED
	//	half. A deposit carrying both axes therefore falls between them and has NO read at all -- which is not a
	//	hypothetical: `allowedSpecialists.city.{SPECIALIST_X}` gated on a team tech is exactly that shape, and its
	//	consumers dangled on it.
	//	⛔ The conditioned tail goes through the ONE evaluator over a ctx the CONTEXTS fill -- never a second
	//	evaluator and never an unconditional sum ([DEC-single-implementation]); an unconditional sum is what
	//	applies every tech-gated slot from turn 0.
	//	⚑ It takes the SAME optional AS-IF-HELD hypothetical the point form does, so "how many slots would this
	//	open if I ALSO had that tech" is the DELTA between two calls ([patterns.md] THE VALUATION PROTOCOL --
	//	contexts in, delta out) rather than a second machine that re-derives which entries a tech gates.
	//	⚠ Values come back AS COMPILED (a flat/count is ×100, a percent is not -- [DEC-fixedpoint-x100]).
	static int expectedKeyedTarget(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
		int iTargetSegment, int iTargetFk,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		const CvCascadeHypothetical* pHypothetical = NULL);

	//	The ctx-taking core of the above, for a caller that already filled one (the groupSum/expectedSum shape).
	static int64_t keyedTargetSum(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
		int iTargetSegment, int iTargetFk, const CvCascadeEvalCtx& evalCtx);

	//	The COLLECT twin of keyedTargetSum -- every keyed target in ONE walk, ACCUMULATED into a caller-owned
	//	array indexed by target FK. It stands to `keyedTargetSum` exactly as `collectKeyedTarget` stands to
	//	`keyedTarget`, one axis over: the conditioned half included.
	//	⚑ WHY IT EXISTS: asking the per-target form once per candidate re-walks the SAME entry list once per
	//	target, so a caller wanting the whole group pays `targets x entries` for a walk that already visits
	//	every target on its way past. The free-specialist read is the measured case -- 40 specialists x the
	//	city's operating set x the empire's sources, per CITIZEN.
	//	⚠ Values accumulate AS COMPILED -- the caller reduces at its point of use. The array is ADDED INTO,
	//	never cleared, so a caller can fold several sources through it; size and zero it once before the walk.
	static void collectKeyedTargetSums(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
		int iTargetSegment, const CvCascadeEvalCtx& evalCtx, std::vector<int64_t>& targetTotals);

	// The ctx-taking core the endpoints share (fill the ctx ONCE per endpoint, fold each channel through this):
	// Σ over the folded scopes of the compiled unconditioned sums (audience-resolved) + the family's conditioned
	// tail (kind/unit-matched, untargeted, gated via the ONE evaluator, per-resolved via the ONE §3.7 resolver).
	static int64_t groupSum(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
		const CvCascadeEvalCtx& evalCtx);

	// The SCOPE-RESTRICTED sibling of groupSum -- ONE info's (family, kind, unit) contribution AT one scope
	// (unconditioned slot sum + the conditioned tail at that scope). The package plane's per-source fold and
	// the receiver combine's specialist term read through this ([DEC-single-implementation]: the gather never
	// grows a second per-info fold beside the valuation's).
	static int64_t groupSumAt(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
		CvCascScope eScope, const CvCascadeEvalCtx& evalCtx);

	// ONE substrate info's OWN untargeted PLOT-scope output of a channel family, for the ctx's TARGET plot
	// (evalCtx.plotContext): the compiled plot-slot unconditioned sum + the plot-scope conditioned tail -- incl. the
	// reverse-landed conditioned entries (a building/civic/tech boost keyed to this substrate lands HERE,
	// CvReversePass) -- evaluated under PLOT-EVAL semantics (a bare {HAS_BONUS:X} reads THIS plot,
	// bonusFromPlot; the groupSumAt sibling evaluates city-relative and cannot serve the substrate leg).
	static int64_t plotOwnYield(const CvModifiers* modifiers, ModifierFamily eFamily, const CvCascadeEvalCtx& evalCtx);

	// THE ISOLATED PLOT-AS-BASE CALC (modifier.md §2 plot note -- "a plot's yield is ONE base package, resolved
	// in isolation BEFORE the city modifiers"; the wave-B substrate valuation leg): the target plot's base-yield
	// package from the substrate infos' compiled entries. Per channel:
	//   nature = max(0, terrain + feature + bonus own-output)  ·  improvement floored at −nature  ·  + route  ·
	//   max(0, total)
	// A NULL substrate slot contributes 0 (no feature / no improvement / no route / no bonus). The plot package
	// rebuild AND every what-if plot read call THIS one function ([DEC-single-implementation]); the caller
	// passes each present substrate's getModifiers() and a ctx whose `plot` is the target plot.
	// pNatureYields optionally receives the SUBSTRATE segment (the floored terrain+feature+bonus term) this
	// already derives, so a caller wanting the pre-improvement value never re-sums those legs itself.
	static void plotBaseYields(const CvModifiers* terrainModifiers, const CvModifiers* featureModifiers,
		const CvModifiers* bonusModifiers, const CvModifiers* improvementModifiers, const CvModifiers* routeModifiers,
		const CvCascadeEvalCtx& evalCtx, int (&plotYields)[NUM_YIELD_TYPES],
		int (*pNatureYields)[NUM_YIELD_TYPES] = NULL);

	// ⚖ THE PLOT'S OWN YIELD SCALING -- "+`amount` per whole `interval` of what this plot already makes".
	// A plot totalling 12 on a 1-per-5 scaling gains 2 and comes out at 14.
	// ⛔ IT IS A RATE, NOT A ONE-SHOT THRESHOLD. The legacy engine paid a single step once the plot passed a
	// threshold (`CvPlayer::updateExtraYieldThreshold`); reading the authored number that way pays out once where
	// the model pays per interval, which is plausible and quietly short on every high-yield tile.
	// ⛔ SAME CHANNEL ONLY (owner, hard rule): the interval is measured against the channel's own plot total and
	// the grant lands on that same channel -- there is no "1 hammer per 5 commerce", by construction.
	// ⚠ The caller passes the PRE-BONUS total, so the grant can never feed itself; the ×100 plane cancels in the
	// ratio, so only `amount` carries the scale. The package resolve and every what-if plot read call THIS one
	// function ([DEC-single-implementation]) -- the arithmetic exists once, here on the calc surface.
	static int64_t plotScaledYield(int64_t iBaseTotal, int64_t iInterval, int64_t iAmount);

	// THE CITY RATE COMBINE (modifier.md §2a -- the sharp two-tier shape, the order load-bearing):
	//   rate100 = (BASE + specialists) x max(0, 100 + percentSum)/100 + 100 x (EXTRA100 / 100)
	// BASE = the worked-plot Σ + the upper-scope flats rolled down at the combine; the specialist term carries
	// its OWN percent layer before joining BASE; EXTRA is the building-flat tier, truncated to whole units
	// before re-scaling (the engine's documented integer-truncation order, mirrored verbatim). The receiver
	// sum rebuild and every realized-rate consumer call THIS one function -- the combine exists once, on the
	// calc surface.
	static int64_t cityRate(int64_t base, int64_t specialists, int iPercentSum, int64_t extra);

	// THE PER-COMMERCE SPLIT (modifier.md §2a's commerce paragraph -- the §2a two-tier shape carried by the
	// channel's own pieces; StoneBase's CommerceSplit calc package). A city's realized gold / research / culture /
	// espionage rate is NOT a fifth receiver slot: the city receives the COMMERCE YIELD, and the empire's slider
	// percentages split that yield across the four channels, each channel adding its own deposits on top.
	//   rate100 = commerceYieldRate x sliderPercent/100 x max(0, 100 + channelPercentSum)/100
	//             + channelDeposits
	//             + (productionYieldRate/100) x (productionToCommerce/100)
	// TIER 1 (BASE, multiplied by the ONE additive stack): the slider share of the commerce yield AND the §2a
	// baseExtra sub-terms (religion / corporation / golden-age / player-extra / the building-commerce block) --
	// which reach this function ALREADY SCALED, inside `channelDeposits`, because the cross-scope roll-up applies
	// the channel's own stack to the channel's own flats (realizedAtCity). `channelPercentSum` is therefore
	// supplied for the SLIDER SHARE alone, so the share meets the same stack the deposits already met.
	// TIER 2 (EXTRA, added AFTER the percentages, never multiplied): the process conversion -- the production
	// yield truncated to whole hammers first, then scaled by the process's own conversion rate (the engine's
	// truncation order, mirrored verbatim).
	// SCALES: every magnitude is ×100 EXCEPT `iSliderPercent`, which is the player's plain slider counter 0..100
	// (json §3.1's GOLD_RATE / RESEARCH_RATE / CULTURE_RATE / ESPIONAGE_RATE tokens, read through
	// EmpireContext::commerceRates) and `channelPercentSum`, which is a PLAIN percent like every stored stack
	// ([DEC-fixedpoint-x100]). `productionToCommerce` IS ×100 and carries its own ÷100 here.
	// ⚠ CULTURE is the lone dual consumer: its `channelDeposits` is the city's MAINTAINED RECEIVER SUM (the
	// combine the gather already wrote), not a roll-up -- so the receiver sum passes through untouched and only
	// the commerce yield is slider-scaled. Scaling a receiver sum by a slider would re-scale a realized total.
	// ⚖ THE SPLIT, TERM BY TERM -- the CityRateTerms treatment for the commerce half. A channel's realized value
	// is FIVE independent quantities collapsed into one int (the slider's share of the commerce yield, the
	// channel's own percent stack, the channel deposits, and the process conversion added after it all), so
	// "research is too low" is unanswerable against the total and immediately answerable against the terms.
	// ⛔ NOT a second combine ([DEC-single-implementation]): commerceSplit IS this with the terms discarded, so
	// a census can never describe arithmetic the split does not actually do.
	// (The struct is CvCommerceSplitTerms at global scope -- a consumer that only passes one, above all `CvCity`'s
	// census read, forward-declares it rather than taking this whole calc header into a core game-object header.)
	// `pTerms` NULL = the ordinary read; non-NULL fills the census beside the same answer.
	static int64_t commerceSplit(int64_t commerceYieldRate, int iSliderPercent, int64_t channelPercentSum,
		int64_t channelDeposits, int64_t productionYieldRate, int iProductionToCommerce,
		CvCommerceSplitTerms* pTerms = NULL);

	// ---- THE CROSS-SCOPE ROLL-UP -- the GAME-OBJECT read role's realized answer (patterns.md § THE TWO READ
	// ---- ROLES: *"what do I HAVE, right now?"*, the live-state twin of the expected* what-if walk above; the
	// ---- two answers sit on ONE calc surface so no second combine can appear beside them).
	// ---- modifier.md §1: deposits accumulate in a package AT THEIR OWN SCOPE and the downward roll is realized
	// ---- AT READ -- so a scope object's realized channel value is the combine over the packages it sits under,
	// ---- computed HERE once for every consumer ([DEC-single-implementation]).
	// ---- ⛔ A lower scope never STORES an upper scope's sums: this composes at read and caches nothing upward.
	// ---- ⛔ Every package read below is a BARE FETCH (readFlat/readPercent/readSum) -- a package carries no other
	// ---- read surface, and a read never triggers work of any kind ([DEC-maintained-sum]).

	// The §2 combine over ONE channel's rolled sums. A group read has no external base (`base` = 0) and
	// multiplier deposits are identity on every package channel, so the arithmetic is
	// `Σflat × max(0, 100 + Σpercent)/100`. The channel's CANONICAL AUTHORED UNIT decides which side IS the
	// answer -- the census verdict declared ONCE beside the vocabulary (infoKindUnit), never re-decided per
	// getter: a PERCENT-unit channel (the maintenance modifier, buildRate, combat, ...) has no flat plane of its
	// own, so its realized value IS the ONE additive percent stack (modifier.md §2a); a FLAT-unit channel is the
	// flat sum that stack scales. A kind the census records as DUAL takes its dominant plane as the base and the
	// opposing plane as the scaler, which is exactly what the §2 combine says. A FLAT answer is ×100 native
	// ([DEC-fixedpoint-x100]); the PERCENT operand carries no scale of its own -- a percentage has no decimals
	// to carry, so readJson never scales one and every stored stack is a plain percent that meets the identity
	// constant 100 directly. cityRate combines its own stack identically: ONE convention on this surface.
	// ⚠ NOT a duplicate of cityRate: that is the §2a RATE specialization (two tiers + the specialist layer) the
	// receiver sums are built from; this is the generic §2 slot combine every other channel answers by.
	static int64_t realizedChannel(int64_t flatSum, int64_t percentSum, CvCascUnit eCanonicalUnit);

	// The realized ×100 value of one channel AT one scope object -- the entry every game-object group read
	// folds per channel. The object IS the scope (patterns.md rule 5: scope is never an argument on this role),
	// so each entry knows its own chain: city = team + empire + (area × owner) + city · empire = team + empire ·
	// team = team · area slot = itself · plot = itself. WORLD is deliberately absent (CONFIG, no package --
	// state-repositories.md), and PLOT never enters an upper scope's chain: a per-plot value resolves INSIDE the
	// isolated plot package before any city stack runs (modifier.md §2 plot-as-base), and the worked-plot Σ that
	// DOES feed a city is the receiver rate's own BASE term, not a channel roll-up.
	// ⚑ THE ORIGIN RULE IS ENFORCED BY THE DATA, not by a hand-written scope filter: a channel is minted into a
	// scope's set only where the compiled deposits author it (CascadeChannelRegistry -- KEYS ONLY WHERE NEEDED),
	// so a scope that carries no flat/percent side of a channel answers 0 with no storage anywhere. That is why
	// the yield/commerce families come out flat-at-plot-and-city / percent-above-plot exactly as the rule states,
	// while the families the rule does not speak for (wellbeing's empire+area flats, the plot-scope health and
	// defense percents) keep the sides their data actually authors.
	// A channel the scope CONSUMES answers its maintained RECEIVER sum instead of a roll-up -- the receiving
	// scope stores its own realized total ([DEC-uniform-cache-shape]: a receiver is the same cache holding a
	// different slot), so re-rolling it here would be a second derivation of a number that already exists.
	// iChannel < 0 (never authored anywhere) answers 0.
	static int realizedAtPlot(const CvPlot& plot, int iChannel);
	// ⚖ THE SPECIALIST TERM of the §2a rate -- each assigned specialist's own output at CITY scope, times how
	// many are assigned. Exposed rather than inlined because several readers need it and none may own it: a
	// second copy would be a second derivation ([DEC-single-implementation]).
	//
	// ⚖ ONE ROW PER SPECIALIST TYPE -- the DECOMPOSITION of that Σ ([http-endpoints.md]: a term that is itself a
	// Σ decomposes again). `specialists` reaches the rate census as ONE int over every held type, so a rate that
	// is short says nothing about WHICH type is short, and the term's disagreement with the package plane could
	// not be attributed at all. The rows come OUT of the same walk the total does -- never re-derived beside it,
	// which is the one thing a census must not do.
	// ⚠ `assigned` and `freeTyped` are reported SEPARATELY and deliberately: the multiplier below counts
	// `assigned` alone, while [modifier.md §2a] and the engine (`CvCity::getSpecialistCount +
	// getFreeSpecialistCount`) both say the count is the SUM. Reporting both measures that gap without moving a
	// value -- which is the whole point, since which surface is the model is an open owner ruling and the
	// arithmetic must not be touched ahead of it ([DEC-baseline-is-a-smell-test]: settle it by which surface the
	// spec names, never by which number looks better).
	struct SpecialistTermRow
	{
		int     specialist;    // the raw SpecialistTypes id -- named by the RENDERER, never resolved at emit
		int     assigned;      // getSpecialistCount -- the ONLY multiplier the term currently applies
		int     freeTyped;     // getFreeSpecialistCount -- counted by the spec and the engine, NOT by the term
		int64_t perUnit;       // this type's own city-scope flat output of the channel, per specialist
		int64_t contribution;  // exactly what this type added to the total (perUnit x assigned)
	};
	// A row is produced for every type the city HOLDS on either count, so a type contributing nothing BECAUSE it
	// is only free-typed appears with contribution 0 rather than vanishing -- an absent row would hide precisely
	// the gap this census exists to size.

	// ONE held trait's improvement-keyed contribution to a rate's BASE, per improvement (modifier.md §4: a
	// trait's keyed deposits stay source-side, so this leg is read per live source and lands in BASE).
	// ⛔ IT HAS ITS OWN LINE FOR THE REASON THE SPECIALIST ROWS DO: the Σ carries axes the term cannot -- WHICH
	// trait and WHICH improvement -- and `rateRead` is at the 16-field cap besides ([event-spine.md]: when a
	// line is full the answer is a SECOND event, never swapping a term out).
	// ⚠ Without it this leg is INVISIBLE: it is added to BASE after `plotBase` is captured, so it appears in NO
	// reported term and the terms silently stop summing to the rate. That gap cost a live investigation --
	// a -6.00 commerce residual had to be recovered by reconciling the combine by hand.
	struct TraitImprovementRow
	{
		int     trait;         // the raw TraitTypes id -- named by the RENDERER, never resolved at emit
		int     improvement;   // the raw ImprovementTypes id the entry is keyed on
		int     workedTiles;   // how many worked tiles carry it (the count the per-tile value multiplies)
		int64_t perTile;       // this entry's resolved value on ONE such tile
		int64_t contribution;  // exactly what this (trait x improvement) added to BASE (perTile x workedTiles)
	};
	//	pHeldTraits (optional) is the caller's ALREADY-WALKED active-set trait list. Pass it whenever the caller
	//	has one: collecting it here as well makes a single realized read pay the whole trait registry twice.
	static int64_t specialistTerm(const CvCity& city, int iChannel, const CvCascadeEvalCtx& evalCtx,
		std::vector<SpecialistTermRow>* pRowsOut = NULL,
		const std::vector<TraitContext::HeldTrait>* pHeldTraits = NULL);

	// ⚖ THE CITY RECEIVER -- the realized §2a RATE of a channel this city CONSUMES, re-summed from the members
	// it is made of rather than read from a stored total. Its BASE is the Σ over the city's WORKED PLOTS of
	// their own package flats -- the members whose realized values a receiver is defined as summing
	// (state-repositories.md § A CROSS-SCOPE receiver total).
	// ⛔ IT IS NOT A MAINTAINED SLOT, and could not be: a receiver is a Σ of COMBINES, and a combine is
	// non-linear in the deposits it reads -- moving one member's Σflat by Δ moves this by Δ scaled by that
	// member's OWN percent stack. There is no ±value a deposit could apply, which is why the receiver plane
	// carries no delta verb at all.
	// ⚖ THE §2a RATE, TERM BY TERM -- the DECOMPOSITION census the observability bar asks a route to carry
	// ([http-endpoints.md]: a route serving ONE number answers nothing when that number is wrong; one serving a
	// value term by term attributes a divergence to a NAMED source). A city's food rate is six independent
	// quantities collapsed into one int, so "production is too low" is unanswerable against the total alone.
	// ⛔ It is NOT a second combine ([DEC-single-implementation]) -- cityReceiverRate IS this function with the
	// terms discarded, so the census can never describe arithmetic the rate does not actually do.
	struct CityRateTerms
	{
		int64_t plotBase;      // Σ over the city's WORKED plots of their own package flats
		// plotBase's three SEGMENTS, summed over the same walk (CvCascadePackage: nature / improvement / rest).
		// The total alone cannot say WHICH leg a short plot yield is short on -- a dead improvement leg and a
		// dead nature leg look identical in it -- so the census carries all three or it attributes nothing.
		// ⚠ RAW: these are the pre-floor segment sums, so they need not add up to plotBase exactly (readFlat
		// floors nature at 0, improvement at -nature, and the total at 0). A gap between Σsegments and plotBase
		// is itself the signal that a floor is biting.
		int64_t plotNature;
		int64_t plotImprovement;
		int64_t plotRest;
		int64_t tradeYield;    // the ONE sanctioned live-yield input, lifted to ×100
		int64_t goldenAge;     // the empire goldenAge member-mirror, while the golden age holds
		int64_t upperFlat;     // team + empire flats -- TIER 1, so the percent stack multiplies them
		int64_t specialists;   // the assigned specialists' own output
		int64_t cityFlat;      // the city's own flats -- TIER 2 EXTRA, added AFTER the stack
		int     percentSum;    // Σ percent across the chain (the stack is 100 + this)
		int     workedPlots;   // how many plots the Σ above actually walked
		int64_t rate;          // what the combine produced
		// `specialists` DECOMPOSED, one row per held type -- filled from the same fold that produced the total,
		// for the same reason the plot segments are (one number cannot say which type it came from).
		std::vector<SpecialistTermRow> specialistRows;
		// The trait improvement leg DECOMPOSED, one row per (trait x improvement) that contributed. It is NOT a
		// decomposition of any field above -- this leg has no term of its own, which is exactly why it needs one.
		std::vector<TraitImprovementRow> traitImprovementRows;
	};
	static int64_t cityReceiverRate(const CvCity& city, int iChannel, CityRateTerms* pTermsOut = NULL);

	// ONE deposit that COULD have landed at this city and did NOT, because its §3.9 condition answered no.
	// ⛔ THIS IS THE ONLY WINDOW ONTO THE REFUSED HALF OF THE COMBINE. Every package read, every receiver total
	// and every term of CityRateTerms reports what IS there; a value that is short because a condition answered
	// NO leaves no trace in any of them, so the shortfall cannot be attributed from the totals at all
	// ([DEC-no-guessing]: at a gap the moves are VERIFY or ASK, and a bare total supports neither).
	// ⚠ It is a DIAGNOSTIC re-walk, not a maintained sum: nothing folds on it and nothing caches it. That is
	// exactly why it may walk the authored entries, which the apply path must never do.
	struct RefusedDeposit
	{
		const char* szSource;            // the source's type string -- BORROWED from the info, which outlives the read
		int64_t iValue;                  // what it deposits (×100 flat side; unscaled percent side)
		bool bPercentSide;
		const CvCondition* pCondition;   // the gate -- BORROWED; NULL = unconditioned
		bool bApplied;                   // did it land? false = the gate refused it
	};
	// EVERY city-scope entry of iChannel that an ACTIVE building authors -- conditioned or not, applied or
	// refused. Sorted by |value| descending.
	// ⚖ BOTH SIDES OR IT ANSWERS NOTHING. A refusal list alone shows what is missing and hides what is wrong: a
	// percent that should not be applying is exactly as invisible in a total as one that should be and is not.
	// Listing the APPLIED entries makes the total checkable -- their Σ is what the package's own city-scope slot
	// should hold, so the census can be reconciled against the number it explains rather than merely narrating
	// beside it.
	// ⚠ CITY SCOPE ONLY, deliberately and statedly: the empire and team legs of the percent stack are NOT walked
	// here (their sources are the player's civics/traits/techs, not this city's buildings), so a per-source
	// attribution of those is still missing. The scope SPLIT is served beside this so the unattributed remainder
	// is at least bounded and named.
	static void cityRefusedDeposits(const CvCity& city, int iChannel,
		std::vector<RefusedDeposit>& refusedOut);

	static int realizedAtCity(const CvCity& city, int iChannel);
	// The CITY chain's two LEGS, before the combine -- the ONE description of what a city sits under (team +
	// empire + (area × owner) + city), shared by realizedAtCity and by every consumer that needs the ONE additive
	// stack as a number of its own (the commerce split's slider share). It is the SAME roll-up, handed back
	// un-combined; a consumer that re-walked the chain itself would be a second description of the chain.
	// ⛔ The flat leg is NOT a realized answer: a channel the city CONSUMES answers its maintained receiver sum,
	// which is realizedAtCity's preference and is deliberately absent here. iChannel < 0 answers both legs 0.
	static void rolledLegsAtCity(const CvCity& city, int iChannel, int64_t& flatSum, int64_t& percentSum);
	// The TIERED form (modifier.md §2a): upper-scope flats are BASE (the stack multiplies them), the city's own
	// are the post-stack EXTRA. A caller computing a RATE wants this one; the flat form answers a plain roll-up.
	static void rolledLegsAtCity(const CvCity& city, int iChannel, int64_t& upperFlatSum,
		int64_t& cityFlatSum, int64_t& percentSum);
	static int realizedAtEmpire(const CvPlayer& player, int iChannel);
	static int realizedAtTeam(const CvTeam& team, int iChannel);

	// ---- the §2a fold seams (their canonical calc functions live HERE so the future package rebuild and the
	// ---- expected* endpoints call the SAME math -- pure statics: inputs in, ×100 out).

	// THE FAMILY-COMBINE FLOOR (modifier.md §2 "a `min` member that floors the combined total" -- declared as
	// FAMILY metadata, never per-deposit): apply the (family, kind) slot's ruled combine floor to a summed group
	// total. Consumes infoCombineFloorAtZero (ruling 28: upkeep.freeMilitary/freeCivilian -- Σfree floored at 0
	// as a GROUP; signed free-amount entries sum first, the floor applies once here).
	static int64_t combinedGroupSum(ModifierFamily eFamily, int iKind, int64_t sum);

	// THE FREE-UPKEEP NET (modifier.md §2a sibling; ruling 28's second, ENGINE-side floor): net class upkeep =
	// max(0, upkeep − free). `freeAmount` is the ALREADY-FLOORED group total (combinedGroupSum above) -- the two
	// floors are distinct by design (free >= 0, then net >= 0; engine CvPlayer.cpp:10295/:10315).
	static int64_t netUpkeepAfterFree(int64_t upkeep, int64_t freeAmount);

	// THE RESOLVED CITY LIMIT (ruling 26): the engine-side read of a civic's base city-limit config -- base ×
	// the world-size scale percent, gated on GAMEOPTION_EXP_OVEREXPANSION_PENALTIES exactly as the archived
	// engine getter (0 when the option is off or the civic carries no limit). The per.above CITY_LIMIT eval leg
	// (MMKernel::perApply) applies the SAME scale; its option gate rides the authored deposit's `enabled`.
	static int resolvedCityLimit(int iBaseCityLimit);

	// THE OPPOSING-PAIR NETS (modifier.md §2b): wellbeing is FOUR ORDINARY CHANNELS summed in opposing pairs,
	// and the pairing is a FINAL-STATE calculation over numbers a group read already handed out -- never a
	// channel of its own and never a getter ([patterns.md] rule 6). It lives here, once, so the city's realized
	// level, the AI's candidate valuation (expectedWellbeing) and the tooltips all net the SAME way
	// ([DEC-single-implementation]).
	// ⛔ PURE: fed the four channels, not an object -- the caller decides WHICH four (a city's realized set, or
	// a candidate's expected delta), which is exactly what lets one implementation serve both.
	// ×100 in, WHOLE out: the discrete boundary is here, because faces and health points are whole
	// ([DEC-fixedpoint-x100]: a whole game count reduces at the point of use).
	static int netHappiness(const int (&wellbeing)[NUM_WELLBEING_CHANNELS]);
	static int netHealth(const int (&wellbeing)[NUM_WELLBEING_CHANNELS]);

};

#endif // CV_INFO_VALUATION_H
