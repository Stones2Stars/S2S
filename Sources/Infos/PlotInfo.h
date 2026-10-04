#pragma once

#ifndef STRUCT_PLOTINFO
#define STRUCT_PLOTINFO

#include <string>

#include "Defines/CvEnums.h"

/// <summary>How a plot's best-build search ended. When it found nothing, this is the FURTHEST stage any
/// candidate improvement reached, so the stages are ordered and a later one outranks an earlier one.</summary>
enum BestBuildOutcome
{
	BEST_BUILD_FOUND = 0,
	BEST_BUILD_NONE_NOT_OWNED,     // the plot is not this city's to improve
	BEST_BUILD_NONE_NO_TECH,       // no improvement has a build the team has researched
	BEST_BUILD_NONE_CANNOT_BUILD,  // researched, but no build is performable on this plot by the asker
	BEST_BUILD_NONE_KEEP_FEATURE,  // performable only by clearing a feature that is being kept
	BEST_BUILD_NONE_NO_GAIN,       // performable, but nothing beats what the plot already yields
	BEST_BUILD_NONE_KEEP_CURRENT,  // the best candidate is the improvement already standing
	BEST_BUILD_NONE_BUSY,          // another unit's build is in progress here and the asker cannot join it
};

struct plotInfo
{
	plotInfo();
	std::string ToJSON();

	BestBuildOutcome outcome;
	int index;
	bool worked;
	bool owned;
	bool bonusImproved;
	int value;
	int newValue;
	short yields[NUM_YIELD_TYPES];
	short newYields[NUM_YIELD_TYPES];
	BonusTypes bonus;
	ImprovementTypes currentImprovement;
	ImprovementTypes newImprovement;
	FeatureTypes currentFeature;
	FeatureTypes newFeature;
	BuildTypes currentBuild;
	BuildTypes newBuild;
};

#endif