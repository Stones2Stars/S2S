#pragma once
#ifndef CV_CASCADE_CHANNELS_H
#define CV_CASCADE_CHANNELS_H

//
//	The CHANNEL INDEX + the ONE uniform package type ([DEC-uniform-cache-shape],
//	docs/architecture/state-repositories.md; docs/plans/structural-cleanup/scope-packages.md §1).
//
//	THE RULE: every derived cache on the cascade plane is the SAME OBJECT TYPE everywhere and they ALL
//	invalidate the SAME WAY. Only WHICH SLOTS carry a value varies by scope. A hand-named scalar field is a
//	DEFECT, not untidiness -- it cannot be addressed uniformly, so it forces a bespoke invalidation path per
//	field (which is how 33 of them accumulated across five bespoke structs).
//
//	A SLOT IS ONE INT. Any yield is an integer and any percentage is an integer (owner), so a slot carries no
//	value-type of its own -- the combine only needs to know whether the slot is a VALUE or a PERCENT, and that
//	is a STATIC property of its position (cascadeIsPercent below). Flat and percent therefore stay structurally
//	separate (modifier.md §2: the unit is part of the slot key) without two storage types: they are simply
//	different positions. `multiplier` is identity throughout the migration (no yield/commerce source authors
//	one), so value-vs-percent is the whole type axis.
//
//	THERE ARE NO SPECIAL CHANNELS. Wellbeing is FOUR ORDINARY CHANNELS (owner) -- happiness, anger, health,
//	unhealth -- summed in opposing pairs at the verdict, NOT one signed-split family with a duplicated bad
//	plane. Routing a negative deposit to its opposing channel happens at FILL; storage stays uniform.
//
//	SLOT KEY = (channel x position). The channel is WHAT is modified; the position is WHERE in the family's
//	combine the sum sits -- which for the deposit sources IS the source kind, so the same slots the game object
//	SUMS are the slots an endpoint DECOMPOSES ("the same bytes", scope-packages.md read-path ruling). Gated sums
//	are their OWN positions, stored UNGATED with the gate applied at read, so a gate flip invalidates nothing.
//
//	THE RECEIVER'S SUM IS A SLOT LIKE ANY OTHER (POS_SUM): the scope that CONSUMES a channel caches its realized
//	total in the same table beside the packages -- CvPlayer gold/research/culture/espionage, CvCity production/
//	culture. There is no separate receiver mechanism, and no dependency-ordered rebuild: one event marks the
//	packages it touches AND the sum slots they feed, and a sum's rebuild reads its packages through their own
//	lazy dirty-check, so a sum can never sit stale behind a clean package.
//
//	NEVER serialized; all-dirty from birth/reset; the load warm-up is the same ensure run eagerly.
//

#include "Defines/CvEnums.h"
#include "Infrastructure/CvDerivedCache.h"
#include <vector>

// ===== THE CHANNEL: every modifiable number, one flat compile-time index =====
// Yields and commerce lead in their engine enum order so CH_YIELD(eYield) / CH_COMMERCE(eCommerce) are adds,
// never lookups. Everything after them is a scalar family (the 33 hand-named fields this index replaces).
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
	CH_GREAT_PEOPLE,            // greatPeopleRate
	CH_DEFENSE,                 // defense.amount
	CH_DEFENSE_BOMBARD,         // defense.bombardDefense
	CH_DEFENSE_MIN,             // defense.min (a FLOOR -- family metadata, combined by max)
	CH_MAINTENANCE,             // maintenance (its own bookkeeping channel, DEC-maintenance-bookkeeping)
	CH_TRADE_ROUTES,            // tradeRoutes
	CH_BUILDRATE_MILITARY,      // buildRate.<scope>.military
	CH_BUILDRATE_SPACE,         // buildRate.<scope>.space
	CH_BUILDRATE_WORLD_WONDER,
	CH_BUILDRATE_TEAM_WONDER,
	CH_BUILDRATE_NATIONAL_WONDER,
	CH_BUILDRATE_UNIT,          // buildRate keyed at units    (the keyed ledger's channel)
	CH_BUILDRATE_BUILDING,      // buildRate keyed at buildings (the keyed ledger's channel)
	CH_FREE_SPECIALISTS,        // freeSpecialists (a COUNT family -- the two-part seam's AMOUNT half)
	// -- wellbeing: FOUR ORDINARY CHANNELS (owner), not one signed-split family with a good/bad plane.
	// happiness sums with anger; health sums with unhealth. The engine's unhappyLevel/badHealth ARE these
	// channels, so no polarity metadata and no duplicated BAD plane exists -- a negative deposit is routed to
	// the opposing channel at FILL, which is a fill concern, never a storage one.
	CH_HAPPINESS,
	CH_ANGER,                   // summed against CH_HAPPINESS at the verdict
	CH_HEALTH,
	CH_UNHEALTH,                // summed against CH_HEALTH at the verdict

	NUM_CASCADE_CHANNELS
};

inline CascadeChannel CH_YIELD(int eYield)       { return (CascadeChannel)(CH_FOOD + eYield); }
inline CascadeChannel CH_COMMERCE_CH(int eComm)  { return (CascadeChannel)(CH_GOLD + eComm); }

// ===== THE POSITION: where in the combine the sum sits (== the deposit SOURCE for the source positions) =====
// One flat enumeration rather than a (source x kind x gate) cross-product: most combinations do not exist, and
// a dense cross-product would be mostly-zero storage per city. Each position's KIND (value vs percent) is
// static -- cascadeIsPercent() -- so the combine needs no per-slot type.
//
// GATED positions are stored UNGATED and gated at READ (state-religion-in-city, coastal, connected-not-capital,
// golden-age): the gate is live, so flipping it invalidates nothing.
enum CascadePosition
{
	// ---- VALUE positions (summed into the base) ----
	POS_PLOT = 0,          // the plot pull (yields only -- the plot package's own base)
	POS_SPECIALIST,        // specialist flats x assigned counts (BASE tier -- takes the percent stack)
	POS_BUILDING,          // building flats (+ perPopulation). For yields this is the EXTRA tier (added AFTER
	                       // the percentages and truncated to whole units, modifier.md §2a) -- NOT the base.
	POS_TRADE,             // trade-route yield: the ONE sanctioned live-yield INPUT the cascade folds
	POS_CIVIC,
	POS_TRAIT,
	POS_TECH,
	POS_BONUS,
	POS_RELIGION,
	POS_CORPORATION,
	POS_PROJECT,
	POS_FEATURE,           // feature/improvement plot-scope substrate (the wellbeing featSubstrate class)
	POS_EVENT,             // event/vote GRANTED persisted state -- folded, never derived (state-repositories)
	POS_GOLDEN_AGE,        // stored UNGATED; the isGoldenAge() gate is live at read
	POS_STATE_RELIGION,    // stored UNGATED; the state-religion-in-city gate is live at read
	POS_COASTAL,           // stored UNGATED; the coastal gate is live at read
	POS_MILITARY_PER_UNIT, // the per-military-unit VALUE; x the LIVE unit count at read (DEC-unit-modifiers-on-top)
	POS_RANKED,            // the ranked `cities` member (json.md §3.3 orderedBy/max) -- rank resolved at fill
	POS_PER_POPULATION,    // the perPopulation pool; x live population at read

	// ---- PERCENT positions (summed into the one additive stack, modifier.md §2a) ----
	POS_PCT_BUILDING,
	POS_PCT_CIVIC,
	POS_PCT_TRAIT,
	POS_PCT_TECH,
	POS_PCT_BONUS,
	POS_PCT_PROJECT,
	POS_PCT_POWER,
	POS_PCT_CAPITAL,
	POS_PCT_STATE_RELIGION,// stored UNGATED; SR-in-city gate live at read
	POS_PCT_CONNECTED,     // stored UNGATED; connected-and-not-capital gate live at read (maintenance)

	// ---- the RECEIVER's realized total (DEC-uniform-cache-shape) ----
	POS_SUM,

	NUM_CASCADE_POSITIONS
};

// Is this position a PERCENT (goes into the additive stack) rather than a VALUE (goes into the base)?
// Static, branch-free-ish, no per-slot type stored -- the owner's rule: "as long as we know if the package is
// value or percentage, we know what to do with it".
inline bool cascadeIsPercent(CascadePosition p)
{
	return (p >= POS_PCT_BUILDING && p <= POS_PCT_CONNECTED);
}

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

// ===== LOAD-TIME RESOLUTION (strings -> ints, ONCE) =====
// Resolve a deposit's dotted address to its (channel, position). Called ONLY from DepositIndex::compile at load;
// the runtime never sees a string ([DEC-materialize-at-mapfrom] applied to the cascade's own gather).
//
//  - `family`  = address segment 0 ("food", "greatPeopleRate", "defense", "stateReligion", ...)
//  - `member`  = address segment 2 where present ("amount", "bombardDefense", "min", "military", ...), else ""
//  - `sourcePos` = the position the SOURCE KIND implies (POS_PCT_CIVIC when walking civics, ...). The resolver
//    OVERRIDES it only where the address itself names a gate (the `stateReligion.*` prefix -> the SR position),
//    because a gated sum is stored ungated in its own slot and gated at read.
//
// Returns false when the address is not a cascade channel (an unmodelled family) -- the caller skips it, which is
// how a retired system stops depositing without any special-casing.
bool cascadeResolveAddress(const char* family, const char* member, const char* unit,
                           CascadeChannel& outChannel, CascadePosition& outGatePos, bool& outIsPercent);

// The scope segment ("world"/"team"/"empire"/"area"/"city"/"plot"/"unit"/"self") -> its index; -1 if unknown.
int cascadeScopeFromSegment(const char* scope);

// ===== THE ONE PACKAGE TYPE =====
// Templated on the owner so it is literally the same type at every scope. `instances` is 1 everywhere except
// CvArea, which is ONE shared map object carrying the sums of EVERY player (the per-player axis is the data's
// shape, not a reason to keep area sums on CvPlayer) -- so area binds with MAX_PLAYERS instances and stays the
// same type rather than being special-cased.
template <class TOwner>
struct CascadePackages
{
	CascadePackages() : m_iInstances(0) {}

	// Size the table. Called from the owner's reset/init beside set.bind() -- storage is zero-filled, and the
	// CvDerivedCacheSet is all-dirty from birth, so the first read recomputes from current state.
	void size(int iInstances)
	{
		m_iInstances = iInstances;
		m_aSlots.assign((size_t)iInstances * NUM_CASCADE_CHANNELS * NUM_CASCADE_POSITIONS, 0);
	}

	int  get(CascadeChannel ch, CascadePosition pos, int iInstance = 0) const
	{
		const size_t i = idx(ch, pos, iInstance);
		return (i < m_aSlots.size()) ? m_aSlots[i] : 0;
	}
	// (named put(), not set() -- `set` is the CvDerivedCacheSet member every scope package already exposes)
	void put(CascadeChannel ch, CascadePosition pos, int iValue, int iInstance = 0)
	{
		const size_t i = idx(ch, pos, iInstance);
		if (i < m_aSlots.size()) m_aSlots[i] = iValue;
	}
	void add(CascadeChannel ch, CascadePosition pos, int iValue, int iInstance = 0)
	{
		const size_t i = idx(ch, pos, iInstance);
		if (i < m_aSlots.size()) m_aSlots[i] += iValue;
	}
	// Route a SIGNED deposit across an opposing channel PAIR (happiness/anger, health/unhealth): a positive
	// value lands in the primary channel, a negative one in its opposing channel as a magnitude. Both are
	// ordinary channels -- this is a FILL-side routing rule, not a storage shape.
	void foldOpposed(CascadeChannel chPos, CascadeChannel chNeg, CascadePosition pos, int iValue,
	                 int iInstance = 0)
	{
		if (iValue >= 0) add(chPos, pos,  iValue, iInstance);
		else             add(chNeg, pos, -iValue, iInstance);
	}

	// Zero one instance's whole channel row -- contract rule 2 (a refresh must FULLY define its output).
	void clearChannel(CascadeChannel ch, int iInstance = 0)
	{
		for (int p = 0; p < NUM_CASCADE_POSITIONS; ++p)
		{
			const size_t i = idx(ch, (CascadePosition)p, iInstance);
			if (i < m_aSlots.size()) m_aSlots[i] = 0;
		}
	}

	int instances() const { return m_iInstances; }

	CvDerivedCacheSet<TOwner> set;   // the ONE dirty protocol -- bind in the owner's ctor/reset

private:
	size_t idx(CascadeChannel ch, CascadePosition pos, int iInstance) const
	{
		return (((size_t)iInstance * NUM_CASCADE_CHANNELS) + (size_t)ch) * NUM_CASCADE_POSITIONS + (size_t)pos;
	}
	CascadePackages(const CascadePackages&);              // noncopyable (the CvDerivedCache contract rule 3)
	CascadePackages& operator=(const CascadePackages&);
	std::vector<int> m_aSlots;
	int m_iInstances;
};

#endif // CV_CASCADE_CHANNELS_H
