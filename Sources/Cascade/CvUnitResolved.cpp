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
	struct UnitSlotAddress
	{
		int scalar;                 // InfoScalar, or -1
		ModifierFamily family;
		int kind;
		CvCascUnit unit;
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
		{ -1,                          MODFAM_COLLATERAL,   COLLATERAL_DAMAGE,      CASC_UNIT_PERCENT }, // URS_COLLATERAL
		{ -1,                          MODFAM_CAPTURE,      CAPTURE_PROBABILITY,    CASC_UNIT_PERCENT }, // URS_CAPTURE_PROBABILITY
		{ -1,                          MODFAM_CAPTURE,      CAPTURE_RESISTANCE,     CASC_UNIT_PERCENT }, // URS_CAPTURE_RESISTANCE
		{ -1,                          MODFAM_UPKEEP,       UPKEEP_EXTRA,           CASC_UNIT_FLAT    }, // URS_UPKEEP_EXTRA
		{ -1,                          MODFAM_VISION,       VISION_CONCEALMENT,     CASC_UNIT_FLAT    }, // URS_CONCEALMENT
		{ -1,                          MODFAM_VISION,       VISION_STRENGTH,        CASC_UNIT_FLAT    }, // URS_VISION
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
			aiOut[iSlot] += (kAddress.scalar >= 0)
				? pInfo->getScalar((InfoScalar)kAddress.scalar, CASC_SCOPE_UNIT, kAddress.unit)
				: pInfo->modifier(kAddress.family, kAddress.kind, CASC_SCOPE_UNIT, kAddress.unit);
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
