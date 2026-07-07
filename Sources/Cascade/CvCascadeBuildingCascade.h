#pragma once
#ifndef CV_CASCADE_BUILDING_CASCADE_H
#define CV_CASCADE_BUILDING_CASCADE_H

//
//	BuildingCascade -- StoneBase CalculateBuildableBuildings.cs: the city's BUILDABLE set (the engine canConstruct
//	TRUE-set), computed IN ISOLATION over the whole-domain frontier (ALL buildings; the engine has NO enables-frontier).
//	Also owns the shared AugmentState prereq-WAIVER set (BuildingCascade.AugmentState), which the unit cascade reuses.
//	See patterns.md (the single-source law) + docs/plans/structural-cleanup/cascade-engine-430.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include <set>

class CvJsonInfo;
class CvPlayer;
class CvTeam;
class CvCity;

class BuildingCascade
{
public:
	// AugmentState's prereq-WAIVER set (ObsoleteBuildings ∪ PrereqWaivedBuildings). Shared by the building + unit
	// cascades (both gate requires.build through the SAME evaluator).
	static void augmentWaived(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& waived);

	// Instance cap (StoneBase Capped): current tally count + in-production making >= allowed, at some scope.
	static bool capped(const CvJsonInfo* j, int eB, const CvPlayer& kPlayer);

	// ScaledPrereq (StoneBase BuildingCascade.ScaledPrereq, VERBATIM): the world-size-scaled required count of a
	// PrereqNumOfBuildings prereq.
	static int scaledPrereq(int baseN, int wsMod, bool selfLimited, bool prereqLimited, bool selfNoScale, int selfCount);

	// The city's BUILDABLE set (the engine canConstruct TRUE-set), computed IN ISOLATION.
	static void buildable(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& avail);

	// #430 the isolated-box TARGETED update (owner 2026-07-05, "per-turn frontier cache you can remove a building
	// from; NEVER rebuilt on building-completed; rebuilt only on mid-turn tech etc."). A building changed (built or
	// lost) in pCity -> REMOVE/re-add just it + re-check its dependents (buildings whose requires reference it),
	// in place, no full rebuild. No-op if the box is dirty (a rebuild is already pending). The reverse index that
	// makes the dependent-set cheap is built lazily on first use.
	static void onBuildingChanged(const CvCity* pCity, int eBuilding);

	// Part A (enabler-frontier-perf.md): build the reverse index at the load-end warm-up instead of lazily on the
	// first onBuildingChanged. Idempotent (guarded flag), so a load-end call is safe.
	static void buildIndices();

	// Part C: a city-local HAVE atom (pop / religion / corporation / power) flipped -> re-check ONLY the buildings
	// that reference it (the matching s_bc* bucket) in this city's buildable box, in place. eHaveKind is a
	// CascadeAccumulator::CascadeHaveKind. Skips a box with a full rebuild already pending.
	static void recheckHave(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, int eHaveKind);
};

#endif // CV_CASCADE_BUILDING_CASCADE_H
