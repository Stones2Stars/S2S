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
//	(the only human->data boundary); the evaluator ([CvCascadeConditionEval]) then pattern-matches typed nodes and
//	NEVER re-reads JSON. Owns its children (freed in the dtor); noncopyable.
//

#include <string>
#include <vector>

// The containment-spine scope (json §3.2, singular). SELF is the off-spine own-build scope.
enum CvCascScope
{
	CASC_SCOPE_WORLD, CASC_SCOPE_TEAM, CASC_SCOPE_EMPIRE, CASC_SCOPE_AREA, CASC_SCOPE_CITY, CASC_SCOPE_PLOT,
	CASC_SCOPE_IMPROVEMENT, CASC_SCOPE_FEATURE, CASC_SCOPE_TERRAIN, CASC_SCOPE_ROUTE, CASC_SCOPE_BUILDING,
	CASC_SCOPE_SPECIALIST, CASC_SCOPE_UNIT, CASC_SCOPE_SELF
};

// How a resource atom reaches the city (json §3.4 `connection`).
enum CvCascConnection { CASC_CONN_NONE, CASC_CONN_TRADE, CASC_CONN_VICINITY, CASC_CONN_TRADE_OR_VICINITY };

// Which radius tiles a vicinity bonus counts (json §3.4; NONE = the DEFAULT owned+neutral, NOT foreign).
enum CvCascVicinity { CASC_VIC_NONE, CASC_VIC_OWNED, CASC_VIC_WORKED, CASC_VIC_CONNECTED, CASC_VIC_CROSSBORDER };

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

	// parameterized (Type param in `param`)
	CASC_PRED_HAS_TERRAIN, CASC_PRED_HAS_IMPROVEMENT, CASC_PRED_HAS_BONUS, CASC_PRED_HAS_RELIGION, CASC_PRED_STATE_RELIGION, CASC_PRED_IS_HOLY_CITY, CASC_PRED_HAS_CORPORATION,
	// numeric-parameterized
	CASC_PRED_LATITUDE, CASC_PRED_EXISTED_FOR,
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
	int min;                                    // presence = min:1; a count threshold otherwise; -1 = unset
	int max;                                    // -1 = unset
	CvCascConnection connection;
	CvCascVicinity vicinity;

	// --- PREDICATE (Predicate, json §3.5) ---
	CvCascPredKind predKind;
	std::string param;                          // a Type param (BONUS_X/TERRAIN_X/RELIGION_X/...) -- empty for bare

	// The engine id of `type` (PRESENCE) or `param` (PREDICATE), FK-resolved ONCE by the parser (the C# compares
	// string sets; the C++ evaluator reads id-keyed engine accessors). -1 = unresolved / not an infotype (a token).
	int id;

	CvCondition()
		: kind(CASC_COND_GROUP), enabled(NULL), disabled(NULL), scope(CASC_SCOPE_CITY), min(-1), max(-1),
		  connection(CASC_CONN_NONE), vicinity(CASC_VIC_NONE), predKind(CASC_PRED_UNKNOWN), id(-1) {}
	~CvCondition();

private:
	CvCondition(const CvCondition&);            // noncopyable -- owns its child nodes
	CvCondition& operator=(const CvCondition&);
};

#endif // CV_CONDITION_H
