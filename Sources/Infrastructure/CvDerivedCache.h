#pragma once
#ifndef CV_DERIVED_CACHE_H
#define CV_DERIVED_CACHE_H

//
//	CvDerivedCache -- the ONE standardized recompute-only derived-value cache
//	(docs/architecture/state-repositories.md). Every derived engine value follows this shape instead of a
//	hand-rolled member+flag pair: recompute-only and NEVER serialized (marked from construction, so a loaded
//	game rebuilds from current state -- never stale-from-save), and the single PULL source (no push, no
//	parallel copy).
//
//	Poor-man's-DI-adjacent (patterns.md): the recompute is the injected dependency -- a member-function-pointer,
//	so the one part that genuinely needs owner state stays owner-side; storage, flag and the trigger are the
//	reusable contract. C++03/VC7.1-safe: no statics, no virtuals, plain templates.
//
//	⛔ A READ IS A BARE FETCH, AND THE REBUILD HAPPENS AT THE MARK. Whatever marks a component is what rebuilds
//	it. A read NEVER recomputes: the ensure-on-read protocol is tombstoned by name (superseded-ideas #14) and was
//	measured grinding unit automation on the AI-hot paths. The consequence IS the point -- if a state change
//	fails to emit, nothing marks the component, nothing rebuilds it, and the value is VISIBLY WRONG. No
//	read-side recompute may paper that over ([DEC-no-self-heal]): a missing event must be findable, and a
//	self-healing recalc is exactly what makes it unfindable.
//
//	THE ONE DEFERRAL -- the load bracket. Between GAME_LOAD_STARTED and GAME_LOAD_FINISHED the marks are BANKED
//	and no rebuild runs at the mark: mid-read the state a recompute reads is half-deserialized (the context
//	stores, the areas, the plot-group network all complete only when the stream ends), and with no read-side
//	recompute that wrong value would then stand forever. Each system drains its OWN banked marks on
//	GAME_LOAD_FINISHED through rebuildMarked() -- the reseed's eager load build (state-repositories.md), never a
//	blanket mark-all. Marking during the load and draining once at its end is the same shape the contexts use.
//
//	⛔ THE RECOMPUTE-FROM-SOURCE IS AN ENDPOINT ORACLE, AND THE COMPARISON HAPPENS OUTSIDE THE DLL
//	(state-repositories.md, owner). recomputeInto() runs the owner's recompute over a CALLER-OWNED buffer and is
//	callable with no gate; an endpoint serves that buffer beside the STORED value, and an external consumer diffs
//	the two. A disagreement is a missed emit -- an OBSERVATION a reader makes about two served numbers, never a
//	happening: it has NO in-DLL representation (no diff, no log line, no event, no field). An event is an
//	invitation to a consumer, and a consumer that "handles" a value known to be wrong corrects it, which is
//	self-heal arriving because the shape invited it ([DEC-no-self-heal]). A PULL cannot grow that consumer.
//	Serving into a caller-owned buffer is what makes "never repairs" STRUCTURAL: the oracle is never handed the
//	stored slots, so there is no snapshot, no restore, and no window in which a half-finished recompute could
//	leave a repaired value behind.
//
//	CONTRACT RULES (the spec-hole plugs):
//	 1. Clear the mark BEFORE recomputing -- a recompute that marks back through the cache keeps its mark for the
//	    next rebuild, and a recompute that reads back through the cache never recurses.
//	 2. The recompute must FULLY DEFINE its output on every call (zero-fill on can't-compute, e.g. an owner not
//	    yet initialized) -- a partial write leaves stale garbage behind a cleared mark.
//	 3. NONCOPYABLE -- the bound owner pointer must stay address-stable; bind in the owner's ctor/init.
//	 4. data() pointers stay valid (inline storage) but the VALUES mutate on rebuild -- never cache the pointer
//	    across game-state changes. Game-thread only (mutable state mutates under const).
//	 5. Fixed compile-time N in the array form; a runtime-sized domain uses the Vec form.
//
//	THREE FORMS:
//	 - CvDerivedCache<TOwner,T,N>   -- the single-flag leaf cache, compile-time N (the plot-yield shape).
//	 - CvDerivedCacheVec<TOwner,T>  -- the single-flag RUNTIME-SIZED form: the recompute receives the vector and
//	   must fully (re)size + define it (rule 2 falls out of an assign(n,0) then fill).
//	 - CvDerivedCacheSet<TOwner,TMask> -- the PARTIAL-DIRTY form: a value composed of several isolated "plugin
//	   number" components carries a mark BITMASK, one bit per component, and ONE shared refresh receives the
//	   mask. Component storage stays OWNER-SIDE (the components are heterogeneous), so this class owns only the
//	   mark protocol -- and the ORACLE therefore belongs to the STORAGE OWNER too, which is the only side that
//	   can hand the refresh a scratch buffer of its own storage shape (CascadeGather's gather-into entry points
//	   for the cascade packages). TMask is the mask WIDTH (default int): the cascade scope packages instantiate
//	   the 64-bit form (the city/empire channel sets exceed a 32-bit mask); every few-bit user keeps the int
//	   default untouched. Exemplar: the modifier scope package (CvCascadePackage).
//

#include "Spine/CvEventSpine.h"   // spineGameLoadInProgress -- the load bracket the mark banks inside
#include <vector>

template <class TOwner, class T, int N>
class CvDerivedCache
{
public:
	CvDerivedCache() : m_pOwner(NULL), m_pfnRecompute(NULL), m_bMarked(true)
	{
		for (int iSlot = 0; iSlot < N; ++iSlot)
		{
			m_aData[iSlot] = T();
		}
	}

	// Wire the owner + its recompute (fills the out-array from CURRENT state). Marked from the start; the first
	// mark-driven rebuild fills it.
	void bind(const TOwner* pOwner, void (TOwner::*pfnRecompute)(T*) const)
	{
		m_pOwner = pOwner;
		m_pfnRecompute = pfnRecompute;
		m_bMarked = true;
	}

	// The TRIGGER -- call at every input-change site. THE MARK IS WHAT REBUILDS, so the read can stay a bare
	// fetch; inside the load bracket the mark is banked for the drain instead.
	void markDirty() const
	{
		m_bMarked = true;
		if (!spineGameLoadInProgress())
		{
			rebuildMarked();
		}
	}

	// Rebuild if still marked -- the mark's own rebuild, and the load drain's entry point. Never a read path.
	void rebuildMarked() const
	{
		if (m_bMarked && m_pOwner != NULL)
		{
			m_bMarked = false;   // clear FIRST (rule 1)
			(m_pOwner->*m_pfnRecompute)(m_aData);
		}
	}

	// Is this cache still marked (banked during the load bracket, awaiting its drain)?
	bool isMarked() const { return m_bMarked; }

	// THE READS -- bare fetches, unconditionally. Nothing is tested and nothing recomputes here.
	T get(int iIndex) const
	{
		return m_aData[iIndex];
	}
	// The raw-array read (for legacy array-form accessors). Read-only by convention.
	T* data() const
	{
		return m_aData;
	}

	// THE ORACLE -- the same recompute over the CALLER'S buffer (N slots), for an endpoint to serve beside the
	// stored value. No gate. The stored slots are never passed in, so this structurally cannot repair them.
	void recomputeInto(T* aOut) const
	{
		if (m_pOwner == NULL)
		{
			for (int iSlot = 0; iSlot < N; ++iSlot)
			{
				aOut[iSlot] = T();   // fully define the output even unbound (contract rule 2)
			}
			return;
		}
		(m_pOwner->*m_pfnRecompute)(aOut);
	}

private:
	CvDerivedCache(const CvDerivedCache&);              // noncopyable: a copied cache would keep the ORIGINAL
	CvDerivedCache& operator=(const CvDerivedCache&);   // owner's pointer -- the dangling-owner footgun
	const TOwner* m_pOwner;
	void (TOwner::*m_pfnRecompute)(T*) const;
	mutable T m_aData[N];
	mutable bool m_bMarked;
};

template <class TOwner, class T>
class CvDerivedCacheVec
{
public:
	CvDerivedCacheVec() : m_pOwner(NULL), m_pfnRecompute(NULL), m_bMarked(true) {}

	// Wire the owner + its recompute. The recompute FULLY (re)sizes and defines the vector from CURRENT state
	// (assign-then-fill) -- sizing is owner-side because the domain count (GC.getNum*Infos) is not known at
	// construction time. Marked from the start.
	void bind(const TOwner* pOwner, void (TOwner::*pfnRecompute)(std::vector<T>&) const)
	{
		m_pOwner = pOwner;
		m_pfnRecompute = pfnRecompute;
		m_bMarked = true;
	}

	// The TRIGGER -- THE MARK IS WHAT REBUILDS (banked inside the load bracket; drained at its end).
	void markDirty() const
	{
		m_bMarked = true;
		if (!spineGameLoadInProgress())
		{
			rebuildMarked();
		}
	}

	void rebuildMarked() const
	{
		if (m_bMarked && m_pOwner != NULL)
		{
			m_bMarked = false;   // clear FIRST (rule 1)
			(m_pOwner->*m_pfnRecompute)(m_aData);
		}
	}

	bool isMarked() const { return m_bMarked; }

	// THE READ -- a bare, bounds-safe fetch: an index outside the rebuilt domain answers T() (the
	// never-authored answer).
	T get(int iIndex) const
	{
		return (iIndex >= 0 && iIndex < (int)m_aData.size()) ? m_aData[iIndex] : T();
	}
	// The stored vector's current size -- an endpoint serving the stored side reads it to bound its walk.
	int size() const { return (int)m_aData.size(); }

	// THE ORACLE -- the same recompute over the CALLER'S vector (which it fully sizes + defines), for an
	// endpoint to serve beside the stored value. No gate; the stored vector is never passed in.
	void recomputeInto(std::vector<T>& aOut) const
	{
		if (m_pOwner == NULL)
		{
			aOut.clear();   // fully define the output even unbound (contract rule 2)
			return;
		}
		(m_pOwner->*m_pfnRecompute)(aOut);
	}

private:
	CvDerivedCacheVec(const CvDerivedCacheVec&);              // noncopyable (see CvDerivedCache)
	CvDerivedCacheVec& operator=(const CvDerivedCacheVec&);
	const TOwner* m_pOwner;
	void (TOwner::*m_pfnRecompute)(std::vector<T>&) const;
	mutable std::vector<T> m_aData;
	mutable bool m_bMarked;
};

template <class TOwner, class TMask = int>
class CvDerivedCacheSet
{
public:
	CvDerivedCacheSet() : m_pOwner(NULL), m_pfnRefresh(NULL), m_iMarked(TMask(-1)) {}

	// Wire the owner + its refresh (recomputes the MASKED components from CURRENT state). All components marked
	// from the start.
	void bind(const TOwner* pOwner, void (TOwner::*pfnRefresh)(TMask) const)
	{
		m_pOwner = pOwner;
		m_pfnRefresh = pfnRefresh;
		m_iMarked = TMask(-1);
	}

	// The TRIGGER -- mark only the components this event feeds ("the rest of the pipe stays the same"), and
	// REBUILD them right here: THE MARK IS WHAT REBUILDS, which is what lets every read be a bare fetch. Inside
	// the load bracket the mark is banked and the rebuild waits for the drain.
	void markDirty(TMask iMask) const
	{
		m_iMarked |= iMask;
		if (!spineGameLoadInProgress())
		{
			rebuildMarked(iMask);
		}
	}
	void markAllDirty() const { markDirty(TMask(-1)); }

	// Is any component in `iMask` still marked (banked during the load bracket, awaiting its drain)? Outside
	// that window nothing stays marked -- the mark rebuilt it.
	bool isDirty(TMask iMask) const { return (m_iMarked & iMask) != 0; }

	// Rebuild every still-marked component -- the load drain's entry point.
	void rebuildMarked() const { rebuildMarked(TMask(-1)); }

	// Rebuild only the WANTED marked components; the rest STAY marked for their own rebuilds. Keeps read paths
	// with disjoint components decoupled (the ACCD_WB lesson: unit moves dirtied wellbeing and every yield read
	// paid the wellbeing walk).
	void rebuildMarked(TMask iWantMask) const
	{
		const TMask iMask = m_iMarked & iWantMask;
		if (iMask != 0 && m_pOwner != NULL)
		{
			m_iMarked &= ~iMask;   // clear FIRST (rule 1) -- only the refreshed bits
			(m_pOwner->*m_pfnRefresh)(iMask);
		}
	}

private:
	CvDerivedCacheSet(const CvDerivedCacheSet&);              // noncopyable (see CvDerivedCache)
	CvDerivedCacheSet& operator=(const CvDerivedCacheSet&);
	const TOwner* m_pOwner;
	void (TOwner::*m_pfnRefresh)(TMask iMask) const;
	mutable TMask m_iMarked;
};

#endif // CV_DERIVED_CACHE_H
