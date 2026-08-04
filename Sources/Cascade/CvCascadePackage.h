#pragma once
#ifndef CV_CASCADE_PACKAGE_H
#define CV_CASCADE_PACKAGE_H

//
//	CvCascadePackage -- the ONE uniform per-scope value package of the modifier cascade
//	(state-repositories.md, the per-scope package model; [DEC-uniform-cache-shape]).
//
//	EVERY scope object (team / player / city / plot) carries this SAME type as a data member; what
//	varies between scopes is only WHICH SLOTS carry a value (the registry's data-derived channel set -- KEYS
//	ONLY WHERE NEEDED). The storage is the owner ruling's TWO DICTIONARIES -- one flats, one percents, each an
//	int keyed by channel (locally indexed by the scope's layout; the unit is part of the slot key, so a flat
//	sum and a percent sum are SEPARATE slots, never fields of one mixed struct).
//
//	⛔ A RECEIVER TOTAL IS NOT ONE OF THESE SLOTS. What a scope CONSUMES (city: food/production/commerce/culture;
//	player: gold/research/culture/espionage/maintenance) is the Σ of its members' REALIZED values -- each one a
//	§2a COMBINE over that member's packages, not a stored deposit sum -- so it is not delta-able from a deposit
//	and is re-summed on the calc surface where the combine lives. See the callout above readValuesInto.
//
//	⛔ IT IS A MAINTAINED SUM, NOT A CACHE -- so it carries NO staleness protocol of any kind: no flag, no mask,
//	no refresh delegate, no rebuild ([DEC-maintained-sum]). A DOMAIN fact names its SOURCE, the compiled deposit
//	index names that source's deposits, and APPLYING them IS the maintenance, through the apply verbs below and
//	nothing else. Three routed planes reach them and all three land in the same slot: the SOURCE route (±value),
//	the COUNT route (±value × Δcount -- the context dictionaries' half, [contexts.md]) and the ATOM route (±value
//	on a condition's verdict crossing). B and C are coupled: C is exact only because B guarantees no count moves
//	unannounced, which is what makes `slot == Σ resolve(d, state_now)` hold at every instant.
//
//	NEVER serialized ([DEC-derived-never-trusted]); a package starts EMPTY and the LOAD build is the reseed's own
//	in-read facts ([DEC-spine-reseed]), never a pass over populated objects. ⛔ A channel no fact ever reached
//	therefore reads ZERO -- loudly and permanently, because nothing re-derives it. That is not a gap in the
//	design, it IS the missed-emit tripwire ([DEC-no-self-heal]): the wrong value is how the missing fact is found.
//
//	Storage is sized lazily (ensureSized) from the registry layout -- the layout is minted at load, after the
//	owners construct. Receiver slots ride in the same package, one variable per channel: there is no separate
//	receiver mechanism.
//

#include "CvCascadeChannelRegistry.h"
#include "Engine/ContextDict.h"   // the applied-source record: an ordinary keyed accumulator
#include "CvCascadeSlotValues.h"
#include <vector>

template <class TOwner>
struct CvCascadePackage
{
	CvCascScope scope;                     // which layout this package lives on (set at bind)
	int identityFirst;                     // the SERVED identity, interpreted per scope (CvCascadeSlotValues.h)
	int identitySecond;                    // its second axis: city id / team id / plot y (-1 = none)
	// ⛔ WIDTH IS PER UNIT, exactly as SCALE is ([DEC-fixedpoint-x100]). An AMOUNT accumulates -- across
	// sources, across scopes, at ×100 -- so it carries 64 bits; a PERCENT is a small whole number by ruling
	// (no decimals, hence no ×100), so it has nothing to accumulate into and stays 32. Widening only the
	// amount side is what lets the combine keep its shape while the overflow ceiling disappears.
	mutable std::vector<int64_t> flat;   // dictionary 1: the channel-indexed x100 flat sums (local slots)
	mutable std::vector<int> percent;    // dictionary 2: the channel-indexed percent sums (local slots)
	// ⚖ WHAT THIS PACKAGE HAS ACTUALLY DEPOSITED -- keyed by the DepositIndex's dense source index, ±1 as a
	// source's deposits are applied and withdrawn. Planes B and C test it before moving an already-deposited
	// amount: a count fact scales what is IN the slot, so applying it for a source that never deposited here
	// would invent a contribution out of nothing.
	// ⛔ IT IS NOT A SECOND COPY OF THE HAVE AXIS, and asking the HAVE axis instead would be WRONG rather than
	// merely duplicative: a building that is PRESENT but DORMANT deposits nothing ([enabler.md] §3.2), so
	// `hasBuilding` answers true where this answers false -- and this one is right, because it records what was
	// applied rather than what is held. It is the apply path's own bookkeeping and cannot disagree with itself.
	// ⚑ Ordinary ContextDict semantics: a COUNT not a bit, because one source may deposit at several
	// multiplicities, and zeroed at owner reset like every other delta store ([DEC-keyed-accumulator]).
	mutable ContextDict appliedSources;
	// A SEGMENT of `flat`: the plot's SUBSTRATE-only contribution -- terrain + feature + bonus, WITHOUT the
	// improvement, the route or the owner's plot-scope sources. A package's parts are segments of it (owner),
	// and this is the one the data asks for: the pre-improvement value an improvement's placement gate tests
	// against and the contexts serve as the plot's nature yield ([contexts.md] -- it is a slot of this same
	// package, never a per-call walk). Sized at PLOT scope only; empty everywhere else, exactly as a channel
	// no scope authors carries no storage.
	mutable std::vector<int64_t> substrateFlat;
	// ⛔ THE OTHER TWO PLOT SEGMENTS -- and they exist because THE PLOT YIELD COMBINE IS NOT LINEAR.
	// modifier.md §2a floors it in three places: nature = max(0, terrain+feature+bonus), the improvement
	// floored at −nature, the total floored at 0. A floor does not distribute over a delta
	// (max(0, x+d) != max(0, x) + d), so the RESOLVED slot cannot itself be a maintained sum -- but each
	// SEGMENT is a plain sum and therefore can be ([DEC-maintained-sum]: the fact applies, nothing recomputes).
	// So the SEGMENTS are what the facts maintain, and `flat` is re-derived from them at APPLY time by
	// resolvePlotFlat -- O(1) arithmetic over three stored numbers, never a walk, which is exactly what keeps
	// every READ a bare fetch. Sized at PLOT scope only; only the yield channels ever carry one.
	mutable std::vector<int64_t> improvementFlat;   // the improvement's own untargeted plot-scope output
	mutable std::vector<int64_t> restFlat;          // route + the owner's plot-scope flats (both unfloored)

	CvCascadePackage() : scope(CASC_SCOPE_CITY), identityFirst(-1), identitySecond(-1) {}

	// Wire the owner's IDENTITY -- what every SERVED value carries, so a divergence an external reader observes
	// between the stored and oracle documents names WHICH object drifted (city 5-8192, plot 12/30) rather than
	// "some city's production flats". The owner types differ per scope (they expose no common id accessor), so
	// identity is passed IN rather than read back off TOwner; either axis may be -1 where the scope has none.
	// ⛔ THERE IS NO REFRESH DELEGATE, BECAUSE THERE IS NO REBUILD TO DELEGATE TO. A package starts EMPTY and is
	// filled ONLY by the apply verbs below, from the facts ([DEC-maintained-sum]). A channel no fact ever reached
	// reads ZERO -- loudly and permanently -- which IS the missed-emit tripwire ([DEC-no-self-heal]); the load
	// build is the reseed's own in-read facts ([DEC-spine-reseed]), never a pass over populated objects.
	void bind(CvCascScope ePackageScope, int iIdentityFirst, int iIdentitySecond)
	{
		scope = ePackageScope;
		identityFirst = iIdentityFirst;
		identitySecond = iIdentitySecond;
	}

	// ---- THE READ PATH: BARE FETCHES, UNCONDITIONALLY. The rebuild already happened AT THE MARK, so a read
	// ---- never recomputes and there is nothing left on it to gate (state-repositories.md: an ensure-on-read
	// ---- protocol is tombstoned, superseded-ideas #14). A channel no event ever marked therefore reads
	// ---- whatever the events built -- visibly wrong if an emit is missing, which is exactly how the missing
	// ---- emit gets found ([DEC-no-self-heal]). A never-authored channel answers 0 without any storage
	// ---- existing anywhere. ----

	int64_t readFlat(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		return iSlot < (int)flat.size() ? flat[iSlot] : 0;
	}

	int readPercent(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		return iSlot < (int)percent.size() ? percent[iSlot] : 0;
	}

	// The SUBSTRATE segment of this channel's flat sum (see `substrateFlat`) -- the plot's PRE-IMPROVEMENT
	// nature yield, which the improvement placement gate and both improvement valuations ask per
	// (plot × improvement × yield) ([contexts.md]). A scope that carries no segment answers 0 with no storage
	// existing anywhere.
	// ⚠ The segment STORES the raw sum (that is what makes it delta-able), so the §2a `max(0, ·)` nature floor
	// is applied here. One comparison, no walk -- the read is still O(1) and touches nothing but this slot.
	int64_t readSubstrateFlat(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		const int64_t iNature = iSlot < (int)substrateFlat.size() ? substrateFlat[iSlot] : 0;
		return iNature > 0 ? iNature : 0;
	}

	// ⛔ A RECEIVER TOTAL IS NOT STORED HERE, AND IT IS NOT A DEPOSIT-FED DELTA (owner: "the receiver re-sums its
	// participating members, and nothing is built to avoid that"). A receiver total is the Σ of its members'
	// REALIZED values, and a realized value is the §2a COMBINE over that member's packages — not a stored
	// deposit sum. So there is no `±value` an arriving deposit could apply: moving one city's Σflat by Δ moves
	// the empire receiver by `Δ × (100 + that city's Σpercent)/100`, which is a property of the MEMBER, not of
	// the deposit. A delta verb here would under- or over-count every receiver by each city's own percent stack
	// — plausible totals, wrong ones, and wrong in proportion to how modified a city is.
	// ⚑ It also needs no tripwire row of its own: a receiver is derived from member slots that ARE diffed, so it
	// cannot disagree unless one of those already does. The re-sum lives on the calc surface, where the §2a
	// combine already lives, and is gated there by the WLTKD/disorder participation test
	// (state-repositories.md § THE CROSS-SCOPE RECEIVER; economy.md).

	// The ENDPOINT-FACING STORED read: this package's slots as they stand, copied into a caller-owned document
	// -- the same shape the oracle's fresh recompute fills, so an external consumer diffs the two field by
	// field. A slot the storage has never been sized for answers 0, exactly as a consumer read would.
	void readValuesInto(CvCascadeSlotValues& kValues) const
	{
		kValues.reset(scope, identityFirst, identitySecond);
		size_t iSlot = 0;
		for (iSlot = 0; iSlot < kValues.flat.size() && iSlot < flat.size(); ++iSlot)
		{
			kValues.flat[iSlot] = flat[iSlot];
		}
		for (iSlot = 0; iSlot < kValues.percent.size() && iSlot < percent.size(); ++iSlot)
		{
			kValues.percent[iSlot] = percent[iSlot];
		}
	}

	// ---- THE WRITE PATH: THE MAINTAINED SUM'S ONE WRITER ([DEC-maintained-sum]) ----
	// A DOMAIN fact names its SOURCE, the compiled deposit index names that source's deposits, and APPLYING
	// them IS the maintenance -- so the slot is correct at the instant the fact arrives, with nothing marked,
	// deferred or batched. Three routed planes reach these verbs, and all three land in the SAME slot: the
	// SOURCE route (±value), the COUNT route (±value × Δcount -- the context dictionaries' half), and the
	// ATOM route (±value on a condition's verdict crossing). There is deliberately no `set`-style writer:
	// a maintained sum is only ever moved by a delta, exactly as ContextDict has no set().
	// ⛔ A MISSED EMIT therefore leaves the slot visibly wrong and nothing re-derives it. That is the design
	// ([DEC-no-self-heal]) -- it is how the missing fact gets found.

	// ⛔ EVERY VERB SIZES FIRST. The storage grows to the load-minted layout on first write, so a delta always
	// lands in a slot that exists and starts from a known ZERO ([DEC-keyed-accumulator]: a delta store is
	// correct only from a known zero). ⚠ Sizing on the READ side instead would be the defect: an unsized
	// vector makes every apply fall through its own bounds test and return, so the package answers 0 forever
	// while reading as fully wired -- the quiet failure [DEC-no-legacy-masking] exists to forbid.
	// A NEGATIVE slot is different and is correctly declined: it means this channel is not authored at this
	// scope at all (the ORIGIN RULE), so there is no slot to move and nothing is being dropped.

	// Record that a source's deposits landed here (or were withdrawn). Called by the apply path with the SAME
	// signed multiplicity it applied, so the record and the slots move together and cannot drift.
	void noteSourceApplied(int iSourceIndex, int iDelta) const
	{
		if (iSourceIndex >= 0 && iDelta != 0)
		{
			appliedSources.add(iSourceIndex, (iDelta > 0) ? 1 : -1);
		}
	}

	// Has this source deposited here? The test planes B and C make before scaling what is already in the slot.
	bool hasAppliedSource(int iSourceIndex) const
	{
		return iSourceIndex >= 0 && appliedSources.has(iSourceIndex);
	}

	void applyFlat(int iChannel, int64_t iDelta) const
	{
		ensureSized();
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0 || iSlot >= (int)flat.size() || iDelta == 0)
		{
			return;
		}
		flat[iSlot] += iDelta;
	}

	void applyPercent(int iChannel, int iDelta) const
	{
		ensureSized();
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0 || iSlot >= (int)percent.size() || iDelta == 0)
		{
			return;
		}
		percent[iSlot] += iDelta;
	}

	// The PLOT yield segments (see the members): WHICH floored term a plot-scope deposit belongs to. A
	// deposit's segment is a property of the SOURCE that carries it, so the caller -- which knows whether it
	// is applying the terrain/feature/bonus leg, the improvement leg, or the route/owner leg -- names it.
	enum PlotSegment
	{
		PLOTSEG_NATURE = 0,        // terrain + feature + bonus (the pre-improvement substrate)
		PLOTSEG_IMPROVEMENT = 1,   // the improvement's own output, floored at −nature at resolve
		PLOTSEG_REST = 2,          // route + the owner's plot-scope flats, both unfloored
	};

	// Apply into a plot yield SEGMENT and re-derive the resolved slot from the three. ⛔ The re-derivation is
	// NOT a recompute: it reads three stored numbers and applies the §2a floors, so it is O(1) arithmetic and
	// touches no source, no info and no game object. Doing it HERE rather than at the read is what keeps every
	// consumer read a bare fetch.
	void applyPlotSegment(PlotSegment eSegment, int iChannel, int64_t iDelta) const
	{
		ensureSized();
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0 || iDelta == 0)
		{
			return;
		}
		std::vector<int64_t>& segment = (eSegment == PLOTSEG_NATURE) ? substrateFlat
		                              : (eSegment == PLOTSEG_IMPROVEMENT) ? improvementFlat
		                              : restFlat;
		if (iSlot >= (int)segment.size())
		{
			return;
		}
		segment[iSlot] += iDelta;
		resolvePlotFlat(iSlot);
	}

	// The §2a plot-as-base combine over the three segments (modifier.md §2a basePlotYield). The SEGMENTS hold
	// raw sums -- that is what makes them delta-able -- so every floor is applied here.
	void resolvePlotFlat(int iSlot) const
	{
		if (iSlot < 0 || iSlot >= (int)flat.size())
		{
			return;
		}
		int64_t iNature = iSlot < (int)substrateFlat.size() ? substrateFlat[iSlot] : 0;
		if (iNature < 0)
		{
			iNature = 0;
		}
		int64_t iImprovement = iSlot < (int)improvementFlat.size() ? improvementFlat[iSlot] : 0;
		if (iImprovement < -iNature)
		{
			iImprovement = -iNature;   // an improvement may consume the nature yield, never drive the base under it
		}
		int64_t iTotal = iNature + iImprovement + (iSlot < (int)restFlat.size() ? restFlat[iSlot] : 0);
		if (iTotal < 0)
		{
			iTotal = 0;
		}
		flat[iSlot] = iTotal;
	}

	// ⛔ EVERY READ ABOVE IS A BARE FETCH, AND IT IS THE ONLY READ SURFACE THERE IS. A slot is correct the instant
	// its fact arrives, so a read tests nothing and a cross-scope input needs no ordering guarantee: addition
	// commutes, and the reseed's facts apply in whatever order they stream ([DEC-maintained-sum],
	// [DEC-spine-reseed]). The load bracket therefore has nothing to drain.

	// ---- storage sizing ----

	// Size the dictionaries to the CURRENT registry layout, zero-filling any new tail. Called by every apply
	// verb before it writes -- the layout is load-minted and append-only, so sizes only ever grow and a
	// previously-written slot never moves.
	void ensureSized() const
	{
		const size_t iChannels = (size_t)CascadeChannelRegistry::scopeChannelCount(scope);
		const size_t iReceivers = (size_t)CascadeChannelRegistry::scopeReceiverCount(scope);
		if (flat.size() < iChannels)
		{
			flat.resize(iChannels, 0);
		}
		if (percent.size() < iChannels)
		{
			percent.resize(iChannels, 0);
		}
		if (scope == CASC_SCOPE_PLOT)
		{
			if (substrateFlat.size() < iChannels)
			{
				substrateFlat.resize(iChannels, 0);
			}
			if (improvementFlat.size() < iChannels)
			{
				improvementFlat.resize(iChannels, 0);
			}
			if (restFlat.size() < iChannels)
			{
				restFlat.resize(iChannels, 0);
			}
		}
	}
	// Slot write access for the gather (refs into the mutable storage; game-thread only).
	int64_t& slotFlat(int iSlotIndex) const { return flat[iSlotIndex]; }
	int& slotPercent(int iSlotIndex) const { return percent[iSlotIndex]; }

private:
	CvCascadePackage(const CvCascadePackage&);              // noncopyable (the set binds an owner pointer)
	CvCascadePackage& operator=(const CvCascadePackage&);
};

#endif // CV_CASCADE_PACKAGE_H
