#pragma once
#ifndef CV_CASCADE_ENABLER_H
#define CV_CASCADE_ENABLER_H

#include "CvCascadeCondition.h"

//
//	The ENABLER -- availability ("can I take this action now?", enabler-cascade-spec.md). This slice is the GATE:
//	given a candidate's reversible `requires` (build + operate) and its `allowed` cap, is it buildable now, and is
//	a built one still operating (dormancy)? The GENERATION half -- the `enables`-family forward index that
//	produces the CAN GET frontier -- is the next slice. Everything reads FORWARD via the step-2 condition
//	evaluator (tally + HAS); no upward/ reverse walk on the hot path.
//

// The availability data readJson fills per entity (the reversible-gate half of its enabler objects).
struct CvEntityAvailability
{
	CvCascadeCondition requiresBuild;   // one-time construction gate (greyed if unmet); NOT re-checked after build
	CvCascadeCondition requiresOperate; // continuous gate: greyed at build AND dormant if lost after build
	CountScope         allowedScope;    // scope the SELF cap applies at (world/team/empire)
	int                allowedCap;      // at most N of SELF at allowedScope; -1 == uncapped (the common case)
	bool               spawnOnly;       // identity.spawnOnly: NOT player-buildable (wildlife/spawned) -> never offered
	bool               notConstructible; // identity.notConstructible (building twin of spawnOnly): legacy getProductionCost()==-1
	                                      // -> the player never constructs it via the production queue (it is autobuilt / property-
	                                      // spawned / outcome-granted / GP-or-event placed / a doctrine toggle) -> never offered.
	bool               autoBuild;         // identity.autoBuild (data-model §4.2b): PLACEMENT marker -- the engine auto-places this
	                                      // in every city where its `requires` holds, instead of offering it in the queue. Subset of
	                                      // notConstructible. Used by the §14 H auto-placement shadow (B-i), NOT the buildability gate.

	CvEntityAvailability() : allowedScope(COUNTSCOPE_WORLD), allowedCap(-1), spawnOnly(false), notConstructible(false), autoBuild(false) {}
};

// Build-time gate: requires.build AND requires.operate must hold, AND the SELF count must be under the allowed
// cap. eSelfDomain/iSelfType = the candidate's own (domain,type), for the cap read.
bool cascadeBuildable(const CvEntityAvailability& kAvail, CountDomain eSelfDomain, int iSelfType, const CvCascadeContext& kCtx);

// Operate-time gate (dormancy): a built thing stays active only while requires.operate still holds.
bool cascadeOperational(const CvEntityAvailability& kAvail, const CvCascadeContext& kCtx);

#endif // CV_CASCADE_ENABLER_H
