#pragma once
#ifndef CV_CASCADE_UNIT_CASCADE_H
#define CV_CASCADE_UNIT_CASCADE_H

//
//	UnitCascade -- StoneBase CalculateTrainableUnits.cs: the city's TRAINABLE set (the engine canTrain TRUE-set),
//	GENERATE-then-GATE over the whole-domain frontier. Units REUSE the building machinery (the shared AugmentState
//	waiver on BuildingCascade) -- only the inputs differ. See patterns.md (the single-source law) +
//	docs/plans/structural-cleanup/cascade-engine-430.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include <map>
#include <set>

class CvInfo;
class CvPlayer;
class CvTeam;
class CvCity;

class UnitCascade
{
public:
	// Unit instance cap (StoneBase UnitCascade.Capped): WORLD = lifetime-created + making >= allowed.world;
	// EMPIRE = live count + making >= ERA-SCALED allowed.empire (waived by NO_NATIONAL_UNIT_LIMIT unless unlimitedException).
	static bool capped(const CvInfo* j, int eU, const CvPlayer& kPlayer, bool noNationalLimit);

	// The city's TRAINABLE set (the engine canTrain TRUE-set), GENERATE-then-GATE. Built on the ONE per-unit
	// availability/trainable primitive pair (file-static uc_isAvailable/uc_isTrainable, incl. the uc_reachable
	// upgrade-reachability closure) shared with the targeted box re-checks -- the bc_isBuildable idiom.
	// bVisible=true yields the VISIBLE (build-list) frontier -- greyable clauses relaxed (connectable resource /
	// unadopted civic -> GREYED, enabler.md §6); the strict (bVisible=false) trainable-now set is unchanged.
	static void trainable(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& result, bool bVisible = false);

	// ===== #430 the isolated-box TARGETED updates (enabler-frontier-perf.md; the UNIT analogue of the proven
	// BuildingCascade incremental path). The trainable box is maintained IN PLACE off targeted re-checks instead
	// of the every-turn full re-walk; a box with a full rebuild pending (CPK_FRONT_U dirty) is skipped, and the
	// per-slice rebuild remains the correctness net. =====

	// Part A: build the unit reverse index (HAVE-atom -> {unit ids that reference it in requires.build}) at the
	// load-end warm-up. Idempotent (guarded flag). Over-inclusive by design; a missed class self-heals at the slice.
	static void buildIndices();

	// Part C: a city-local HAVE atom (pop / religion / corporation / power) flipped -> re-check ONLY the units
	// that reference it (the matching s_uc* bucket) in this city's trainable box, in place. eHaveKind is a
	// CascadeAccumulator::CascadeHaveKind.
	static void recheckHave(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, int eHaveKind);

	// Part B: a unit's EMPIRE count changed (trained / lost) -> unit caps are empire/world-scoped, so re-check the
	// unit + its dependents across EVERY one of the player's cities (skipping any with a rebuild pending). A no-op
	// for a unit with no `allowed` cap that no other unit references (the common case -- most combat units).
	static void onUnitChanged(const CvPlayer& kPlayer, int eUnit);

	// Part B: a building changed in pCity -> re-check ONLY the units the building's provides.bonuses (vicinity
	// supply) / enables.units / building-prereq reference (via the unit reverse index), in place. Replaces the
	// blanket CPK_FRONT_U dirty that buildingProcessed used to fire.
	static void onBuildingChangedUnits(const CvCity* pCity, int eBuilding);
};

#endif // CV_CASCADE_UNIT_CASCADE_H
