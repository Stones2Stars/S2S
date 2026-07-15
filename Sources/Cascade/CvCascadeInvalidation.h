#pragma once
#ifndef CV_CASCADE_INVALIDATION_H
#define CV_CASCADE_INVALIDATION_H

//
//	CvCascadeInvalidation -- the #430 F0 cache consumer (R3): the ONE IEventConsumer with two halves -- the
//	ENABLER deltas (LOAD-ACTIVE: the reseed's in-read emits build the enabler domains through the same appliers
//	as play, DEC-spine-reseed) and the MODIFIER package dirty-marks (load-inert; the spine as invalidation's
//	front door). It also emits a [CASCADE] invalidate observability line per mark, so the invalidation flow is
//	verifiable in Cascade.log. See the .cpp for the staged-wiring rationale + the per-half load behaviour.
//

void cascadeRegisterInvalidation();   // register the R3 consumer on the event spine (from spineRegisterConsumers)

#endif // CV_CASCADE_INVALIDATION_H
