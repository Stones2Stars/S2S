#pragma once
#ifndef CV_TRAIT_CONTEXT_H
#define CV_TRAIT_CONTEXT_H

//
//	TraitContext -- the empire's HELD-TRAIT set: storage, maintenance and its declared interest set in ONE place
//	(docs/cascade.md §What a context STORES vs FORWARDS (a dictionary is a spine consumer)). The PolicyContext shape, over the TRAIT_ id space.
//
//	⚖ WHY IT IS A STORE AT ALL, WHEN `hasTrait` IS ALREADY O(1) -- the test is a SCAN, not a hop (contexts.md).
//	Asking "does this player hold TRAIT_X" resolves through one pointer, so it is correctly FORWARDED
//	(EmpireContext::hasTrait) and no store is earned. ENUMERATING what a player holds is a DIFFERENT read: off
//	`CvPlayer::m_pabHasTrait` it walks the whole trait registry -- 369 records -- to rediscover the handful a
//	leader carries. That is the O(registry) sweep the event-built state exists to delete
//	([state-repositories.md]: "the sum walks what the unit HOLDS, never the registry ... that sweep costs the
//	DATABASE per gather"). ⚑ The rule, stated once: ask what the read WALKS. One pointer forwards; a registry
//	scan earns a store.
//
//	⚑ WHO ASKS, and why the enumeration is on a hot path at all: the KEYED-DEPOSIT read ([modifier.md] §5). A
//	trait's target-keyed deposits stay SOURCE-SIDE -- the per-set carve-out, because a split trait carries
//	different values in the simple vs complex set ([modifier.md] §4) -- so "how much does this source give THIS
//	target" is answered by asking each LIVE SOURCE what it deposits onto that key. That read is cheap because it
//	iterates the handful an entity AUTHORED; it stops being cheap the moment DISCOVERING the live sources costs
//	the registry.
//
//	⛔ THE RECORD IS THE ACTIVE SET'S, NEVER THE RAW REGISTRY ENTRY. A held id resolves through
//	MMKernel::traitData (the GAMEOPTION_LEADER_COMPLEX_TRAITS selector), so a simple and a complex record can
//	never be read for one another ([modifier.md] §4). That resolution lives HERE, once, rather than at each
//	consumer (docs/architecture/patterns.md §DRY (single implementation)).
//
//	⛔ id -> COUNT, never a set (docs/cascade.md §EVERY DERIVED STORE IS ONE SHAPE (keyed accumulator)). The engine's own has-array is a bool and its setter
//	announces only genuine transitions, so the count sits at 0/1 today -- but a building's `grants.traits`
//	confers a trait on the OWNER empire while it is active ([json.md] §8), so a second grantor is a DATA edit
//	away and a bitset would break silently on one.
//
//	⛔ THE FACT'S IDENTITY IS THE DIRECTION -- a handler NEVER re-reads `hasTrait`. `setHasTraitInternal` writes
//	`m_pabHasTrait` BEFORE it emits, so by the time any consumer runs the array already holds the NEW value and
//	the old contribution is gone -- the same reason a city can never reach down for a plot's old bits
//	(contexts.md). ADDED is +1 and REMOVED is -1, read off WHICH event arrived.
//
//	⚑ NO LOAD SPECIAL CASE, and it needs none for the reason PolicyContext needs none: a player EXISTS when its
//	own facts stream, and `CvPlayer::read` emits ADDED per held trait after the wholesale has-array read, so the
//	reseed builds this store through the appliers play uses (docs/spine.md §5 (the load reseed)). The NEW-GAME half rides the
//	same pair: initMore assigns the leader's traits through `setHasTraitInternal` AFTER the lifecycle announce
//	has primed, so the initial assignment announces exactly as a runtime swap does -- one mechanism, no
//	lifecycle fold beside it.
//
//	⛔ IT ONLY CONSUMES. Facts in, state out, nothing back.
//
//	⚠ REGISTRATION ORDER IS A CONTRACT: inside the CONTEXTS band of contexts -> enabler -> modifier -> triggers.
//

#include "ContextDict.h"
#include <vector>

class CvPlayer;
class CvTraitInfo;
struct CvSpineEvent;

class TraitContext : public ContextDict
{
public:
	TraitContext() : m_player(NULL) {}
	void bind(const CvPlayer* pPlayer) { m_player = pPlayer; }

	// --- THE READ SURFACE -------------------------------------------------------------------------------------
	// ⚑ THE ID TRAVELS WITH THE RECORD, and it has to: an info does NOT know its own engine id (contexts.md --
	// which is why a source-slot predicate needs the carrier named), so a consumer holding only the record
	// cannot say WHICH trait it is holding. The dictionary key is that id, so the one read hands over both and
	// no caller has to search a registry backwards to recover it.
	struct HeldTrait
	{
		int id;                    // the raw TraitTypes id -- the dictionary key
		const CvTraitInfo* info;   // the ACTIVE set's record for it (simple vs complex by the live option)
	};

	// The player's held traits, APPENDED to a caller-owned vector (the group-read shape: state in, the whole
	// group out). A consumer never walks the registry to find them, and never resolves an id to a record itself.
	void heldTraits(std::vector<HeldTrait>& heldTraits) const;

	// --- THE MAINTENANCE: reached ONLY through this type's own spine consumer -----------------------------------
	// ⚖ THE DECLARED INTEREST SET -- the facts that maintain this store, stated AT the store.
	static bool wantsEvent(int iEventId);
	static void onSpineEvent(const CvSpineEvent& kEvent);

	// ⛔ CONSTRAINT: no entry point other than the consumer above. Public only because the spine needs a
	// registerable object to dispatch through; a direct call beside the emit would be a second surface
	// maintaining one fact.
	void foldTrait(int iTrait, int iSign);

private:
	const CvPlayer* m_player;   // the bound game object; a binding, never cleared
};

void traitContextRegisterConsumer();   // register on the event spine (from spineRegisterConsumers; idempotent)

#endif // CV_TRAIT_CONTEXT_H
