#pragma once
#ifndef CV_ENABLER_CONSUMER_H
#define CV_ENABLER_CONSUMER_H

//
//	CvEnablerConsumer -- the ENABLER's own spine consumer.
//
//	⛔ ONE CONSUMER PER SYSTEM (owner). The enabler and the modifier cascade are SEPARATE SYSTEMS
//	([DEC-enabler-not-cascade]) and each owns its own IEventConsumer, registered independently on the spine --
//	exactly as logging, the /events stream and the grants engine already do. A single class routing DOMAIN
//	events to BOTH systems was the last place the two were structurally welded together, and that welding is
//	how "the modifier reads the enabler's internals" kept reading as ordinary rather than as crossing a boundary.
//
//	LOAD-ACTIVE by contract: the reseed's in-read emits are what BUILD the enabler domains, applying source-side
//	edges straight off the loaded info objects ([DEC-spine-reseed]). It therefore does NOT suppress during the
//	load bracket -- unlike a result-producer such as grants.
//

void enablerRegisterConsumer();   // register on the event spine (from spineRegisterConsumers)

#endif // CV_ENABLER_CONSUMER_H
