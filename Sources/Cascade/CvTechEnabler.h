#pragma once
#ifndef CV_TECH_ENABLER_H
#define CV_TECH_ENABLER_H

//
//	TechEnabler -- StoneBase CalculateAvailableTechs.cs: the player's "researchable now" set (the engine canResearch
//	TRUE-set), computed IN ISOLATION over the whole-domain frontier (the engine has NO enables-frontier). A tech is
//	available iff not disabled, not held, under allowed.world, and requires.build holds. See patterns.md (the
//	single-source law) + docs/plans/structural-cleanup/cascade-engine-430.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include <set>

class CvPlayer;
class CvTeam;

class TechEnabler
{
public:
	// A tech is available iff not disabled, not held, under allowed.world, requires.build holds.
	// (Default flags -- TechEnabler uses `new ConditionEvaluator()`.) The all-techs+requires set is "researchable now".
	static void available(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& avail);
};

#endif // CV_TECH_ENABLER_H
