#pragma once
#ifndef CV_AMENITY_CONTEXT_H
#define CV_AMENITY_CONTEXT_H

//
//	AmenityContext -- the city's AMENITY state: ONE PLACE RESPONSIBLE for its storage, its maintenance, and the
//	facts that drive it.
//
//	⛔ STORAGE AND MAINTENANCE ARE NOT SPLIT (owner): "splitting storage and maintenance on these objects creates
//	a fragmentation we don't want -- we want to have 1 place responsible." So the dictionaries live HERE, the
//	appliers that move them live HERE, and the DECLARED SET OF FACTS that reaches them lives HERE
//	([DEC-dict-is-a-consumer]). A store whose state sits on one object while a router elsewhere decides when it
//	moves has two homes and no owner.
//
//	⚖ IT IS A PACKAGE in the owner's sense -- a holder of FINAL STATE that something else will sum or evaluate,
//	read as a bare fetch. The yield and percent packages are the same role over a different payload; what differs
//	is only what a slot HOLDS (a grantor COUNT here, an x100 magnitude there), which decides scale rules and
//	nothing else ([DEC-keyed-accumulator]).
//
//	⚖ THE READ IS BOOLEAN, THE STORAGE IS A COUNT (the semiboolean contract, [state-repositories.md]). Several
//	grantors can confer the SAME amenity, so each contributes +1 and a departure DECREMENTS -- losing one power
//	plant must not darken a city that has two. `has(id)` is `count(id) > 0`; consumers ask `has`.
//
//	⛔ THE BUILDING LEG RIDES THE OPERATE CROSSING, NOT PRESENCE. A DORMANT building confers nothing
//	([enabler.md] §3.2), so the feeder is the enabler's own active<->dormant verdict, announced where it changes.
//	Presence (ADDED / REMOVED) cannot tell dormant from operating. ⛔ And never the "processed" completion notice,
//	which is DIAGNOSTIC -- state is never derived from an announcement that an apply RAN.
//	⚑ The crossing fires in BOTH phases -- the operate fixpoint at play, and the enabler's load seed announcing the
//	verdict it just computed -- so this store builds itself with no load-phase special case and no second build
//	mechanism beside the event stream ([DEC-spine-reseed]).
//
//	⛔ IT ONLY CONSUMES. Facts in, state out, nothing back -- which is why it can close no loop, and why the
//	ordering ban in [contexts.md] does not reach it: that ban is on a store that RE-DERIVES by reading another
//	system's built set, and a delta-CONSUMING store has no such dependency. The enabler is a SOURCE OF FACTS here,
//	never the home of the answer; this is the final stopping place, and every reader -- the enabler's own gate
//	included -- reads it here.
//	⚠ THE ONE THING THAT LEAVES is a derived VERDICT CROSSING (0 <-> non-zero for a key a consumer routes on).
//	That is this context announcing a genuine derived state change, not a store talking back, and it is what stops
//	the retired hand-named counters from taking their facts with them ([DEC-close-event-gaps-now]).
//
//	⚠ REGISTRATION ORDER IS A CONTRACT: registered inside the CONTEXTS band of
//	contexts -> enabler -> modifier -> triggers, because the enabler's load-end gate pass evaluates THROUGH these
//	stores. Order is a property of the band, never of translation-unit init order.
//

#include "ContextDict.h"
#include "CvCondition.h"   // CvCascPredKind -- the gate-flip axis

class CvCity;
class CvClassificationBlock;   // a grantor's §8 `amenities` block, folded by pointer (never included here)
struct CvSpineEvent;

//	IS-A ContextDict: the storage is inherited, not held ([DEC-dict-is-a-consumer]). `has` / `count` /
//	`add` / `clear` are the base's; this type adds only the binding, the by-key read and its own
//	declared interest set.
class AmenityContext : public ContextDict
{
public:
	AmenityContext() : m_city(NULL) {}
	void bind(const CvCity* pCity) { m_city = pCity; }   // set once by the owning CvCity's context

	// --- THE READ SURFACE ---------------------------------------------------------------------------------------
	// `has(id)` / `count(id)` / `clear()` are INHERITED -- there is no forwarding layer, because this IS the
	// dictionary. `has` is `count > 0`: the semiboolean contract, unchanged.
	// The BY-KEY read, for a caller holding a name rather than an id: amenity ids are MINTED AT LOAD, so there is
	// no compile-time id to pass ([DEC-classification-infos]) -- the caller keeps a static cache the registry
	// fills once.
	bool hasKey(int& iIdCache, const char* szKey) const;

	// ⚠ Zeroing at owner reset is the BASE's `clear()`, called by CvCity::reset -- a delta store is correct
	// only from a known zero, and a CvCity is recycled out of an FFreeListTrashArray.

	// --- THE MAINTENANCE: reached ONLY through this type's own spine consumer ------------------------------------
	// ⚖ THE DECLARED INTEREST SET -- the facts that maintain this store, stated at the store. A fact absent from
	// this list does not reach it, and that is READABLE HERE rather than inferable from a router.
	static bool wantsEvent(int iEventId);
	static void onSpineEvent(const CvSpineEvent& kEvent);

	// ⛔ CONSTRAINT: the appliers below have NO entry point other than this context's own spine consumer. They are
	// public only because the spine needs a registerable object to dispatch through; a direct call from a choke
	// point beside the event would be a second surface maintaining the same state.
	// A grantor STARTED (sign +1) / STOPPED (sign -1) contributing: fold its `amenities` block. A pure DELTA --
	// it reads no other system's built set, which is what lets it build identically at load and at play.
	void onGrantorCrossing(int iBuilding, int iSign);
	// The EMPIRE-scope grantors, as a DELTA. A civic confers on EVERY city of the empire, so a swap is
	// `-old / +new` over the player's cities -- the fact carries both ([DEC-facts-name-happenings]).
	void foldCivic(int iCivic, int iSign);
	// Every live civic of a player, folded with one sign. The initialization form: a city that STARTS EXISTING
	// (founded, conquered, or streamed off a save) folds its owner's standing civics from zero. ⚠ It is a fold
	// from a KNOWN state, never a re-derivation of an unknown one -- the distinction that separates it from the
	// refresh this replaced.
	void foldAllCivicsOf(int iPlayer, int iSign);
	// The owner's ACTIVE empire-level members (DEC-empire-level-buildings) -- the same fold-in, one grantor
	// kind over: the city-starts-existing leg and the load build both call it beside the civic one.
	void foldAllEmpireBuildingsOf(int iPlayer, int iSign);
	// ⚖ THE GATE FLIP -- the conditioned tail as a DELTA rather than a re-resolution. When an atom a grant is
	// gated on moves, the FACT supplies the verdict and its direction, so the entries gated on that atom are
	// applied WITHOUT evaluating it. That is what makes the capital move exact: after the flip, asking
	// "is this the capital?" of the city that just LOST it answers no, and a re-evaluating withdrawal would
	// subtract nothing and strand the grant forever ([DEC-no-self-heal]).
	// ⛔ This is why no record of past contributions is kept: the fact remembers, so the store need not.
	void foldGateFlip(int iPlayer, CvCascPredKind ePredicate, int iSign);
	void foldBlock(const CvClassificationBlock* pBlock, int iSign);
	// ⚖ THE POWERED CROSSING -- announced against CvCity::isPowered, the ONE definition of "powered". A STATUS is
	// middleware gating DELIVERY, so a blackout crosses this verdict while moving no count in the store; the caller
	// passes what held BEFORE its own input moved. ⛔ The status never enters the store and never reaches the
	// cascade -- this is the whole of its reach.
	void announcePowerCrossing(bool bPoweredBefore);

private:
	// The single write point, so the CROSSING announcement exists exactly once -- the GENERIC amenity crossing
	// for every key, plus power's gated verdict beside it.
	void applyKey(int iAmenityId, int iSign);

	const CvCity* m_city;   // the bound game object; a binding, never cleared
};

void amenityContextRegisterConsumer();   // register on the event spine (from spineRegisterConsumers; idempotent)

#endif // CV_AMENITY_CONTEXT_H
