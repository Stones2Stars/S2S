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
};

#endif // CV_CASCADE_BUILDING_CASCADE_H
