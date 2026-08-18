#pragma once

#ifndef CyState_h__
#define CyState_h__

#include <string>

//
//	CyState -- the RESIDUE of the flat live-state surface, and it is BEING DISSOLVED (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (accessor homing)).
//
//	⛔ NOTHING NEW GOES HERE. A game object's data is read from that object's OWN accessor -- a city's from
//	CyCity, an empire's from CyPlayer -- and the mechanical test is the method NAME: the moment a name carries a
//	DIFFERENT object's noun (getCityPopulation(owner, id)) it is homed wrong by construction, because an accessor
//	that owns its subject needs no noun in its verbs. The CITY plane has moved; what stands here is what has no
//	accessor to move to YET, and each of those is the NEXT pass rather than a sanctioned residue:
//	  - the UNIT plane -- units are the deliberate FUTURE role-specific scope (contexts.md), and CyUnit carries
//	    only its identity set today, so there is nowhere to home these until that pass is taken;
//	  - a few PLAYER-scope stragglers awaiting the player pass;
//	  - genuinely GLOBAL facts (the active player, the game turn, the constants block, the live defines) and the
//	    current SELECTION -- these name no game object, so no accessor owns them and they are not misplaced.
//
//	⚑ WHY IT IS ADDRESSED BY (owner, id) AT ALL. The engine->Python CALLBACK direction is KEPT (patterns.md: the
//	cut is DIRECTIONAL), and an event payload hands over the identity PAIR rather than a handle
//	(Cy::PyIdentity, CvPython.h) -- so a handler is given ids and nothing else. A consumer holding a pair reaches
//	the accessor through the published chain, GC.getPlayer(owner).getCity(id), which is the point rather than a
//	cost: it STATES where the value comes from instead of flattening containment into an argument pair.
//
//	THE GRAMMAR the surviving group reads obey is the game-object half's, unchanged:
//	  - ONE READ PER GROUP, and the getter IS the group. There is NO scalar getter per channel; a script wanting
//	    one value indexes the returned list, so the surface grows by GROUPS, never by channels.
//	  - THE EXISTING ENGINE ENUM INDEXES THE RESULT, never the call. A family with no engine enum is indexed by
//	    its own kind enum, and the name says so (get<Family>Kinds).
//
//	⛔ EVERY VALUE IS x100 NATIVE (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)) -- there is no `100` in any name, no scale variant, and
//	no getter reduces. A script divides by 100 at the point it displays or compares against a whole game count.
//	A PERCENT is NOT scaled, so a percent-unit channel is already the number you want.
//
//	⛔ EVERY READ IS A BARE FETCH -- nothing here gates, ensures or recomputes, exactly as on the C++ side, so a
//	missed invalidation shows up in script as a visibly wrong number rather than being silently repaired at the
//	boundary (state-repositories.md; docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT).
//
//	BOOST: this file uses ONLY the `python::` alias, never a bare `boost::` and never a using-directive -- the
//	tree carries TWO Boosts and an unqualified name can resolve to the wrong one through the PCH (engine.md).
//

class CyState
{
public:
	CyState() {}

	// ---- THE CURRENT SELECTION, as an IDENTITY. ----
	// ⛔ Asked of the library, NOT of the EXE's CyInterface. getHeadSelectedCity/Unit there hand back a Cy*
	// HANDLE, and those wrappers carry ZERO defs -- so a script is given an object it cannot ask anything, not
	// even which one it is. That return type is the closed EXE's and cannot be changed, so the only way the
	// selection is reachable at all is to answer it HERE, in the (owner, id) pair every read on this surface
	// takes. It is the same identity a callback hands over (Cy::PyIdentity), from the other direction.
	// ⚠ Answers [-1, -1] when nothing is selected -- a real owner/id pair is never negative, so a caller tests
	// the id rather than inferring emptiness from a missing value.
	python::list getHeadSelectedCityId() const;
	python::list getHeadSelectedUnitId() const;
	// The WHOLE selection, as [owner, id] pairs -- what a stack panel iterates. CyInterface::getSelectionUnit
	// hands back a CyUnit the script cannot read, so the list is answered here for the same reason the head
	// selection is.
	python::list getSelectedUnitIds() const;

	// The UNIT twin. ⚠ A unit's position is part of the IDENTITY SET on the handle
	// ([patterns.md] THE IDENTITY SET: owner, id, POSITION), but an EVENT PAYLOAD carries only (owner, id) --
	// so a handler that was handed a payload has no handle to ask and needs this. Answers (-1, -1) for a unit
	// that does not resolve OR is off-map, which is a real state and not only a save defect
	// ([unit-lifecycle.md] THE OFF-MAP UNIT).
	python::list getUnitPosition(int iPlayer, int iUnit) const;
	int getGreatPeopleThresholdNonMilitary(int iPlayer) const;

	// How many of a building the empire holds. ⚠ It replaces the DELETED CvPlayer::getBuildingCountWithUpgrades,
	// and the difference is a stated behaviour change ([validation.md]: the spec leads, a change is named rather
	// than hidden): that one also counted the building's upgrade-chain predecessors, this one counts the building.
	// The engine accessor it wraps is the surviving plain count.
	int getBuildingCount(int iPlayer, int iBuilding) const;

	// ---- THE UNIT PLANE ----
	// ⛔ Addressed by the same (owner, id) pair as everything else. CyUnit carries ZERO defs, so a script handed
	// one -- by a callback or by CyPlot::getUnit -- can ask it nothing; the ids are the only way a unit is
	// reachable at all, which is why the plot enumeration below is a prerequisite rather than a convenience.
	python::list getUnitRead(int iPlayer, int iUnit) const;
	python::list getUnitFlags(int iPlayer, int iUnit) const;
	std::wstring getUnitName(int iPlayer, int iUnit) const;
	// The unit's CUSTOM name only -- empty when it carries none, unlike getUnitName which falls back to the
	// type's description. A scenario writes this one, because a fallback name is not authored data.
	std::wstring getUnitNameNoDesc(int iPlayer, int iUnit) const;
	std::string getUnitScriptData(int iPlayer, int iUnit) const;
	// The units standing on a plot, as [owner, id] pairs, in the engine's own plot order -- what a plot list
	// iterates. ⚠ Answers EVERY unit present; the caller applies its own visibility test below, because
	// visibility is per-OBSERVER and the list is drawn for one team.
	python::list getPlotUnitIds(int iX, int iY) const;
	// The SELECTOR predicates: each asks about a PAIR, so the subject is in the call rather than a flag slot.
	bool isUnitInvisible(int iPlayer, int iUnit, int iTeam) const;
	bool hasUnitPromotion(int iPlayer, int iUnit, int iPromotion) const;
	// ⛔ The LIVE per-unit verdict, NOT the type's skill block. hiddenNationality is promotion-grantable
	// (PROMOTION_PROUD_PIRATE, [skills.md]), so `INFO.isHiddenNationality(unitType)` answers a DIFFERENT question
	// and silently misses every unit that earned it -- which is why this is a STATE read and named for the unit.
	bool isUnitHiddenNationality(int iPlayer, int iUnit) const;
	// ⛔ A unit whose death is DECIDED but not yet performed still answers TRUE here, and a consumer iterating
	// a player's units must skip it: combat holds raw pointers across the exchange, so a killed unit stays a
	// live object until the delayed-death pass reaps it ([unit-lifecycle.md]). Anything listing or counting
	// units without this shows the dead ones.
	bool isUnitDead(int iPlayer, int iUnit) const;
	// The owner a VIEWER sees -- which differs from the real owner for a hidden-nationality unit. A log or a
	// message must use this one, or it names the civ the mechanic exists to conceal.
	int getUnitVisualOwner(int iPlayer, int iUnit) const;
	// Base combat strength on the HUMAN scale. ⛔ Deliberately NOT baseCombatStr(): that one returns x100 under
	// GAMEOPTION_COMBAT_SIZE_MATTERS and human without it, so its scale depends on live game state and no caller
	// can reason about it ([fixed-point-and-scales.md] §4c-ter). This is the boundary read, always human.
	int getUnitBaseCombatStr(int iPlayer, int iUnit) const;
	// A plot query asked RELATIVE TO A UNIT (whose owner decides who counts as an enemy). It takes the unit's
	// identity + the tile rather than a unit handle, because a unit reaches Python as (owner, id).
	int getNumVisiblePotentialEnemyDefenders(int iPlayer, int iUnit, int iX, int iY) const;
	// ⛔ The unit's HELD promotions, as ONE list. Asking per-promotion instead costs a boundary crossing per
	// PROMOTION per unit per redraw, and the registry is ~1500 entries -- a stack panel then spends tens of
	// thousands of crossings a frame. The per-promotion read above stays for a single POINTED question.
	// ⚠ Overridden promotions are excluded: a held-but-overridden promotion is not one the unit HAS in any sense
	// a display cares about, and every caller was already filtering them out by hand.
	python::list getUnitPromotions(int iPlayer, int iUnit) const;
	bool isUnitPromotionOverridden(int iPlayer, int iUnit, int iPromotion) const;
	// Does this unit hold that COMBAT CLASS? Its primary + subs + promotion-granted, composed -- the same
	// question CvUnit::isHasUnitCombat answers, asked by (owner, id) like every other read here.
	bool hasUnitCombat(int iPlayer, int iUnit, int iUnitCombat) const;
	// CAN this unit take that promotion right now? The per-unit applicability leg the enabler evaluates on
	// demand at level-up ([enabler.md] par.7.1 carve-out) -- a POINTED question about one candidate.
	bool canUnitAcquirePromotion(int iPlayer, int iUnit, int iPromotion) const;
	// ⚖ THE SIBLING, AND THE TWO ARE NOT INTERCHANGEABLE. canUnitAcquirePromotion answers the LEVEL-UP question;
	// this answers whether a promotion may be APPLIED to the unit at all, which is the gate a GRANT uses -- a
	// free promotion bypasses tech prereqs ([special-systems.md]). ⛔ Gating a grant on the acquire read instead
	// refuses every promotion an EVENT hands out, since those sit outside the normal list and cannot be taken by
	// XP at all (owner).
	bool isUnitPromotionValid(int iPlayer, int iUnit, int iPromotion) const;
	bool isUnitActionRecommended(int iPlayer, int iUnit, int iAction) const;
	// Can this unit become that one -- a PAIR question (this unit, that target type), so the target is in
	// the call. bTestVisible asks the display question ("show the button") rather than the strict one.
	bool canUnitUpgrade(int iPlayer, int iUnit, int iToUnit, bool bTestVisible) const;
	// ⛔ "Can this unit upgrade to ANYTHING?" is ONE question and therefore ONE read. Asking it by looping the
	// whole unit registry from script costs a boundary crossing PER UNIT TYPE, per unit, per redraw -- and a
	// boost::python call costs far more than the lookup inside it ([patterns.md]). The walk belongs here.
	bool canUnitUpgradeToAny(int iPlayer, int iUnit) const;

	// ---- Plain live FACTS: genuine lone values, so they stay bare typed reads (patterns.md category 4), never
	// forced into a group that would mean nothing. ----
	int getActivePlayer() const;
	int getGameTurn() const;
	// Is this player still in the game, and whose team are they on. Both are asked constantly by anything that
	// walks the player range, and neither belongs to a group -- they are lone facts about the slot itself.
	bool isPlayerAlive(int iPlayer) const;
	int getPlayerTeam(int iPlayer) const;
	//	What THIS TEAM actually pays for a tech, in beakers. COMPUTED GAME STATE, not info data: the authored
	//	base (CyInfo PYINT_COST) is scaled by gamespeed, the tech's era, the handicap and the team's member
	//	count ([culture-religion-research.md] Tech cost). ⛔ It belongs here rather than on the info plane
	//	precisely because an info never reads game state ([json.md] §9) -- the two sit side by side, and a
	//	consumer showing "what will this cost ME" wants this one ([pedia-read-map.md] finding 5).
	int getTechResearchCost(int iTeam, int iTech) const;
	bool isFinalInitialized() const;   // is the game up enough to be asked / shown a message
	// The closed CONSTANTS block (python-read-map: a small closed set, trivially served by the library).
	// Compile-time engine limits, so they are bare reads with no owner and no scope.
	int getMAX_PLAYERS() const;
	int getMAX_PC_PLAYERS() const;
	int getMAX_TEAMS() const;
	int getMAX_PC_TEAMS() const;
	int getBARBARIAN_PLAYER() const;

	// The global DEFINES. They sit on the live-state half rather than the vocabulary because a define is a
	// LIVE option -- user-changeable mid-game through the BUG bridge, which is exactly why nothing STATIC may be
	// gated on one (python-read-map). The READS are in scope; the writes are not.
	int getDefineINT(const std::string& szName) const;
	float getDefineFLOAT(const std::string& szName) const;
	int getAIAutoPlay(int iPlayer) const;
	// The empire's colour on the map (and on its unit sprites), resolved to a COLOR_ id in ONE crossing. The
	// legacy shape was a two-hop -- ask the player for its PLAYERCOLOR_, then ask that info for its primary --
	// which is plumbing rather than a question: every consumer wants the colour, none wants the intermediate.
	// ⚠ It is the PRIMARY only, because that is what the live consumers draw with; a secondary read is added
	// when something actually wants one, never mirrored ahead of demand.
	// ⛔ Deliberately NOT a slot on the generic intrinsic plane: this value belongs to ONE registry, and that
	// plane answers -1 for a prefix it was never wired for, which reads as a legitimate "no colour"
	// ([patterns.md] -- an opaque slot re-creates the fault it was meant to cure).
	int getPlayerColorPrimary(int iPlayer) const;
	std::wstring getPlayerName(int iPlayer) const;

	static void pythonPublish();
};

#endif // CyState_h__
