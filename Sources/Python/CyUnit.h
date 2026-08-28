#pragma once

#ifndef CyUnit_h__
#define CyUnit_h__

//
// Python wrapper class for CvUnit
//

class CvUnit;
class CyArea;
class CyPlot;
class CySelectionGroup;

class CyUnit
{
public:
	DllExport explicit CyUnit(CvUnit* pUnit);		// Call from C++

	CvUnit* getUnit() const { return m_pUnit; };	// Call from C++

	void NotifyEntity(int /*MissionTypes*/ eMission);

	bool canEnterPlot(const CyPlot& kPlot, bool bAttack, bool bDeclareWar, bool bIgnoreLoad) const;
	bool isAutoUpgrading() const;
	bool isAutoPromoting() const;

	int /*TechTypes*/ getDiscoveryTech() const;
	int getDiscoverResearch(int /*TechTypes*/ eTech) const;
	int getHurryProduction(const CyPlot& kPlot) const;
	bool canTrade(const CyPlot& kPlot, bool bTestVisible) const;
	int getGreatWorkCulture(const CyPlot& kPlot) const;


	int /*HandicapTypes*/ getHandicapType() const;
	int /*CivilizationTypes*/ getCivilizationType() const;
	int /*SpecialUnitTypes*/ getSpecialUnitType() const;
	int /*UnitCombatTypes*/ getUnitCombatType() const;
	DomainTypes getDomainType() const;

	bool isNPC() const;
	bool isHuman() const;

	int baseMoves() const;
	int movesLeft() const;

	bool isOnlyDefensive() const;

	bool isFound() const;

	int getMaxHP() const;
	int getHP() const;
	bool isHurt() const;
	void setBaseCombatStr(int iCombat);
	int baseCombatStr() const;

	bool canFight() const;

	// Air combat strength on the HUMAN scale -- the air sibling of getBaseCombatStr above, and human for the
	// same reason: UNIT_READ_AIR_BASE_COMBAT carries the engine value, whose scale moves with a game option.
	int getAirBaseCombatStr() const;

	bool isWaiting() const;
	bool isFortifyable() const;

	int bombardRate() const;

	int /*SpecialUnitTypes*/ getSpecialCargo() const;
	int /*DomainTypes*/ getDomainCargo() const;
	int cargoSpace() const;
	void changeCargoSpace(int iChange);
	bool isFull() const;

	int getID() const;

	int getGroupID() const;
	CySelectionGroup* getGroup() const;


	int getX() const;
	int getY() const;
	void setXY(int iX, int iY, bool bGroup, bool bUpdate, bool bShow);
	CyPlot* plot() const;
	CyArea* area() const;

	int getDamage() const;
	void changeDamage(int iChange, int /*PlayerTypes*/ ePlayer);
	int getMoves() const;
	void changeMoves(int iChange);
	int getExperience() const;
	int getLevel() const;
	void setLevel(int iNewLevel);
	int getFacingDirection() const;
	void rotateFacingDirectionClockwise();
	void rotateFacingDirectionCounterClockwise();
	int getCargo() const;
	int getFortifyTurns() const;
	void setFortifyTurns(int iNewValue);

	bool isRiver() const;



	bool isMadeAttack() const;
	void setMadeAttack(bool bNewValue);

	bool isMadeInterception() const;
	void setMadeInterception(bool bNewValue);

	bool isPromotionReady() const;
	void setPromotionReady(bool bNewValue);
	int getOwner() const;

	// ⚖ THE IDENTITY SET -- see CyCity.h. Owner + id + position only: what a legacy consumer needs to say WHICH
	// unit it holds. ⛔ Never the legacy stat surface; a consumer wanting unit DATA asks the unit itself / CyInfo.
	static void pythonPublish();
	int getTeam() const;

	int /*UnitTypes*/ getUnitType() const;
	int /*UnitTypes*/ getLeaderUnitType() const;

	CyUnit* getTransportUnit() const;
	bool isCargo() const;
	void setTransportUnit(const CyUnit& kTransportUnit, const bool bLoad);

	std::wstring getName() const;
	std::wstring getNameKey() const;

	bool isHasPromotion(int /*PromotionTypes*/ ePromotion) const;
	bool isHasUnitCombat(int /*UnitCombatTypes*/ eUnitCombat) const;
	void setHasPromotion(int /*PromotionTypes*/ eIndex, bool bNewValue);


	int /*UnitAITypes*/ getUnitAIType() const;
	void setAIType(int /*UnitAITypes*/ iNewValue);

	std::string getButton() const;

	void setCommander(bool bNewValue);
	void setCommodore(bool bNewValue);

	//	==== THE UNIT READ + WRITE SURFACE -- the receiver IS the unit ====
	// CAN this unit take that promotion right now? The per-unit applicability leg the enabler evaluates on
	// demand at level-up ([enabler.md] par.7.1 carve-out) -- a POINTED question about one candidate.
	bool canAcquirePromotion(int iPromotion) const;
	// Can this unit become that one -- a PAIR question (this unit, that target type), so the target is in
	// the call. bTestVisible asks the display question ("show the button") rather than the strict one.
	bool canUpgrade(int iToUnit, bool bTestVisible) const;
	// ⛔ "Can this unit upgrade to ANYTHING?" is ONE question and therefore ONE read. Asking it by looping the
	// whole unit registry from script costs a boundary crossing PER UNIT TYPE, per unit, per redraw -- and a
	// boost::python call costs far more than the lookup inside it ([patterns.md]). The walk belongs here.
	bool canUpgradeToAny() const;
	// A plot query asked RELATIVE TO A UNIT (whose owner decides who counts as an enemy). It takes the unit's
	// identity + the tile rather than a unit handle, because a unit reaches Python as (owner, id).
	int getNumVisiblePotentialEnemyDefenders(int iX, int iY) const;
	// Base combat strength on the HUMAN scale. ⛔ Deliberately NOT baseCombatStr(): that one returns x100 under
	// GAMEOPTION_COMBAT_SIZE_MATTERS and human without it, so its scale depends on live game state and no caller
	// can reason about it ([fixed-point-and-scales.md] §4c-ter). This is the boundary read, always human.
	int getBaseCombatStr() const;
	python::list getFlags() const;
	// The unit's CUSTOM name only -- empty when it carries none, unlike getUnitName which falls back to the
	// type's description. A scenario writes this one, because a fallback name is not authored data.
	std::wstring getNameNoDesc() const;
	// The UNIT twin. ⚠ A unit's position is part of the IDENTITY SET on the handle
	// ([patterns.md] THE IDENTITY SET: owner, id, POSITION), but an EVENT PAYLOAD carries only (owner, id) --
	// so a handler that was handed a payload has no handle to ask and needs this. Answers (-1, -1) for a unit
	// that does not resolve OR is off-map, which is a real state and not only a save defect
	// ([unit-lifecycle.md] THE OFF-MAP UNIT).
	python::list getPosition() const;
	// ⛔ The unit's HELD promotions, as ONE list. Asking per-promotion instead costs a boundary crossing per
	// PROMOTION per unit per redraw, and the registry is ~1500 entries -- a stack panel then spends tens of
	// thousands of crossings a frame. The per-promotion read above stays for a single POINTED question.
	// ⚠ Overridden promotions are excluded: a held-but-overridden promotion is not one the unit HAS in any sense
	// a display cares about, and every caller was already filtering them out by hand.
	python::list getPromotions() const;
	// ---- THE UNIT PLANE ----
	// ⛔ Addressed by the same (owner, id) pair as everything else. CyUnit carries ZERO defs, so a script handed
	// one -- by a callback or by CyPlot::getUnit -- can ask it nothing; the ids are the only way a unit is
	// reachable at all, which is why the plot enumeration below is a prerequisite rather than a convenience.
	python::list getRead() const;
	std::string getScriptData() const;
	// The owner a VIEWER sees -- which differs from the real owner for a hidden-nationality unit. A log or a
	// message must use this one, or it names the civ the mechanic exists to conceal.
	int getVisualOwner() const;
	// Does this unit hold that COMBAT CLASS? Its primary + subs + promotion-granted, composed -- the same
	// question CvUnit::isHasUnitCombat answers, asked by (owner, id) like every other read here.
	bool hasCombat(int iUnitCombat) const;
	bool hasPromotion(int iPromotion) const;
	bool isActionRecommended(int iAction) const;
	// ⛔ A unit whose death is DECIDED but not yet performed still answers TRUE here, and a consumer iterating
	// a player's units must skip it: combat holds raw pointers across the exchange, so a killed unit stays a
	// live object until the delayed-death pass reaps it ([unit-lifecycle.md]). Anything listing or counting
	// units without this shows the dead ones.
	bool isDead() const;
	// ⛔ The LIVE per-unit verdict, NOT the type's skill block. hiddenNationality is promotion-grantable
	// (PROMOTION_PROUD_PIRATE, [skills.md]), so `INFO.isHiddenNationality(unitType)` answers a DIFFERENT question
	// and silently misses every unit that earned it -- which is why this is a STATE read and named for the unit.
	bool isHiddenNationality() const;
	// The SELECTOR predicates: each asks about a PAIR, so the subject is in the call rather than a flag slot.
	bool isInvisible(int iTeam) const;
	bool isPromotionOverridden(int iPromotion) const;
	// ⚖ THE SIBLING, AND THE TWO ARE NOT INTERCHANGEABLE. canUnitAcquirePromotion answers the LEVEL-UP question;
	// this answers whether a promotion may be APPLIED to the unit at all, which is the gate a GRANT uses -- a
	// free promotion bypasses tech prereqs ([special-systems.md]). ⛔ Gating a grant on the acquire read instead
	// refuses every promotion an EVENT hands out, since those sit outside the normal list and cannot be taken by
	// XP at all (owner).
	bool isPromotionValid(int iPromotion) const;
	bool changeExperience(int iChange, int iMax,
							  bool bFromCombat, bool bInBorders, bool bUpdateGlobal) const;
	///<summary>Issues one of the engine's own unit COMMANDS (gift, upgrade, join, …) through
	/// CvUnit::doCommand, so the command runs its normal validation and emits its normal facts.</summary>
	///<returns>false when the unit handle does not resolve, or the command is out of range.</returns>
	bool doCommand(int iCommand, int iData1, int iData2) const;
	// ---- The UNIT actions the COMBAT / unit-lifecycle handlers need ----
	// ⚖ Each earns its place the way this surface requires -- by an EXISTING call site that needs it
	// ([roadmap] scope decision 6: where the write a handler needs is not published yet, ADD it to the surface
	// that already exists; "the write surface is parked" is explicitly not a reason to skip a fix). The call
	// sites are CvEventManager's onCombatResult / onUnitKilled / onUnitCreated and the two DancingHoskuld
	// combat mods -- all EXISTING gameplay being kept working, never new logic authored in script.
	bool finishMoves() const;
	// Kill a unit; bDelay is the engine's delayed-death flag ([unit-lifecycle.md]). ⛔ With bDelay false the
	// object is deleted immediately, so an iterating caller must hold a snapshot.
	bool kill(bool bDelay, int iByPlayer) const;
	bool setDamage(int iDamage, int iByPlayer) const;
	// CvUnit::changeExperience. ⚠ The HUMAN-scale entry point: XP is stored x100 and changeExperience multiplies
	// on the way in, so a caller passes whole levels of XP exactly as the legacy script did -- do NOT hand it a
	// x100 value ([special-systems.md]).
	// Absolute twin of changeUnitExperience -- the engine publishes both, so a caller that means SET says set.
	bool setExperience(int iExperience) const;
	// A unit's LEADER attachment (the subdued/tamed-animal beastmaster art link), -1 to clear.
	bool setLeaderUnitType(int iLeaderUnitType) const;
	// Moves SPENT, in move points -- the partial-moves sibling of finishUnitMoves.
	bool setMoves(int iMoves) const;
	bool setName(std::wstring szName) const;
	// Give or take a unit's PROMOTION. ⚖ This earns its place the way this surface requires -- by an existing
	// call site that needs it: CvEventManager.onUnitPromoted redirects an AI's promotion pick, which is EXISTING
	// gameplay being kept working, not new logic authored in script. It routes through CvUnit::setHasPromotion,
	// so the promotion fact is emitted and the resolved plane re-gathers exactly as an engine-side promote does.
	bool setPromotion(int iPromotion, bool bNewValue) const;
	// The unit's SCRIPT DATA -- the write sibling of getScriptData above, and the exact twin of
	// setCityScriptData above. Python-authoritative gameplay keeps its own per-unit state here (SdToolKit), and
	// with an identity-tuple payload a handler has no handle to call setScriptData on.
	bool setScriptData(std::string szData) const;
	// ⚖ THE IMMOBILE TIMER IS A STATUS NOW, so this is the STATUS verb rather than a revived setImmobileTimer:
	// "immobilised for N turns" is `STATUS_PARALYZED` on the per-scope status store ([state.md]: a status is a
	// counter applied, ticked down, over at zero -- and `CvUnit::setStatus` is its ONE write path, which is what
	// makes the 0-crossing announce). ⛔ Do not add a per-timer verb beside it; a new status is a new enum member.
	bool setStatus(int iStatus, int iTurns) const;
	//	Turns REMAINING on a status, 0 when not held -- the read twin of setStatus. ⛔ There is no per-status
	//	named accessor and there must not be one: `getStatus(STATUS_PARALYZED)` IS the immobile-timer read.
	int getStatus(int iStatus) const;


	//	Select this unit's GROUP in the interface (the shift/ctrl/alt modifiers the engine's own click uses).
	bool selectGroup(bool bShift, bool bCtrl, bool bAlt) const;
	//	CvUnit::convert -- carry another unit's identity/XP/promotions onto THIS one, and optionally kill it.
	//	⚠ bKillOriginal DELETES the source, so the caller must not touch it afterwards ([unit-lifecycle.md]).
	bool convert(int iFromPlayer, int iFromUnit, bool bKillOriginal) const;

	//	==== THE ORDER PLANE ====
	//	Civ4 keeps activity, automation and missions on the unit's selection GROUP, which is why the group's
	//	state already surfaces through UNIT_READ_ACTIVITY / _AUTOMATE / _MISSION. These are the WRITE half of
	//	that same arrangement, homed on the unit for the same reason: the group is an engine detail, and a
	//	handle nobody can obtain serves nothing.
	//	⚠ A mission target is addressed by POSITION, so (-1, -1) expresses 'no target' -- a plot handle cannot.
	bool setActivity(int iActivityType);
	bool isReadyToMove(bool bAny);
	bool canStartMission(int iMission, int iData1, int iData2, int iX, int iY, bool bTestVisible) const;
	bool pushMission(int iMission, int iData1, int iData2, int iFlags, bool bAppend, bool bManual, int iMissionAI, int iX, int iY);
protected:
	CvUnit* m_pUnit;
};

// A unit crosses as its (owner, id) IDENTITY, for the same reason a city does -- see CyCity.h.
DECLARE_PY_IDENTITY(CvUnit*, getOwner(), getID());

#endif // CyUnit_h__
