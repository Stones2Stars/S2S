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

class CvJsonInfo;
class CvPlayer;
class CvTeam;
class CvCity;

class UnitCascade
{
public:
	// Unit instance cap (StoneBase UnitCascade.Capped): WORLD = lifetime-created + making >= allowed.world;
	// EMPIRE = live count + making >= ERA-SCALED allowed.empire (waived by NO_NATIONAL_UNIT_LIMIT unless unlimitedException).
	static bool capped(const CvJsonInfo* j, int eU, const CvPlayer& kPlayer, bool noNationalLimit);

	// reachable(v) (StoneBase UnitCascade.Reachable): v is itself available OR some DIRECT upgrade of v is reachable.
	static bool reachable(int v, const std::set<int>& available, std::map<int, bool>& cache, std::set<int>& inProgress);

	// The city's TRAINABLE set (the engine canTrain TRUE-set), GENERATE-then-GATE.
	static void trainable(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& result);
};

#endif // CV_CASCADE_UNIT_CASCADE_H
