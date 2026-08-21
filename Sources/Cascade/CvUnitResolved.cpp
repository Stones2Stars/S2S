//
//	UnitResolvedValues -- see the header. The gather over the unit's HELD SET, from the COMPILED slot sums.
//

#include "CvGameCoreDLL.h"
#include "CvUnitResolved.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvUnit.h"
#include "Infos/CvClassificationIds.h"
#include "Infos/CvInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvPromotionInfo.h"
#include "Infos/CvUnitCombatInfo.h"
#include "Infos/CvHideAndSeekSection.h"
#include <algorithm>

namespace
{
	//	One held promotion's LINE MEMBERSHIP, kept while the held walk is already in hand so the supersession
	//	verdict needs no second pass over the unit's promotions.
	struct UnitHeldRung
	{
		UnitHeldRung(int iPromotionId, int iLineId, int iPriorityValue)
			: iPromotion(iPromotionId), iLine(iLineId), iPriority(iPriorityValue) {}
		int iPromotion;
		int iLine;
		int iPriority;
	};

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
		{ -1,                          MODFAM_HEAL,         HEAL_VICTORY,           CASC_UNIT_FLAT    }, // URS_HEAL_VICTORY
		{ -1,                          MODFAM_HEAL,         HEAL_SUPPORT,           CASC_UNIT_FLAT    }, // URS_HEAL_SUPPORT
		{ -1,                          MODFAM_HEAL,         HEAL_VICTORY_STACK,     CASC_UNIT_FLAT    }, // URS_HEAL_VICTORY_STACK
		{ -1,                          MODFAM_HEAL,         HEAL_VICTORY_ADJACENT,  CASC_UNIT_FLAT    }, // URS_HEAL_VICTORY_ADJACENT
		{ -1,                          MODFAM_HEAL,         HEAL_SELF_MODIFIER,     CASC_UNIT_PERCENT }, // URS_HEAL_SELF_MODIFIER
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

	// ONE contributor's share of the `hideAndSeek` BLOCK, added in. The block is a SECTION rather than a
	// modifier-family address, so it folds beside the slot table rather than into it (see the header).
	void urs_addHideAndSeek(const CvInfo& kCarrier, const CvHideAndSeekSection& kBlock,
		const std::vector<int>& aMethodSkills, UnitResolvedHideAndSeek& kOut)
	{
		kOut.concealment += kBlock.concealment;

		// The carrier's rows, method ids already resolved through the section's ONE resolve path -- so a
		// late-minted SKILL_* cannot resolve differently here than it does at the section's own read.
		std::vector<std::pair<int, int> > aRows;
		kBlock.collectDetectionInto(aRows);
		for (size_t iRow = 0; iRow < aRows.size(); ++iRow)
		{
			kOut.addDetection(aRows[iRow].first, aRows[iRow].second);
		}

		// THE MEMBERSHIP FOLD -- which methods this carrier grants (or revokes) the unit hiding by. The skill
		// plane is the block's membership filter (vision.md §4), so it folds here beside the magnitudes, over
		// the hoisted method-skill list -- never the whole skill registry.
		for (size_t iSkill = 0; iSkill < aMethodSkills.size(); ++iSkill)
		{
			const int iMethodSkill = aMethodSkills[iSkill];
			if (kCarrier.hasSkill(iMethodSkill))
			{
				kOut.addMethodSkill(iMethodSkill, 1);
			}
			if (kCarrier.revokesSkill(iMethodSkill))
			{
				kOut.addMethodSkill(iMethodSkill, -1);
			}
		}
		if (kCarrier.hasSkill(CLS_SKILL_NO_INVISIBILITY))
		{
			kOut.noInvisibilityNet += 1;
		}
		if (kCarrier.revokesSkill(CLS_SKILL_NO_INVISIBILITY))
		{
			kOut.noInvisibilityNet -= 1;
		}
	}

	// ONE contributor's share of every slot, added in. A bare compiled-sum fetch per slot -- no anatomy walk,
	// no string address, nothing evaluated (docs/architecture/patterns.md §Materialize at mapFrom).
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
		// THE AIR-RANGE SLOT SUMS TWO ADDRESSES. The unit's own BASE rides the top-level `range` family
		// (range.unit.flat -- where the curator lands the legacy iAirRange), while boosts ride the `air`
		// family's range member (air.unit.range) the table row above reads. The slot serves their SUM:
		// reading only the boost member left every un-boosted airplane at range 0 -- no recon, no rebase,
		// no strike -- while the authored 12 sat in a family nothing gathered.
		aiOut[URS_AIR_RANGE] += pInfo->getScalar(SCALAR_RANGE, CASC_SCOPE_UNIT, CASC_UNIT_FLAT);
	}
}

//	ONE WALK OF THE HELD SET, FILLING EVERY HALF (docs/architecture/patterns.md §DRY (single implementation)). The slot table, the `hideAndSeek`
//	block and the `heal` block fold over exactly the same three carriers and move on exactly the same two facts,
//	so walking three times would be three implementations of one traversal -- and the extras would be the ones
//	that drift.
static void urs_gatherAll(const CvUnit& kUnit, int (&aiOut)[NUM_UNIT_RESOLVED_SLOTS],
	UnitResolvedHideAndSeek& kBlockOut, UnitResolvedHeal& kHealOut, std::vector<int>& aOverriddenOut)
{
	for (int iSlot = 0; iSlot < NUM_UNIT_RESOLVED_SLOTS; ++iSlot)
	{
		aiOut[iSlot] = 0;   // fully define the output every call (the derived-cache contract's rule 2)
	}
	kBlockOut.clear();
	kHealOut.clear();
	aOverriddenOut.clear();

	// The self-recovery LINES, resolved ONCE per gather rather than once per candidate promotion. A per-call
	// string-keyed lookup is what docs/architecture/patterns.md §Materialize at mapFrom bans; paying two at MARK cadence is not that.
	const int iSelfHealLine = GC.getInfoTypeForString("PROMOTIONLINE_SELF_HEAL", /*bHideAssert*/true);
	const int iSelfRepairLine = GC.getInfoTypeForString("PROMOTIONLINE_SELF_REPAIR", /*bHideAssert*/true);

	// The METHOD-SKILL list, hoisted once per gather: which SKILL_* ids are hiding methods at all. Both the
	// detection rows and the membership fold key on the method's SKILL, never the INVISIBLE_ index
	// (vision.md §4).
	std::vector<int> aMethodSkills;
	for (int iMethod = 0; iMethod < GC.getNumInvisibleInfos(); ++iMethod)
	{
		const int iMethodSkill = GC.getMethodSkill((InvisibleTypes)iMethod);
		if (iMethodSkill >= 0
			&& std::find(aMethodSkills.begin(), aMethodSkills.end(), iMethodSkill) == aMethodSkills.end())
		{
			aMethodSkills.push_back(iMethodSkill);
		}
	}

	// THE HELD SET, in full: the unit's own type, every held promotion, every held unit-combat class. A unit's
	// combat classes are its primary + subs + promotion-granted, and CvUnit::isHasUnitCombat already answers the
	// composed question, so this needs no second derivation of the class set.
	urs_addContributor(&kUnit.getUnitInfo(), aiOut);
	urs_addHideAndSeek(kUnit.getUnitInfo(), kUnit.getUnitInfo().getHideAndSeek(), aMethodSkills, kBlockOut);

	// ⛔ STRENGTH IS THE ONE SLOT WHOSE BASE IS PER-UNIT STATE, NOT A FUNCTION OF THE TYPE (owner): WorldBuilder
	// edits an individual unit's strength, and the WBS scenario format persists it (`CombatStr=`, written only
	// when it differs from the type). So the BASE lives on CvUnit as the serialized m_iBaseCombat and THIS PLANE
	// CARRIES THE DELTA ONLY -- promotions and unit-combats -- exactly as the #430 F4 migration ledger records
	// it (Assets/savemigration.txt). The type's own strength is dropped here because the consumer
	// (baseCombatStr*PreCheck) adds m_iBaseCombat; counting it in both places double-counts every unit's
	// authored base. ⚠ The array was zeroed above and only the unit's own info has been added, so this removes
	// exactly the type's contribution and nothing else.
	aiOut[URS_STRENGTH_FLAT] = 0;

	// ⛔ WALK WHAT THE UNIT HOLDS -- NEVER THE REGISTRY. Sweeping every promotion and every unit-combat asking
	// "do I have this?" re-discovers a set the unit already enumerates, and each ask is a map lookup, so the
	// cost tracks the DATABASE rather than the handful the unit carries. That is the O(registry) shape the
	// event-built state exists to delete ([contexts.md]: a read that walks per call is the efficiency defect to
	// reject in review) and it is the same own-data inversion docs/cascade.md §1 (reverse lookups are populated once, at load) bans one plane over.
	// The keyed maps hold an entry per promotion / class the unit has ever touched, and the has-flag tested here
	// is the SAME test isHasPromotion / isHasUnitCombat apply -- so the contributor set is identical.
	std::vector<UnitHeldRung> aHeldLineRungs;
	const std::map<PromotionTypes, PromotionKeyedInfo>& kHeldPromotions = kUnit.getPromotionKeyedInfo();
	for (std::map<PromotionTypes, PromotionKeyedInfo>::const_iterator itPromotion = kHeldPromotions.begin();
		itPromotion != kHeldPromotions.end(); ++itPromotion)
	{
		if (itPromotion->second.m_bHasPromotion)
		{
			const CvPromotionInfo& kPromotion = GC.getPromotionInfo(itPromotion->first);
			urs_addContributor(&kPromotion, aiOut);
			urs_addHideAndSeek(kPromotion, kPromotion.getHideAndSeek(), aMethodSkills, kBlockOut);

			const int iLine = (int)kPromotion.getPromotionLine();
			if (iLine >= 0 && (iLine == iSelfHealLine || iLine == iSelfRepairLine))
			{
				kHealOut.healsOutsideFriendlyTerritory = true;
			}
			if (iLine >= 0)
			{
				aHeldLineRungs.push_back(UnitHeldRung((int)itPromotion->first, iLine, kPromotion.getLinePriority()));
			}
		}
	}

	const std::map<UnitCombatTypes, UnitCombatKeyedInfo>& kHeldCombats = kUnit.getUnitCombatKeyedInfo();
	for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator itCombat = kHeldCombats.begin();
		itCombat != kHeldCombats.end(); ++itCombat)
	{
		if (itCombat->second.m_bHasUnitCombat)
		{
			const CvUnitCombatInfo& kCombat = GC.getUnitCombatInfo(itCombat->first);
			urs_addContributor(&kCombat, aiOut);
			urs_addHideAndSeek(kCombat, kCombat.getHideAndSeek(), aMethodSkills, kBlockOut);
		}
	}

	//	THE SUPERSESSION VERDICT, resolved over the HELD rungs alone. A rung is overridden when the unit ALSO
	//	holds a higher-priority rung of the same line -- so the answer needs only what the unit carries, never
	//	the promotion registry. `held x held` over a handful, once per promotion change, replacing the
	//	`held x REGISTRY` sweep every read of the unit panel used to pay.
	for (size_t iMine = 0; iMine < aHeldLineRungs.size(); ++iMine)
	{
		for (size_t iOther = 0; iOther < aHeldLineRungs.size(); ++iOther)
		{
			if (aHeldLineRungs[iMine].iLine == aHeldLineRungs[iOther].iLine
				&& aHeldLineRungs[iMine].iPriority < aHeldLineRungs[iOther].iPriority)
			{
				aOverriddenOut.push_back(aHeldLineRungs[iMine].iPromotion);
				break;
			}
		}
	}
	std::sort(aOverriddenOut.begin(), aOverriddenOut.end());
}

int UnitResolvedHideAndSeek::detectionAgainst(int iMethodSkillId) const
{
	if (iMethodSkillId < 0)
	{
		return 0;
	}
	for (size_t iRow = 0; iRow < detection.size(); ++iRow)
	{
		if (detection[iRow].first == iMethodSkillId)
		{
			return detection[iRow].second;
		}
	}
	return 0;   // this unit answers that method not at all
}

void UnitResolvedHideAndSeek::addDetection(int iMethodSkillId, int iValue)
{
	// The rows SUM rather than max, so counter-detection stays an ordinary negative deposit -- the same rule the
	// section applies within one carrier, applied across the carriers a unit holds.
	for (size_t iRow = 0; iRow < detection.size(); ++iRow)
	{
		if (detection[iRow].first == iMethodSkillId)
		{
			detection[iRow].second += iValue;
			return;
		}
	}
	detection.push_back(std::make_pair(iMethodSkillId, iValue));
}

bool UnitResolvedHideAndSeek::holdsMethodSkill(int iMethodSkillId) const
{
	if (iMethodSkillId < 0)
	{
		return false;
	}
	for (size_t iRow = 0; iRow < methodSkills.size(); ++iRow)
	{
		if (methodSkills[iRow].first == iMethodSkillId)
		{
			return methodSkills[iRow].second > 0;
		}
	}
	return false;
}

void UnitResolvedHideAndSeek::addMethodSkill(int iMethodSkillId, int iNet)
{
	for (size_t iRow = 0; iRow < methodSkills.size(); ++iRow)
	{
		if (methodSkills[iRow].first == iMethodSkillId)
		{
			methodSkills[iRow].second += iNet;
			return;
		}
	}
	methodSkills.push_back(std::make_pair(iMethodSkillId, iNet));
}

void UnitResolvedValues::gatherInto(const CvUnit& kUnit, int (&aiOut)[NUM_UNIT_RESOLVED_SLOTS])
{
	UnitResolvedHideAndSeek kUnusedBlock;
	UnitResolvedHeal kUnusedHeal;
	std::vector<int> aUnusedOverridden;
	urs_gatherAll(kUnit, aiOut, kUnusedBlock, kUnusedHeal, aUnusedOverridden);
}

void UnitResolvedValues::gatherHideAndSeekInto(const CvUnit& kUnit, UnitResolvedHideAndSeek& kOut)
{
	int aiUnusedSlots[NUM_UNIT_RESOLVED_SLOTS];
	UnitResolvedHeal kUnusedHeal;
	std::vector<int> aUnusedOverridden;
	urs_gatherAll(kUnit, aiUnusedSlots, kOut, kUnusedHeal, aUnusedOverridden);
}

void UnitResolvedValues::gatherHealInto(const CvUnit& kUnit, UnitResolvedHeal& kOut)
{
	int aiUnusedSlots[NUM_UNIT_RESOLVED_SLOTS];
	UnitResolvedHideAndSeek kUnusedBlock;
	std::vector<int> aUnusedOverridden;
	urs_gatherAll(kUnit, aiUnusedSlots, kUnusedBlock, kOut, aUnusedOverridden);
}

void UnitResolvedValues::markDirty(const CvUnit& kUnit)
{
	// THE MARK IS WHAT REBUILDS -- which is what lets every read be a bare fetch (state-repositories.md). The
	// load bracket is not special-cased here: the held set is restored by the save read and the two dirty facts
	// are emitted from inside it, so a unit gathers once its own promotions/combats have streamed in.
	m_bDirty = false;
	urs_gatherAll(kUnit, m_aiValue, m_hideAndSeek, m_heal, m_aOverriddenPromotions);
}

bool UnitResolvedValues::isPromotionOverridden(int iPromotion) const
{
	return std::binary_search(m_aOverriddenPromotions.begin(), m_aOverriddenPromotions.end(), iPromotion);
}
