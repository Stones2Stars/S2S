#pragma once

#ifndef CyEnabler_h__
#define CyEnabler_h__

//
//	CyEnabler -- the Python AVAILABILITY surface: the "can I, right now?" half of the GAME-OBJECT read role
//	(enabler.md par.8), exposed to script.
//
//	⛔ THIS IS A NEW SURFACE, NOT A WIDENED BINDING ([DEC-cy-not-fixed]). The legacy per-type wrappers
//	(CyCity::canTrain, CyPlayer::canConstruct, ...) are NOT extended and NOT reused: bolting availability onto
//	them is precisely how the boundary gets shaped by the old contract instead of by the model. It is therefore
//	deliberately ID-BASED -- it takes plain player / city / entity ids and holds no CyCity or CyPlayer -- so the
//	new surface has no dependency on the old one and the legacy wrappers can be CUT AWAY without touching it.
//
//	THE GRAMMAR mirrors the C++ side exactly, one read pair per DOMAIN:
//	  - a TRI-STATE verdict (EnablerState: ENABLER_HIDDEN / GREYED / LISTED), returned WHOLE rather than reduced
//	    to a bool, because HIDDEN ("not in the tree at all") vs GREYED ("in the tree, requirements unmet") is the
//	    "why not" a build-list needs. A script wanting a plain boolean tests == ENABLER_LISTED.
//	  - the FRONTIER as a plain Python list of ids -- the small offered set a script iterates INSTEAD of walking
//	    the whole entity database (enabler.md par.6).
//
//	⛔ NO WHAT-IF ARGUMENTS. Every read is a bare O(1) fetch of the maintained tri-state: no gate runs, no
//	calculator is called, `requires` is never evaluated. A script asking "could I if I had X?" is asking the
//	PICKING LOGIC's question, not availability's (enabler.md par.8), and answering it here would drag the legacy
//	nine-argument shape back across the boundary.
//	⛔ NO player-level train/construct VERDICT exists: construction and training are CITY concerns, because the
//	gate needs the city-local supply (what is in VICINITY, and in the PLOT GROUP). The empire-wide question is
//	the explicitly-named FAN, which walks the player's cities.
//
//	BOOST: this file uses ONLY the `python::` alias (= boost::python, the 1.32 compiled bridge -- the ONE Boost
//	that can host the Py2.4 binding). It never writes a bare `boost::` and never opens a using-directive: the
//	tree carries TWO Boosts (1.32 as `boost::`, header-only 1.55 as `boost155::`), and an unqualified `bind` or
//	`function` can silently resolve to the wrong one through the PCH (engine.md). Qualify, always.
//

class CyEnabler
{
public:
	CyEnabler() {}

	// ---- CITY domains: construction + training, on one plane ----
	int getBuildingAvailability(int iPlayer, int iCity, int /*BuildingTypes*/ eBuilding) const;
	int getUnitAvailability(int iPlayer, int iCity, int /*UnitTypes*/ eUnit) const;
	bool isBuildingContinuable(int iPlayer, int iCity, int /*BuildingTypes*/ eBuilding) const;
	python::list getAvailableBuildings(int iPlayer, int iCity) const;
	python::list getAvailableUnits(int iPlayer, int iCity) const;

	// ---- PLAYER domains ----
	int getTechAvailability(int iPlayer, int /*TechTypes*/ eTech) const;
	int getCivicAvailability(int iPlayer, int /*CivicTypes*/ eCivic) const;
	int getProjectAvailability(int iPlayer, int /*ProjectTypes*/ eProject) const;
	int getProcessAvailability(int iPlayer, int /*ProcessTypes*/ eProcess) const;
	python::list getAvailableTechs(int iPlayer) const;
	python::list getAvailableCivics(int iPlayer) const;
	python::list getAvailableProjects(int iPlayer) const;
	python::list getAvailableProcesses(int iPlayer) const;

	// ---- the two CARVE-OUTS (enabler.md par.7.1): the UNLOCKED half only. A build's plot-validity and a
	// promotion's per-unit applicability are evaluated LIVE at their decision points, so a script treating
	// these as the whole verdict will over-offer.
	int getBuildUnlocked(int iPlayer, int /*BuildTypes*/ eBuild) const;
	int getPromotionUnlocked(int iPlayer, int /*PromotionTypes*/ ePromotion) const;

	// ---- the empire-wide FAN: walks the player's cities and returns the BEST state any of them holds, as an
	// EnablerState. It is not a player-scope verdict (there is none -- construction and training are city
	// concerns) and it is deliberately not reduced to a bool: a script wanting "offered somewhere" compares
	// == ENABLER_LISTED, one wanting "in the tree somewhere" compares >= ENABLER_GREYED.
	int getUnitAvailabilityAnywhere(int iPlayer, int /*UnitTypes*/ eUnit) const;
	int getBuildingAvailabilityAnywhere(int iPlayer, int /*BuildingTypes*/ eBuilding) const;

	//	---- CAN-I-EVER: is this entity barred for the WHOLE GAME by its entity-level gate?
	//	⛔ A DIFFERENT QUESTION FROM THE TRI-STATE, not a variant of it. The tri-state answers CAN-I-NOW and is
	//	per (player, city); this answers whether a game option bars the thing outright, so the verdict is the
	//	SAME for every player and city and takes no owner ([enabler.md] par.8: where the bar IS an entity gate,
	//	the ever-question is the enabler's, and so is the option read).
	//	⚑ Parameterized over the EDGE BUCKET rather than split per domain, exactly as the kernel is -- so it
	//	answers for domains that are not enabler DOMAINS at all (corporations, religions), which is precisely
	//	what the removed `CyGame::canEverSpread` was asking.
	//	⚠ It is TOTAL: a type whose data authors no gate answers "never barred", so a newly-authored gate lights
	//	up as pure DATA with no engine change.
	bool isEverAvailable(int /*EnEdgeBucket*/ eBucket, int iId) const;
	// CAN-I-EVER for a TECH. ⛔ NOT served by isEverAvailable: a tech's permanent bar is a COMPOSITION, not an
	// entity gate (enabler.md par.8), so this delegates to the picking logic's own single implementation.
	bool canEverResearch(int iPlayer, int iTech) const;

	// Publishes this surface + the three tri-state constants a script compares against
	// (ENABLER_HIDDEN / ENABLER_GREYED / ENABLER_LISTED). Called from DLLPublishToPython.
	// The BONUSES a corporation's `requires` names -- the forward "what does this need?" read, answered from the
	// compiled condition tree by the ONE shared walker ([DEC-single-implementation]) rather than by a caller's
	// own recursion. ⚑ Its consumer is the trade screen: a corporation makes bonuses SEMI-VOLUMETRIC (it consumes
	// copies), so a player still wants more of a resource they already hold -- the one case where presence is not
	// the answer. The desired-bonus set is (theirs - mine) UNION this, so this term needs no holdings context.
	python::list getRequiredBonuses(int iCorporation) const;

	static void pythonPublish();
};

#endif // CyEnabler_h__
