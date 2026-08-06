#pragma once

#ifndef CyAct_h__
#define CyAct_h__

//
//	CyAct -- the Python ACTION surface: the things script ASKS THE ENGINE TO DO, as opposed to the things it
//	reads. Sibling of CyState ("what do I HAVE?"), CyInfo ("what do I CARRY?") and CyEnabler ("can I?").
//
//	⛔ WHY THIS EXISTS AT ALL. The read library is deliberately READ-ONLY ([patterns.md] THE PYTHON READ
//	BOUNDARY: data fetching, not gameplay), and that is right -- but it left script with no way to ask the
//	engine for anything, and some of what script does is not a read. The interface has to be able to say
//	"select this city"; that is not a value it can fetch.
//
//	⛔ ID-BASED, LIKE EVERY OTHER SURFACE HERE. An action names its subject by the (owner, id) pair, never by a
//	Cy* handle -- the wrappers carry zero defs, so script has no handle to pass and the EXE's own accessors hand
//	back ones it cannot use. Taking ids is what makes an action expressible at all.
//
//	⚖ THE LINE THIS SURFACE DOES NOT CROSS (owner): developing GAME LOGIC in Python is banned; keeping the
//	existing logic working is not. So what belongs here is the engine ACTION a script asks for -- never a
//	gameplay rule re-expressed in script, and never a setter minted to let Python compute something the engine
//	should own. ⚠ A new verb earns its place by an existing call site that needs it, exactly as the value-struct
//	registrations do -- this is not a place to pre-emptively mirror the old mutating bindings.
//
//	BOOST: only the `python::` alias, never a bare `boost::` -- two Boosts coexist (engine.md).
//

class CyAct
{
public:
	CyAct() {}

	// Make this city the selected one (the city screen / camera follow it). The engine's own selectCity takes a
	// CvCity*, which script cannot hold -- so the pair is resolved here and the engine called on its behalf.
	// Answers whether the city resolved, so a caller can tell "did nothing" from "did it".
	bool selectCity(int iPlayer, int iCity, bool bTestProduction) const;

	// Select the group this unit belongs to (the plot-list click). The engine's selectGroup takes a CvUnit*,
	// which script cannot hold, so the pair is resolved here. The three modifier flags are the click's.
	bool selectUnitGroup(int iPlayer, int iUnit, bool bShift, bool bCtrl, bool bAlt) const;

	// ---- The city screen's LIST VIEW state: which filter/sort/grouping the player left its lists on. ----
	// ⚖ These are the one place this surface writes, and they are deliberately narrow: VIEW state, not
	// gameplay. What the owner ruled banned is DEVELOPING game logic in Python; keeping the existing logic
	// working is not ([roadmap] scope decision 6), and a list's sort order is not game logic by any reading --
	// it changes what the player SEES, never what the game does. The matching READS are on CyState, so the
	// lists both render and respond to a click through one coherent pair.
	bool setBuildingListFilterActive(int iPlayer, int iCity, int iFilter, bool bActive) const;
	bool setBuildingListSorting(int iPlayer, int iCity, int iSorting) const;
	bool setUnitListFilterActive(int iPlayer, int iCity, int iFilter, bool bActive) const;
	bool setUnitListGrouping(int iPlayer, int iCity, int iGrouping) const;
	bool setUnitListSorting(int iPlayer, int iCity, int iSorting) const;

	// Enable/disable a worker BUILD at runtime -- the in-game settings toggle. ⚖ Sanctioned by the enabler's own
	// design ([CvBuildEnabler]): "the isDisabled runtime toggle (Python settings scripts) stays a live check
	// beside the bare read". So this is a live OPTION the settings screens own, not gameplay authored in script.
	bool setBuildDisabled(int iBuild, bool bDisabled) const;
	// Mark a build list stale so the next read rebuilds it. This is the screen ASKING for work, which is why it
	// is an action and not folded into the read -- a read that rebuilt itself would be the self-healing shape
	// the whole surface avoids ([DEC-no-self-heal]).
	bool invalidateUnitList(int iPlayer, int iCity) const;
	bool invalidateBuildingList(int iPlayer, int iCity) const;

	// Give or take a unit's PROMOTION. ⚖ This earns its place the way this surface requires -- by an existing
	// call site that needs it: CvEventManager.onUnitPromoted redirects an AI's promotion pick, which is EXISTING
	// gameplay being kept working, not new logic authored in script. It routes through CvUnit::setHasPromotion,
	// so the promotion fact is emitted and the resolved plane re-gathers exactly as an engine-side promote does.
	bool setUnitPromotion(int iPlayer, int iUnit, int iPromotion, bool bNewValue) const;

	static void pythonPublish();
};

#endif // CyAct_h__
