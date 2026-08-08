#pragma once
#ifndef CV_CASCADE_OPERATING_BUILDINGS_H
#define CV_CASCADE_OPERATING_BUILDINGS_H

//
//	OperatingBuildings -- the per-city CASCADE-COMPUTED operating buildings: the ACTIVE (non-dormant) building
//	set + the in-vicinity provided bonuses (json.md §5a), at the operate/provides LEAST fixpoint
//	(EnablerKernel::recomputeOperatingBuildingsInto).
//
//	⛔ THIS IS NOT AN INPUT/OUTPUT CACHE AND CARRIES NO DIRTY PROTOCOL (state-repositories.md: "the ENABLER's
//	sets are themselves derived state, but maintained by TARGETED PROPAGATION"). It is computed ONCE
//	(EnablerKernel::seedOperatingBuildings -- city creation and the load seed) and thereafter each HAVE-change
//	ripples through the AFFECTED SUBSET ONLY, updating this dataset IN PLACE via the operate reverse-index (the
//	on*Active hooks; enabler.md §7). It is never blanket-invalidated-and-recomputed -- running the fixpoint as a
//	dirty/recompute cache is "burning down the library of Alexandria" (DESPAIR_INDEX #2) -- and never a parallel
//	shadow-delta. Reads are BARE FETCHES: a propagation that fails to fire leaves the set visibly wrong, which is
//	how the missing hook is found ([DEC-no-self-heal]). It is found by an EXTERNAL reader diffing the served
//	set (built once by the load seed, which
//	fills a caller-owned buffer) -- the DLL neither compares nor reports.
//
//	STATE HOME: a mutable CvCity member (`m_operatingBuildings`). Never serialized -- empty from birth, so a
//	loaded game is populated by the seed rather than from the save. Query surface:
//	EnablerKernel::operatingBuildings / wireOperatingBuildings.
//

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
};

#endif // CV_CASCADE_OPERATING_BUILDINGS_H
