#pragma once

//------------------------------------------------------------------------------------------------
//
//  FILE:    ConstructRequirement.h
//
//  PURPOSE: A single, introspectable construction/training prerequisite, expressed uniformly
//           as a requirement over a game-object-modifier (GOM) type. Aggregates the
//           historically scattered typed Prereq* fields of CvBuildingInfo / CvUnitInfo into
//           one queryable form so that consumers (the Civilopedia, help text, the
//           constructibility enabler index) can ask "what gates this?" in one place.
//
//           This is a READ-ONLY DESCRIPTION, not an evaluator. Evaluation still lives in
//           CvCity::canConstruct / CvPlayer::canTrain and keeps its probability-hint and
//           gate-stratification behaviour. (#195 Phase 2 -- unified prerequisite model;
//           see Sources/docs/plans/unified-prerequisites-and-constructibility.md)
//
//------------------------------------------------------------------------------------------------
#ifndef CONSTRUCT_REQUIREMENT_H
#define CONSTRUCT_REQUIREMENT_H

// How the listed ids must relate to the city/player state for the requirement to be met.
enum ConstructRequirementOp
{
	REQOP_REQUIRE_ALL,    // must have every listed id        (AND)
	REQOP_REQUIRE_ANY,    // must have at least one listed id  (OR)
	REQOP_FORBID,         // must have none of the listed ids  (NOT)
	REQOP_REQUIRE_COUNT,  // must have at least iCount of the single listed id
};

struct ConstructRequirement
{
	GOMTypes eGOM;                 // what kind of thing: GOM_BUILDING / GOM_TECH / GOM_BONUS / ...
	ConstructRequirementOp eOp;    // how the ids are required
	std::vector<int> aiIds;        // the type ids this requirement concerns
	int iCount;                    // threshold for REQOP_REQUIRE_COUNT (else 0)

	ConstructRequirement()
		: eGOM(NO_GOM), eOp(REQOP_REQUIRE_ALL), iCount(0)
	{}

	ConstructRequirement(GOMTypes gom, ConstructRequirementOp op)
		: eGOM(gom), eOp(op), iCount(0)
	{}

	// Single-id convenience (also used for REQUIRE_COUNT with a threshold).
	ConstructRequirement(GOMTypes gom, ConstructRequirementOp op, int iId, int iCountIn = 0)
		: eGOM(gom), eOp(op), iCount(iCountIn)
	{
		aiIds.push_back(iId);
	}
};

#endif // CONSTRUCT_REQUIREMENT_H
