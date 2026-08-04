#pragma once
#ifndef CV_PLOT_CONTEXT_H
#define CV_PLOT_CONTEXT_H

//
//	PlotContext -- the per-PLOT live-state STORE, the plot-scope sibling of CityContext / EmpireContext (same rules,
//	kept symmetric so a reader always knows where to go: plot state here, city state on CityContext, empire state on
//	EmpireContext). Bound to its CvPlot by pointer (never a value copy -- passing a bound reference is far cheaper
//	than snapshotting values, owner).
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
//	THE TWO STORED BLOCKS -- both stored; they differ only in how WIDE the trigger that re-derives them is:
//	 - OWN-PLOT facts:  IS_WATER / IS_LAND / IS_FLATLANDS / HAS_HILLS / HAS_PEAK / HAS_RIVER / HAS_IRRIGATION /
//	                    HAS_FEATURE / HAS_LANDMARK / IS_OWNED / IS_WORKED -- derived from THIS plot alone.
//	 - ADJACENCY facts: HAS_COAST (CvPlot::isCoastalLand scans the 8 neighbours) and HAS_FRESHWATER
//	                    (CvPlot::isFreshWater carries a rect(1,1) leg over the neighbours' water+fresh-terrain
//	                    state) -- derived from this plot AND its neighbours. Because a neighbour's adjacency verdict
//	                    reads only facts that live in THIS plot's block, a change to any bit of this plot's block
//	                    re-derives the 8 neighbours' adjacency block and NOTHING further: the fan-out is exactly one
//	                    hop, bounded and event-driven (Engine/ContextConsumer owns the fan-out and the triggers).
//
//	MAINTENANCE IS EXTERNAL AND EVENT-DRIVEN -- a read NEVER recomputes. The refresh entry points are const with the
//	bitset `mutable` (the CvDerivedCache shape) so the maintainer can drive them through the bound plot's const
//	accessor without a second, mutable path onto the plot.
//
//	NEVER SERIALIZED (DEC-derived-never-trusted): the bitset is rebuilt from the save read's own in-read DOMAIN
//	emits (DEC-spine-reseed), never trusted from a save.
//

#include "CvCondition.h"   // CASC_PRED_* -- the ONE predicate vocabulary the stored bitset keys on
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES -- the realized-yield group forward's out-array extent

class CvPlot;

class PlotContext
{
public:
	PlotContext() : m_plot(NULL), m_attributeBits(0) {}
	void bind(const CvPlot* plot) { m_plot = plot; }   // set once by the owning CvPlot; the pointer IS the owner (never dangles)
	// ZEROED at owner reset. The verdict bits are a DELTA store -- each is SET by the fact that names it, never
	// re-derived -- so they are correct only from a known zero ([DEC-keyed-accumulator]). A plot object is reused
	// across a regen/load, and a bit no later fact happens to touch would otherwise survive from the last world.
	void clear() { m_attributeBits = 0; }

	// --- STORED: the CASC_PRED_* verdict bitset (both blocks) ---------------------------------------------------
	// The raw mask, for a reader that folds every set bit at once (CityContext::onPlotChanged is the one such
	// reader: a city's plotAttrs is literally the sum of its member plots' bits, so the two granularities of the
	// same vocabulary cannot drift).
	unsigned int attributeBits() const { return m_attributeBits; }
	bool hasAttribute(int predicateId) const { return (m_attributeBits & bitFor(predicateId)) != 0; }

	// --- MAINTENANCE: called ONLY by the contexts' spine consumer -------------------------------------------------
	// Each re-derives its block from the bound plot through the SAME CvPlot accessor a read used to call, ONCE per
	// change instead of once per read. The maintainer takes the fan-out decision from the attributeBits() DELTA
	// across the pair (below), so neither reports movement itself.
	void refreshOwnFacts() const;
	void refreshAdjacencyFacts() const;

	// The stored bits whose movement can change a NEIGHBOUR's adjacency verdict -- the maintainer's one-hop
	// fan-out gate, applied to the attributeBits() delta.
	// CONSTRAINT: IS_WORKED is deliberately EXCLUDED. It is city-membership state that no neighbour's HAS_COAST /
	// HAS_FRESHWATER verdict reads, and it flips at CITIZEN-REASSIGNMENT cadence -- fanning out on it would put
	// an 8-neighbour rescan on the hottest trigger in the block, for a verdict that provably cannot move.
	static unsigned int fanOutTriggerMask();

	// --- STORED READS: the json par.3.5 bare HAS_/IS_ plot predicates -- O(1) bit tests over the block above -----
	bool isWater() const        { return hasAttribute(CASC_PRED_IS_WATER); }        // IS_WATER
	bool isLand() const         { return hasAttribute(CASC_PRED_IS_LAND); }         // IS_LAND (!water)
	bool isFlatlands() const    { return hasAttribute(CASC_PRED_IS_FLATLANDS); }    // IS_FLATLANDS (no relief: neither hills nor peak)
	bool hasHills() const       { return hasAttribute(CASC_PRED_HAS_HILLS); }       // HAS_HILLS
	bool hasPeak() const        { return hasAttribute(CASC_PRED_HAS_PEAK); }        // HAS_PEAK
	bool hasCoast() const       { return hasAttribute(CASC_PRED_HAS_COAST); }       // HAS_COAST (adjacency; the city-radius minArea form is city-scoped)
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
	bool hasImprovement(int eImprovement) const; // {HAS_IMPROVEMENT: I}
	bool hasRoute(int eRoute) const;             // the plot carries this route (the route-prereq vicinity scan)
	bool hasBonus(int eBonus, int eTeam) const;  // {HAS_BONUS: B} (a plot bonus is revealed per team, so it cannot be one stored verdict)
	// The city-PRESENCE read stays FORWARDED -- CvPlot answers it O(1), so the STORES-vs-FORWARDS split keeps it
	// here rather than as a stored bit. Its mutation now announces SEVT_PLOT_CITY_CHANGED (CvPlot::setPlotCity),
	// so a reader that must REACT to a city appearing on the plot has a fact to hang on.
	bool isCity() const;                         // the plot holds a city
	int  owner() const;                          // CvPlot::getOwner (the vicinity scans' owned-plot test; NO_PLAYER = unowned)
	int  latitude() const;                       // CvPlot::getLatitude (the latitude band predicate)
	// The plot's own PRE-improvement (substrate) yield, read as a SEGMENT of its package -- terrain + feature +
	// bonus, before the route, the improvement or the owner's sources. ⚠ Takes no team: a plot resolves in
	// isolation (modifier.md §2), so its substrate carries ONE value.
	int  natureYield(int eYield) const;             // the plot's own PRE-improvement
	                                             // nature yield of a channel (per-channel AND per-team, so not a verdict bit)
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
	static unsigned int ownFactsMask();
	static unsigned int adjacencyFactsMask();

	const CvPlot* m_plot;              // the bound game object; the derivation reads it -- never a value copy
	mutable unsigned int m_attributeBits;   // the stored verdicts; derived state, so NEVER serialized
};

#endif // CV_PLOT_CONTEXT_H
