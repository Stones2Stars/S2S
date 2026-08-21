#pragma once
#ifndef CV_MODIFIER_CONSUMER_H
#define CV_MODIFIER_CONSUMER_H

//
//	CvModifierConsumer -- the MODIFIER cascade's OWN spine consumer (event-spine.md: one consumer per system,
//	docs/specs/enabler.md (enabler and cascade are two separate systems) -- the enabler has its own; a shared consumer welded the two machines and is
//	dead). DOMAIN events in, deposits APPLIED: the event names its SOURCE (WHAT/WHO/WHERE); the source's
//	compiled deposits (DepositIndex::depositsFor + the gatedBy* condition-dependency routes) name the exact
//	entries to move, and the consumer resolves WHICH OWNER OBJECTS the event addresses and applies each
//	entry's resolved value into the slot it feeds -- the maintained sum's one write path
//	(docs/cascade.md §THE MAINTAINED SUM). No event site carries a hand-wired list; no blanket rebuild, no mark-all, no
//	turn-roll self-heal exists anywhere (docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT -- a missed invalidation must surface as a live
//	divergence).
//
//	LOAD-ACTIVE (docs/spine.md §5 (the load reseed)): the reseed's in-read emits apply through this same consumer, so the load
//	builds the packages from the save's own facts; only result-producers (grants) suppress during the load
//	bracket.
//

// Register the modifier consumer on the event spine (idempotent). Called from spineRegisterConsumers.
void modifierRegisterConsumer();

#endif // CV_MODIFIER_CONSUMER_H
