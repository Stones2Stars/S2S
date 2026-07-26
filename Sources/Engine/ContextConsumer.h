#pragma once
#ifndef CV_CONTEXT_CONSUMER_H
#define CV_CONTEXT_CONSUMER_H

//
//	ContextConsumer -- the per-scope CONTEXTS' OWN spine consumer (one consumer per system, event-spine.md) and the
//	single maintainer of every stored context aggregate:
//	 - PlotContext's CASC_PRED_* verdict BITSET (contexts.md) -- re-derived from the plot substrate DOMAIN facts
//	   (terrain / feature / improvement / route / bonus / owner), with the bounded one-hop ADJACENCY FAN-OUT.
//	 - CityContext.plotAttrs -- the per-predicate COUNTS, which are the FOLD of the member plots' bitsets.
//	Both are derived state: never serialized, rebuilt from the save read's own in-read emits (DEC-spine-reseed --
//	the genuine read emits, never a post-deserialization walk that fabricates events).
//
//	THE PLOT BITSET
//	 - Every plot substrate DOMAIN event -- terrain / feature / improvement / route / bonus / owner / plot TYPE /
//	   river / irrigation / landmark / worked -- re-derives that plot's WHOLE block (one uniform derivation, not a
//	   bespoke per-event bit mask), then -- ONLY if a bit the neighbours read moved (PlotContext::fanOutTriggerMask)
//	   -- re-derives the 8 adjacent plots' ADJACENCY block. That gate is exact: a neighbour's HAS_COAST /
//	   HAS_FRESHWATER verdict reads nothing but facts that live in this plot's own block, so the fan-out is one hop
//	   and can never cascade further.
//	 - DEFERRED WHILE THE MAP IS UNSETTLED. The adjacency derivation dereferences an adjacent water plot's CvArea,
//	   which does not exist during world generation or mid-save-read. So while !CvGame::isFinalInitialized() the
//	   announcing plot is only MARKED, and the marks drain -- own block + adjacency block, against the complete map
//	   -- on the first event after the game reports final-initialized. The drain is complete because every plot
//	   announces itself in both paths (CvPlot::read emits its terrain fact UNCONDITIONALLY; world generation sets
//	   every plot's type/terrain). Neighbours are deliberately NOT marked alongside: a plot missing from the drain
//	   is then a visible divergence rather than a silently self-healed one.
//
//	CityContext.plotAttrs -- the ONE applier is CvCity::onCityPlotChanged (-> CityContext::onPlotChanged), which
//	folds the plot's stored BITSET so the two granularities of one vocabulary cannot drift. Triggers:
//	 - MEMBERSHIP: the CvPlot::updateWorkingCity choke point folds directly at the mutation (+1 / -1) and emits the
//	   DOMAIN fact; this consumer IGNORES that event at play (the choke point already applied).
//	 - A MEMBER PLOT'S BITS MOVING: this consumer unfolds the old bits and refolds the new ones around every
//	   derivation, so a mid-membership change reaches the city counts.
//	 - LOAD: the deserialized membership facts fire from inside CvPlot::read, BEFORE the cities exist (the map
//	   streams ahead of the players), so the consumer BUFFERS the bracket's working-city facts and drains them at
//	   SEVT_GAME_LOAD_FINISHED -- the "apply once after the stream ends" option of the enabler.md par.7.1 order
//	   rule (never the mixed form). The plot bitsets drain FIRST, so the fold sees final bits.
//
//	EmpireContext.policies needs no consumer leg: a derived-from-source aggregate, rebuilt whole at the end of
//	CvPlayer::read (DEC-derived-never-trusted).
//
//	CONSTRAINT: the maintenance has NO entry point other than a spine event. Every plot mutation that moves a stored
//	verdict emits its own DOMAIN fact, so the consumer is the single trigger path -- a direct call from a choke
//	point beside the event would be a second surface maintaining the same state.
//

void contextRegisterConsumer();   // register on the event spine (from spineRegisterConsumers; idempotent)

#endif // CV_CONTEXT_CONSUMER_H
