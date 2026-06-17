#pragma once
#ifndef CV_SCOPED_ACCUMULATOR_H
#define CV_SCOPED_ACCUMULATOR_H

#include <map>

//
//	CvScopedAccumulator -- the shared SUBSTRATE primitive of the #430 cascade.
//	Design: docs/dev/plans/cascade-engine-430.md (section 1.0, "the scope spine + an additive
//	accumulator") + tally-cascade-spec.md.
//
//	NAME -- read it as "an accumulator SCOPED TO (owned by) a scope", NOT "an accumulator OF scopes".
//	It does not accumulate scopes. The primitive itself is scope-AGNOSTIC: a keyed additive sum. The
//	"scoped" part is its ROLE -- the tally/modifier host ONE instance per scope object (city, player,
//	team, game), and those roll up the scope spine. (Earlier name CvScopeAccumulator was dropped for
//	exactly this ambiguity -- owner 2026-06-17.)
//
//	WHAT IT IS: an ADDITIVE ACCUMULATOR. deposit(key, delta) folds a value into key's running sum;
//	get(key) is the summed read. ONE primitive, instantiated per machine for what it sums:
//	  - the TALLY    sums presence-COUNTS (one of each Type "had" here)            -- built first
//	  - the MODIFIER will sum effect-MAGNITUDES (flat/percent/multiplier deposits) -- later
//	Keys are Type indices WITHIN ONE domain (buildings, units, ...). The Type-namespace prefix routes
//	which accumulator a count goes to (tally-spec section 5), so compose one accumulator per domain
//	rather than colliding domains in one map.
//
//	---------------------------------------------------------------------------------------------
//	DELIBERATELY FRESH -- and explicitly NOT the derived-data repository (owner 2026-06-17).
//	We have iterated the derived-data repository structure (CvDerivedData.h: TLazy / TDependency /
//	version+dirty+bounded-staleness, v1-on-AI-subclasses -> v2-on-base-objects). This accumulator
//	must NOT be poisoned by any of that prior tinkering:
//	  - it is NOT a repository tenant and does NOT use TLazy / TDependency / version / dirty;
//	  - the repository is ADVISORY-only (never synced, never gates control flow, stale-tolerant,
//	    lazily recomputed); this accumulator is AUTHORITATIVE -- the enabler reads it to gate what is
//	    buildable (`allowed` caps, `requires` count-thresholds), so it is a plain, exact additive
//	    aggregate with no lazy/versioned recompute layer.
//	The repository skeleton stays physically in place during shadow (its init()/reset() wiring is
//	live; removal is a deferred cutover step) -- we just never build on it.
//
//	C++03 / VC7.1 only (no C++11). Header-light: depends on <map> alone, no engine/XML headers, so it
//	stays a pure primitive the three machines build on.
//
class CvScopedAccumulator
{
public:
	// Fold iDelta into iKey's running sum. A sum that returns to 0 drops the key (stays sparse).
	void deposit(int iKey, int iDelta);

	// The summed read for iKey; 0 if nothing was ever deposited (the absence == zero invariant).
	int get(int iKey) const;

	// Drop all sums (e.g. before a full recompute).
	void clear();

	bool empty() const { return m_sums.empty(); }
	int distinctKeys() const { return (int)m_sums.size(); }

	// Iteration -- for the additive roll-up (city -> empire -> team -> world) and the shadow dump.
	typedef std::map<int, int>::const_iterator const_iterator;
	const_iterator begin() const { return m_sums.begin(); }
	const_iterator end() const { return m_sums.end(); }

private:
	std::map<int, int> m_sums; // sparse: Type-index (within a domain) -> summed value
};

#endif // CV_SCOPED_ACCUMULATOR_H
