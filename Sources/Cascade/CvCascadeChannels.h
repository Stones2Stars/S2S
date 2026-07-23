#pragma once
#ifndef CV_CASCADE_CHANNELS_H
#define CV_CASCADE_CHANNELS_H

//
//	The CHANNEL INDEX + the ONE storage shape ([DEC-uniform-cache-shape], modifier.md §1:
//	"Sigma flat and Sigma percent each their OWN package per channel").
//
//	THE SHAPE (owner): anything that STORES modifiers -- CvPlot, CvCity, CvPlayer, CvArea, CvGame, and anywhere
//	else -- holds TWO DICTIONARIES keyed by the unified channel enum below: one for percentages, one for flats.
//
//	⛔ THE STORAGE IS THE EXISTING COMPONENT -- do NOT hand-roll a cache here. Each dictionary is a plain
//	int[NUM_CASCADE_CHANNELS] on the owner plus a CvDerivedCacheSet with ONE DIRTY BIT PER CHANNEL
//	(NUM_CASCADE_CHANNELS fits a 32-bit mask with room). CvDerivedCacheSet already IS the partial-dirty form --
//	owner-side storage, the component owning only the dirty protocol -- and it carries the contract rules
//	(clear-dirty-before-recompute, fully-define-the-output, noncopyable) that a bespoke slot type re-derives
//	wrongly. state-repositories.md has ONE reusable component precisely so no per-cache variant gets invented.
//
//	Consequences:
//	 - DIRTINESS IS PER CHANNEL: an event marks the channels it feeds (1 << ch) and nothing else refills.
//	 - VALUE vs PERCENT is the WHOLE type axis (owner: "any yield is an integer, any percentage is an integer --
//	   as long as we know if the package is value or percentage, we know what to do with it"). WHICH DICTIONARY
//	   a number sits in IS its type; nothing stores a per-value type and no position axis exists.
//	 - NO CONDITIONS LIVE HERE. Liveness (enabled/disabled/dormancy) is the ENABLER's verdict
//	   ([DEC-enabler-not-cascade]); the cascade sums the live list it is handed and never asks "is this on?".
//	 - A package holds ONLY its own scope's deposits (modifier.md §1); the downward roll happens at READ.
//
//	NEVER serialized; all-dirty from construction, so the first read after a load refills from current state.
//

#include "Defines/CvEnums.h"

// ===== THE UNIFIED CHANNEL ENUM =====
// One index across every modifiable number ("if you have to compute a unified enum for all yields, you do that"
// -- owner). Yields and commerce lead in their engine enum order so CH_YIELD(y) / CH_COMMERCE_CH(c) are adds,
// never lookups; the scalar families follow.
enum CascadeChannel
{
	// -- yields: MUST match YieldTypes order --
	CH_FOOD = 0,
	CH_PRODUCTION,
	CH_COMMERCE,
	// -- commerce: MUST match CommerceTypes order --
	CH_GOLD,
	CH_RESEARCH,
	CH_CULTURE,
	CH_ESPIONAGE,
	// -- the scalar families --
	CH_GREAT_PEOPLE,
	CH_DEFENSE,
	CH_DEFENSE_BOMBARD,
	CH_DEFENSE_MIN,
	CH_MAINTENANCE,
	CH_TRADE_ROUTES,
	CH_BUILDRATE_MILITARY,
	CH_BUILDRATE_SPACE,
	CH_BUILDRATE_WORLD_WONDER,
	CH_BUILDRATE_TEAM_WONDER,
	CH_BUILDRATE_NATIONAL_WONDER,
	CH_BUILDRATE_UNIT,
	CH_BUILDRATE_BUILDING,
	CH_FREE_SPECIALISTS,
	// -- wellbeing: four ordinary channels (owner), summed in opposing pairs at the verdict --
	CH_HAPPINESS,
	CH_ANGER,
	CH_HEALTH,
	CH_UNHEALTH,

	NUM_CASCADE_CHANNELS
};

inline CascadeChannel CH_YIELD(int eYield)      { return (CascadeChannel)(CH_FOOD + eYield); }
inline CascadeChannel CH_COMMERCE_CH(int eComm) { return (CascadeChannel)(CH_GOLD + eComm); }

// ===== THE SCOPE INDEX =====
// The containment spine (json.md §3.2) as a compact index, so a compiled deposit carries its scope as an int.
enum CascadeScope
{
	CSC_WORLD = 0,
	CSC_TEAM,
	CSC_EMPIRE,
	CSC_AREA,
	CSC_CITY,
	CSC_PLOT,
	CSC_UNIT,
	CSC_SELF,              // the off-spine `self` scope (buildRate.self)
	NUM_CASCADE_SCOPES
};

// ===== LOAD-TIME ADDRESS RESOLUTION (strings -> ints, ONCE) =====
// Resolve a deposit's dotted address to its channel + which dictionary it belongs in. Called ONLY from
// DepositIndex::compile at load; the runtime never sees a string.
//  - `family` = address segment 0 ("food", "greatPeopleRate", "defense", ...)
//  - `member` = address segment 2 where present ("amount", "bombardDefense", "military", ...), else ""
//  - `unit`   = the unit segment ("flat"/"percent"/...)
// Returns false when the address is not a cascade channel -- the caller skips it, which is how the unit-plane
// families and any retired system drop out with no special-casing.
bool cascadeResolveAddress(const char* family, const char* member, const char* unit,
                           CascadeChannel& outChannel, bool& outIsPercent);

// The scope segment ("world"/"team"/"empire"/"area"/"city"/"plot"/"unit"/"self") -> its index; -1 if unknown.
int cascadeScopeFromSegment(const char* scope);

#endif // CV_CASCADE_CHANNELS_H
