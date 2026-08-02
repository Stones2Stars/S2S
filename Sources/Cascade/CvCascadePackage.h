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
//	sum and a percent sum are SEPARATE slots, never fields of one mixed struct). RECEIVER sum slots -- the
//	realized totals this scope CONSUMES (city: food/production/commerce/culture; player:
//	gold/research/culture/espionage) -- ride BESIDE the packages in the same cache, one variable per channel,
//	same type, same invalidation: there is no separate receiver mechanism.
//
//	NOT a cache implementation -- CvDerivedCacheSet IS the cache (the ONE component; the 64-bit form because
//	the city/empire channel sets exceed 32 bits). This struct only holds the owner-side storage the
//	component's contract requires, beside the set that guards it. All-dirty from bind; NEVER serialized; the
//	first read after a load recomputes from current state. ONE dirty bit per channel (both dictionaries: a
//	single gather pass over a source fills a channel's flat AND percent together -- the staleness question is
//	one question); the flat-vs-percent bit split is the spec's own later increment
//	(state-repositories.md CAPSTONE: "the bit-layout split is the increment after the bare-fetch shape
//	verifies"). Receiver bits occupy the registry's FIXED top-bit region (the bit contract is
//	order-independent of channel minting -- CvCascadeChannelRegistry.h).
//
//	The refresh delegate is an OWNER member function (the component's poor-man's-DI contract); its body is a
//	one-line delegation to the ONE gather implementation (CascadeGather -- [DEC-single-implementation]).
//	Storage is sized lazily by the refresh (ensureSized) from the registry layout -- the layout is minted at
//	load, after the owners construct.
//

#include "CvCascadeChannelRegistry.h"
#include "CvCascadeSlotValues.h"
#include "Infrastructure/CvDerivedCache.h"
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
	mutable std::vector<int64_t> sum;    // the receiver slots: the realized x100 totals this scope consumes
	// A SEGMENT of `flat`: the plot's SUBSTRATE-only contribution -- terrain + feature + bonus, WITHOUT the
	// improvement, the route or the owner's plot-scope sources. A package's parts are segments of it (owner),
	// and this is the one the data asks for: the pre-improvement value an improvement's placement gate tests
	// against and the contexts serve as the plot's nature yield ([contexts.md] -- it is a slot of this same
	// package, never a per-call walk). Sized at PLOT scope only; empty everywhere else, exactly as a channel
	// no scope authors carries no storage.
	mutable std::vector<int64_t> substrateFlat;
	CvDerivedCacheSet<TOwner, int64_t> set;   // THE dirty protocol -- the one component, 64-bit mask form

	CvCascadePackage() : scope(CASC_SCOPE_CITY), identityFirst(-1), identitySecond(-1) {}

	// Wire the owner + its refresh delegate. All-dirty from the start (a loaded game recomputes on first read).
	// iIdentityFirst/iIdentitySecond are the identity every SERVED value carries, so a divergence an external
	// reader observes between the stored and oracle documents names WHICH object drifted (city 5-8192, plot
	// 12/30) rather than "some city's production flats". The owner types differ per scope (city/plot/team/
	// scope owners expose no common id accessor), so identity is passed IN here rather than read back off TOwner;
	// either axis may be -1 where the scope genuinely has none.
	void bind(CvCascScope ePackageScope, const TOwner* pOwner, void (TOwner::*pfnRefresh)(int64_t) const,
	          int iIdentityFirst, int iIdentitySecond)
	{
		scope = ePackageScope;
		identityFirst = iIdentityFirst;
		identitySecond = iIdentitySecond;
		set.bind(pOwner, pfnRefresh);
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

	// The SUBSTRATE segment of this channel's flat sum (see `substrateFlat`). A bare fetch like its siblings;
	// a scope that carries no segment answers 0 with no storage existing.
	int64_t readSubstrateFlat(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		return iSlot < (int)substrateFlat.size() ? substrateFlat[iSlot] : 0;
	}

	int64_t readSum(int iChannel) const
	{
		const int iReceiver = CascadeChannelRegistry::scopeReceiverIndex(scope, iChannel);
		if (iReceiver < 0)
		{
			return 0;
		}
		return iReceiver < (int)sum.size() ? sum[iReceiver] : 0;
	}

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
		for (iSlot = 0; iSlot < kValues.sum.size() && iSlot < sum.size(); ++iSlot)
		{
			kValues.sum[iSlot] = sum[iSlot];
		}
	}

	// ---- THE REBUILD-PATH INPUT READS -- CascadeGather ONLY, never a consumer read path. A combine runs
	// ---- INSIDE a rebuild, where a cross-scope input the SAME event marked may not have reached its own
	// ---- rebuild yet; reading it through its mark makes the mark ORDER within one event irrelevant, so a sum
	// ---- can never sit stale behind an input its own event dirtied (state-repositories.md: "a sum's rebuild
	// ---- reads its packages through their own dirty-check ... there is no dependency-ordered rebuild pass").
	// ---- ⛔ This is NOT the retired ensure-on-read: it can only fire for a slot something DID mark. A MISSED
	// ---- emit leaves the slot unmarked, so it reads clean here too and stays visibly wrong -- the tripwire is
	// ---- untouched. It is also what makes the load drain order-free: every banked mark rebuilds its own
	// ---- inputs first, whatever order the drain walks the owners in. ----

	int64_t sourceFlat(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		set.rebuildMarked(CascadeChannelRegistry::scopeChannelBit(scope, iChannel));
		return iSlot < (int)flat.size() ? flat[iSlot] : 0;
	}

	int sourcePercent(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		set.rebuildMarked(CascadeChannelRegistry::scopeChannelBit(scope, iChannel));
		return iSlot < (int)percent.size() ? percent[iSlot] : 0;
	}

	int64_t sourceSum(int iChannel) const
	{
		const int iReceiver = CascadeChannelRegistry::scopeReceiverIndex(scope, iChannel);
		if (iReceiver < 0)
		{
			return 0;
		}
		set.rebuildMarked(CascadeChannelRegistry::scopeReceiverBit(scope, iChannel));
		return iReceiver < (int)sum.size() ? sum[iReceiver] : 0;
	}

	// The load drain (state-repositories.md): inside the load bracket the marks are BANKED, and this rebuilds
	// every one of them once the stream has ended. Drains only what was marked -- never a mark-all.
	void rebuildMarked() const { set.rebuildMarked(); }

	// ---- the triggers (called ONLY by the modifier consumer's derived marks -- no hand-wired mutation-site
	// ---- calls, state-repositories.md: the dirty flags fall out of the deposit addresses). The mark is ALSO
	// ---- the rebuild: CvDerivedCacheSet::markDirty recomputes the marked components right here, which is what
	// ---- lets every read above be a bare fetch. ----

	void markChannel(int iChannel) const { set.markDirty(CascadeChannelRegistry::scopeChannelBit(scope, iChannel)); }
	void markSum(int iChannel) const { set.markDirty(CascadeChannelRegistry::scopeReceiverBit(scope, iChannel)); }
	// The derived route mask, already in this scope's bit space (SourceRoute / the dependency masks).
	void markMask(int64_t iMask) const
	{
		if (iMask != 0)
		{
			set.markDirty(iMask);
		}
	}

	// ---- the refresh side (the gather's write surface; contract rule 2: fully define the output) ----

	// Size the dictionaries to the CURRENT registry layout (zero-filling any new tail). Called by the gather
	// at the top of every refresh -- the layout is load-minted and append-only, so sizes only ever grow.
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
		if (sum.size() < iReceivers)
		{
			sum.resize(iReceivers, 0);
		}
		if (scope == CASC_SCOPE_PLOT && substrateFlat.size() < iChannels)
		{
			substrateFlat.resize(iChannels, 0);
		}
	}
	// Slot write access for the gather (refs into the mutable storage; game-thread only).
	int64_t& slotFlat(int iSlotIndex) const { return flat[iSlotIndex]; }
	int& slotPercent(int iSlotIndex) const { return percent[iSlotIndex]; }
	int64_t& slotSum(int iReceiverIndex) const { return sum[iReceiverIndex]; }

private:
	CvCascadePackage(const CvCascadePackage&);              // noncopyable (the set binds an owner pointer)
	CvCascadePackage& operator=(const CvCascadePackage&);
};

#endif // CV_CASCADE_PACKAGE_H
