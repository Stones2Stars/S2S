#pragma once
#ifndef CV_CASCADE_MODIFIER_MATH_H
#define CV_CASCADE_MODIFIER_MATH_H

//
//	The [MODIFIER] spine domain + the per-turn census. The modifier machine itself lives in the calc classes
//	(MMKernel/PercentStack/YieldBasePackages/BuildingPackage/CommerceCalc) consumed by CvCascadeAccumulator; the
//	shadow harness that once lived here died with the shadow phase (validation.md).
//

void cvCascadeModifierPerfCensus();      // the [MODIFIER/perf] + [MODIFIER/repo] census (ruled perf/observability surface) -- called every turn

#endif // CV_CASCADE_MODIFIER_MATH_H
