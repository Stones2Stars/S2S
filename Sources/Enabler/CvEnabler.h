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
//	obsoleted-candidate edge case). The GATE (par.7.1 step 2) splits a tree member's state by the REASON it
//	stored (m_aGateReason, set by the domain enabler's gate-on-entry + EDGEF_REQUIRED_BY re-gates): a greyable
//	reason -> GREYED, a hide reason -> HIDDEN, none -> LISTED. The gate NEVER changes membership. A domain whose
//	gate stage has not landed simply never sets a reason -- every tree member LISTED (the enable-side over-offer).
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
		STATE_GREYED = 1,   // in CAN GET, a GREYABLE clause unmet -- the player is told what to go get
		STATE_LISTED = 2    // in CAN GET, gate passed (enable-side stage: every tree member)
	};

	// WHY the gate refused ([enabler.md] par.6: the gate that decides buildability yields the REASON, never a
	// bare bool -- otherwise a greyed entry hands the player, and the AI, a question instead of an answer).
	// ⛔ The reason is what is STORED; hide-vs-grey is READ OFF it (reasonHides) rather than being the whole of
	// the verdict, so the two consumers the tri-state serves are both answerable from one stored fact.
	//
	// ⛔ THE `requires` HALF IS ONE REASON PER ATOM KIND, NEVER ONE BUNDLED VERDICT (par.6). A requires tree
	// mixes kinds freely -- `all: [TECH_X, BONUS_Y]` is the ordinary shape -- so a single disposition cannot be
	// right for both halves: a missing BONUS is exactly the "go get copper" case grey exists for, while an
	// unresearched TECH is not something the asker can go and fetch.
	enum GateReason
	{
		GATEREASON_NONE = 0,      // the gate passed
		GATEREASON_DORMANT,       // a dormancy successor stands in this city (only-highest-active)
		GATEREASON_REPLACED,      // HIDE_REPLACED: a reachable successor supersedes it
		GATEREASON_OPTION,        // the entity-level enabled/disabled game-option gate (DEC-entity-gate)
		GATEREASON_CAP_SELF,      // its own `allowed` world/team/empire cap is consumed
		GATEREASON_CAP_GROUP,     // the SpecialBuilding group cap is consumed by a sibling
		GATEREASON_CAP_CATEGORY,  // this city's culture-level wonder-CATEGORY cap is full

		// ==== the `requires` family -- one entry per ATOM KIND, CONTIGUOUS ====
		// Contiguous so "is this a requires reason" is a range test (isRequiresReason) rather than a list every
		// consumer has to keep in step. ⛔ The kinds STAY DISTINCT even where two currently share a disposition
		// (par.6): collapsing later is one edit to reasonHides below, while pre-merging cannot be undone -- a
		// merged entry no longer records which kind it meant.
		GATEREASON_REQUIRES,              // an atom kind this vocabulary does not name (an unknown predicate)
		GATEREASON_REQUIRES_TECH,
		GATEREASON_REQUIRES_BUILDING,
		GATEREASON_REQUIRES_BONUS,
		GATEREASON_REQUIRES_CIVIC,
		GATEREASON_REQUIRES_RELIGION,
		GATEREASON_REQUIRES_CORPORATION,
		GATEREASON_REQUIRES_HERITAGE,
		GATEREASON_REQUIRES_UNIT,
		GATEREASON_REQUIRES_PROMOTION,
		GATEREASON_REQUIRES_POPULATION,   // the city is too small
		GATEREASON_REQUIRES_CITY_COUNT,   // an empire/team COUNT token (CITY / TEAM)
		// A PROPERTY_ band. Every band the data authors today is PURE DORMANCY on a queue-excluded carrier (188
		// atoms, all in `operate`, none on a constructible entity), so this reason is currently unreachable --
		// it is carried because a constructible entity gated on a band SPAN is a design the owner is keeping
		// open, and the kind is what lets that arrive as data rather than as an engine change.
		GATEREASON_REQUIRES_PROPERTY,
		GATEREASON_REQUIRES_CULTURELEVEL,
		GATEREASON_REQUIRES_VICTORY,      // a victory condition the game was not set up with
		GATEREASON_REQUIRES_TERRAIN,
		GATEREASON_REQUIRES_FEATURE,
		GATEREASON_REQUIRES_IMPROVEMENT,
		GATEREASON_REQUIRES_ROUTE,
		GATEREASON_REQUIRES_MAPCATEGORY,  // earth / space / ... -- DERIVED from the plot's terrain
		GATEREASON_REQUIRES_PLOT,         // the ground itself: river / coast / hills / latitude / nature yield
		GATEREASON_REQUIRES_LAST = GATEREASON_REQUIRES_PLOT
	};

	// The DISPOSITION, read off the reason -- a MAPPING OVER the kind (par.6), never a property stored per
	// entity. A clause the asker can act on GREYS (it names what to go get); one they cannot act on HIDES,
	// because leaving it in the list says "unavailable" and nothing more.
	// ⚠ A CAP is the worked case: a built world wonder can never be built again anywhere, so a greyed row for it
	// is pure noise -- there is no action it could prompt.
	static bool reasonHides(unsigned char eReason)
	{
		switch (eReason)
		{
		// HIDE -- nothing the asker can do reaches these. A TECH is not fetched (and it is the `enables` edge
		// besides, so before it lands the entity is normally not in the tree at all, par.2); the GROUND under a
		// city never changes, so a landlocked city greying a harbour forever is pure noise.
		case GATEREASON_DORMANT:
		case GATEREASON_REPLACED:
		case GATEREASON_OPTION:
		case GATEREASON_CAP_SELF:
		case GATEREASON_CAP_GROUP:
		case GATEREASON_CAP_CATEGORY:
		case GATEREASON_REQUIRES_TECH:
		case GATEREASON_REQUIRES_PROMOTION:
		case GATEREASON_REQUIRES_TERRAIN:
		case GATEREASON_REQUIRES_MAPCATEGORY:
		case GATEREASON_REQUIRES_PLOT:
		case GATEREASON_REQUIRES_VICTORY:
			return true;
		default:
			// GREY, and NONE. Staying VISIBLE is the safe direction, the same asymmetry par.5 draws for the
			// reverse index: an extra greyed row costs a line, a wrong HIDE costs the asker the answer entirely.
			return false;
		}
	}

	// Is this reason a `requires` clause (whatever its atom kind)? The range test the contiguous block above
	// exists for -- a consumer that renders WHICH atom is unmet asks this, never an equality against one entry.
	static bool isRequiresReason(unsigned char eReason)
	{
		return eReason >= (unsigned char)GATEREASON_REQUIRES && eReason <= (unsigned char)GATEREASON_REQUIRES_LAST;
	}

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
	// The GATE verdict (par.7.1 step 2): set by the domain enabler's gate evaluation (gate-on-entry + the
	// EDGEF_REQUIRED_BY re-gates). It carries WHY it failed, so a greyed candidate can say what is missing and a
	// hide-clause can stop occupying the list. Membership itself is untouched -- the gate never adds or removes
	// a candidate (par.1).
	void setGateReason(int iId, unsigned char eReason);
	// WHY this candidate is not offered (GATEREASON_NONE = it is). The tooltip's "what is needed" read.
	unsigned char gateReason(int iId) const;
	// The QUEUED overlay (par.7.1 step 3 / par.8): a building currently in this city's production queue leaves the
	// FRESH OFFER (listed / listedIds) but stays CONTINUABLE (listedForContinue) and in-tree. A read-time overlay,
	// NOT a gate reason -- set from the live getFirstBuildingOrder read on SEVT_CITY_ORDER_CHANGED (+ the load-end
	// gate pass). Split out from the gate REASON precisely so bContinue can tell "queued" from "gate refused".
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
	// IN CAN GET -- the MEMBERSHIP verdict, which is what "in tree" means (par.1: the gate never changes
	// membership). ⛔ It is deliberately NOT `state() >= GREYED` any more: a HIDE-clause candidate reports
	// STATE_HIDDEN while remaining a tree member, and the domain enablers guard their re-gate loops on this read
	// -- so deriving it from the display state would freeze such a candidate out of every future re-gate and it
	// could never come back when its cap frees or its game option flips.
	bool inTree(int iId) const;
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
	// FLAG_IN_TREE is the stored MEMBERSHIP verdict, written by refresh() alongside the display state. It exists
	// because the two genuinely differ once a hide-clause is in play, and inTree() must answer the membership
	// half ([DEC-single-implementation]: one formula, one place -- refresh() applies it and both reads take it
	// from there rather than each re-deriving it).
	enum { FLAG_HELD = 1, FLAG_STATIC_EXCLUDED = 2, FLAG_IN_TREE = 4, FLAG_QUEUED = 8 };

	bool inRange(int iId) const { return iId >= 0 && iId < (int)m_aState.size(); }
	bool isQueued(int iId) const { return inRange(iId) && (m_aFlags[iId] & (unsigned char)FLAG_QUEUED) != 0; }
	void refresh(int iId);                         // re-derive m_aState[iId] from the formula

	std::vector<unsigned char> m_aState;           // tri-state per enum id -- the frontier the reads serve
	std::vector<short> m_aiEnable;                 // held sources enabling id
	std::vector<short> m_aiRemove;                 // held sources removing id (obsoletes/replaces/disables)
	std::vector<unsigned char> m_aFlags;           // FLAG_HELD | FLAG_STATIC_EXCLUDED | FLAG_IN_TREE | FLAG_QUEUED
	std::vector<unsigned char> m_aGateReason;      // GateReason per id -- WHY the gate refused
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
