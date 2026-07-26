#pragma once
#ifndef CV_MODIFIER_CONSUMER_H
#define CV_MODIFIER_CONSUMER_H

//
//	CvModifierConsumer -- the MODIFIER cascade's OWN spine consumer (event-spine.md: one consumer per system,
//	[DEC-enabler-not-cascade] -- the enabler has its own; a shared consumer welded the two machines and is
//	dead). DOMAIN events in, DERIVED dirty marks out: the event names its SOURCE (WHAT/WHO/WHERE); the
//	source's compiled deposits name the channels x scopes x targets it touches (DepositIndex::routeFor + the
//	condition-dependency routes) -- the consumer only resolves WHICH OWNER OBJECTS the event addresses and
//	applies the derived masks to their packages AND the receiver sums they feed. No event site carries a
//	hand-wired mask; no blanket rebuild, no mark-all, no turn-roll self-heal exists anywhere
//	([DEC-no-self-heal] -- a missed invalidation must surface as a live divergence).
//
//	LOAD-ACTIVE (DEC-spine-reseed): the reseed's in-read emits mark through this same consumer, so the load
//	builds the cascade's dirty picture and the first reads after load recompute from current state; only
//	result-producers (grants) suppress during the load bracket.
//

// Register the modifier consumer on the event spine (idempotent). Called from spineRegisterConsumers.
void modifierRegisterConsumer();

#endif // CV_MODIFIER_CONSUMER_H
