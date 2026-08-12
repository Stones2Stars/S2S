#pragma once
#ifndef CV_CONDITION_H
#define CV_CONDITION_H

//
//	CvCondition -- the PORT of StoneBase `Domain/Conditions/Condition.cs`: the typed condition tree (json
//	§3.4/§3.5), the parity-proven shared boolean vocabulary that the `enabled`/`disabled` clauses and
//	`requires.build`/`requires.operate` resolve through. StoneBase is the validated pseudo-code; this is a FAITHFUL
//	transcription (owner ruling 2026-06-30: "the logic works because of StoneBase, we just port the C# code").
//
//	C++03 form (StoneBase's own note): the C# abstract-base + sealed-derived discriminated union becomes ONE tagged
//	struct dispatched by a `kind` switch (no virtual visitor, no RTTI). The parser fills it ONCE from the curated JSON
//	(the only human->data boundary); the evaluator (`cascadeEvalCondition`) then pattern-matches typed nodes and
//	NEVER re-reads JSON. Owns its children (freed in the dtor); noncopyable.
//

#include <string>
#include <vector>

// This class carries `min`/`max` MEMBERS, and the windef.h min/max FUNCTION-MACROS are live on any include path
// that reaches here before Engine/CvGameCoreUtils.h's #undef (boost pulls windows.h in ahead of the PCH's
// NOMINMAX). Inside that window `min(-1)` in the ctor's initializer list macro-expands and the class will not
// parse. The guard belongs HERE, on the header that owns the members, rather than on whichever header happens to
// include it -- a per-includer guard only ever covers the paths someone thought of, and a new path silently
// breaks the build somewhere else entirely. The macros are dead in engine code by standing rule.
#undef min
#undef max

// The containment-spine scope (json §3.2, singular). SELF is the off-spine own-build scope.
//
// A scope must be unambiguously OWNABLE -- either UNIVERSAL (world: it affects everyone, always, so ownership
// never arises) or owned by exactly ONE player up the chain (team / empire / city / plot). That is what lets a
// deposit roll DOWN and a target read one combined total. A LANDMASS satisfies neither: it is shared by several
// empires at once, so an effect on it is inherently a per-(landmass x player) CROSS-PRODUCT rather than a scope,
// and modelling it as one forces a bespoke slot in the middle of the spine.
enum CvCascScope
{
	CASC_SCOPE_WORLD, CASC_SCOPE_TEAM, CASC_SCOPE_EMPIRE, CASC_SCOPE_CITY, CASC_SCOPE_PLOT,
	CASC_SCOPE_IMPROVEMENT, CASC_SCOPE_FEATURE, CASC_SCOPE_TERRAIN, CASC_SCOPE_ROUTE, CASC_SCOPE_BUILDING,
	CASC_SCOPE_SPECIALIST, CASC_SCOPE_UNIT, CASC_SCOPE_SELF
};

// How a resource atom reaches the city (json §3.4 `connection`).
enum CvCascConnection { CASC_CONN_NONE, CASC_CONN_TRADE, CASC_CONN_VICINITY, CASC_CONN_TRADE_OR_VICINITY };

// Which radius tiles a vicinity bonus counts (json §3.4; NONE = the DEFAULT owned+neutral, NOT foreign).
enum CvCascVicinity { CASC_VIC_NONE, CASC_VIC_OWNED, CASC_VIC_WORKED, CASC_VIC_ONSITE, CASC_VIC_CROSSBORDER };

// The canonical predicate vocabulary (json §3.5) -- bare + parameterized share the enum; param/min/max carry the
// parameters. (Order/membership mirrors StoneBase PredicateKind; UNKNOWN=0 is IGNORED by the evaluator, never false.)
enum CvCascPredKind
{
	CASC_PRED_UNKNOWN = 0,
	// environment / domain (IS_<where>)
	CASC_PRED_IS_WATER, CASC_PRED_IS_LAND, CASC_PRED_IS_FLATLANDS, CASC_PRED_IS_AIR, CASC_PRED_IS_SPACE, CASC_PRED_IS_LUNAR, CASC_PRED_IS_MARS,
	// plot attributes (HAS_<attr>) -- HasCoast carries an optional minArea in min; HasFeature an optional type in param
	CASC_PRED_HAS_PEAK, CASC_PRED_HAS_HILLS, CASC_PRED_HAS_COAST, CASC_PRED_HAS_RIVER, CASC_PRED_HAS_FRESHWATER, CASC_PRED_HAS_IRRIGATION, CASC_PRED_HAS_FEATURE, CASC_PRED_HAS_LANDMARK,
	// plot city-relative state (VICINITY superset WORKABLE superset IS_WORKED)
	CASC_PRED_VICINITY, CASC_PRED_WORKABLE, CASC_PRED_IS_WORKED,
	// world
	CASC_PRED_NO_NUKES,
	// city / player
	CASC_PRED_IS_CAPITAL, CASC_PRED_IS_GOVERNMENT_CENTER, CASC_PRED_HAS_POWER, CASC_PRED_HAS_STATE_RELIGION, CASC_PRED_STATE_RELIGION_IN_CITY, CASC_PRED_IS_GOLDEN_AGE, CASC_PRED_IS_STATE_RELIGION_HOLY_CITY,
	CASC_PRED_IS_ANARCHY, CASC_PRED_IS_OWNED,   // #430 outcome gates: player in anarchy; plot is in owned territory
	// The player is in REVOLT against a parent civ. A genuine empire STATE other families will want, minted as a
	// predicate rather than left as an arithmetic branch inside one formula ([DEC-conditions-are-predicates]).
	CASC_PRED_IS_REBEL,
	// the §3.7 counted-kind RELIGION filter's per-religion test (ruling 23: `religion: "!IS_STATE_RELIGION"`):
	// evaluated against the COUNTED religion (ctx.religion) -- true iff it is the owner's state religion
	CASC_PRED_IS_STATE_RELIGION,

	// parameterized (Type param in `param`)
	CASC_PRED_HAS_TERRAIN, CASC_PRED_HAS_IMPROVEMENT, CASC_PRED_HAS_BONUS, CASC_PRED_HAS_RELIGION, CASC_PRED_STATE_RELIGION, CASC_PRED_IS_HOLY_CITY, CASC_PRED_HAS_CORPORATION,
	// {IS_HEADQUARTERS: CORPORATION_X} -- the city is the corp's HQ city (ruling 10, the {IS_HOLY_CITY: R}
	// pattern; the corp HQ-revenue entries' gate). Bare form = HQ of ANY corporation.
	CASC_PRED_IS_HEADQUARTERS,
	// {CIVIC_CATEGORY: CIVICOPTION_X} -- the CIVIC whose value is being resolved sits in that category. A
	// SOURCE-SLOT predicate (contexts.md § THE SOURCE SLOTS): it asks about the entity the walk is resolving,
	// so with no civic in hand it answers FALSE rather than resolving against whatever was reached last.
	// It carries the FULL `CIVICOPTION_` id, never a bare `RELIGION`, so it can never collide with a RELIGION_ type.
	CASC_PRED_CIVIC_CATEGORY,
	// numeric-parameterized
	CASC_PRED_LATITUDE, CASC_PRED_EXISTED_FOR,
	// {natureYield:{food:N,...}} -- the improvement PLACEMENT threshold (one node per channel): the target
	// plot's PRE-improvement nature yield of the channel must be >= `min` (the engine gate: CvPlot.cpp
	// canHaveImprovement, calculateNatureYield(channel) < prereq -> invalid). `id` carries the
	// YieldTypes channel (no Type param -- `param` stays empty so the reverse walks never FK-route it).
	CASC_PRED_NATURE_YIELD,
	// classification-TAG membership: IS_<TAG> against a UNIT target (json §8/§3.5). `param` holds the full
	// TAG_<SUFFIX> type name; the id is resolved lazily at eval (the TAG_* infotypes are minted AFTER condition parse).
	CASC_PRED_IS_TAG
};

// The node discriminator (StoneBase's three Condition subtypes).
enum CvCascCondKind { CASC_COND_GROUP, CASC_COND_PRESENCE, CASC_COND_PREDICATE };

//
//	One node of the typed condition tree -- a tagged union (the C++03-faithful form). `kind` selects which fields are
//	live: GROUP uses all/anyOf/noneOf/enabled/disabled; PRESENCE uses type/scope/min/max/connection/vicinity;
//	PREDICATE uses predKind/param/min/max. `min`/`max` of -1 mean "unset" (StoneBase's nullable int).
//
class CvCondition
{
public:
	CvCascCondKind kind;

	// --- GROUP (ConditionGroup, json §3.4) ---
	std::vector<CvCondition*> all;       // AND -- every child must hold
	std::vector<CvCondition*> anyOf;     // OR  -- at least one (plain OR over direct children)
	std::vector<CvCondition*> noneOf;    // NONE -- no child may hold
	CvCondition* enabled;                // applies only while this holds (NULL = always)
	CvCondition* disabled;               // suppressed while this holds (NULL = never)

	// --- PRESENCE (PresenceAtom, json §3.4) ---
	std::string type;                           // an INFOTYPE id: TECH_*/BUILDING_*/BONUS_*/CIVIC_*/... (or a token)
	CvCascScope scope;                          // implied-from-type at parse, explicit here
	int min;                                    // presence = min:1; a count threshold otherwise
	int max;
	// ⛔ WHETHER A BOUND WAS AUTHORED, as its own flag -- NEVER inferred from the value's sign. A PROPERTY_ band
	// bound is legitimately NEGATIVE (the low-education tiers are authored entirely in negative bands), so a
	// `min < 0 means absent` test silently drops a real bound and collapses the clause to always-true.
	bool hasMin;
	bool hasMax;
	CvCascConnection connection;
	CvCascVicinity vicinity;

	// --- PREDICATE (Predicate, json §3.5) ---
	CvCascPredKind predKind;
	std::string param;                          // a Type param (BONUS_X/TERRAIN_X/RELIGION_X/...) -- empty for bare

	// The engine id of `type` (PRESENCE) or `param` (PREDICATE), FK-resolved ONCE by the parser (the C# compares
	// string sets; the C++ evaluator reads id-keyed engine accessors). -1 = unresolved / not an infotype (a token).
	int id;

	CvCondition()
		: kind(CASC_COND_GROUP), enabled(NULL), disabled(NULL), scope(CASC_SCOPE_CITY), min(-1), max(-1), hasMin(false), hasMax(false),
		  connection(CASC_CONN_NONE), vicinity(CASC_VIC_NONE), predKind(CASC_PRED_UNKNOWN), id(-1) {}
	~CvCondition();

private:
	CvCondition(const CvCondition&);            // noncopyable -- owns its child nodes
	CvCondition& operator=(const CvCondition&);
};

#endif // CV_CONDITION_H
