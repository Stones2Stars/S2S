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

	CvEntityAvailability() : allowedScope(COUNTSCOPE_WORLD), allowedCap(-1), spawnOnly(false) {}
};

// Build-time gate: requires.build AND requires.operate must hold, AND the SELF count must be under the allowed
// cap. eSelfDomain/iSelfType = the candidate's own (domain,type), for the cap read.
bool cascadeBuildable(const CvEntityAvailability& kAvail, CountDomain eSelfDomain, int iSelfType, const CvCascadeContext& kCtx);

// Operate-time gate (dormancy): a built thing stays active only while requires.operate still holds.
bool cascadeOperational(const CvEntityAvailability& kAvail, const CvCascadeContext& kCtx);

#endif // CV_CASCADE_ENABLER_H
