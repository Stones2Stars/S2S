#pragma once
#ifndef CV_CASCADE_CITY_FACTS_H
#define CV_CASCADE_CITY_FACTS_H

//
//	CascadeCityFacts -- the per-city CASCADE-COMPUTED building facts as a STANDING derived cache: the ACTIVE
//	(non-dormant) building set + the in-vicinity provided bonuses (json.md §5a), at the operate/provides LEAST
//	fixpoint (EnablerKernel::recomputeCityFactsInto). This replaces the turn-scoped facts memo -- the facts are
//	now EVENT-INVALIDATED (building/religion/corporation flips mark dirty; player-level events ride the shared
//	accumulator epoch; the turn roll is the self-heal cadence for the unhooked classes), so the old memo's
//	"shadow-phase-only, must be event-invalidated before any consumer cut" caveat is CLOSED, not carried.
//
//	STATE HOME: a mutable CvCity member (`m_cascadeFacts`), CvDerivedCacheSet-driven -- the exact idiom of the
//	rate slots (m_cascadeRateSlots / state-repositories.md). Never serialized; all-dirty from birth, so a loaded
//	game recomputes from current state; warmed transitively by the load-end slots warm-up (the slot refresh
//	consumes the facts). Query surface: EnablerKernel::cityFacts / wireFacts.
//

#include "Infrastructure/CvDerivedCache.h"
#include <set>

class CvCity;

struct CascadeCityFacts
{
	std::set<int> active;      // the cascade-computed ACTIVE (non-dormant) building set (fixpoint w/ provides)
	std::set<int> provided;    // the in-vicinity provided bonuses (json §5a) at the same fixpoint
	// ONE freshness philosophy (scope-packages.md): events mark the Set directly (building/religion/corp
	// flips; tech/civic/GA via markPlayerScopeAndCities; the slice boundary is the self-heal) -- no stamps.
	CvDerivedCacheSet<CvCity> set;   // the dirty protocol (bind in CvCity's ctor); noncopyable via this member
};

#endif // CV_CASCADE_CITY_FACTS_H
