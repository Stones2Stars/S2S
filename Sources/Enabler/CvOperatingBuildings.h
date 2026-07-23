#pragma once
#ifndef CV_CASCADE_OPERATING_BUILDINGS_H
#define CV_CASCADE_OPERATING_BUILDINGS_H

//
//	OperatingBuildings -- the per-city CASCADE-COMPUTED operating buildings as a STANDING derived cache: the ACTIVE
//	(non-dormant) building set + the in-vicinity provided bonuses (json.md §5a), at the operate/provides LEAST
//	fixpoint (EnablerKernel::recomputeOperatingBuildingsInto). This replaces the turn-scoped operating buildings memo -- the operating buildings are
//	now EVENT-INVALIDATED (building/religion/corporation flips mark dirty; player-level events ride the shared
//	accumulator epoch; the turn roll is the self-heal cadence for the unhooked classes), so the old memo's
//	"shadow-phase-only, must be event-invalidated before any consumer cut" caveat is CLOSED, not carried.
//
//	STATE HOME: a mutable CvCity member (`m_operatingBuildings`), CvDerivedCacheSet-driven -- the exact idiom of the
//	rate slots (m_cascadeRateSlots / state-repositories.md). Never serialized; all-dirty from birth, so a loaded
//	game recomputes from current state; warmed transitively by the load-end slots warm-up (the slot refresh
//	consumes the operating buildings). Query surface: EnablerKernel::operatingBuildings / wireOperatingBuildings.
//

#include "Infrastructure/CvDerivedCache.h"
#include <set>
#include <map>

class CvCity;

struct OperatingBuildings
{
	std::set<int> active;      // the cascade-computed ACTIVE (present ∧ non-dormant ∧ non-obsolete) set (fixpoint w/ provides)
	std::set<int> obsolete;    // PRESENT ∧ obsoleted-by-held-tech: the modifier reads its `whenObsolete` tree in place of the
	                           // normal families (json §4.2), and it provides nothing -- a THIRD outcome of the SAME
	                           // obsoletion process (recompute/ripple) that computes `active`; read via cascadeIsBuildingObsolete
	std::set<int> provided;    // the in-vicinity provided bonuses (json §5a) at the same fixpoint
	std::map<int, int> providedCount;   // per-bonus ACTIVE-provider ref-count (provided == its keyset>0); the
	                                    // targeted-ripple bookkeeping so removing one provider only un-provides at 0
	// ONE freshness philosophy (scope-packages.md): events mark the Set directly (building/religion/corp
	// flips; tech/civic/GA via markPlayerScopeAndCities; the slice boundary is the self-heal) -- no stamps.
	CvDerivedCacheSet<CvCity> set;   // the dirty protocol (bind in CvCity's ctor); noncopyable via this member
};

#endif // CV_CASCADE_OPERATING_BUILDINGS_H
