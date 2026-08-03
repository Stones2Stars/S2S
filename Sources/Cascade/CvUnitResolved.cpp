//
//	UnitResolvedValues -- see the header. The gather over the unit's HELD SET, from the COMPILED slot sums.
//

#include "CvGameCoreDLL.h"
#include "CvUnitResolved.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvUnit.h"
#include "Infos/CvInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvPromotionInfo.h"
#include "Infos/CvUnitCombatInfo.h"

namespace
{
	// How each resolved slot is addressed on a contributing info. Two homes, because the vocabulary has two:
	// a family+kind pair, or an InfoScalar straggler (patterns.md's getScalar row). `scalar` >= 0 selects the
	// straggler form; otherwise (family, kind) is used. The table IS the spec of this plane -- adding a unit
	// value is a row here, never a new member + getter.
	// A row may defer the percent-vs-flat verdict to the vocabulary rather than state it: the kinds of one
	// family do not share a unit, so hand-picking per row is where a silent mis-sum would come from.
	const CvCascUnit UNIT_CANONICAL = (CvCascUnit)-1;

	struct UnitSlotAddress
	{
		int scalar;                 // InfoScalar, or -1
		ModifierFamily family;
		int kind;
		CvCascUnit unit;            // UNIT_CANONICAL -> ask infoKindUnit for this (family, kind)
	};

	const UnitSlotAddress g_aSlotAddress[NUM_UNIT_RESOLVED_SLOTS] =
	{
		{ SCALAR_STRENGTH,             MODFAM_STRENGTH,     0,                      CASC_UNIT_FLAT    }, // URS_STRENGTH_FLAT
		{ -1,                          MODFAM_COMBAT,       COMBAT_AMOUNT,          CASC_UNIT_PERCENT }, // URS_STRENGTH_PERCENT (ruling 5: `combat` MODIFIES the base)
		{ SCALAR_WITHDRAWAL,           MODFAM_WITHDRAWAL,   0,                      CASC_UNIT_PERCENT }, // URS_WITHDRAWAL
		{ SCALAR_FIRST_STRIKES,        MODFAM_FIRST_STRIKE, 0,                      CASC_UNIT_FLAT    }, // URS_FIRST_STRIKES
		{ SCALAR_FIRST_STRIKE_CHANCES, MODFAM_FIRST_STRIKE, 0,                      CASC_UNIT_FLAT    }, // URS_FIRST_STRIKE_CHANCE
		{ -1,                          MODFAM_HEAL,         HEAL_ENEMY_TERRITORY,   CASC_UNIT_FLAT    }, // URS_HEAL_ENEMY
		{ -1,                          MODFAM_HEAL,         HEAL_NEUTRAL_TERRITORY, CASC_UNIT_FLAT    }, // URS_HEAL_NEUTRAL
		{ -1,                          MODFAM_HEAL,         HEAL_FRIENDLY_TERRITORY,CASC_UNIT_FLAT    }, // URS_HEAL_FRIENDLY
		{ -1,                          MODFAM_HEAL,         HEAL_SAME_TILE,         CASC_UNIT_FLAT    }, // URS_HEAL_SAME_TILE
		{ -1,                          MODFAM_HEAL,         HEAL_ADJACENT,          CASC_UNIT_FLAT    }, // URS_HEAL_ADJACENT
		{ -1,                          MODFAM_AIR,          AIR_EVASION,            CASC_UNIT_PERCENT }, // URS_EVASION
		{ -1,                          MODFAM_AIR,          AIR_INTERCEPT,          CASC_UNIT_PERCENT }, // URS_INTERCEPT
		{ -1,                          MODFAM_AIR,          AIR_RANGE,              UNIT_CANONICAL    }, // URS_AIR_RANGE
		{ -1,                          MODFAM_COLLATERAL,   COLLATERAL_DAMAGE,      CASC_UNIT_PERCENT }, // URS_COLLATERAL
		{ -1,                          MODFAM_CAPTURE,      CAPTURE_PROBABILITY,    CASC_UNIT_PERCENT }, // URS_CAPTURE_PROBABILITY
		{ -1,                          MODFAM_CAPTURE,      CAPTURE_RESISTANCE,     CASC_UNIT_PERCENT }, // URS_CAPTURE_RESISTANCE
		{ -1,                          MODFAM_UPKEEP,       UPKEEP_EXTRA,           CASC_UNIT_FLAT    }, // URS_UPKEEP_EXTRA
		{ -1,                          MODFAM_VISION,       VISION_STRENGTH,        CASC_UNIT_FLAT    }, // URS_VISION
		// The `combat` family. UNIT_CANONICAL rather than a hand-picked unit per row: infoKindUnit already owns
		// the percent-vs-flat verdict for a kind ([fixed-point-and-scales.md]: ask the KIND's unit, never the
		// family's), so asking it is what keeps a flat-unit kind from being summed as a percent.
		{ -1, MODFAM_COMBAT, COMBAT_ATTACK,           UNIT_CANONICAL }, // URS_COMBAT_ATTACK
		{ -1, MODFAM_COMBAT, COMBAT_DEFENSE,          UNIT_CANONICAL }, // URS_COMBAT_DEFENSE
		{ -1, MODFAM_COMBAT, COMBAT_CITY_ATTACK,      UNIT_CANONICAL }, // URS_CITY_ATTACK
		{ -1, MODFAM_COMBAT, COMBAT_CITY_DEFENSE,     UNIT_CANONICAL }, // URS_CITY_DEFENSE
		{ -1, MODFAM_COMBAT, COMBAT_HILLS_ATTACK,     UNIT_CANONICAL }, // URS_HILLS_ATTACK
		{ -1, MODFAM_COMBAT, COMBAT_HILLS_DEFENSE,    UNIT_CANONICAL }, // URS_HILLS_DEFENSE
		{ -1, MODFAM_COMBAT, COMBAT_ANIMAL,           UNIT_CANONICAL }, // URS_ANIMAL_COMBAT
		{ -1, MODFAM_COMBAT, COMBAT_RELIGIOUS,        UNIT_CANONICAL }, // URS_RELIGIOUS_COMBAT
		{ -1, MODFAM_COMBAT, COMBAT_VS_BARBS,         UNIT_CANONICAL }, // URS_VS_BARBS
		{ -1, MODFAM_COMBAT, COMBAT_LUNGE,            UNIT_CANONICAL }, // URS_LUNGE
		{ -1, MODFAM_COMBAT, COMBAT_UNNERVE,          UNIT_CANONICAL }, // URS_UNNERVE
		{ -1, MODFAM_COMBAT, COMBAT_ENCLOSE,          UNIT_CANONICAL }, // URS_ENCLOSE
		{ -1, MODFAM_COMBAT, COMBAT_TAUNT,            UNIT_CANONICAL }, // URS_TAUNT
		{ -1, MODFAM_COMBAT, COMBAT_DYNAMIC_DEFENSE,  UNIT_CANONICAL }, // URS_DYNAMIC_DEFENSE
		{ -1, MODFAM_COMBAT, COMBAT_DAMAGE_MODIFIER,  UNIT_CANONICAL }, // URS_DAMAGE_MODIFIER
		{ -1, MODFAM_COMBAT, COMBAT_STEALTH,          UNIT_CANONICAL }, // URS_STEALTH
		{ -1, MODFAM_COMBAT, COMBAT_STEALTH_STRIKES,  UNIT_CANONICAL }, // URS_STEALTH_STRIKES
	};

	// ONE contributor's share of every slot, added in. A bare compiled-sum fetch per slot -- no anatomy walk,
	// no string address, nothing evaluated ([DEC-materialize-at-mapfrom]).
	void urs_addContributor(const CvInfo* pInfo, int (&aiOut)[NUM_UNIT_RESOLVED_SLOTS])
	{
		if (pInfo == NULL)
		{
			return;
		}
		for (int iSlot = 0; iSlot < NUM_UNIT_RESOLVED_SLOTS; ++iSlot)
		{
			const UnitSlotAddress& kAddress = g_aSlotAddress[iSlot];
			if (kAddress.scalar >= 0)
			{
				aiOut[iSlot] += pInfo->getScalar((InfoScalar)kAddress.scalar, CASC_SCOPE_UNIT, kAddress.unit);
				continue;
			}
			const CvCascUnit eUnit = (kAddress.unit == UNIT_CANONICAL)
				? infoKindUnit(kAddress.family, kAddress.kind, CASC_SCOPE_UNIT)
				: kAddress.unit;
			aiOut[iSlot] += pInfo->modifier(kAddress.family, kAddress.kind, CASC_SCOPE_UNIT, eUnit);
		}
	}
}

void UnitResolvedValues::gatherInto(const CvUnit& kUnit, int (&aiOut)[NUM_UNIT_RESOLVED_SLOTS])
{
	for (int iSlot = 0; iSlot < NUM_UNIT_RESOLVED_SLOTS; ++iSlot)
	{
		aiOut[iSlot] = 0;   // fully define the output every call (the derived-cache contract's rule 2)
	}

	// THE HELD SET, in full: the unit's own type, every held promotion, every held unit-combat class. A unit's
	// combat classes are its primary + subs + promotion-granted, and CvUnit::isHasUnitCombat already answers the
	// composed question, so this needs no second derivation of the class set.
	urs_addContributor(&kUnit.getUnitInfo(), aiOut);

	// ⛔ STRENGTH IS THE ONE SLOT WHOSE BASE IS PER-UNIT STATE, NOT A FUNCTION OF THE TYPE (owner): WorldBuilder
	// edits an individual unit's strength, and the WBS scenario format persists it (`CombatStr=`, written only
	// when it differs from the type). So the BASE lives on CvUnit as the serialized m_iBaseCombat and THIS PLANE
	// CARRIES THE DELTA ONLY -- promotions and unit-combats -- exactly as the #430 F4 migration ledger records
	// it (Assets/savemigration.txt). The type's own strength is dropped here because the consumer
	// (baseCombatStr*PreCheck) adds m_iBaseCombat; counting it in both places double-counts every unit's
	// authored base. ⚠ The array was zeroed above and only the unit's own info has been added, so this removes
	// exactly the type's contribution and nothing else.
	aiOut[URS_STRENGTH_FLAT] = 0;

	for (int iPromotion = 0; iPromotion < GC.getNumPromotionInfos(); ++iPromotion)
	{
		if (kUnit.isHasPromotion((PromotionTypes)iPromotion))
		{
			urs_addContributor(&GC.getPromotionInfo((PromotionTypes)iPromotion), aiOut);
		}
	}
	for (int iCombat = 0; iCombat < GC.getNumUnitCombatInfos(); ++iCombat)
	{
		if (kUnit.isHasUnitCombat((UnitCombatTypes)iCombat))
		{
			urs_addContributor(&GC.getUnitCombatInfo((UnitCombatTypes)iCombat), aiOut);
		}
	}
}

void UnitResolvedValues::markDirty(const CvUnit& kUnit)
{
	// THE MARK IS WHAT REBUILDS -- which is what lets every read be a bare fetch (state-repositories.md). The
	// load bracket is not special-cased here: the held set is restored by the save read and the two dirty facts
	// are emitted from inside it, so a unit gathers once its own promotions/combats have streamed in.
	m_bDirty = false;
	gatherInto(kUnit, m_aiValue);
}
