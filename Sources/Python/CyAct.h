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

	// Create a unit for this player at (iX, iY). ⚖ It earns its place the way this surface requires -- by
	// EXISTING call sites that need it, several of them: the WarPrizes capture in CvEventManager.onCombatResult,
	// the CRUSADE and BIODOME wonder spawns in onCityDoTurn, and CaptureSlaves' captive. All are existing
	// gameplay being kept working, never new logic authored in script ([roadmap] scope decision 6: where the
	// write a handler needs is not published yet, the answer is to ADD it to the surface that already exists).
	// Answers the NEW UNIT'S ID so the caller can go on reading it through CyState by (iPlayer, id); -1 if the
	// unit type or the plot was invalid.
	// ⛔ The BIRTHMARK is drawn from the SYNCHRONIZED stream, exactly as every engine caller draws it, and
	// exactly on the paths the legacy binding drew it on -- NOT on the two invalid ones. The number of draws and
	// their order are shared save state ([DEC-synced-rng-is-shared-state]), so adding or skipping one desyncs
	// multiplayer and stops a save replaying; this is not a detail to tidy.
	int createUnit(int iPlayer, int iUnitType, int iX, int iY, int iUnitAI, int iDirection) const;

	// Give a freshly-created unit the experience a unit BUILT in this city would start with. Its call site is the
	// CRUSADE wonder's per-turn spawn, which has always handed the new crusader the city's production XP -- so
	// this keeps existing behaviour working rather than authoring any.
	bool addUnitProductionExperience(int iPlayer, int iCity, int iUnit, bool bConscript) const;

	// ---- The UNIT actions the COMBAT / unit-lifecycle handlers need ----
	// ⚖ Each earns its place the way this surface requires -- by an EXISTING call site that needs it
	// ([roadmap] scope decision 6: where the write a handler needs is not published yet, ADD it to the surface
	// that already exists; "the write surface is parked" is explicitly not a reason to skip a fix). The call
	// sites are CvEventManager's onCombatResult / onUnitKilled / onUnitCreated and the two DancingHoskuld
	// combat mods -- all EXISTING gameplay being kept working, never new logic authored in script.
	bool finishUnitMoves(int iPlayer, int iUnit) const;
	// Kill a unit; bDelay is the engine's delayed-death flag ([unit-lifecycle.md]). ⛔ With bDelay false the
	// object is deleted immediately, so an iterating caller must hold a snapshot.
	bool killUnit(int iPlayer, int iUnit, bool bDelay, int iByPlayer) const;
	bool setUnitDamage(int iPlayer, int iUnit, int iDamage, int iByPlayer) const;
	// CvUnit::convert -- carry a unit's identity/XP/promotions onto another and (optionally) kill the original.
	// The RESPAWN handler's call: the reborn unit inherits what the dead one was.
	// ⚠ bKillOriginal DELETES the source, so the caller must not touch it afterwards ([unit-lifecycle.md]).
	bool convertUnit(int iPlayer, int iUnit, int iFromPlayer, int iFromUnit, bool bKillOriginal) const;
	// CvUnit::changeExperience. ⚠ The HUMAN-scale entry point: XP is stored x100 and changeExperience multiplies
	// on the way in, so a caller passes whole levels of XP exactly as the legacy script did -- do NOT hand it a
	// x100 value ([special-systems.md]).
	// Absolute twin of changeUnitExperience -- the engine publishes both, so a caller that means SET says set.
	bool setUnitExperience(int iPlayer, int iUnit, int iExperience) const;
	bool changeUnitExperience(int iPlayer, int iUnit, int iChange, int iMax,
							  bool bFromCombat, bool bInBorders, bool bUpdateGlobal) const;
	bool setUnitName(int iPlayer, int iUnit, std::wstring szName) const;
	// A unit's LEADER attachment (the subdued/tamed-animal beastmaster art link), -1 to clear.
	bool setUnitLeaderUnitType(int iPlayer, int iUnit, int iLeaderUnitType) const;

	// ⚖ THE IMMOBILE TIMER IS A STATUS NOW, so this is the STATUS verb rather than a revived setImmobileTimer:
	// "immobilised for N turns" is `STATUS_PARALYZED` on the per-scope status store ([state.md]: a status is a
	// counter applied, ticked down, over at zero -- and `CvUnit::setStatus` is its ONE write path, which is what
	// makes the 0-crossing announce). ⛔ Do not add a per-timer verb beside it; a new status is a new enum member.
	bool setUnitStatus(int iPlayer, int iUnit, int iStatus, int iTurns) const;
	// The unit's SCRIPT DATA -- the write sibling of CyState::getUnitScriptData, and the exact twin of
	// setCityScriptData above. Python-authoritative gameplay keeps its own per-unit state here (SdToolKit), and
	// with an identity-tuple payload a handler has no handle to call setScriptData on.
	bool setUnitScriptData(int iPlayer, int iUnit, std::string szData) const;

	// ---- The two remaining non-unit writes those handlers need ----
	// ⚠ changePlayerGold is a DELTA, and deliberately so: the WarPrizes booty moves gold between two players, so
	// an absolute setter would make every caller read-modify-write a value another handler may have moved.
	bool changePlayerGold(int iPlayer, int iChange) const;
	// ⚠ ADDITIVE, and distinct from setCityCulture above -- the Alamo grant ADDS to whatever the city holds.
	// Culture is int64_t and ×100 on both sides ([culture-religion-research.md]: it accumulates and never decays,
	// which is why it is 64-bit at all), so the delta is int64_t too.
	bool changeCityCulture(int iPlayer, int iCity, int iForPlayer, int64_t iChange, bool bPlots) const;

	// ---- The SCENARIO APPLY: what WorldBuilder puts back when it reads a .CivBeyondSwordWBSave ----
	// ⚖ These earn their place the way this surface requires -- by existing call sites that need them
	// (`CvWBDesc.CvCityDesc.apply`). They keep EXISTING behaviour working; nothing here is game logic authored
	// in script, and the scenario format is the same one the engine has always round-tripped.
	// ⛔ WorldBuilder is NOT a lower tier of consumer, and its breakage is not accepted (owner: "we cannot
	// accept actually breaking worldbuilder stuff, we fix things we see" -- [roadmap] scope decision 1b).
	// ⚠ WB mutates ARBITRARY state directly, which is exactly why it goes through the engine's own setters
	// here: each one emits the fact the normal path emits, so no cache, context or enabler set is left
	// describing a world that no longer exists ([roadmap] 1b: WB adding or removing anything EMITS, with no WB
	// special case anywhere).
	bool setCityName(int iPlayer, int iCity, std::wstring szName) const;
	// ⛔ BOTH shapes are published for population and stored food because the ENGINE has both, and a caller that
	// means a DELTA must be able to say so. Making it read-then-write instead would turn one atomic mutation into
	// two steps that another consumer can interleave -- a different operation wearing the same name.
	bool setCityPopulation(int iPlayer, int iCity, int iPopulation) const;
	bool changeCityPopulation(int iPlayer, int iCity, int iChange) const;
	bool setCityStoredFood(int iPlayer, int iCity, int iFood) const;
	// bHandleGrowth defaults FALSE in the engine, matching every caller here: an event handing a city food is
	// topping up the store, not resolving a growth step this instant.
	bool changeCityStoredFood(int iPlayer, int iCity, int iChange) const;
	// The whip-anger countdown a demolition/event charges (CvCity::changeHurryAngerTimer).
	bool changeCityHurryAngerTimer(int iPlayer, int iCity, int iChange) const;
	// Moves SPENT, in move points -- the partial-moves sibling of finishUnitMoves.
	bool setUnitMoves(int iPlayer, int iUnit, int iMoves) const;
	// The city's culture HELD BY ONE PLAYER. ⚠ It is ×100 and 64-bit on both sides -- the exact twin of
	// CyState::getCultureForPlayer, so a scenario round-trips the value it was handed rather than a rescaled one
	// ([culture-religion-research.md]: city culture accumulates the ×100 rate and never decays, which is why it
	// is int64_t at all).
	bool setCityCulture(int iPlayer, int iCity, int iForPlayer, int64_t iCulture) const;
	bool setCityScriptData(int iPlayer, int iCity, std::string szData) const;
	// PRESENCE of a building in this city, both directions. ⛔ ONE bool-parameterized verb, because the ENGINE
	// models it as one (CvCity::changeHasBuilding) -- an add-only verb with a remove twin bolted beside it would be
	// two Python spellings of a single transition, and the two drift ([DEC-single-implementation]).
	// ⚑ The removal leg is NOT a field poke: it runs the ledger, the setup and processBuilding(-1), so the
	// contribution is withdrawn and the domain fact fires exactly as a demolition in-game does.
	bool setCityBuilding(int iPlayer, int iCity, int iBuilding, bool bNewValue) const;
	// The city CEASES TO EXIST. ⛔ Routed through CvPlayer::disband -- the same path TASK_DISBAND takes -- and NOT
	// through CvCity::kill, because disband owns bookkeeping kill() does not: it clears foundedFirstCity for a
	// player losing their last city, and registers the name in the destroyed-city registry.
	// ⚑ Every Python caller used to reach bare kill(), so each was silently skipping both.
	bool disbandCity(int iPlayer, int iCity) const;
	bool setCityReligion(int iPlayer, int iCity, int iReligion, bool bHolyCity) const;
	bool setCityCorporation(int iPlayer, int iCity, int iCorporation, bool bHeadquarters) const;
	bool addCityFreeSpecialist(int iPlayer, int iCity, int iSpecialist, int iChange) const;
	bool pushCityOrder(int iPlayer, int iCity, int iOrderType, int iId) const;
	bool setCityDefenseDamage(int iPlayer, int iCity, int iDamage) const;
	bool setCityOccupation(int iPlayer, int iCity, int iTurns) const;
	// The one-shot EVENT/VOTE grant store -- the twin of CyState's getCityGrantedExtras. A scenario that could
	// read them and not write them back would drop them on every round trip.
	bool setCityGrantedExtra(int iPlayer, int iCity, int iKind, int iValue) const;
	bool setBuildingGrantedYield(int iPlayer, int iCity, int iBuilding, int iYield, int iValue) const;
	bool setBuildingGrantedCommerce(int iPlayer, int iCity, int iBuilding, int iCommerce, int iValue) const;
	bool setBuildingGrantedWellbeing(int iPlayer, int iCity, int iBuilding, int iKind, int iValue) const;

	static void pythonPublish();
};

#endif // CyAct_h__
