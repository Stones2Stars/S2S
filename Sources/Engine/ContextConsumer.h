#pragma once
#ifndef CV_CONTEXT_CONSUMER_H
#define CV_CONTEXT_CONSUMER_H

//
//	ContextConsumer -- the per-scope CONTEXTS' OWN spine consumer (one consumer per system, event-spine.md):
//	the load reseed of the stored context aggregates. Today that is ONE aggregate: CityContext.plotAttrs
//	(contexts.md) -- built from the in-read SEVT_WORKING_CITY_CHANGED DOMAIN events (DEC-spine-reseed: the
//	genuine save-read emits, never a post-deserialization walk that fabricates events, never a warm-up seed
//	beside the stream).
//
//	The ONE applier is CvCity::onCityPlotChanged (-> CityContext::onPlotChanged), play and load alike; only the
//	trigger differs by phase:
//	 - PLAY: the CvPlot::updateWorkingCity choke point folds directly at the mutation (contexts.md maintenance)
//	   and emits the DOMAIN fact; this consumer IGNORES out-of-bracket events (the choke point already applied).
//	 - LOAD: the deserialized facts fire from inside CvPlot::read, BEFORE the cities exist (the map streams
//	   ahead of the players), so the consumer BUFFERS the bracket's working-city facts and drains them once at
//	   SEVT_GAME_LOAD_FINISHED -- the "apply once after the stream ends" option of the enabler.md §7.1 order
//	   rule (never the mixed form). The buffer is fed PURELY by the event stream.
//
//	EmpireContext.policies needs no consumer leg: a derived-from-source aggregate, rebuilt whole at the end of
//	CvPlayer::read (DEC-derived-never-trusted).
//

void contextRegisterConsumer();   // register on the event spine (from spineRegisterConsumers; idempotent)

#endif // CV_CONTEXT_CONSUMER_H
