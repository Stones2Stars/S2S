//
//	CvCascadeEnabler -- the availability GATE (see CvCascadeEnabler.h). Forward-only reads via the step-2
//	condition evaluator over the tally.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeEnabler.h"

bool cascadeBuildable(const CvEntityAvailability& kAvail, CountDomain eSelfDomain, int iSelfType, const CvCascadeContext& kCtx)
{
	// Build-time gate = build ∧ operate (need both present to construct); either missing -> greyed.
	if (!cascadeEvalCondition(kAvail.requiresBuild, kCtx))
	{
		return false;
	}
	if (!cascadeEvalCondition(kAvail.requiresOperate, kCtx))
	{
		return false;
	}
	// The declarative cap: permitted while the SELF count at the cap's scope is below it. (cascadeWithinAllowed
	// returns true when allowedCap < 0, i.e. uncapped.)
	if (!cascadeWithinAllowed(eSelfDomain, iSelfType, kAvail.allowedScope, kAvail.allowedCap, kCtx))
	{
		return false;
	}
	return true;
}

bool cascadeOperational(const CvEntityAvailability& kAvail, const CvCascadeContext& kCtx)
{
	// Operate-time gate (dormancy) re-checks ONLY the continuous part -- the built thing parks when it fails,
	// wakes when it returns. (enabler-spec §3.)
	return cascadeEvalCondition(kAvail.requiresOperate, kCtx);
}
