//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:	CvUnitSort.cpp
//
//  PURPOSE: Sorting classes for units
//
//------------------------------------------------------------------------------------------------

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "Engine/CvCity.h"
#include "Engine/CvGame.h"          // GC.getGame().isOption -- the SizeMatters flag the strength read takes
#include "Defines/CvGlobals.h"
#include "CvUnitInfo.h"             // the rebuilt unit poco: the family point reads these criteria sort on
#include "CvInfoKinds.h"            // the kind vocabulary (CombatKind/MovementKind/... + the InfoScalar table)
#include "Engine/CvPlayer.h"

bool UnitSortBase::isLesserUnit(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit1, UnitTypes eUnit2) const
{
	// To keep the strict weak ordering for sorting, the result of the comparison cannot just be inverted, equal must always be false
	if (m_bInvert)
		return getUnitValue(pPlayer, pCity, eUnit1) < getUnitValue(pPlayer, pCity, eUnit2);
	else
		return getUnitValue(pPlayer, pCity, eUnit1) > getUnitValue(pPlayer, pCity, eUnit2);
}

bool UnitSortBase::isInverse() const
{
	return m_bInvert;
}

bool UnitSortBase::setInverse(bool bInvert)
{
	const bool bChanged = bInvert != m_bInvert;
	m_bInvert = bInvert;
	return bChanged;
}

UnitSortBase::~UnitSortBase()
{
}

// The unit-statistic criteria are STAT sorts: each reads the unit type's OWN compiled data, so none of them
// needs a city or a player. Every family/scalar read below is ×100 (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)) and is left at that
// scale on purpose -- a sort only ORDERS, so it is scale-invariant and a ÷100 here would buy nothing but a
// truncation. Where a criterion maxes several magnitudes they are all on the same ×100 plane.
int UnitSortStrength::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	// The ONE strength read: it selects identity.base.airCombat for DOMAIN_AIR and strength.unit.flat otherwise,
	// folds in the combat-class contribution, and applies the SizeMatters rank. The info is pure data and reads
	// no game state, so the consumer supplies the option flag -- the same idiom CvGameTextMgr prints with, so the
	// list orders by the strength the player is shown.
	const bool bSizeMatters = GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS);
	return GC.getUnitInfo(eUnit).getTotalModifiedCombatStrength(bSizeMatters);
}

int UnitSortMove::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	// movement.unit.flat -- the memberless deposit, which IS the MOVEMENT_MOVES slot.
	return GC.getUnitInfo(eUnit).getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT);
}

int UnitSortCollateral::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	// collateral.unit.damage is authored PERCENT, so the populated slot is the modifier one, not the flat.
	return GC.getUnitInfo(eUnit).getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT);
}

int UnitSortRange::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	// The three reach magnitudes a unit can carry. They live in three different families -- air range is the
	// top-level `range` family, nuke range sits under air, drop range under movement -- but all three are ×100
	// flats, so the max compares like with like.
	const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);
	const int iAirRange = kUnit.getScalar(SCALAR_RANGE, CASC_SCOPE_UNIT, CASC_UNIT_FLAT);
	const int iNukeRange = kUnit.getAir(AIR_NUKE_RANGE, CASC_SCOPE_UNIT);
	const int iDropRange = kUnit.getMovement(MOVEMENT_DROP_RANGE, CASC_SCOPE_UNIT);
	return std::max(std::max(iAirRange, iNukeRange), iDropRange);
}

int UnitSortBombard::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	// bombard.unit.rate is authored PERCENT (BOMBARD_AIR_BOMB_RATE is the separate flat air-bomb plane).
	return GC.getUnitInfo(eUnit).getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT);
}

int UnitSortCargo::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	// cargo.unit.space.flat -- the unqualified capacity plane (a hold restricted to a cargo class carries a
	// `unit:` predicate on its entries, which is a CONSUMER-side test against a candidate, not a capacity).
	return GC.getUnitInfo(eUnit).getCargo(CARGO_SPACE, CASC_SCOPE_UNIT);
}

int UnitSortWithdrawal::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	// withdrawal.unit.percent -- a straggler family, read through the InfoScalar table.
	return GC.getUnitInfo(eUnit).getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
}

int UnitSortPower::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	// identity.militaryWorth -- the power-valuation config the legacy power value became.
	return GC.getUnitInfo(eUnit).getMilitaryWorth();
}

int UnitSortCost::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	if (pCity)
	{
		return pCity->getProductionNeeded(eUnit) - pCity->getProgressOnUnit(eUnit);
	}
	return pPlayer->getProductionNeeded(eUnit);
}

// dummy
int UnitSortName::getUnitValue(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit) const
{
	return 0;
}

// name sorting defaults to A first
bool UnitSortName::isLesserUnit(const CvPlayer *pPlayer, const CvCity *pCity, UnitTypes eUnit1, UnitTypes eUnit2) const
{
	if (m_bInvert)
		return wcscmp(GC.getUnitInfo(eUnit1).getDescription(), GC.getUnitInfo(eUnit2).getDescription()) > 0;
	else
		return wcscmp(GC.getUnitInfo(eUnit1).getDescription(), GC.getUnitInfo(eUnit2).getDescription()) < 0;
}

UnitSortList::UnitSortList(const CvPlayer *pPlayer, const CvCity *pCity)
{
	m_pPlayer = pPlayer;
	m_pCity = pCity;

	m_apUnitSort[UNIT_SORT_NAME] = new UnitSortName();
	m_apUnitSort[UNIT_SORT_COST] = new UnitSortCost(true);
	m_apUnitSort[UNIT_SORT_STRENGTH] = new UnitSortStrength();
	m_apUnitSort[UNIT_SORT_MOVE] = new UnitSortMove();
	m_apUnitSort[UNIT_SORT_COLLATERAL] = new UnitSortCollateral();
	m_apUnitSort[UNIT_SORT_RANGE] = new UnitSortRange();
	m_apUnitSort[UNIT_SORT_BOMBARD] = new UnitSortBombard();
	m_apUnitSort[UNIT_SORT_CARGO] = new UnitSortCargo();
	m_apUnitSort[UNIT_SORT_WITHDRAWAL] = new UnitSortWithdrawal();
	m_apUnitSort[UNIT_SORT_POWER] = new UnitSortPower();

	m_eActiveSort = UNIT_SORT_COST;
}

UnitSortList::~UnitSortList()
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < NUM_UNIT_SORT; i++)
	{
		delete m_apUnitSort[i];
	}
}

UnitSortTypes UnitSortList::getActiveSort() const
{
	return m_eActiveSort;
}

bool UnitSortList::setActiveSort(UnitSortTypes eActiveSort)
{
	FASSERT_BOUNDS(0, NUM_UNIT_SORT, eActiveSort);
	const bool bChanged = m_eActiveSort != eActiveSort;
	m_eActiveSort = eActiveSort;
	return bChanged;
}

void UnitSortList::setCity(const CvCity *pCity)
{
	m_pCity = pCity;
}

void UnitSortList::setPlayer(const CvPlayer *pPlayer)
{
	m_pPlayer = pPlayer;
}

bool UnitSortList::operator ()(UnitTypes eUnit1, UnitTypes eUnit2) const
{
	return m_apUnitSort[m_eActiveSort]->isLesserUnit(m_pPlayer, m_pCity, eUnit1, eUnit2);
}