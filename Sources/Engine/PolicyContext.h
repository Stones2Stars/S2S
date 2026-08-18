#pragma once
#ifndef CV_POLICY_CONTEXT_H
#define CV_POLICY_CONTEXT_H

//
//	PolicyContext -- the empire's ENACTED-POLICY state: storage, maintenance and its declared interest set, in ONE
//	place (docs/cascade.md §What a context STORES vs FORWARDS (a dictionary is a spine consumer)). The AmenityContext shape, at empire scope.
//
//	IS-A ContextDict: the storage is inherited, never held. `has` / `count` / `add` / `clear` are the base's.
//
//	⛔ id -> COUNT, never a set. Several grantors can enact the SAME policy -- a civic, a trait, and (as the data
//	grows) a project or a wonder -- so dropping one must not clear a state another live grantor still justifies
//	(docs/cascade.md §EVERY DERIVED STORE IS ONE SHAPE (keyed accumulator)). ⚠ A whole-union REFILL hides that: it recounts every time, so the multi-grantor case
//	appears to work while nothing delta-maintains it. That is what this replaced.
//
//	⛔ DELTAS ONLY -- no refill, no recount, no record of past contributions. Every fact carries what the
//	withdrawal needs: a civic SWAP names both the adopted civic and the one it displaced, and a trait names its
//	direction. Nothing has to remember what it gave.
//
//	⚑ NO LOAD SPECIAL CASE, and unlike the city stores it needs none: policies are PLAYER-scope and a player
//	EXISTS when its own facts stream from CvPlayer::read, so the reseed's civic/trait emits build this store
//	through the same appliers play uses (docs/spine.md §5 (the load reseed)). The reseed's civic fact carries NO_CIVIC as the
//	displaced side, so it is a pure +.
//
//	⛔ IT ONLY CONSUMES. Facts in, state out, nothing back.
//
//	⚠ REGISTRATION ORDER IS A CONTRACT: inside the CONTEXTS band of contexts -> enabler -> modifier -> triggers.
//

#include "ContextDict.h"

class CvPlayer;
class CvClassificationBlock;
struct CvSpineEvent;

class PolicyContext : public ContextDict
{
public:
	PolicyContext() : m_player(NULL) {}
	void bind(const CvPlayer* pPlayer) { m_player = pPlayer; }

	// --- THE MAINTENANCE: reached ONLY through this type's own spine consumer -----------------------------------
	// ⚖ THE DECLARED INTEREST SET -- the facts that maintain this store, stated AT the store.
	static bool wantsEvent(int iEventId);
	static void onSpineEvent(const CvSpineEvent& kEvent);

	// ⛔ CONSTRAINT: no entry point other than the consumer above. Public only because the spine needs a
	// registerable object to dispatch through.
	void foldCivic(int iCivic, int iSign);
	void foldTrait(int iTrait, int iSign);

private:
	void foldBlock(const CvClassificationBlock* pBlock, int iSign);

	const CvPlayer* m_player;   // the bound game object; a binding, never cleared
};

void policyContextRegisterConsumer();   // register on the event spine (from spineRegisterConsumers; idempotent)

#endif // CV_POLICY_CONTEXT_H
