#pragma once
#ifndef CV_DERIVED_CACHE_H
#define CV_DERIVED_CACHE_H

//
//	CvDerivedCache -- the ONE standardized recompute-only derived-value cache
//	(docs/architecture/state-repositories.md, formalized 2026-06-28; BUILT 2026-07-03 -- the substrate/final-
//	migration moment the spec named). Every derived engine value follows this shape instead of a hand-rolled
//	member+flag pair: lazy + dirty-flagged (the expensive recompute runs once per change-then-read, never per
//	change and never per read), recompute-only and NEVER serialized (dirty-on-construct, so a loaded game
//	recomputes from current state -- never stale-from-save; owner 2026-07-03: trading longer save-load for
//	shorter turn times is an easy trade), and the single PULL source (no push, no parallel copy).
//
//	Poor-man's-DI-adjacent (patterns.md): the recompute is the injected dependency -- a member-function-pointer,
//	so the one part that genuinely needs owner state stays owner-side; storage, flag, pull-on-read, and the
//	trigger are the reusable contract. C++03/VC7.1-safe: no statics, no virtuals, plain templates.
//
//	CONTRACT RULES (the spec-hole plugs, owner 2026-07-03 "find holes and plug them"):
//	 1. Clear-dirty BEFORE recompute -- a mid-recompute markDirty survives to the next read; a recompute that
//	    reads back through the cache never recurses.
//	 2. The recompute must FULLY DEFINE its output on every call (zero-fill on can't-compute, e.g. an owner not
//	    yet initialized) -- a partial write leaves stale garbage behind a clean flag.
//	 3. NONCOPYABLE -- the bound owner pointer must stay address-stable; bind in the owner's ctor/init.
//	 4. data() pointers stay valid (inline storage) but the VALUES mutate on recompute -- never cache the
//	    pointer across game-state changes. Game-thread only (mutable state mutates on read).
//	 5. Fixed compile-time N -- a runtime-sized domain (NumBuildingInfos...) needs a vector variant when first
//	    needed, same contract.
//
//	THREE FORMS:
//	 - CvDerivedCache<TOwner,T,N>   -- the single-flag leaf cache, compile-time N (the plot-yield shape).
//	 - CvDerivedCacheVec<TOwner,T>  -- the single-flag RUNTIME-SIZED form (contract rule 5's named variant,
//	   built 2026-07-05 at first need): the recompute receives the vector and must fully (re)size + define it
//	   (rule 2 falls out naturally -- an assign(n,0) then fill). Exemplar: the player building-commerce ledger
//	   (NumBuildingInfos x NUM_COMMERCE_TYPES, flat-indexed).
//	 - CvDerivedCacheSet<TOwner,TMask> -- the PARTIAL-DIRTY form (owner ruling 2026-07-03): a value composed of
//	   several isolated "plugin number" components carries a dirty BITMASK, one bit per component, and ONE
//	   shared refresh pass receives the mask (component storage stays owner-side -- the components are
//	   heterogeneous; this class owns only the dirty protocol). TMask is the mask WIDTH (default int): the
//	   cascade scope packages instantiate the 64-bit form (state-repositories.md KEYS-ONLY-WHERE-NEEDED: the
//	   city/empire channel sets exceed a 32-bit mask); every few-bit user keeps the int default untouched.
//	   Exemplar: the modifier scope package (CvCascadePackage).
//

#include <vector>

template <class TOwner, class T, int N>
class CvDerivedCache
{
public:
	CvDerivedCache() : m_pOwner(NULL), m_pfnRecompute(NULL), m_bDirty(true)
	{
		for (int i = 0; i < N; ++i) m_aData[i] = T();
	}

	// Wire the owner + its recompute (fills the out-array from CURRENT state). Dirty from the start.
	void bind(const TOwner* pOwner, void (TOwner::*pfnRecompute)(T*) const)
	{
		m_pOwner = pOwner;
		m_pfnRecompute = pfnRecompute;
		m_bDirty = true;
	}

	// The TRIGGER -- call at every input-change site. No eager recompute, no push.
	void markDirty() const { m_bDirty = true; }

	T get(int i) const { ensure(); return m_aData[i]; }
	// The raw-array read (for legacy array-form accessors). Read-only by convention.
	T* data() const { ensure(); return m_aData; }

private:
	void ensure() const
	{
		if (m_bDirty && m_pOwner != NULL)
		{
			m_bDirty = false;   // clear FIRST: a recompute that reads back through the owner must not recurse
			(m_pOwner->*m_pfnRecompute)(m_aData);
		}
	}
	CvDerivedCache(const CvDerivedCache&);              // noncopyable: a copied cache would keep the ORIGINAL
	CvDerivedCache& operator=(const CvDerivedCache&);   // owner's pointer -- the dangling-owner footgun
	const TOwner* m_pOwner;
	void (TOwner::*m_pfnRecompute)(T*) const;
	mutable T m_aData[N];
	mutable bool m_bDirty;
};

template <class TOwner, class T>
class CvDerivedCacheVec
{
public:
	CvDerivedCacheVec() : m_pOwner(NULL), m_pfnRecompute(NULL), m_bDirty(true) {}

	// Wire the owner + its recompute. The recompute FULLY (re)sizes and defines the vector from CURRENT
	// state (assign-then-fill) -- sizing is owner-side because the domain count (GC.getNum*Infos) is not
	// known at construction time. Dirty from the start.
	void bind(const TOwner* pOwner, void (TOwner::*pfnRecompute)(std::vector<T>&) const)
	{
		m_pOwner = pOwner;
		m_pfnRecompute = pfnRecompute;
		m_bDirty = true;
	}

	// The TRIGGER -- call at every input-change site. No eager recompute, no push.
	void markDirty() const { m_bDirty = true; }

	// Bounds-safe read: an index outside the recomputed domain answers T() (the never-authored answer).
	T get(int i) const
	{
		ensure();
		return (i >= 0 && i < (int)m_aData.size()) ? m_aData[i] : T();
	}

private:
	void ensure() const
	{
		if (m_bDirty && m_pOwner != NULL)
		{
			m_bDirty = false;   // clear FIRST (rule 1)
			(m_pOwner->*m_pfnRecompute)(m_aData);
		}
	}
	CvDerivedCacheVec(const CvDerivedCacheVec&);              // noncopyable (see CvDerivedCache)
	CvDerivedCacheVec& operator=(const CvDerivedCacheVec&);
	const TOwner* m_pOwner;
	void (TOwner::*m_pfnRecompute)(std::vector<T>&) const;
	mutable std::vector<T> m_aData;
	mutable bool m_bDirty;
};

template <class TOwner, class TMask = int>
class CvDerivedCacheSet
{
public:
	CvDerivedCacheSet() : m_pOwner(NULL), m_pfnRefresh(NULL), m_iDirty(TMask(-1)) {}

	// Wire the owner + its refresh (recomputes the MASKED components from CURRENT state). All-dirty from the start.
	void bind(const TOwner* pOwner, void (TOwner::*pfnRefresh)(TMask) const)
	{
		m_pOwner = pOwner;
		m_pfnRefresh = pfnRefresh;
		m_iDirty = TMask(-1);
	}

	// The TRIGGER -- mark only the components this event feeds ("the rest of the pipe stays the same").
	void markDirty(TMask iMask) const { m_iDirty |= iMask; }
	void markAllDirty() const { m_iDirty = TMask(-1); }
	// Is any component in `iMask` dirty (pending rebuild)? Lets a caller do a TARGETED incremental update only on
	// a currently-BUILT component (a dirty one gets its full rebuild on next ensure, so skip the targeted work).
	bool isDirty(TMask iMask) const { return (m_iDirty & iMask) != 0; }

	// Pull-on-read: refresh the dirty components once, then reads are pure arithmetic owner-side.
	void ensure() const
	{
		if (m_iDirty != 0 && m_pOwner != NULL)
		{
			const TMask iMask = m_iDirty;
			m_iDirty = 0;   // clear FIRST: a refresh that reads back through ensure() must not recurse
			(m_pOwner->*m_pfnRefresh)(iMask);
		}
	}

	// The MASKED pull: refresh only the WANTED dirty components; the rest STAY dirty for their own readers.
	// Decouples read paths with disjoint components (a hot rate read must never pay a cold component's
	// recompute -- the ACCD_WB lesson: unit moves dirtied WB and every yield read paid the wellbeing walk).
	void ensure(TMask iWantMask) const
	{
		const TMask iMask = m_iDirty & iWantMask;
		if (iMask != 0 && m_pOwner != NULL)
		{
			m_iDirty &= ~iMask;   // clear FIRST (only the refreshed bits)
			(m_pOwner->*m_pfnRefresh)(iMask);
		}
	}

private:
	CvDerivedCacheSet(const CvDerivedCacheSet&);              // noncopyable (see CvDerivedCache)
	CvDerivedCacheSet& operator=(const CvDerivedCacheSet&);
	const TOwner* m_pOwner;
	void (TOwner::*m_pfnRefresh)(TMask iMask) const;
	mutable TMask m_iDirty;
};

#endif // CV_DERIVED_CACHE_H
