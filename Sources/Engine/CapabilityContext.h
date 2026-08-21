#pragma once
#ifndef CV_CAPABILITY_CONTEXT_H
#define CV_CAPABILITY_CONTEXT_H

//
//	CapabilityContext -- the empire's ABILITY state: storage, maintenance and its declared interest set in ONE
//	place (docs/cascade.md §What a context STORES vs FORWARDS (a dictionary is a spine consumer)). The PolicyContext shape, over the capability plane's four blocks.
//
//	⛔ IT IS THE PLAYER'S, NEVER THE TEAM'S. `CvTeam` is the TECH BRIDGE and owns no live-state surface, so a
//	derived store landing on it is misplaced by construction (docs/cascade.md §The contexts (plot/city/player own one live-state context), contexts.md). The team carries
//	the unified TECH and PROJECT lists as MEMBERSHIP; anything DERIVED off them -- this union -- is the player's.
//
//	⛔ FOUR DICTIONARIES, NEVER ONE. The blocks are DISJOINT REGISTRIES that each start at 0
//	(CLSD_CAPABILITY / CLSD_CANTRADE / CLSD_CANWORKON, and TERRAIN_ FKs), so a merged store re-opens the
//	cross-registry id collision the CLS_ prefix closed by construction (ContextDict.h; contexts.md's vicinity
//	ruling: "there is nothing wrong with having 2 dictionaries ... what I don't want is the constant rewalk").
//	The four share ONE liveness rule -- a grantor is held -- which is why one maintainer serves them, exactly as
//	CityContext maintains plotAttrs beside its vicinity tiers.
//
//	⛔ id -> COUNT, never a set. Several grantors may confer the same ability -- capabilities.md keeps civic and
//	building grantors as model headroom beside techs -- so dropping one must not clear a state another live
//	grantor still justifies (docs/cascade.md §EVERY DERIVED STORE IS ONE SHAPE (keyed accumulator)). A whole-union REFILL hides that: it recounts every time, so
//	the multi-grantor case appears to work while nothing delta-maintains it. That is what this replaced.
//
//	⚖ THE TECH LEG HAS TWO TRIGGERS, because tech is TEAM-held while this store is PER-PLAYER:
//	   - AT LOAD each member emits per-self (`CvPlayer::read` walks its team's held techs), so the fact already
//	     reaches every player and the fold is a plain +1.
//	   - AT PLAY `CvTeam::setHasTech` emits ONCE, naming the acquiring player, so the fold FANS over that team's
//	     members. Unguarded, that fan would count every member twice against the load build -- so it is guarded to
//	     the non-load path, exactly as the amenity fold guards its play-time civic fan (contexts.md).
//	  ⚑ The withdrawal needs no new fact: `setHasTech` already announces BOTH directions past its no-change guard
//	  (SEVT_EMPIRE_TECH_ADDED / _REMOVED), so the advanced-start refund -- the one live path that clears a tech --
//	  withdraws exactly what it deposited.
//
//	⛔ IT ONLY CONSUMES. Facts in, state out, nothing back.
//
//	⚠ REGISTRATION ORDER IS A CONTRACT: inside the CONTEXTS band of contexts -> enabler -> modifier -> triggers.
//

#include "ContextDict.h"

class CvPlayer;
class CvInfo;
struct CvSpineEvent;

class CapabilityContext
{
public:
	CapabilityContext() : m_player(NULL), m_corpRevenueMod(0) {}
	void bind(const CvPlayer* pPlayer) { m_player = pPlayer; }

	// --- THE READ SURFACE -------------------------------------------------------------------------------------
	// One dictionary per authored block; the id argument is the generated compile-time constant for its domain
	// (CvClassificationIds.h), so every read is an O(1) map probe and no string survives load.
	// ⚑ EACH READ ORs THE UNIVERSAL BASELINE. `TECH_GAME_START` is the synthetic root every civ holds and it
	// lives OFF the InfoRepo -- it has no engine id and is not in `m_pabHasTech` (AGENTS.md; the enabler
	// accumulates it separately for the same reason), so no tech fact can ever carry it and it cannot be a
	// delta. It is universal and immutable, so it is read as a CONSTANT rather than stored: that is stateless,
	// order-independent, and cannot drift out of step with the dictionary beside it.
	bool hasCapability(int iCapabilityId) const;
	bool hasCanTrade(int iCanTradeId) const;
	bool hasCanWorkOn(int iCanWorkOnId) const;
	bool canTradeOnTerrain(int iTerrain) const;
	// The DERIVED-from-grantor corporation revenue modifier: a maintained SUM over the live grantors' compiled
	// `commerce.<scope>.corporation` point reads, moved by the same fold (docs/cascade.md §THE MAINTAINED SUM). A PERCENT, so it
	// is not scaled (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)) and its consumer combines it as the human percent it already is.
	int corporationRevenueModifier() const      { return m_corpRevenueMod; }

	// The commerce channel -> its SLIDER capability id. GOLD is the RESIDUAL channel and has no slider
	// (capabilities.md), so it answers -1 -- "no such capability", never a flag that would always read false.
	// ONE implementation, because every consumer asking "may this channel's rate be set" needs the same map
	// (docs/architecture/patterns.md §DRY (single implementation)).
	static int commerceRateCapability(int eCommerce);

	// Zeroing at owner reset -- a delta store is correct only from a known zero.
	void clear();

	// The served ENUMERATION -- the four dictionaries, so a renderer names what it holds rather than probing
	// every minted id. Read-only; the maintenance verb is the fold below.
	const ContextDict& capabilityDict() const { return m_capabilities; }
	const ContextDict& canTradeDict() const   { return m_canTrade; }
	const ContextDict& canWorkOnDict() const  { return m_canWorkOn; }
	const ContextDict& canTradeOnDict() const { return m_canTradeOn; }

	// --- THE MAINTENANCE: reached ONLY through this type's own spine consumer ----------------------------------
	// ⚖ THE DECLARED INTEREST SET -- the facts that maintain this store, stated AT the store.
	static bool wantsEvent(int iEventId);
	static void onSpineEvent(const CvSpineEvent& kEvent);

	// ⛔ CONSTRAINT: no entry point other than the consumer above. Public only because the spine needs a
	// registerable object to dispatch through; a direct call beside the emit would be a second surface
	// maintaining one fact.
	// A TECH started (+1) / stopped (-1) being held. Techs are the only grantor kind the data authors today;
	// capabilities.md keeps civic and building grantors as model headroom, and each arrives as its own fold off
	// its own fact -- never by widening this one to guess at a grantor kind it was not given.
	void foldTech(int iTech, int iSign);

private:
	ContextDict m_capabilities;   // CLSD_CAPABILITY ids -- the flat `capabilities` block
	ContextDict m_canTrade;       // CLSD_CANTRADE ids   -- what may appear on the trade table
	ContextDict m_canWorkOn;      // CLSD_CANWORKON ids  -- which coarse plot classes citizens may work
	ContextDict m_canTradeOn;     // TERRAIN_ FK ids     -- which plot types carry trade
	int m_corpRevenueMod;

	const CvPlayer* m_player;     // the bound game object; a binding, never cleared
};

void capabilityContextRegisterConsumer();   // register on the event spine (from spineRegisterConsumers; idempotent)

#endif // CV_CAPABILITY_CONTEXT_H
