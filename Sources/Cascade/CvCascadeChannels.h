#pragma once
#ifndef CV_CASCADE_CHANNELS_H
#define CV_CASCADE_CHANNELS_H

//
//	The CHANNEL INDEX + the ONE storage shape ([DEC-uniform-cache-shape], modifier.md §1:
//	"Sigma flat and Sigma percent each their OWN package per channel").
//
//	THE SHAPE (owner): anything that STORES modifiers -- CvPlot, CvCity, CvPlayer, CvArea, CvGame, and anywhere
//	else -- holds TWO DICTIONARIES: one for percentages, one for flats. Both are keyed by the unified channel
//	enum below. Each entry is ONE SLOT OBJECT holding that channel's cached number, its OWN dirty flag, and a
//	straight read.
//
//	Consequences, all of them simplifications:
//	 - DIRTINESS IS PER CHANNEL. A slot knows whether it is stale by itself, so there is no shared bitmask, no
//	   mask vocabulary per scope, and no "which bit covers this field" question. An event marks the channels it
//	   feeds and nothing else rebuilds.
//	 - VALUE vs PERCENT is the WHOLE type axis (owner: "any yield is an integer, any percentage is an integer --
//	   as long as we know if the package is value or percentage, we know what to do with it"). Which dictionary
//	   a slot sits in IS its type; no per-slot type is stored and no position axis exists.
//	 - NO CONDITIONS LIVE HERE. Liveness (enabled/disabled/dormancy) is the ENABLER's verdict
//	   ([DEC-enabler-not-cascade]); the cascade sums the live list it is handed and never asks "is this on?".
//	   So there are no gated slots and no gate bookkeeping.
//	 - The slot carries no owner pointer and no recompute callback: it is storage + a flag + a read. The filler
//	   refills dirty channels; reads are BARE FETCHES.
//
//	NEVER serialized; dirty from construction, so the first read after a load recomputes from current state.
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

// ===== THE SLOT: one channel's cached number =====
// Storage + its own dirty flag + a straight read. No owner pointer, no recompute callback, no template: the
// filler refills what is dirty, readers fetch. Dirty from construction (never trusted from a save).
struct CascadeSlot
{
	int  value;
	bool dirty;

	CascadeSlot() : value(0), dirty(true) {}

	int  read() const   { return value; }           // the straight read -- never recomputes
	bool isDirty() const { return dirty; }
	void markDirty()    { dirty = true; }
	void store(int v)   { value = v; dirty = false; }
	void add(int v)     { value += v; }             // during a refill (the filler clears + accumulates)
	void beginFill()    { value = 0; }              // fully redefine on every refill (CvDerivedCache rule 2)
	void endFill()      { dirty = false; }
};

// ===== THE TWO DICTIONARIES =====
// The one storage shape, identical on every object that holds modifiers. Not templated on the owner -- there is
// nothing owner-specific left in it, which is what makes "the same object type everywhere" literal.
struct CascadeModifiers
{
	CascadeSlot flat[NUM_CASCADE_CHANNELS];
	CascadeSlot percent[NUM_CASCADE_CHANNELS];

	// -- straight reads --
	int  readFlat(CascadeChannel ch) const    { return flat[ch].read(); }
	int  readPercent(CascadeChannel ch) const { return percent[ch].read(); }

	// -- the dirty protocol, per channel --
	void markDirty(CascadeChannel ch)        { flat[ch].markDirty(); percent[ch].markDirty(); }
	void markFlatDirty(CascadeChannel ch)    { flat[ch].markDirty(); }
	void markPercentDirty(CascadeChannel ch) { percent[ch].markDirty(); }
	void markAllDirty()
	{
		for (int i = 0; i < NUM_CASCADE_CHANNELS; ++i) { flat[i].markDirty(); percent[i].markDirty(); }
	}
	bool anyDirty() const
	{
		for (int i = 0; i < NUM_CASCADE_CHANNELS; ++i)
			if (flat[i].dirty || percent[i].dirty) return true;
		return false;
	}
};

// ===== THE INFO-SIDE ARRAYS =====
// An info's AUTHORED values for one scope, materialized at load ([DEC-materialize-at-mapfrom]). Plain ints:
// authored data is static, so unlike the object-side CascadeSlot there is no dirty flag and nothing to refresh.
// This is the "info can have a getFlats and a getPercentages" shape (owner) -- the cascade is then pointed at a
// LIST of infos and sums these, and the summing needs no knowledge of what kind of source it is walking.
struct CascadeInfoModifiers
{
	int flat[NUM_CASCADE_CHANNELS];
	int percent[NUM_CASCADE_CHANNELS];
	CascadeInfoModifiers()
	{
		for (int i = 0; i < NUM_CASCADE_CHANNELS; ++i) { flat[i] = 0; percent[i] = 0; }
	}
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
