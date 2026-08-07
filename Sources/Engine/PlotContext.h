#pragma once
#ifndef CV_PLOT_CONTEXT_H
#define CV_PLOT_CONTEXT_H

//
//	PlotContext -- the per-PLOT live-state STORE, the plot-scope sibling of CityContext / EmpireContext (same rules,
//	kept symmetric so a reader always knows where to go: plot state here, city state on CityContext, empire state on
//	EmpireContext). Bound to its CvPlot by pointer (never a value copy -- passing a bound reference is far cheaper
//	than snapshotting values, owner).
//
//	⛔ STORAGE, MAINTENANCE AND THE FACTS THAT DRIVE IT LIVE IN ONE PLACE (owner) -- the dictionaries here, the
//	appliers here, and the DECLARED SET OF FACTS here ([DEC-dict-is-a-consumer]). A store whose state sits on one
//	object while a router elsewhere decides when it moves has two homes and no owner, and a missing route then hides
//	in a `switch` that looks complete instead of being visible AT the store.
//
//	A context is an EVENT-BUILT STORE, not a forwarding facade (contexts.md). What it STORES is the plot's DERIVED
//	predicate verdicts, held as a BITSET keyed by the CASC_PRED_* condition vocabulary (CvCondition.h) -- ONE
//	alphabet shared with the conditions an info authors, which is what makes the stored state distinguishable ("yes,
//	I will actually deliver this, based on this state"). Every bare HAS_/IS_ read is therefore an O(1) bit test and
//	never a walk (patterns.md: a predicate that walks plots per call is the efficiency defect to reject in review).
//	What it FORWARDS is only the RAW substrate CvPlot already holds O(1) -- the terrain / feature / improvement /
//	route / bonus ids a PARAMETERIZED predicate ({HAS_TERRAIN: T}) keys on, plus the owner / latitude / nature-yield
//	scalars; storing a second copy of raw data would be duplication.
//
//	⛔ THE FACT SETS THE BIT IT NAMES -- it does not trigger a callback that goes and asks (owner, contexts.md).
//	There is no whole-block re-derivation here and no `refresh*` entry point: re-deriving every bit because SOME
//	substrate fact arrived is the legacy read path RESCHEDULED from read-time to event-time, not deleted. What
//	replaces it is a PER-BIT TABLE (PlotContext.cpp): each bit declares its own derivation AND the substrate AXES
//	that feed it, so the routing is DERIVED from that table rather than hand-written per event. A new bit is one
//	row; a new fact is one axis. ⚠ That is the guard the retired justification was right about -- a hand-written
//	per-event bit mask drifts from what the bits actually read -- answered by declaring the dependency beside the
//	derivation instead of by recomputing everything.
//
//	THE TWO STORED BLOCKS -- they differ in WHAT THEY READ, which is what decides the fan-out:
//	 - OWN-PLOT facts:  IS_WATER / IS_LAND / IS_FLATLANDS / HAS_HILLS / HAS_PEAK / HAS_RIVER / HAS_IRRIGATION /
//	                    HAS_FEATURE / HAS_LANDMARK / IS_OWNED / IS_WORKED -- derived from THIS plot alone.
//	 - ADJACENCY facts: HAS_COAST and HAS_FRESHWATER -- derived from this plot AND its neighbours.
//
//	BESIDE THE BITSET, ONE STORED ID: the SERVED-RESOURCE verdict -- which bonus this tile makes available ON SITE
//	(it carries the bonus AND its improvement trades it). It is an id rather than a bit because a plot holds at most
//	one bonus, so the answer names a resource; everything else about it is the bitset's contract exactly (derived
//	when an axis it reads moves, one write point, the crossing announced and nothing else).
//
//	⚖ HAS_COAST IS SYMMETRIC: LAND WITH ADJACENT WATER, **OR WATER WITH ADJACENT LAND** (owner). Off the stored
//	bits that collapses to one statement -- A NEIGHBOUR WHOSE `IS_WATER` DIFFERS FROM MINE, i.e. the plot sits on
//	the land/water boundary -- so the verdict reads entirely off blocks this store already holds and needs no walk
//	back through CvPlot. ⚠ It also FIXES a live defect: the derivation this replaced called `isCoastalLand()`,
//	which returns false for a water plot outright, so every water tile read HAS_COAST false.
//	⚑ And it is what deletes the deferred-drain machinery: the size test `isCoastal` applies is `>= iMinWaterSize`
//	with the bare predicate's default of -1, which every existing area passes, so the ONLY reason that derivation
//	touched `CvArea` at all was a comparison that can never fail. Nothing here dereferences an area now, so there
//	is no unsettled-map window to defer against. (The city-scope `{HAS_COAST:{minArea:N}}` form is a different
//	question and stays CityContext's -- it is the one that genuinely needs the water-body SIZE.)
//
//	⚖ HAS_FRESHWATER KEEPS CALLING `CvPlot::isFreshWater`, DELIBERATELY. Its verdict is seven legs deep (fresh
//	terrain, river, the plot city's own fresh-water access, impassability, a feature that adds fresh water, and the
//	rect(1,1) neighbour scan) and the ENGINE still consults the same predicate for irrigation and farm gates -- so
//	re-expressing it over stored bits would fork a live predicate into two implementations that drift, which is the
//	disease [DEC-single-implementation] exists to prevent. Deriving a bit by calling the ONE accessor is what that
//	rule asks for; what contexts.md bans is re-deriving the WHOLE BLOCK, and per-bit routing is exactly what stops
//	that. ⚠ Its neighbour leg is therefore the one walk left in this file, and it is now paid only when a fact
//	that actually feeds it arrives at that plot -- never per read, and never on an unrelated substrate fact.
//
//	NEVER SERIALIZED (DEC-derived-never-trusted): the bitset is rebuilt from the save read's own in-read DOMAIN
//	emits (DEC-spine-reseed), never trusted from a save.
//

#include "CvCondition.h"   // CASC_PRED_* -- the ONE predicate vocabulary the stored bitset keys on
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES -- the realized-yield group forward's out-array extent

class CvPlot;
struct CvSpineEvent;

//	The SUBSTRATE AXES a stored bit can depend on -- the vocabulary the per-bit table declares its dependencies in.
//	One axis per substrate fact PAIR (the direction is the fact's own id, never a payload), so a bit names WHAT it
//	reads rather than which events happen to exist.
enum PlotContextAxis
{
	PLOTAXIS_TYPE       = 1 << 0,   // land / water / hills / peak (CvPlot::setPlotType -- isWater() IS the plot type)
	PLOTAXIS_TERRAIN    = 1 << 1,
	PLOTAXIS_FEATURE    = 1 << 2,
	PLOTAXIS_RIVER      = 1 << 3,
	PLOTAXIS_IRRIGATION = 1 << 4,
	PLOTAXIS_LANDMARK   = 1 << 5,
	PLOTAXIS_OWNER      = 1 << 6,
	PLOTAXIS_WORKED     = 1 << 7,
	PLOTAXIS_CITY       = 1 << 8,   // a city sat down on / left the plot (isFreshWater reads the plot city's access)
	PLOTAXIS_IMPROVEMENT = 1 << 9,  // the SERVED-RESOURCE verdict's second leg -- an improvement that TRADES the bonus
	PLOTAXIS_BONUS      = 1 << 10   // ...and its first: which resource the tile carries at all
};

class PlotContext
{
public:
	PlotContext() : m_plot(NULL), m_attributeBits(0), m_servedBonus(-1) {}
	void bind(const CvPlot* plot) { m_plot = plot; }   // set once by the owning CvPlot; the pointer IS the owner (never dangles)
	// ZEROED at owner reset. The verdict bits are a DELTA store -- each is SET by the fact that names it, never
	// re-derived wholesale -- so they are correct only from a known zero ([DEC-keyed-accumulator]). A plot object is
	// reused across a regen/load, and a bit no later fact happens to touch would otherwise survive from the last world.
	// ⚠ The served bonus resets to -1, NOT 0: 0 is a REAL bonus id, so zeroing it would hand a recycled plot the
	// first resource in the registry.
	void clear() { m_attributeBits = 0; m_servedBonus = -1; }

	// --- STORED: the CASC_PRED_* verdict bitset (both blocks) ---------------------------------------------------
	// The raw mask, for a reader that folds every set bit at once (CityContext::onPlotChanged is the one such
	// reader: a city's plotAttrs is literally the sum of its member plots' bits, so the two granularities of the
	// same vocabulary cannot drift).
	unsigned int attributeBits() const { return m_attributeBits; }
	bool hasAttribute(int predicateId) const { return (m_attributeBits & bitFor(predicateId)) != 0; }

	// --- STORED: the SERVED-RESOURCE verdict ----------------------------------------------------------------------
	// ⚖ WHICH resource this tile makes available ON SITE, or -1. A tile serves its bonus once an improvement that
	// TRADES that bonus stands on it -- which is the whole of the json par.3.4 `onSite` MAP half, and is deliberately
	// NOT the same question as any ownership tier: `owned` is raw presence on an owned tile, improved or not.
	// ⛔ It is an ID and not a bit, so it does not ride the CASC_PRED_* table: a plot carries at most one bonus, so
	// the answer is a single id and its crossing is the ordinary ADDED / REMOVED pair.
	// ⚠ NOT gated on the tile being WORKED -- a fort cannot be worked by definition and a fort is exactly how a
	// resource gets served (owner). The city-side OWNERSHIP half is the CITY's: it asks whether the tile is its
	// owner's, which no per-plot verdict can answer for every asker.
	int servedBonus() const { return m_servedBonus; }

	// --- THE MAINTENANCE: reached ONLY through this store's own spine consumer -----------------------------------
	// ⚖ THE DECLARED INTEREST SET -- the facts that maintain this store, stated at the store. A fact absent from
	// this list does not reach it, and that is READABLE HERE rather than inferable from a router.
	static bool wantsEvent(int iEventId);
	static void onSpineEvent(const CvSpineEvent& kEvent);

	// ⛔ CONSTRAINT: the appliers below have NO entry point other than this store's own spine consumer. They are
	// public only because the consumer dispatches through them; a direct call from a mutation choke point beside
	// the event would be a second surface maintaining the same state.
	// Re-derive exactly the bits that READ the given axes -- never the whole block. Announces each crossing.
	void applyAxes(int iAxisMask) const;
	// Re-derive the ADJACENCY block alone: this plot's own neighbour-reading verdicts. Driven by a NEIGHBOUR's
	// land/water crossing or terrain move, which is the only thing that can change it without touching this plot.
	void applyAdjacency() const;

	// The axes whose movement a NEIGHBOUR's adjacency verdict can read -- the one-hop fan-out gate. Exact: a
	// neighbour's HAS_COAST reads my IS_WATER, and its HAS_FRESHWATER reads my water+fresh-terrain state, so TYPE
	// and TERRAIN are the whole of it.
	// ⛔ The fan-out is driven by these AXES and never by an adjacency bit's own crossing, which is what bounds it
	// to one hop: an adjacency verdict is read by nobody's adjacency verdict, so it cannot propagate further.
	static int neighbourVisibleAxes() { return PLOTAXIS_TYPE | PLOTAXIS_TERRAIN; }

	// --- STORED READS: the json par.3.5 bare HAS_/IS_ plot predicates -- O(1) bit tests over the block above -----
	bool isWater() const        { return hasAttribute(CASC_PRED_IS_WATER); }        // IS_WATER
	bool isLand() const         { return hasAttribute(CASC_PRED_IS_LAND); }         // IS_LAND (!water)
	bool isFlatlands() const    { return hasAttribute(CASC_PRED_IS_FLATLANDS); }    // IS_FLATLANDS (no relief: neither hills nor peak)
	bool hasHills() const       { return hasAttribute(CASC_PRED_HAS_HILLS); }       // HAS_HILLS
	bool hasPeak() const        { return hasAttribute(CASC_PRED_HAS_PEAK); }        // HAS_PEAK
	bool hasCoast() const       { return hasAttribute(CASC_PRED_HAS_COAST); }       // HAS_COAST (on the land/water boundary, either side; the city-radius minArea form is city-scoped)
	bool hasRiver() const       { return hasAttribute(CASC_PRED_HAS_RIVER); }       // HAS_RIVER
	bool hasFreshWater() const  { return hasAttribute(CASC_PRED_HAS_FRESHWATER); }  // HAS_FRESHWATER (fresh water or river; adjacency leg)
	bool hasIrrigation() const  { return hasAttribute(CASC_PRED_HAS_IRRIGATION); }  // HAS_IRRIGATION
	bool hasLandmark() const    { return hasAttribute(CASC_PRED_HAS_LANDMARK); }    // HAS_LANDMARK (getLandmarkType != NO_LANDMARK)
	bool hasFeatureAny() const  { return hasAttribute(CASC_PRED_HAS_FEATURE); }     // HAS_FEATURE (any feature)
	bool isOwned() const        { return hasAttribute(CASC_PRED_IS_OWNED); }        // IS_OWNED (the plot lies in owned territory)
	bool isWorked() const       { return hasAttribute(CASC_PRED_IS_WORKED); }       // IS_WORKED (a citizen works it this turn)

	// --- FORWARDED: the RAW substrate CvPlot already holds O(1) -- a parameterized predicate keys on the id, so a
	// stored copy would duplicate raw data. Defined out-of-line (PlotContext.cpp) because CvPlot.h includes this
	// header (only a fwd-decl of CvPlot is available here). ------------------------------------------------------
	bool hasFeature(int eFeature) const;         // {HAS_FEATURE: F}
	bool hasTerrain(int eTerrain) const;         // {HAS_TERRAIN: T}
	// The plot's raw TERRAIN id -- forwarded, not stored (the object owns it O(1)). Read by the fold-target
	// index: a generalized plot predicate (IS_WATER, HAS_PEAK) resolves against the terrains its foldTargets
	// info names, because the plot-TYPE axis those predicates used to read announces for only part of the map
	// (json.md par.3.5 -- we never fold onto a boolean).
	int terrainId() const;
	bool hasImprovement(int eImprovement) const; // {HAS_IMPROVEMENT: I}
	bool hasRoute(int eRoute) const;             // the plot carries this route (the route-prereq vicinity scan)
	bool hasBonus(int eBonus, int eTeam) const;  // {HAS_BONUS: B} (a plot bonus is revealed per team, so it cannot be one stored verdict)
	// The city-PRESENCE read stays FORWARDED -- CvPlot answers it O(1), so the STORES-vs-FORWARDS split keeps it
	// here rather than as a stored bit. Its mutation announces SEVT_PLOT_CITY_ADDED / _REMOVED, so a reader that
	// must REACT to a city appearing on the plot has a fact to hang on.
	bool isCity() const;                         // the plot holds a city
	int  owner() const;                          // CvPlot::getOwner (the vicinity scans' owned-plot test; NO_PLAYER = unowned)
	int  latitude() const;                       // CvPlot::getLatitude (the latitude band predicate)
	// The plot's own PRE-improvement (substrate) yield, read as a SEGMENT of its package -- terrain + feature +
	// bonus, before the route, the improvement or the owner's sources. ⚠ Takes no team: a plot resolves in
	// isolation (modifier.md §2), so its substrate carries ONE value.
	int  natureYield(int eYield) const;
	// The plot's CURRENT REALIZED YIELDS -- CvPlot::getYields, the plot's own O(1) group read, handed on
	// unchanged: the WHOLE isolated per-plot base package (substrate + improvement + route + the keyed/plots
	// flats), which is what the city sums into its rate BASE (modifier.md §2 plot-as-base). The plot-scope twin
	// of CityContext::yields, forwarded for the same reason and never stored. ⚠ Distinct from natureYield above,
	// which is the PRE-improvement leg only and COMPUTES on every call; this one is a bare cache fetch.
	// ×100 native, indexed by YieldTypes.
	void yields(int (&realizedYields)[NUM_YIELD_TYPES]) const;

private:
	// One stored bit per CASC_PRED_* id. CONSTRAINT: every stored predicate id must be < 32 (the mask width); the
	// guard below fails the build if the vocabulary grows past it, at which point the mask widens rather than
	// silently truncating.
	typedef char PlotContextStoredBitsFitTheMask[(CASC_PRED_IS_OWNED < 32) ? 1 : -1];

	static unsigned int bitFor(int predicateId)
	{
		return (predicateId >= 0 && predicateId < 32) ? (1u << predicateId) : 0u;
	}

	// THE ONE WRITE POINT, so the crossing is announced exactly once and from exactly one place. Commits the bit
	// and, only on a genuine 0 <-> 1 crossing, emits SEVT_PLOT_PREDICATE_ADDED / _REMOVED -- the fact that carries
	// the bit UP to the city ([contexts.md]: the plot sends it up, the city never reaches down).
	void setPredicate(int predicateId, bool bHeld) const;
	// The served-resource twin of setPredicate, and the same contract: commits the id and announces the CROSSING
	// only. A tile MOVING from one served resource to another announces both halves (the old at REMOVED, the new at
	// ADDED), so a counting consumer's withdrawal is exact rather than something it has to reconstruct.
	void setServedBonus(int iBonus) const;

	const CvPlot* m_plot;              // the bound game object; the derivation reads it -- never a value copy
	mutable unsigned int m_attributeBits;   // the stored verdicts; derived state, so NEVER serialized
	mutable int m_servedBonus;              // the resource this tile serves on site, or -1; derived, never serialized
};

void plotContextRegisterConsumer();   // register on the event spine (from spineRegisterConsumers; idempotent)

#endif // CV_PLOT_CONTEXT_H
