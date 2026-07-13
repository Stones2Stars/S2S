#pragma once
#ifndef CV_CASCADE_INVALIDATION_H
#define CV_CASCADE_INVALIDATION_H

//
//	CvCascadeInvalidation -- the #430 F0 cache-invalidation consumer (R3): the ONE IEventConsumer that turns a DOMAIN
//	state-change event into the package dirty-marks its source touches, so the event SPINE is the front door for
//	invalidation. It also emits a [CASCADE] invalidate observability line per mark, so the invalidation flow is
//	verifiable in Cascade.log. See the .cpp for the staged-wiring rationale + the load-inert behaviour.
//

void cascadeRegisterInvalidation();   // register the R3 consumer on the event spine (from spineRegisterConsumers)

#endif // CV_CASCADE_INVALIDATION_H
