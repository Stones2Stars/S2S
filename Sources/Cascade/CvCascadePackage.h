#pragma once
#ifndef CV_CASCADE_PACKAGE_H
#define CV_CASCADE_PACKAGE_H

//
//	CvCascadePackage -- the ONE uniform per-scope value package of the modifier cascade
//	(state-repositories.md, the per-scope package model; [DEC-uniform-cache-shape]).
//
//	EVERY scope object (team / player / area / city / plot) carries this SAME type as a data member; what
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
#include "Infrastructure/CvDerivedCache.h"
#include <vector>

template <class TOwner>
struct CvCascadePackage
{
	CvCascScope scope;                     // which layout this package lives on (set at bind)
	mutable std::vector<int> flat;      // dictionary 1: the channel-indexed x100 flat sums (local slots)
	mutable std::vector<int> percent;   // dictionary 2: the channel-indexed x100 percent sums (local slots)
	mutable std::vector<int> sum;       // the receiver slots: the realized x100 totals this scope consumes
	CvDerivedCacheSet<TOwner, int64_t> set;   // THE dirty protocol -- the one component, 64-bit mask form

	CvCascadePackage() : scope(CASC_SCOPE_CITY) {}

	// Wire the owner + its refresh delegate. All-dirty from the start (a loaded game recomputes on first read).
	void bind(CvCascScope ePackageScope, const TOwner* pOwner, void (TOwner::*pfnRefresh)(int64_t) const)
	{
		scope = ePackageScope;
		set.bind(pOwner, pfnRefresh);
	}

	// ---- the reads: refresh THIS slot's bit if stale, then a bare fetch. Disjoint channels never pay for
	// ---- each other; a never-authored channel answers 0 without any storage existing anywhere. ----

	int readFlat(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		set.ensure(CascadeChannelRegistry::scopeChannelBit(scope, iChannel));
		return iSlot < (int)flat.size() ? flat[iSlot] : 0;
	}

	int readPercent(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0)
		{
			return 0;
		}
		set.ensure(CascadeChannelRegistry::scopeChannelBit(scope, iChannel));
		return iSlot < (int)percent.size() ? percent[iSlot] : 0;
	}

	int readSum(int iChannel) const
	{
		const int iReceiver = CascadeChannelRegistry::scopeReceiverIndex(scope, iChannel);
		if (iReceiver < 0)
		{
			return 0;
		}
		set.ensure(CascadeChannelRegistry::scopeReceiverBit(scope, iChannel));
		return iReceiver < (int)sum.size() ? sum[iReceiver] : 0;
	}

	// Raw fetches with no ensure -- for a combine that has already ensured the bits it walks.
	int rawFlat(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		return (iSlot >= 0 && iSlot < (int)flat.size()) ? flat[iSlot] : 0;
	}
	int rawPercent(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		return (iSlot >= 0 && iSlot < (int)percent.size()) ? percent[iSlot] : 0;
	}

	// ---- the triggers (called ONLY by the modifier consumer's derived marks -- no hand-wired mutation-site
	// ---- calls, state-repositories.md: the dirty flags fall out of the deposit addresses) ----

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
	}
	// Slot write access for the gather (refs into the mutable storage; game-thread only).
	int& slotFlat(int iSlotIndex) const { return flat[iSlotIndex]; }
	int& slotPercent(int iSlotIndex) const { return percent[iSlotIndex]; }
	int& slotSum(int iReceiverIndex) const { return sum[iReceiverIndex]; }

private:
	CvCascadePackage(const CvCascadePackage&);              // noncopyable (the set binds an owner pointer)
	CvCascadePackage& operator=(const CvCascadePackage&);
};

#endif // CV_CASCADE_PACKAGE_H
