#pragma once
#ifndef CV_TRIGGERS_H
#define CV_TRIGGERS_H

//
//	CvTriggers -- the `triggers` section as ONE composable unit (json.md §5): the happening-fired / rolled /
//	state-conditioned effects an entity carries. Each entry is the three-part anatomy in reading order --
//	`trigger` (WHEN/WHY: an on-token and/or a §3 state condition) -> `chance` (the odds, always on the trigger,
//	§3.7 per-scalable) -> `action` (the open verb object). Composed BY VALUE on the derived infos that author
//	triggers (buildings / features / improvements / traits / units -- the data-grounded table).
//	WRITE-ONCE AT LOAD. Owns its entries, their conditions, and any nested grant payload.
//
//	Scale: magnitudes are ×100 at parse (the one human->fixed-point boundary): the chance value, the heal
//	amount, the property amount. Counts stay raw (the heal unit cap, the happening interval, the spatial radius).
//

#include "CvCondition.h"
#include <string>
#include <vector>

namespace picojson { class value; }
class CvGrants;

// One `triggers` entry. Exactly ONE action verb group is live per entry (the authored §5 shapes:
// promote / grant / heal / property delta).

// The FREE-PROMOTION happening, in ONE place because three sites string-match it and a typo in any of them
// silently grants nothing. It is the unit ENTERING the city: the applier is targeted propagation off
// SEVT_UNIT_ENTERED_CITY, with a source going ACTIVE completing the same relation for units already present.
// ⛔ There is NO per-turn sweep on this plane and none is to be added: the rescan it replaced measured 42,336
// assign calls in ONE turn, nearly all of them re-checking promotions the units already held.
// ⚠ NOT to be confused with the genuine recurring trigger `onTurn`, which is a real happening and STAYS (the
// property-scaled criminal spawn, json.md §5).
extern const char* const TRIGGER_UNIT_ENTERED_CITY;

class CvTriggerEntry
{
public:
	// --- trigger: the WHEN/WHY, in one of THREE forms ---
	std::string happening;         // the on-token ("onTurn" / "onUnitEnteredCity" / ... -- the spine's DOMAIN
	                               // happenings in authoring form, an OPEN registry); "" = state-conditioned only
	// The IMPLICIT happening: this entry came from the entity's own `grants` block, so it fires on the source's
	// own CONSIDERED ACTION -- a building's construction, a tech's research, a civic's adoption, a settler's
	// founding (json.md §5). It is never AUTHORED (a modder writes a plain `grants` block and no trigger field),
	// which is exactly why it is a compiled flag rather than an on-token: there is no token to collide with, and
	// the dispatcher's own event already names which considered action it is.
	bool consideredAction;
	int happeningInterval;         // {"onTurn": N} = every N turns (1 = every turn)
	CvCondition* condition;    // a §3 state condition (NULL = none); a state-only trigger evaluates each turn
	// --- chance: the odds (0 = no roll -> the action always lands when the trigger fires) ---
	int chanceValue;            // percent ×100
	int chancePerTypeId;           // the §3.7 per count-scaler type FK (-1 = none/token)
	std::string chancePerToken;    // the catch-all token when the per type is no FK (POPULATION / ...)
	int chancePerEach;             // the per quantum (default 1)
	int chancePerScope;            // the AUTHORED per scope (a CvCascScope value; -1 = the entry's own scope)
	std::vector<int> chancePerAnyOf;   // per.anyOf summed-count FK ids (json §3.7)
	// --- action: destroy (the trigger's own SUBJECT is removed) ---
	// The subject is the entity the trigger is authored on, implicit exactly as a `grants` happening is
	// (json.md §5), so "self" needs no target vocabulary. Live carrier: a FEATURE dying as its city grows past
	// the authored population -- the containment chain is ordinary (a city knows its plot, the plot carries the
	// feature), so the feature reads the city's POPULATION fact and goes.
	bool destroySelf;
	// --- action: promote (units in scope gain promotions) ---
	std::vector<int> promotePromotions;   // action.promote.promotions FK ids
	std::string promoteUnits;             // action.promote.units target selector ("present")
	// --- action: grant (the §5 payload vocabulary nested whole) ---
	CvGrants* grant;           // owned (NULL = none)
	// --- action: heal ---
	int heal;                   // action.heal: N (×100; 0 = none)
	bool healFull;                 // action.heal: "full"
	int healUnitCombatId;          // action.unitCombat FK -- the healed class (-1 = any)
	int healCount;                 // action.count -- the full-heal unit cap (raw count)
	// --- action: property delta (+ the #429 spatial intent the distribution reads) ---
	int propertyId;                // action.PROPERTY_* FK (-1 = none)
	int propertyAmount;         // its per-fire amount (×100, signed)
	std::string spatialOn;         // action.on ("plot" / "" = the emitting object)
	std::string spatialRelation;   // action.relation ("near" / "same" / "")
	int spatialDistance;           // action.distance -- the radius for "near"

	CvTriggerEntry();
	~CvTriggerEntry();

private:
	CvTriggerEntry(const CvTriggerEntry&);            // noncopyable -- owns the condition + the nested grant
	CvTriggerEntry& operator=(const CvTriggerEntry&);
};

// The ONE payload plane an entity carries. `triggers` and `grants` are TWO AUTHORING SHAPES that compile into
// THIS ONE ENTRY LIST -- json.md §5: "the split is about AUTHORING, not about two runtime mechanisms". A
// `grants` block becomes a single entry with the implicit considered-action happening, no condition and no roll
// (the degenerate case), so nothing about a grant has machinery of its own.
class CvTriggers
{
public:
	CvTriggers() : m_iConsidered(-1) {}
	~CvTriggers();

	// The unit's single load-time writer: parse the whole `triggers` array (every §5 entry shape; an
	// unrecognized entry/action key surfaces via the unconsumed census, never silently drops).
	void parse(const picojson::value& v);
	// The OTHER authoring shape: the whole `grants` block -> ONE entry in this same list, implicit happening.
	void parseGrants(const picojson::value& v);
	void clearParsed();   // frees the owned entries + resets (the clear-first half of the section re-map)

	const std::vector<CvTriggerEntry*>& entries() const { return m_entries; }
	bool isEmpty() const { return m_entries.empty(); }

	// The considered-action entry's payload -- the `grants` block's compiled home (NULL when the entity authors
	// none). O(1): the entry's index is captured at parse, never searched for at read.
	const CvGrants* consideredGrant() const;

private:
	std::vector<CvTriggerEntry*> m_entries;   // owned
	int m_iConsidered;                        // index into m_entries of the considered-action entry (-1 = none)

	CvTriggers(const CvTriggers&);            // noncopyable -- owns its entries
	CvTriggers& operator=(const CvTriggers&);
};

#endif // CV_TRIGGERS_H
