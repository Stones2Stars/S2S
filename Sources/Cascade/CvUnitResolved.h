#pragma once
#ifndef CV_UNIT_RESOLVED_H
#define CV_UNIT_RESOLVED_H

//
//	UnitResolvedValues -- the UNIT plane's storage. NOT a CvCascadePackage, and that is the spec rather than an
//	omission ([state-repositories.md](../../docs/architecture/state-repositories.md)):
//
//	  "UNIT is RESOLVED VALUES, not a package -- when the number is put on the unit, no more percentages or
//	   whatever is involved, the data just IS. The exact set of numbers a unit carries is known, so they are
//	   summed and stored individually, and they dirty on a different trigger from everything else: ONLY when a
//	   promotion or combat class changes. It is the most static plane in the engine."
//
//	So there is no Sigmaflat/Sigmapercent slot pair here, no scope chain to roll up, and no channel registry: a unit's
//	value is RESOLVED at gather time and read back as a bare number. The 12 unit-only families never enter any
//	scope's channel set, which is why this plane is correctly its own shape and not a consolidation target.
//
//	⛔ WHY IT EXISTS AT ALL: the #430 cut removed the legacy per-unit accumulators from every save
//	(`Assets/savemigration.txt` -- the entries are a REPLACEMENT-OBLIGATION LEDGER, save.md §3), each on the
//	promise of a named gatherer. That gatherer and its storage were archived by the clean-slate revert, so those
//	values had NO SOURCE: the fields are gone from the stream and nothing computed them. This is that source.
//
//	THE GATHER -- over the unit's HELD SET, from the COMPILED slot sums:
//	    the unit's own CvUnitInfo  U  every held promotion  U  every held unit-combat class
//	each contributing its `(family, kind, CASC_SCOPE_UNIT, unit)` compiled sum (CvModifiers::sum -- a bare array
//	load, no anatomy walk, no string address, [DEC-materialize-at-mapfrom]). Percent-unit kinds sum as percents
//	and flat-unit kinds as flats; nothing multiplies here, because a unit value is already resolved.
//
//	THE DIRTY TRIGGER -- exactly two spine facts, per the model's "ONLY when a promotion or combat class
//	changes": SEVT_UNIT_PROMOTION_CHANGED and SEVT_UNIT_COMBAT_CHANGED. Unit MOVEMENT never dirties this (nor any
//	cache -- [DEC-unit-modifiers-on-top]).
//
//	⛔ COMMANDER-FREE BY CONSTRUCTION (owner, [modifier.md] §2b). A COMMANDER's contribution is NOT part of the
//	unit -- it rides ON TOP, exactly as unit-sourced happiness rides on top of a city. It is deliberately absent
//	from the held set above, and that is load-bearing rather than an omission: attaching, detaching or moving a
//	commander is neither a promotion nor a combat-class change, so NO fact would dirty this cache and a folded-in
//	commander value would be permanently stale the moment the commander moved. The fold belongs to the COMBAT
//	CALC, which is also the only place that can ask whether the commander has CONTROL POINTS left to spend --
//	a question a stat read cannot answer.
//

#include "Infos/CvInfoKinds.h"

class CvUnit;

// The resolved slots a unit carries. Enum-indexed rather than hand-named scalars: "stored individually" is
// satisfied by one value per slot, while an enum keeps the set addressable and extensible by DATA -- a new unit
// family is a new row in the table below, never a new member + getter.
enum UnitResolvedSlot
{
	URS_STRENGTH_FLAT = 0,        // strength.unit.flat            -- the base value
	URS_STRENGTH_PERCENT,         // combat.unit.percent           -- ruling 5: `strength` is the BASE value
	                              //                                  only; `combat` is what modifies it.
	                              // (the SM size scaling is the `sizeMatters` BLOCK, json.md §9 -- not a
	                              //  modifier family, so deliberately NOT a slot here)
	URS_WITHDRAWAL,               // withdrawal.unit.percent
	URS_FIRST_STRIKES,            // firstStrike.unit.flat
	URS_FIRST_STRIKE_CHANCE,      // firstStrike.unit.chance
	URS_HEAL_ENEMY,               // heal.unit.enemy
	URS_HEAL_NEUTRAL,             // heal.unit.neutral
	URS_HEAL_FRIENDLY,            // heal.unit.friendly
	URS_HEAL_SAME_TILE,           // heal.unit.sameTile
	URS_HEAL_ADJACENT,            // heal.unit.adjacent
	URS_EVASION,                  // air.unit.evasion
	URS_INTERCEPT,                // air.unit.intercept
	URS_AIR_RANGE,                // air.unit.range                -- the unit's OWN air range. The team's
	                              //   domain-moves award and the player's flight/missile range changes are
	                              //   separate scopes and are added at the read, not gathered here.
	URS_COLLATERAL,               // collateral.unit.percent
	URS_CAPTURE_PROBABILITY,      // capture.unit.probability
	URS_CAPTURE_RESISTANCE,       // capture.unit.resistance
	URS_UPKEEP_EXTRA,             // upkeep.unit.extra
	URS_CONCEALMENT,              // vision.unit.concealment       -- how well it HIDES (vision.md §4). The
	                              //   METHOD it hides by is its tag; this is the one strength a seeker's
	                              //   detection against that method is weighed against.
	URS_VISION,                   // vision.unit.flat              -- the unit's sight STRENGTH (vision.md):
	                              //   its own base plus its combat classes plus its promotions. Elevation is
	                              //   NOT here -- that belongs to the ground and is added at read.
	// The `combat` family -- json.md §6: `strength` is the BASE, `combat` is everything that MODIFIES it. One
	// slot per KIND, because a consumer asks a specific question ("what is my city-attack?") and the kinds do
	// not sum with each other.
	URS_COMBAT_ATTACK,            // combat.unit.attack
	URS_COMBAT_DEFENSE,           // combat.unit.defense
	URS_CITY_ATTACK,              // combat.unit.cityAttack
	URS_CITY_DEFENSE,             // combat.unit.cityDefense
	URS_HILLS_ATTACK,             // combat.unit.hillsAttack
	URS_HILLS_DEFENSE,            // combat.unit.hillsDefense
	URS_ANIMAL_COMBAT,            // combat.unit.animal
	URS_RELIGIOUS_COMBAT,         // combat.unit.religious
	URS_VS_BARBS,                 // combat.unit.vsBarbs
	URS_LUNGE,                    // combat.unit.lunge
	URS_UNNERVE,                  // combat.unit.unnerve
	URS_ENCLOSE,                  // combat.unit.enclose
	URS_TAUNT,                    // combat.unit.taunt
	URS_DYNAMIC_DEFENSE,          // combat.unit.dynamicDefense
	URS_DAMAGE_MODIFIER,          // combat.unit.damageModifier
	URS_STEALTH,                  // combat.unit.stealth
	URS_STEALTH_STRIKES,          // combat.unit.stealthStrikes
	NUM_UNIT_RESOLVED_SLOTS
};

//	The resolved values + their dirty flag. A DATA MEMBER on CvUnit (the guardrail bars adding vtable BASES to
//	EXE-bound classes, never members -- patterns.md). NEVER SERIALIZED: derived data is never trusted from a save
//	([DEC-derived-never-trusted]), and the held set it gathers from is itself restored by the save read.
class UnitResolvedValues
{
public:
	UnitResolvedValues() : m_bDirty(true)
	{
		for (int iSlot = 0; iSlot < NUM_UNIT_RESOLVED_SLOTS; ++iSlot)
		{
			m_aiValue[iSlot] = 0;
		}
	}

	// THE MARK IS WHAT REBUILDS (state-repositories.md), so a read is a bare fetch and never recomputes.
	void markDirty(const CvUnit& kUnit);
	int get(UnitResolvedSlot eSlot) const { return m_aiValue[eSlot]; }

	// The gather, exposed so an endpoint can recompute FROM SOURCE into a caller-owned buffer without ever being
	// handed the stored values -- the oracle shape that makes "never repairs" structural, not a discipline.
	static void gatherInto(const CvUnit& kUnit, int (&aiOut)[NUM_UNIT_RESOLVED_SLOTS]);

private:
	int m_aiValue[NUM_UNIT_RESOLVED_SLOTS];
	bool m_bDirty;
};

#endif // CV_UNIT_RESOLVED_H
