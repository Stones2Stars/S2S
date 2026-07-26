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
class CvTriggerEntry
{
public:
	// --- trigger: the WHEN/WHY ---
	std::string happening;         // the on-token ("onTurn" / "onTurnEnd" / ... -- the spine's DOMAIN happenings
	                               // in authoring form, an OPEN registry); "" = state-conditioned only
	int happeningInterval;         // {"onTurn": N} = every N turns (1 = every turn)
	CvCondition* condition;    // a §3 state condition (NULL = none); a state-only trigger evaluates each turn
	// --- chance: the odds (0 = no roll -> the action always lands when the trigger fires) ---
	int chanceValue;            // percent ×100
	int chancePerTypeId;           // the §3.7 per count-scaler type FK (-1 = none/token)
	std::string chancePerToken;    // the catch-all token when the per type is no FK (POPULATION / ...)
	int chancePerEach;             // the per quantum (default 1)
	int chancePerScope;            // the AUTHORED per scope (a CvCascScope value; -1 = the entry's own scope)
	std::vector<int> chancePerAnyOf;   // per.anyOf summed-count FK ids (json §3.7)
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

class CvTriggers
{
public:
	CvTriggers() {}
	~CvTriggers();

	// The unit's single load-time writer: parse the whole `triggers` array (every §5 entry shape; an
	// unrecognized entry/action key surfaces via the unconsumed census, never silently drops).
	void parse(const picojson::value& v);
	void clearParsed();   // frees the owned entries + resets (the clear-first half of the section re-map)

	const std::vector<CvTriggerEntry*>& entries() const { return m_entries; }
	bool isEmpty() const { return m_entries.empty(); }

private:
	std::vector<CvTriggerEntry*> m_entries;   // owned

	CvTriggers(const CvTriggers&);            // noncopyable -- owns its entries
	CvTriggers& operator=(const CvTriggers&);
};

#endif // CV_TRIGGERS_H
