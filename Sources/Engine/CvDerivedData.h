#pragma once
#ifndef CV_DERIVED_DATA_H
#define CV_DERIVED_DATA_H

#include <vector>
#include "FAssert.h"

//
//	Derived-data repositories -- four levels: Game > Team > Player > City.
//	Architecture: Sources/docs/plans/derived-data-repository.md (the read-side of
//	Sources/docs/plans/ai-architecture-north-star.md).
//
//	Each repository holds memoized, CHANGE-DRIVEN derived AI data for its scope: computed lazily and
//	refreshed only when something that affects it changed, instead of the per-turn "wipe everything
//	and rebuild from scratch" the AI does today. Goals: (1) only recompute what changed, (2) one API
//	surface for reuse, (3) de-duplicate the hand-rolled re-derivations scattered across the AI files,
//	(4) shrink those files by moving the logic here.
//
//	PLACEMENT: on the BASE game objects (CvGame / CvTeam / CvPlayer / CvCity, via dataRepository()),
//	NOT the AI subclasses -- the AI classes are being dissolved (north-star §1) and the flagship
//	tenants (the canConstruct caches) are shared with the UI. Each datum lives at the HIGHEST level
//	where its inputs are invariant (a tech fact at Team, a civic value at Player, the constructible
//	set at City), so one recompute serves everything below it.
//	NOTE: truly STATIC XML-derived data (the prereq/enabler reverse-indices) does NOT live here --
//	it lives on cvInternalGlobals, built once in doPostLoadCaching and never invalidated (see
//	getBuildingsEnabledBy / getUnitsEnabledBy). The Game repository is for GAME-STATE-derived,
//	game-scoped data only.
//
//	---------------------------------------------------------------------------------------------
//	HOW STALENESS IS TRACKED -- two complementary mechanisms:
//
//	1. DIRTY FLAG (push): a mutation endpoint calls invalidate() on the data it touches, AT THE
//	   LEVEL THAT OWNS THE MUTATED INPUT only. The getter recomputes on the next read.
//	2. VERSION COUNTER (pull): every datum carries version(), bumped only when a recompute actually
//	   CHANGED the value. A datum derived from another level's datum records the upstream version it
//	   computed against (TDependency) and recomputes when that moved -- cross-LEVEL staleness is
//	   self-detecting, so push events never need fan-out wiring across levels (the missed-hook
//	   failure class that killed the building-value retention experiment).
//
//	Same-level getter idiom:
//	    const T& CvCityDataRepository::getFoo()
//	    {
//	        if (m_foo.dirty())
//	            m_foo.set(computeFoo());        // set() only bumps version() if the value changed
//	        return m_foo.value();
//	    }
//
//	Cross-level idiom (a City datum derived from a Player datum) -- freshen the upstream THROUGH
//	ITS GETTER first, then compare versions:
//	    const T& CvCityDataRepository::getBar()
//	    {
//	        CvPlayerDataRepository& kUp = ...;  // owner()->getOwner()'s repository
//	        kUp.getUpstream();                                       // ensure upstream is fresh
//	        if (m_bar.dirty() || m_barDep.changed(kUp.upstreamVersion()))
//	        {
//	            m_bar.set(computeBar());
//	            m_barDep.seen(kUp.upstreamVersion());
//	        }
//	        return m_bar.value();
//	    }
//
//	BOUNDED STALENESS: a datum whose inputs move too often for event-exact invalidation (e.g. the
//	constructible set also depends on population / culture / properties) is declared with a max age
//	in turns; dirty() then also returns true once the value is older than that. This is the
//	"recompute every N turns" backstop done systematically instead of per-datum hand-rolls.
//
//	RULES (hard-won -- see the plan doc):
//	- Repository data is ADVISORY only: never synced game state, and never an input to control
//	  flow that could spin (the building-VALUE cache hang). Consumers must be stale-tolerant.
//	- Never serialized as truth: each owner's reset() (game init AND load) calls
//	  dataRepository().reset(), marking everything dirty; data rebuilds lazily.
//	- Registration is automatic: every datum registers with its repository on construction, so
//	  invalidateAll()/reset() can never drift out of sync with the member list.
//	- Asserts here are INVARIANT guards (programming errors only). Migration verification
//	  (recompute == cache) goes through the gated [PERF] logging channel, which ships in
//	  FinalRelease -- FAssert does not.
//	- READ-ONLY PHASE: for the future parallel read pass (north-star §4), set
//	  TLazyBase::setReadOnlyPhase(true) around the pass -- any recompute or invalidation inside it
//	  asserts. Data must be precomputed before the phase and only read during it.
//

class CvGame;
class CvTeam;
class CvPlayer;
class CvCity;

class TLazyBase;

//	Datum registry + whole-repository operations. Non-copyable: the registered pointers refer to
//	the owning object's own data members.
class CvDataRepositoryBase
{
public:
	CvDataRepositoryBase() {}

	//	Called by TLazyBase on construction -- never call manually.
	void registerDatum(TLazyBase* pDatum) { m_data.push_back(pDatum); }

	//	Coarse bounded-staleness backstop -- mark every registered datum dirty.
	void invalidateAll();

	//	Game-init / load: derived data is never trusted from a save -- drop it all, rebuild lazily.
	void reset() { invalidateAll(); }

private:
	CvDataRepositoryBase(const CvDataRepositoryBase&);             // not copyable
	CvDataRepositoryBase& operator=(const CvDataRepositoryBase&);  // not assignable

	std::vector<TLazyBase*> m_data;
};

//	Per-datum bookkeeping: dirty flag, change version, computed-turn stamp (bounded staleness).
//	Non-template so the repository registry can hold every datum uniformly.
class TLazyBase
{
public:
	//	iMaxAgeTurns: -1 = event-exact (never expires by age); >= 0 = bounded staleness -- the
	//	datum also counts as dirty once it is older than this many turns.
	TLazyBase(CvDataRepositoryBase& repository, int iMaxAgeTurns = -1)
		: m_bDirty(true)
		, m_iVersion(1)
		, m_iComputedOnTurn(-1)
		, m_iMaxAgeTurns(iMaxAgeTurns)
	{
		repository.registerDatum(this);
	}

	bool dirty() const;        // invalidated, or past its max age
	void invalidate();
	unsigned int version() const { return m_iVersion; }   // bumped only on real value change

	//	Parallel-pass support: while the read-only phase is set, recompute/invalidate assert.
	static void setReadOnlyPhase(bool bReadOnly);
	static bool isReadOnlyPhase();

protected:
	void markFresh(bool bValueChanged);   // clears dirty, stamps the turn, bumps version if changed

private:
	TLazyBase(const TLazyBase&);             // not copyable
	TLazyBase& operator=(const TLazyBase&);  // not assignable

	bool         m_bDirty;
	unsigned int m_iVersion;
	int          m_iComputedOnTurn;
	int          m_iMaxAgeTurns;

	static bool  s_bReadOnlyPhase;
};

//	Lazy/dirty holder for one derived value (see the getter idioms above). T needs operator==.
template <typename T>
class TLazy : public TLazyBase
{
public:
	explicit TLazy(CvDataRepositoryBase& repository, int iMaxAgeTurns = -1)
		: TLazyBase(repository, iMaxAgeTurns)
		, m_value()
	{}

	const T& value() const
	{
		FAssertMsg(!dirty(), "TLazy::value() read while dirty -- the getter must recompute first");
		return m_value;
	}

	//	Recompute landing point. Bumps version() only when the value actually changed, so
	//	downstream version checks skip their own recompute when an upstream rebuild produced the
	//	same answer (the common case -- e.g. the constructible set is flat turn-over-turn).
	void set(const T& v)
	{
		const bool bChanged = !(m_value == v);
		if (bChanged)
		{
			m_value = v;
		}
		markFresh(bChanged);
	}

private:
	T m_value;
};

//	Records the upstream version() a datum last computed against -- the pull side of cross-level
//	dependencies (see the cross-level getter idiom above).
class TDependency
{
public:
	TDependency() : m_iSeenVersion(0) {}   // versions start at 1, so 0 == never seen

	bool changed(unsigned int iUpstreamVersion) const { return m_iSeenVersion != iUpstreamVersion; }
	void seen(unsigned int iUpstreamVersion)          { m_iSeenVersion = iUpstreamVersion; }

private:
	unsigned int m_iSeenVersion;
};

//	Common base for the four scope repositories: typed back-pointer to the owning BASE game object
//	(used to recompute from owner state and to reach the level above: City -> Player -> Team -> Game).
template <typename TOwner>
class CvDataRepository : public CvDataRepositoryBase
{
public:
	CvDataRepository() : m_pOwner(NULL) {}

	void    init(TOwner* pOwner) { m_pOwner = pOwner; }
	TOwner* owner() const        { return m_pOwner; }

protected:
	TOwner* m_pOwner;
};

//	-- Game level: GAME-STATE-derived, game-scoped data. (Static XML-derived indices live on
//	   cvInternalGlobals instead: built once at load, never invalidated.) --
class CvGameDataRepository   : public CvDataRepository<CvGame>   {};

//	-- Team level: tech / obsolete-building / war-shared facts --
class CvTeamDataRepository   : public CvDataRepository<CvTeam>   {};

//	-- Player level: tech / civic / promotion / bonus values, counts, area unit-AI counts --
class CvPlayerDataRepository : public CvDataRepository<CvPlayer> {};

//	-- City level: constructible set, building values, the city's declared needs --
class CvCityDataRepository   : public CvDataRepository<CvCity>   {};

#endif // CV_DERIVED_DATA_H
