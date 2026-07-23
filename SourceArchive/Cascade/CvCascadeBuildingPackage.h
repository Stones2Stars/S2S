#pragma once
#ifndef CV_CASCADE_BUILDING_PACKAGE_H
#define CV_CASCADE_BUILDING_PACKAGE_H

//
//	BuildingPackage -- StoneBase BuildingPackage.cs: the AFTER-tier per-building flat (the lone AFTER term added flat
//	OUTSIDE the §1 percent stack; also the per-building own-flat the §2 commerce splitter folds in as BASE). See
//	patterns.md (single-source law) + docs/plans/structural-cleanup/modifier-machine.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx -- the eval target for deposit conditions
#include <string>

class CvCity;

class BuildingPackage
{
public:
	// AFTER tier -- BuildingPackage (modifier.md §2a / calc-map §1.4): Σ ACTIVE buildings' {ch}.city.flat +
	// {ch}.city.perPopulation × population, ×100. The lone AFTER term in the §1 yield rate (added flat OUTSIDE the percent
	// stack); also the per-building own-flat the §2 commerce splitter folds in as BASE. Same value, two tier tags.
	static long buildingFlat(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);
};

#endif // CV_CASCADE_BUILDING_PACKAGE_H
