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
//
//	⛔ WITH ONE CARVE-OUT, AND IT IS A DESIGN RATHER THAN AN EXCEPTION: **URS_STRENGTH_FLAT IS DELTA-ONLY** --
//	the unit's own type does NOT contribute to it. A unit's BASE strength is PER-UNIT STATE, not a function of
//	its type (owner): WorldBuilder edits an individual unit's strength and the WBS format persists it, so the
//	base is the serialized CvUnit::m_iBaseCombat and the consumer adds the two. Strength is the only stat with a
//	per-unit editable base, which is why it is the only slot that carves out. ⚠ Counting the type in both places
//	is a silent DOUBLE COUNT of every unit's authored strength -- the defect this carve-out exists to prevent.
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

#include <utility>
#include <vector>

class CvUnit;

//	The unit's folded `hideAndSeek` block -- how well it HIDES, and how well it SEES each hiding METHOD.
//
//	⛔ IT IS ITS OWN BLOCK BESIDE THE SLOT TABLE, NOT A `URS_*` ROW, AND THAT IS THE SPEC. The slot table
//	addresses modifier-FAMILY entries by `(family, kind, scope, unit)`; `hideAndSeek` is a SECTION
//	([json.md] §9) with no such address, so it cannot ride the table -- and a hand-named scalar pair beside it
//	would be the shape [DEC-uniform-cache-shape] calls a defect. It gets a cached block on the SAME MARK
//	PROTOCOL instead, which is what keeps one fact route maintaining one unit's whole resolved state.
//
//	⚑ WHY IT BELONGS HERE AT ALL: it folds over EXACTLY the three carriers the slot table already folds over --
//	the unit's own info, its held promotions, its held unit-combat classes -- so the two facts that move the slot
//	table are precisely the two that move this. There is no third trigger to find and none to invent.
struct UnitResolvedHideAndSeek
{
	UnitResolvedHideAndSeek() : concealment(0), noInvisibilityNet(0) {}

	void clear()
	{
		concealment = 0;
		noInvisibilityNet = 0;
		detection.clear();
		methodSkills.clear();
	}

	// Detection against ONE method (×100). A linear scan of a HANDFUL -- the rows a unit's own carriers actually
	// author -- never the method registry, and never the promotion/unit-combat registries.
	int detectionAgainst(int iMethodSkillId) const;

	// Fold one carrier's resolved rows in, summing per method.
	void addDetection(int iMethodSkillId, int iValue);

	// MEMBERSHIP -- does this unit hide by that method at all? Holding the method SKILL is the membership
	// question (vision.md §4), and it is answered from the SAME fold as the magnitudes: net grants minus
	// revokes over the three carriers. This is what lets a promotion grant a hiding method (optical
	// camouflage) -- an info-only membership read never saw one.
	bool holdsMethodSkill(int iMethodSkillId) const;
	void addMethodSkill(int iMethodSkillId, int iNet);

	int concealment;                                  // ×100; MAY BE NEGATIVE (a negative row strips cover)
	int noInvisibilityNet;                            // net grants of the `noInvisibility` canceller skill
	std::vector<std::pair<int, int> > detection;      // (methodSkill, summed value), one entry per method answered
	std::vector<std::pair<int, int> > methodSkills;   // (methodSkill, net grant count); held iff net > 0
};

//	The unit's folded HEAL block. Heal is its OWN SET, not a skill: a skill is a pure boolean enabler carrying no
//	value ([skills.md]), and heal carries magnitudes -- which is why its rates already occupy `URS_HEAL_*` slots.
//	What lives HERE is the part of the set that is not a `(family, kind)` magnitude: the VERDICTS the heal
//	arithmetic gates on.
//
//	⚑ IT IS UPDATED WHEN THE PROMOTION LANDS, which is the whole point of it. The verdict below is a pure
//	function of the unit's held promotions, so it is resolved ONCE on the promotion / combat-class fact and read
//	as a bare fetch -- never re-derived by the reader.
//
//	⛔ THE MECHANIC IS UNTOUCHED, ONLY THE FEED ([roadmap.md]: "we don't change how heal works, but we have to
//	change how the data is fed to heal"). The arithmetic that consumes this stands exactly as it did; what
//	changed is that it is HANDED the verdict instead of rediscovering it per call.
struct UnitResolvedHeal
{
	UnitResolvedHeal() : healsOutsideFriendlyTerritory(false) {}

	void clear()
	{
		healsOutsideFriendlyTerritory = false;
	}

	// May this unit recover outside friendly territory on its own? Legacy expresses it as membership of the
	// self-heal / self-repair promotion LINES, so the fold asks the lines once per mark rather than per read.
	bool healsOutsideFriendlyTerritory;
};

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
	URS_HEAL_VICTORY,             // heal.unit.victory   -- recovery on winning; promotions author it
	URS_HEAL_SUPPORT,             // heal.unit.support   -- how many others this unit can support-heal
	URS_HEAL_VICTORY_STACK,       // heal.unit.victoryStack
	URS_HEAL_VICTORY_ADJACENT,    // heal.unit.victoryAdjacent
	URS_HEAL_SELF_MODIFIER,       // heal.unit.selfModifier -- a PERCENT, so unscaled ([DEC-fixedpoint-x100])
	URS_EVASION,                  // air.unit.evasion
	URS_INTERCEPT,                // air.unit.intercept
	URS_AIR_RANGE,                // air.unit.range                -- the unit's OWN air range. The team's
	                              //   domain-moves award and the player's flight/missile range changes are
	                              //   separate scopes and are added at the read, not gathered here.
	URS_COLLATERAL,               // collateral.unit.percent
	URS_CAPTURE_PROBABILITY,      // capture.unit.probability
	URS_CAPTURE_RESISTANCE,       // capture.unit.resistance
	URS_UPKEEP_EXTRA,             // upkeep.unit.extra
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

	// The `hideAndSeek` reads -- bare fetches, exactly like the slot read above.
	int concealment() const { return m_hideAndSeek.concealment; }
	int detectionAgainst(int iMethodSkillId) const { return m_hideAndSeek.detectionAgainst(iMethodSkillId); }
	bool holdsMethodSkill(int iMethodSkillId) const { return m_hideAndSeek.holdsMethodSkill(iMethodSkillId); }
	bool noInvisibilityGranted() const { return m_hideAndSeek.noInvisibilityNet > 0; }

	// The HEAL block's verdicts -- bare fetches on the same terms.
	bool healsOutsideFriendlyTerritory() const { return m_heal.healsOutsideFriendlyTerritory; }

	//	Is this held promotion SUPERSEDED by a higher rung of its own line that the unit also holds?
	//	⛔ A BARE FETCH over a HANDFUL. This is a pure function of the held set, so it is resolved ONCE when the
	//	promotion landed. Asked per read it was `held x REGISTRY` -- the panel asks it for every promotion a unit
	//	carries, and each ask swept every promotion in the game to rediscover the unit's own lines, so the cost
	//	grew QUADRATICALLY with promotions held and an unpromoted unit paid nothing. That shape is the own-data
	//	inversion ([DEC-one-reverse-view]) and the per-read scan the event-built state exists to delete.
	bool isPromotionOverridden(int iPromotion) const;

	// The gathers, exposed so a caller can recompute FROM SOURCE into its own buffer without ever being handed
	// the stored values -- which is what makes "never repairs" structural rather than a discipline.
	static void gatherInto(const CvUnit& kUnit, int (&aiOut)[NUM_UNIT_RESOLVED_SLOTS]);
	static void gatherHideAndSeekInto(const CvUnit& kUnit, UnitResolvedHideAndSeek& kOut);
	static void gatherHealInto(const CvUnit& kUnit, UnitResolvedHeal& kOut);

private:
	int m_aiValue[NUM_UNIT_RESOLVED_SLOTS];
	UnitResolvedHideAndSeek m_hideAndSeek;
	UnitResolvedHeal m_heal;
	std::vector<int> m_aOverriddenPromotions;   // ascending; the held rungs a higher held rung supersedes
	bool m_bDirty;
};

#endif // CV_UNIT_RESOLVED_H
