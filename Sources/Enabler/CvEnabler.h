#pragma once
#ifndef CV_ENABLER_H
#define CV_ENABLER_H

//
//	The STANDARDIZED ENABLER component (enabler.md par.7 + par.7.1) -- DELTA-APPLIED, never a value cache:
//	there is NO dirty->recompute path at all. The CAN-HAVE content is built
//	PURELY on the events of already-HAS -- ONE mechanism for load and play (DEC-spine-reseed): at load the
//	in-read reseed emits stream through the same O(delta) appliers the play-time emits use (the domain is
//	init'd -- sized + static exclusions, NO content -- at its owner's lifecycle start, before the emits). The
//	only non-event fold is the city-created applier's cross-scope fold (team techs + player civics predate the
//	city; no events can carry them to it). Every read is a bare O(1) lookup.
//
//	EnablerDomain -- ONE domain's storage on its scope owner: the tri-state array (HIDDEN/GREYED/LISTED, the par.6
//	tri-state IS the array) + the two membership refcount planes. Membership is the FORMULA, never the operation
//	sequence (par.7.1 step 1):
//
//	    in CAN GET  <=>  enableCount > 0  &&  removeCount == 0  &&  !held  &&  !staticExcluded
//
//	REMOVAL WINS regardless of event arrival order -- the sequenced add/erase delta is BANNED (an enables-add
//	arriving after an obsoletes-remove must NOT re-insert the candidate; the TECH_GAME_START-arrives-last /
//	obsoleted-candidate edge case). The REQUIRES GATE (par.7.1 step 2) splits a tree member's state:
//	gate passed -> LISTED, gate failed -> GREYED (the FLAG_GATE_FAILED bit, set by the domain enabler's
//	gate-on-entry + EDGEF_REQUIRED_BY re-gates); the gate NEVER changes membership. A domain whose gate stage
//	has not landed simply never sets the flag -- every tree member LISTED (the enable-side over-offer).
//
//	The per-domain enabler (TechEnabler / BuildingEnabler / UnitEnabler) owns the delta LOGIC (which edges feed
//	which domain); this component owns only the standardized storage + formula. HAVE itself is NEVER stored here --
//	the enabler ties into the object-owned has-lists that already exist (the held flag mirrors them per candidate
//	for the formula's exclusion only).
//

#include <set>
#include <vector>

class EnablerDomain
{
public:
	enum State
	{
		STATE_HIDDEN = 0,   // not in CAN GET -- generation never reached it, or a held source removes it
		STATE_GREYED = 1,   // in CAN GET, requires unmet (the gate stage -- unused until it lands)
		STATE_LISTED = 2    // in CAN GET, gate passed (enable-side stage: every tree member)
	};

	EnablerDomain() : m_bSeeded(false) {}

	// The lifecycle: init() sizes + zeroes and marks the domain ready (isSeeded) -- the DOMAIN events then build
	// the content (the appliers skip an un-init'd domain: its owner's own read/init emits replay its facts);
	// reset() un-readies (a new/other game re-inits fresh).
	void init(int iCount);
	void reset();
	bool isSeeded() const { return m_bSeeded; }

	// The delta primitives -- each recomputes the touched id's state from the FORMULA (O(1)).
	void addEnable(int iId, int iDelta);           // a held source's enables edge (+1 acquired / -1 lost)
	void addRemove(int iId, int iDelta);           // a held source's obsoletes/replaces/disables edge
	void setHeld(int iId, bool bHeld);             // the candidate itself is now held/built (leaves the frontier)
	void setStaticExcluded(int iId, bool bExcluded); // static never-offered (identity.disable class) -- set at seed
	// The REQUIRES-GATE verdict (par.7.1 step 2 -- the gate stage): set by the domain enabler's gate evaluation
	// (gate-on-entry + the EDGEF_REQUIRED_BY re-gates); a failed gate flips a tree member LISTED -> GREYED
	// (membership itself is untouched -- requires never adds/removes candidates, par.1).
	void setGateFailed(int iId, bool bFailed);
	// The QUEUED overlay (par.7.1 step 3 / par.8): a building currently in this city's production queue leaves the
	// FRESH OFFER (listed / listedIds) but stays CONTINUABLE (listedForContinue) and in-tree. A read-time overlay,
	// NOT a gate reason -- set from the live getFirstBuildingOrder read on SEVT_CITY_ORDER_CHANGED (+ the load-end
	// gate pass). Split out from FLAG_GATE_FAILED precisely so bContinue can tell "queued" from "gate failed".
	void setQueued(int iId, bool bQueued);
	bool isHeld(int iId) const;
	// The STATIC-EXCLUSION read (enabler.md par.8): the permanent, life-of-the-owner bars seeded at initDomain
	// (a tech's identity.disable + its civilization's never-researchable list; a building's notConstructible; a
	// unit's spawnOnly). It is the membership plane's own verdict, so a CAN-I-EVER consumer asks HERE rather
	// than re-reading the authoring off the infos -- that duplicate is a second implementation of one verdict
	// ([DEC-single-implementation]).
	bool isStaticExcluded(int iId) const;

	// The bare O(1) reads.
	unsigned char state(int iId) const;
	bool inTree(int iId) const { return state(iId) >= (unsigned char)STATE_GREYED; }
	// listed = the FRESH OFFER (canConstruct bContinue=false): gate-passed AND not currently queued. The QUEUED
	// overlay (FLAG_QUEUED, the enabler.md par.8 "!bContinue getFirstBuildingOrder exclusion") is a read-time
	// filter, NOT a gate/membership reason -- kept separate so a CONTINUE check can see past it.
	// ⛔ THE QUEUED OVERLAY IS A **BUILDING** RULE, AND UNITS MUST NEVER SET IT (owner: you can build multiple
	// copies). A city holds at most ONE of a building, so queueing it withdraws the fresh offer; a UNIT stays
	// trainable however many are queued or already built -- it leaves only on a CAP or a SUPERSESSION
	// ([enabler.md] par.7.1: "the leave-rules differ per domain"). ⚠ `CvBuildingEnabler` is therefore the ONLY
	// caller of setQueued, and that asymmetry is the DESIGN, not an unfinished half: wiring setQueued for units
	// "for symmetry" would silently delete every queued unit from the frontier, and the AI production loops
	// iterate exactly this list.
	bool listed(int iId) const { return state(iId) == (unsigned char)STATE_LISTED && !isQueued(iId); }
	// listedForContinue = the CONTINUE verdict (canConstruct bContinue=true): gate-passed, IGNORING the queued
	// overlay. A building already in the queue IS queued by definition, so excluding it here would cancel every
	// in-progress build each turn (the doCheckProduction purge). Reads past FLAG_QUEUED; the requires-gate still applies.
	bool listedForContinue(int iId) const { return state(iId) == (unsigned char)STATE_LISTED; }
	// The raw membership-plane reads (bare O(1)) -- for a composite gate that OVERLAYS per-instance planes on
	// the maintained ones before applying the formula (the promotions level-up gate: the player domain's tech
	// planes + the unit's held-promo/unitcombat planes; membership = Σenable > 0 && Σremove == 0).
	int enableCount(int iId) const;
	int removeCount(int iId) const;

	// THE MEMBERSHIP FORMULA ITSELF, exposed so it exists exactly ONCE ([DEC-single-implementation]). The
	// maintained refresh() and the AS-IF-HELD overlay (CvEnablerOverlay.h) both resolve membership through
	// this, so a hypothetical can never drift from the real frontier it is overlaid on -- a second copy would
	// diverge silently the first time the formula gained a term, and the hypothetical would then describe a
	// game state the frontier does not agree with.
	static bool isMember(int iEnable, int iRemove, bool bHeld, bool bStaticExcluded)
	{
		return iEnable > 0 && iRemove == 0 && !bHeld && !bStaticExcluded;
	}

	// FRONTIER ITERATION (enabler.md par.6 -- the AI's production/research decisions iterate ONLY this small
	// offered set, never the whole entity database; the F2b consumer sweep). One O(N) byte-scan fills `out` with
	// the matching ids, replacing a caller's per-id whole-database gate probe. `out` is caller-owned (cleared
	// first) so a hot caller reuses one buffer. `listedIds` = gate-passed/offerable (LISTED); `inTreeIds` =
	// LISTED + GREYED (the visible tri-state, for a UI that shows greyed candidates too).
	void listedIds(std::vector<int>& out) const;
	void inTreeIds(std::vector<int>& out) const;

private:
	enum { FLAG_HELD = 1, FLAG_STATIC_EXCLUDED = 2, FLAG_GATE_FAILED = 4, FLAG_QUEUED = 8 };

	bool inRange(int iId) const { return iId >= 0 && iId < (int)m_aState.size(); }
	bool isQueued(int iId) const { return inRange(iId) && (m_aFlags[iId] & (unsigned char)FLAG_QUEUED) != 0; }
	void refresh(int iId);                         // re-derive m_aState[iId] from the formula

	std::vector<unsigned char> m_aState;           // tri-state per enum id -- the frontier the reads serve
	std::vector<short> m_aiEnable;                 // held sources enabling id
	std::vector<short> m_aiRemove;                 // held sources removing id (obsoletes/replaces/disables)
	std::vector<unsigned char> m_aFlags;           // FLAG_HELD | FLAG_STATIC_EXCLUDED
	bool m_bSeeded;
};

// The per-PLAYER enabler object (enabler.md par.7.1: player domains = techs, civics, projects, processes,
// hurries). Projects/processes are PLAYER-held even though cities build them (owner ruling): their axes are
// team/player-scope, so per-city copies would be byte-identical duplicated state that must never drift -- the
// city gate reads through its owner instead (a dynamic GET_PLAYER(getOwner()) lookup, conquest-safe; never a
// stored pointer), and the one city-local fact (the project map-category gate) stays a live check at the gate
// when the requires stage lands (the par.7.1 worker-builds split).
struct PlayerEnabler
{
	EnablerDomain techs;     // the researchable list -- maintained by TechEnabler (init + onTechChanged)
	EnablerDomain civics;    // the adoptable list -- maintained by CivicEnabler (init + onTechChanged)
	EnablerDomain projects;  // the creatable list -- maintained by ProjectEnabler (init + tech/project deltas)
	EnablerDomain processes; // the maintainable list -- maintained by ProcessEnabler (init + onTechChanged)
	EnablerDomain builds;    // the unlocked worker-builds set (par.7.1) -- maintained by BuildEnabler (init + onTechChanged)
	EnablerDomain traits;    // the learnable list -- maintained by TraitEnabler (init + onTraitChanged); the
	                         // ladder is the trait's own `enables.traits` edge, so a rung lists when the one
	                         // beneath it is held -- no authored rank, no gate beside it
	EnablerDomain promotions; // the unlocked-promotions set (par.7.1) -- maintained by PromotionEnabler (init + onTechChanged); the per-unit gate overlays its planes at level-up

	void reset() { techs.reset(); civics.reset(); projects.reset(); processes.reset(); builds.reset(); promotions.reset(); traits.reset(); }
};

// The per-CITY enabler object (enabler.md par.7.1: city domains = buildings, units -- the domains with
// genuinely city-local HAVE axes). EACH CITY owns its own instance. The domain arrays are the ONLY mutable
// state (par.7.1) -- every HAVE-event applies its source's edge deltas DIRECTLY (the event carries the delta;
// a thin event is an emit-surface gap to fix, never a license for side state), and the load seed replays the
// same appliers over the object-owned has-lists.
struct CityEnabler
{
	EnablerDomain buildings;   // the constructible list -- maintained by BuildingEnabler (seed + on*Changed)
	EnablerDomain units;       // the trainable list -- maintained by UnitEnabler (seed + on*Changed)

	void reset() { buildings.reset(); units.reset(); }
};

#endif // CV_ENABLER_H
