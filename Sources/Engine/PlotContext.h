#pragma once
#ifndef CV_PLOT_CONTEXT_H
#define CV_PLOT_CONTEXT_H

//
//	PlotContext -- the per-PLOT READ SURFACE, the plot-scope sibling of CityContext / EmpireContext (same rules, kept
//	symmetric so a reader always knows where to go: plot state here, city state on CityContext, empire state on
//	EmpireContext). Bound to its CvPlot by pointer (never a value copy -- passing a bound reference is far cheaper
//	than snapshotting values, owner).
//
//	⛔ It STORES nothing yet: a plot's facts are all already O(1) on CvPlot, so every read FORWARDS through the bound
//	pointer, never duplicated (the store-only-the-unique-aggregate rule -- a plot has no aggregate lacking a CvPlot
//	home today, so the ContextDict slot CityContext/EmpireContext carry has no plot analog; it is defined WHEN one
//	appears). The forwards delegate to the SAME CvPlot accessors the one condition evaluator reads
//	(CvConditionEval.cpp), so each HAS_/IS_ plot fact has a single source (DEC-single-implementation).
//

class CvPlot;

class PlotContext
{
public:
	PlotContext() : m_plot(NULL) {}
	void bind(const CvPlot* p) { m_plot = p; }   // set once by the owning CvPlot; the pointer IS the owner (never dangles)

	// --- FORWARDED: the json §3.5 HAS_/IS_ plot predicates -- read through the bound CvPlot, no stored copy. Defined
	// out-of-line (PlotContext.cpp) because CvPlot.h includes this header (only a fwd-decl of CvPlot is available here). ---
	bool isWater() const;                        // IS_WATER
	bool isLand() const;                         // IS_LAND (!water)
	bool isFlatlands() const;                    // IS_FLATLANDS (no relief: neither hills nor peak)
	bool hasHills() const;                       // HAS_HILLS
	bool hasPeak() const;                        // HAS_PEAK
	bool hasCoast() const;                       // HAS_COAST (coastal-adjacent land; the city-radius minArea form is city-scoped)
	bool hasRiver() const;                       // HAS_RIVER
	bool hasFreshWater() const;                  // HAS_FRESHWATER (fresh water or river)
	bool hasIrrigation() const;                  // HAS_IRRIGATION
	bool hasLandmark() const;                    // HAS_LANDMARK (getLandmarkType != NO_LANDMARK)
	bool hasFeatureAny() const;                  // HAS_FEATURE (any feature)
	bool hasFeature(int eFeature) const;         // {HAS_FEATURE: F}
	bool hasTerrain(int eTerrain) const;         // {HAS_TERRAIN: T}
	bool hasImprovement(int eImprovement) const; // {HAS_IMPROVEMENT: I}
	bool hasBonus(int eBonus, int eTeam) const;  // {HAS_BONUS: B} (a plot bonus is revealed per team)
	bool isWorked() const;                       // IS_WORKED (a citizen works it this turn)
	bool isCity() const;                         // the plot holds a city

private:
	const CvPlot* m_plot;   // the bound game object; forwarding accessors read it -- never a value copy
};

#endif // CV_PLOT_CONTEXT_H
