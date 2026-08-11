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
#include "Data/CvInfoValuation.h"   // InfoValuation::plotScaledYield -- the plot scaling calc lives on the calc surface
#include <vector>

// ⚖ THE PLOT'S OWN YIELD SCALING -- "+amount per whole interval of what this plot already makes" (owner).
// A plot whose terrain + feature + improvement + route totals 12, on a 1-per-5 scaling, gains 2 and outputs 14.
//
// ⛔ IT IS A RATE, NOT THE LEGACY ONE-SHOT THRESHOLD. The old engine paid a single step once a plot passed a
// threshold (`CvPlayer::updateExtraYieldThreshold`); reading the authored number that way pays out once where the
// model pays per interval -- plausible, and quietly short on exactly the best tiles.
//
// ⛔ THE TWO OPERANDS ARE STORED ON THE PLOT, NOT REACHED FOR (owner). The interval and the amount are FED IN as
// two separate numbers and maintained here like any other plot state, so the resolve is entirely PLOT-LOCAL: it
// reads this plot's own segments and this plot's own two operands, and touches no player, no trait and no other
// scope. ⚑ That is what keeps it out of the cross-scope event-triggered-recalc shape -- the operand arrives by an
// ordinary fan when the owner's scaling moves, exactly as a deposit does.
//
// ⛔ AND THE SCALING ONLY EVER AFFECTS ITS OWN CHANNEL (owner, HARD RULE): the interval is measured against THAT
// channel's own plot total and the grant lands on THAT channel. There is no "1 hammer per 5 commerce" and none
// may be authored -- which is precisely why one pair of numbers per channel suffices, and why no ordering between
// channels can arise.
//
// ⚠ The arithmetic itself is not here: it lives once on the calc surface (InfoValuation::plotScaledYield), which
// the what-if plot reads share ([DEC-single-implementation]). This plane only stores and feeds.

// ⛔ THE YIELD ORIGIN IS PART OF THE PACKAGE'S TYPE ([DEC-hard-typing-or-rollerskate]).
// A city's yields come from THREE origins -- plots, SPECIALISTS and BUILDINGS -- and they are three separate
// packages, not one ([state-repositories.md] § THE ORIGIN RULE). The split is FORCED: modifier.md §2a puts
// specialists in TIER 1 (inside the percent stack) and buildings in TIER 2 (added after it), while the maintained
// sum bans a per-source decomposition -- so two origins sharing one Σ slot can never be told apart again, and the
// rate becomes inexpressible.
// ⛔ THIS IS A TYPE AND NOT A CONVENTION FOR ONE REASON: as prose ("specialists do not live in the building
// package") it was re-corrected more times than the owner cares to count. As a type parameter, a specialist
// deposit reaching the building plane DOES NOT COMPILE.
enum CvCascOrigin
{
	CASC_ORIGIN_SINGLE = 0,     // every scope but CITY: one yield origin, so there is nothing to keep apart
	CASC_ORIGIN_PLOT,           // CITY tier 1 -- the worked plots' own packages, folded in by the worked fact
	CASC_ORIGIN_SPECIALIST,     // CITY tier 1 -- multiplied by the percent stack
	CASC_ORIGIN_BUILDING        // CITY tier 2 -- added flat, AFTER the stack, never multiplied
};

template <class TOwner, int Origin = CASC_ORIGIN_SINGLE>
struct CvCascadePackage
{
	CvCascScope scope;                     // which layout this package lives on (set at bind)
	int identityFirst;                     // the SERVED identity, interpreted per scope (CvCascadeSlotValues.h)
	int identitySecond;                    // its second axis: city id / team id / plot y (-1 = none)
	// ⛔ WIDTH IS PER UNIT, exactly as SCALE is ([DEC-fixedpoint-x100]). An AMOUNT accumulates -- across
	// sources, across scopes, at ×100 -- so it carries 64 bits; a PERCENT is a small whole number by ruling
	// (no decimals, hence no ×100), so it has nothing to accumulate into and stays 32. Widening only the
	// amount side is what lets the combine keep its shape while the overflow ceiling disappears.
	struct BookedDeposit
	{
		int iChannel;
		bool bPercent;
		int64_t iValue;
		BookedDeposit() : iChannel(-1), bPercent(false), iValue(0) {}
	};

	mutable std::map<const void*, BookedDeposit> bookedGated;   // the gated-deposit book (see above)
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
	// ⚖ THE CITY'S WORKED-PLOT Σ -- `basePlotYield`, modifier.md §2a TIER-1, held as a MAINTAINED SLOT.
	// Sized at CITY scope only, exactly as the segments above are sized at PLOT scope only.
	//
	// ⛔ WHY IT EXISTS: `InfoValuation::cityReceiverRate` used to SUM THE RING ON EVERY CALL
	// (`getCityIndexPlot` x NUM_CITY_PLOTS), and CvCity::getYields is specified as a bare fetch. That is the
	// per-read walk [state-repositories.md] names with this exact caller -- "re-summing the radius on every
	// getPlotYield call turns the game's hottest read O(radius) ... measured at 913M plot reads in one turn
	// inside the governor's valuation". The governor's valuation is AI_assignWorkingPlots, and it hung a
	// late-game turn on a saturated core.
	// ⚑ IT IS DELTA-ABLE EVEN THOUGH A PLOT'S RESOLVED FLAT IS NOT. The floors break linearity over a DEPOSIT
	// delta (the note above), so this is NOT maintained from deposits -- it is maintained from the RESOLVE
	// delta: resolvePlotFlat knows the slot's old and new value, and (new - old) is exact whatever the floors
	// did. Two facts move it, and together they are total: a worked plot's resolved flat changing, and a plot
	// entering or leaving the WORKED set (± that plot's whole resolved value).
	// ⚠ WORKED, not owned: only worked plots contribute to the city's base ([modifier.md] §2a).
	mutable std::vector<int64_t> plotBaseFlat;
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
	// The plot-local YIELD-SCALING operands (PLOT scope only), one pair per channel -- see the callout above the
	// template. ⚖ IT IS A RATE, NOT A ONE-SHOT THRESHOLD (owner): `interval` is "per how much", `amount` is what
	// each whole interval grants, so a plot whose own total is 12 on a 1-per-5 scaling gains 2 and ends at 14.
	// ⛔ The legacy engine's single step at a threshold (`>= N` once, `CvPlayer::updateExtraYieldThreshold`) is
	// NOT this mechanic, and reading the authored number as that threshold silently pays out once where the
	// model pays out per interval.
	// ⚠ Both are FED IN and maintained; neither is derived at resolve time, which is what keeps the resolve
	// plot-local. The interval is a resolved SELECTION rather than a summed deposit ([modifier.md] §2a).
	mutable std::vector<int64_t> yieldScaleInterval;
	mutable std::vector<int64_t> yieldScaleAmount;

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
		// ⛔ ZEROED AT OWNER RESET -- a delta store is correct ONLY from a known zero
		// ([DEC-keyed-accumulator]; [state-repositories.md] § THE SEMIBOOLEAN STATE). Every bind() site is an
		// owner's reset/init (CvCity / CvPlot / CvPlayer / CvTeam), and `CvCity` and `CvPlot` are RECYCLED out of
		// an FFreeListTrashArray -- so without this a reused slot inherits the previous occupant's sums, its
		// applied-source record and its book, and NO later ±1 can ever correct them.
		// ⚠ The applied-source record is what makes the omission compound rather than merely mis-state: a
		// recycled city answered `hasAppliedSource` TRUE for the dead city's sources, so every plane that tests
		// liveness before moving a slot -- and the owner-source fold that skips what is already applied --
		// silently declined to deliver the new occupant's deposits at all.
		// ⚑ This is what the neighbouring `m_cityContext.clear()` / `m_amenity.clear()` / `m_enabler.reset()`
		// calls do for the other delta stores at the same choke point; the package was the one that never got it,
		// while its own member comments asserted it did.
		flat.clear();
		percent.clear();
		substrateFlat.clear();
		improvementFlat.clear();
		restFlat.clear();
		appliedSources.clear();
		bookedGated.clear();
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
	// The CITY's worked-plot Σ for one channel -- a BARE FETCH, which is the whole point of it.
	int64_t readPlotBaseFlat(int iChannel) const
	{
		if (scope != CASC_SCOPE_CITY)
		{
			return 0;
		}
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0 || iSlot >= (int)plotBaseFlat.size())
		{
			return 0;
		}
		return plotBaseFlat[iSlot];
	}

	// Move the city's worked-plot Σ by an EXACT delta the caller computed (a resolve delta, or ± a whole plot
	// as it joins or leaves the worked set). ⛔ Never a recount -- the caller always knows both ends.
	void applyPlotBaseFlat(int iChannel, int64_t iDelta) const
	{
		if (scope != CASC_SCOPE_CITY || iDelta == 0)
		{
			return;
		}
		ensureSized();
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return;
		}
		if (iSlot >= (int)plotBaseFlat.size())
		{
			plotBaseFlat.resize(iSlot + 1, 0);
		}
		plotBaseFlat[iSlot] += iDelta;
	}

	// THE MEMBERSHIP LEG of the worked-plot Σ: fold a plot's WHOLE resolved flat vector into this CITY package,
	// +1 as it enters the worked set and -1 as it leaves. The resolve leg (applyPlotSegment's return) carries a
	// worked plot's ONGOING changes; together the two are total, which is what makes the slot exact.
	// ⚠ It reads the plot's RESOLVED slots, so it must run while the membership fact still describes reality --
	// on the ADD after the plot is worked, on the REMOVE before its value is touched.
	//	⚠ A MEMBER TEMPLATE because the two packages are different instantiations -- the city's is
	//	CvCascadePackage<CvCity> and the plot's CvCascadePackage<CvPlot>, so a TOwner-typed parameter cannot
	//	name it.
	template <class TPlotOwner, int PlotOrigin>
	void applyWorkedPlot(const CvCascadePackage<TPlotOwner, PlotOrigin>& kPlotPackage, int iSign) const
	{
		if (scope != CASC_SCOPE_CITY || iSign == 0)
		{
			return;
		}
		for (int iSlot = 0; iSlot < (int)kPlotPackage.flat.size(); ++iSlot)
		{
			const int64_t iValue = kPlotPackage.flat[iSlot];
			if (iValue == 0)
			{
				continue;
			}
			const int iChannel = CascadeChannelRegistry::scopeSlotChannel(CASC_SCOPE_PLOT, iSlot);
			if (iChannel >= 0)
			{
				applyPlotBaseFlat(iChannel, iSign > 0 ? iValue : -iValue);
			}
		}
	}

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

	// The IMPROVEMENT and REST segments, RAW -- the census decomposition of readFlat. readFlat collapses the
	// three segments into one number, so it can say a plot's yield is short and never WHICH leg is short: a
	// dead improvement leg and a dead nature leg are indistinguishable in the total. These are the only reads
	// that can tell them apart, and they exist for that census alone ([DEC-obs-scale]).
	// ⚠ RAW ON PURPOSE -- unfloored, so the three sum to the pre-floor base and a NEGATIVE improvement (an
	// improvement that consumes its nature yield) stays visible instead of being clamped into agreement.
	// Never a consumer read: the value a consumer wants is readFlat, which is the floored §2a combine.
	int64_t readImprovementFlat(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		return iSlot < (int)improvementFlat.size() ? improvementFlat[iSlot] : 0;
	}

	int64_t readRestFlat(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		return iSlot < (int)restFlat.size() ? restFlat[iSlot] : 0;
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
	int64_t applyPlotSegment(PlotSegment eSegment, int iChannel, int64_t iDelta) const
	{
		ensureSized();
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0 || iDelta == 0)
		{
			return 0;
		}
		std::vector<int64_t>& segment = (eSegment == PLOTSEG_NATURE) ? substrateFlat
		                              : (eSegment == PLOTSEG_IMPROVEMENT) ? improvementFlat
		                              : restFlat;
		if (iSlot >= (int)segment.size())
		{
			return 0;
		}
		segment[iSlot] += iDelta;
		return resolvePlotFlat(iSlot, iChannel);
	}

	// ⚖ RE-RESOLVE ONE CHANNEL'S PLOT SLOT WITHOUT MOVING A SEGMENT -- for when an operand of the RESOLVE moved
	// rather than a deposit. The segments are untouched; only the non-linear step over them is recomputed.
	// ⛔ The threshold is the case that needs it: it reads the plot OWNER's threshold channels, so it moves when a
	// TRAIT is gained or lost, and that fact names no plot. Without this route every plot would keep the step it
	// resolved under the old trait set, permanently ([DEC-no-self-heal]).
	// ⚠ It is NOT a rebuild: nothing is re-derived from sources, and a plot whose step did not change writes the
	// same number back.
	int64_t refreshPlotResolve(int iChannel) const
	{
		if (scope != CASC_SCOPE_PLOT)
		{
			return 0;
		}
		ensureSized();
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		return iSlot >= 0 ? resolvePlotFlat(iSlot, iChannel) : 0;
	}

	// The §2a plot-as-base combine over the three segments (modifier.md §2a basePlotYield). The SEGMENTS hold
	// raw sums -- that is what makes them delta-able -- so every floor is applied here.
	int64_t resolvePlotFlat(int iSlot, int iChannel) const
	{
		if (iSlot < 0 || iSlot >= (int)flat.size())
		{
			return 0;
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
		// JUST BEFORE OUTBOUND (owner): the plot-local yield SCALING, applied to the total those floors just
		// produced and folded into the slot every consumer reads -- so the city base, the AI plot valuation and
		// the tooltips all see one number ([DEC-single-implementation]).
		// ⚖ A RATE: whole intervals of the plot's OWN total, each granting `amount` of the SAME channel. 12 on a
		// 1-per-5 scaling gains 2 and outputs 14.
		// ⚠ The division is over the PRE-BONUS total, so the grant cannot feed itself; and both operands live on
		// the ×100 plane, which the ratio cancels -- only `amount` carries the scale.
		// ⛔ The arithmetic itself is NOT written here: it lives once on the calc surface
		// (InfoValuation::plotScaledYield), which the what-if plot reads share. This end only FEEDS it the two
		// stored numbers ([DEC-single-implementation]).
		if (scope == CASC_SCOPE_PLOT && iSlot < (int)yieldScaleInterval.size())
		{
			iTotal = InfoValuation::plotScaledYield(iTotal, yieldScaleInterval[iSlot], yieldScaleAmount[iSlot]);
		}
		//	⚑ ANSWERS THE EXACT DELTA IT MOVED, which is what makes the CITY's worked-plot Σ maintainable at all:
		//	the floors above break linearity over a DEPOSIT delta, but (new - old) on the resolved slot is exact
		//	whatever they did. The caller -- which knows the PLOT and therefore its working city -- applies it.
		const int64_t iPrevious = flat[iSlot];
		flat[iSlot] = iTotal;
		return iTotal - iPrevious;
	}

	// ⛔ EVERY READ ABOVE IS A BARE FETCH, AND IT IS THE ONLY READ SURFACE THERE IS. A slot is correct the instant
	// its fact arrives, so a read tests nothing and a cross-scope input needs no ordering guarantee: addition
	// commutes, and the reseed's facts apply in whatever order they stream ([DEC-maintained-sum],
	// [DEC-spine-reseed]). The load bracket therefore has nothing to drain.

	// ---- THE GATED-DEPOSIT BOOK ----
	// ⛔ WHICH CONDITIONED DEPOSITS ARE CURRENTLY BOOKED INTO THIS PACKAGE. Without it the two planes that can
	// apply the same deposit cannot agree: plane A books it when its SOURCE arrives (evaluating the gate against
	// live state), and plane C books it when the ATOM crosses -- so a source arriving while the atom already
	// holds is booked once by A and again by C, and the package quietly holds twice what its data authorizes.
	// ⚑ MEASURED: London's food percent read 155 against 82 authorized, and production 841 against 576, while
	// COMMERCE -- whose percents are barely atom-gated, so plane C never touched them -- reconciled exactly.
	// ⚖ The flag makes the two planes IDEMPOTENT rather than additive: a deposit is booked iff its gate holds,
	// whichever plane notices first, and the second one finds it already booked and does nothing. That also
	// retires the as-if-held pin for this purpose -- a withdrawal no longer has to reconstruct an old verdict,
	// because the BOOK remembers what was actually applied ([state-repositories.md] § THE INVARIANT).
	// ⚠ Keyed on the compiled ENTRY pointer, which is stable for the process (write-once infos) and is what the
	// deposit index hands out. Derived state: never serialized, rebuilt from the same facts as the slots.
	// ⚖ THE BOOK STORES THE VALUE, NOT A FLAG -- and that is what makes ONE mechanism serve BOTH planes.
	// A flag answers "is this deposit in the slot"; the VALUE answers "how much of it is", which is the question
	// plane B (the COUNT route) asks. A deposit scaled by a count moves when the count moves without its gate
	// changing at all, so a boolean book cannot express it and the count route had to be a separate mechanism --
	// which is why it was never wired. With the value booked, EVERY re-book is the same operation: resolve what
	// the data says now, subtract what is recorded, apply the difference. Conditions, counts and scale changes
	// all fall out of it, and it is idempotent, so no plane can stack on another ([DEC-maintained-sum]).
	bool isGatedBooked(const void* pEntry) const
	{ return pEntry != NULL && bookedGated.find(pEntry) != bookedGated.end(); }

	// What this package currently holds for one deposit. An absent entry IS zero, so a never-booked deposit and a
	// withdrawn one are the same state -- which is what keeps the difference arithmetic honest.
	BookedDeposit bookedDeposit(const void* pEntry) const
	{
		if (pEntry != NULL)
		{
			const typename std::map<const void*, BookedDeposit>::const_iterator it = bookedGated.find(pEntry);
			if (it != bookedGated.end()) { return it->second; }
		}
		return BookedDeposit();
	}

	void setBookedDeposit(const void* pEntry, int iChannel, bool bPercent, int64_t iValue) const
	{
		if (pEntry == NULL) { return; }
		if (iValue == 0) { bookedGated.erase(pEntry); return; }
		BookedDeposit& kBooked = bookedGated[pEntry];
		kBooked.iChannel = iChannel;
		kBooked.bPercent = bPercent;
		kBooked.iValue = iValue;
	}

	void setGatedBooked(const void* pEntry, bool bBooked) const
	{
		if (pEntry == NULL || bBooked) { return; }   // clearing only; a booking carries its value
		bookedGated.erase(pEntry);
	}

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
			if (yieldScaleInterval.size() < iChannels)
			{
				yieldScaleInterval.resize(iChannels, 0);
			}
			if (yieldScaleAmount.size() < iChannels)
			{
				yieldScaleAmount.resize(iChannels, 0);
			}
		}
	}

	// ⚖ FEED THE PLOT ITS TWO SCALING OPERANDS -- the fan's write point, and the only way they ever move.
	// Returns true when either number actually CHANGED, so a caller re-resolves nothing it need not.
	// ⛔ It SETS rather than accumulates: the interval is a resolved selection, not a sum of deposits, so
	// accumulating it would produce a "per N" no source ever authored.
	bool setYieldScaling(int iChannel, int64_t iInterval, int64_t iAmount) const
	{
		if (scope != CASC_SCOPE_PLOT)
		{
			return false;
		}
		ensureSized();
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0 || iSlot >= (int)yieldScaleInterval.size())
		{
			return false;
		}
		if (yieldScaleInterval[iSlot] == iInterval && yieldScaleAmount[iSlot] == iAmount)
		{
			return false;
		}
		yieldScaleInterval[iSlot] = iInterval;
		yieldScaleAmount[iSlot] = iAmount;
		resolvePlotFlat(iSlot, iChannel);
		return true;
	}

	// Slot write access for the gather (refs into the mutable storage; game-thread only).
	int64_t& slotFlat(int iSlotIndex) const { return flat[iSlotIndex]; }
	int& slotPercent(int iSlotIndex) const { return percent[iSlotIndex]; }

private:
	CvCascadePackage(const CvCascadePackage&);              // noncopyable (the set binds an owner pointer)
	CvCascadePackage& operator=(const CvCascadePackage&);
};

#endif // CV_CASCADE_PACKAGE_H
