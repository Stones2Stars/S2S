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
#include "Infrastructure/CvDerivedCache.h"   // CvDerivedCacheSet -- THE cache component; never hand-roll another

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

// ===== THE PACKAGE: one dictionary =====
// ⛔ NOT a cache implementation -- CvDerivedCacheSet IS the cache. This only holds the owner-side STORAGE the
// component's contract requires ("component storage stays owner-side; this class owns only the dirty protocol")
// next to the set that guards it, so the pair is declared once instead of four loose members per object.
// ONE DIRTY BIT PER CHANNEL: mark with (1 << ch), read ensures only that channel's bit.
// An object holds TWO of these -- one flats, one percents -- which is the whole type axis.
template <class TOwner>
struct CascadePackages
{
	int flat[NUM_CASCADE_CHANNELS];
	int percent[NUM_CASCADE_CHANNELS];
	// ONE bit per CHANNEL, covering BOTH dictionaries. Not an oversight: a single pass over a source's deposits
	// fills a channel's flat AND percent together, so splitting the MASK by dictionary would double the walk to
	// save nothing. The dictionaries stay separate STORAGE (the type axis); the staleness question -- "is this
	// channel stale?" -- is genuinely one question.
	CvDerivedCacheSet<TOwner> set;

	CascadePackages()
	{
		for (int i = 0; i < NUM_CASCADE_CHANNELS; ++i) { flat[i] = 0; percent[i] = 0; }
	}

	// The reads: refill THIS channel if stale, then a bare fetch. Disjoint channels never pay for each other.
	int readFlat(CascadeChannel ch) const    { set.ensure(1 << (int)ch); return flat[ch]; }
	int readPercent(CascadeChannel ch) const { set.ensure(1 << (int)ch); return percent[ch]; }
	// Raw fetches with no ensure -- for a caller that has already ensured (a combine walking many channels).
	int rawFlat(CascadeChannel ch) const    { return flat[ch]; }
	int rawPercent(CascadeChannel ch) const { return percent[ch]; }

	void markDirty(CascadeChannel ch) const { set.markDirty(1 << (int)ch); }
	void markAllDirty() const { set.markAllDirty(); }
};

class CvInfo;

// ===== THE GENERIC SUM =====
// The whole gather, for every scope and every source kind. The cascade is POINTED AT A LIST of infos and adds
// their deposits into the two dictionaries -- it does not know, and must never ask, what KIND of source each
// info is ([DEC-enabler-not-cascade]: the list is the enabler's answer, the summing is the cascade's job).
// One pass per info over its own deposits; no address string, no per-channel rescan, no per-source walker.
class CascadeSum
{
public:
	// Add every deposit `d` makes AT `iScope` into the dictionaries, restricted to the channels in iChanMask
	// (the dirty set being refilled). aFlat/aPercent are the owner-side int[NUM_CASCADE_CHANNELS] arrays.
	static void addInfo(const CvInfo* d, int iScope, int iChanMask, int* aFlat, int* aPercent);

	// Zero the channels in iChanMask in both dictionaries -- a refill must FULLY DEFINE its output
	// (CvDerivedCache contract rule 2: a partial write leaves stale values behind a clean flag).
	static void beginRefill(int iChanMask, int* aFlat, int* aPercent);
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
