//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:	CvBuildingFilters.cpp
//
//  PURPOSE: Classes to filter buildings
//
//------------------------------------------------------------------------------------------------

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "CvBuildingFilters.h"
#include "CvBuildListValuation.h"
#include "CvBuildingInfo.h"
#include "CvInfoKinds.h"
#include "CvModEntry.h"
#include "CvModifiers.h"
#include "Infrastructure/CvBugOptions.h"
#include "Engine/CvCity.h"
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "AI/CvPlayerAI.h"

namespace
{
	// Does the building author ANY deposit in this modifier family? The compiled point sums answer only the
	// scope-wide (memberless) slot, and the military families are authored overwhelmingly through KEYED targets
	// (experience.city.unitCombats.UNITCOMBAT_*, combat.city.unitCombats.*). Keyed/targeted groups are entry-list
	// reads by design (CvBuildingInfo.h), so family MEMBERSHIP is asked of the compiled entry list.
	bool bf_authorsFamily(const CvBuildingInfo& kBuilding, ModifierFamily eFamily)
	{
		const CvModifiers* pModifiers = kBuilding.getModifiers();
		if (pModifiers == NULL)
		{
			return false;
		}
		foreach_(const CvModEntry* pEntry, pModifiers->entries())
		{
			if (pEntry->family == eFamily)
			{
				return true;
			}
		}
		return false;
	}

	// The building's own compiled contribution to one property, over the scopes a building authors the open
	// per-property plane at (PROPERTY_X.city.flat, PROPERTY_X.empire.flat). Signed and x100.
	int bf_propertyAmount(const CvBuildingInfo& kBuilding, PropertyTypes eProperty)
	{
		const CvModifiers* pModifiers = kBuilding.getModifiers();
		if (pModifiers == NULL)
		{
			return 0;
		}
		return pModifiers->propertySum((int)eProperty, CASC_SCOPE_CITY, CASC_UNIT_FLAT)
			+ pModifiers->propertySum((int)eProperty, CASC_SCOPE_EMPIRE, CASC_UNIT_FLAT);
	}
}

void BuildingFilterBase::Activate()
{
	setActive(true);
}

void BuildingFilterBase::Deactivate()
{
	setActive(false);
}

bool BuildingFilterBase::isActive() const
{
	return m_bActive;
}

bool BuildingFilterBase::setActive(bool bActive)
{
	const bool bChanged = m_bActive ^ bActive;
	m_bActive = bActive;
	return bChanged;
}

bool BuildingFilterBase::isFiltered(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	return !m_bActive || (m_bInvert ^ isFilteredBuilding(pPlayer, pCity, eBuilding));
}

BuildingFilterBase::~BuildingFilterBase()
{

}

bool BuildingFilterCanBuild::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	if (pCity)
	{
		// The list is an OFFER consumer, so its normal content is the fresh offer -- gate-passed AND not
		// queued. A queued building is already being handled, and the raw tri-state deliberately ignores the
		// queued overlay (CvCity.h), so the offer question is asked of the offer read, never of the state.
		if (pCity->isBuildingOffered(eBuilding))
		{
			return true;
		}
		// Show-unbuildable mode adds the GREYED tree members. A queued candidate reads LISTED, not GREYED, so
		// it stays excluded in both modes.
		return m_bShowSomeUnconstructable && pCity->getBuildingAvailability(eBuilding) == EnablerDomain::STATE_GREYED;
	}
	const EnablerDomain::State eFloor = m_bShowSomeUnconstructable ? EnablerDomain::STATE_GREYED : EnablerDomain::STATE_LISTED;
	return pPlayer->getBuildingAvailabilityAnywhere(eBuilding) >= eFloor;
}

bool BuildingFilterIsWonder::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	return isLimitedWonder(eBuilding);
}

bool BuildingFilterIsGreatWonder::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	return isWorldWonder(eBuilding);
}

bool BuildingFilterIsNationalWonder::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	return isNationalWonder(eBuilding);
}

BuildingFilterIsCommerce::BuildingFilterIsCommerce(CommerceTypes eCommerce, bool bInvert) : BuildingFilterBase(bInvert), m_eCommerce(eCommerce) {}

bool BuildingFilterIsCommerce::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	int aFlatCommerce[NUM_COMMERCE_TYPES];
	if (!CvBuildListValuation::buildingFlatCommerce(eBuilding, pPlayer, pCity, aFlatCommerce))
	{
		return false;
	}
	return aFlatCommerce[m_eCommerce] > 0;
}

BuildingFilterIsYieldAndCommerce::BuildingFilterIsYieldAndCommerce(YieldTypes eYield, CommerceTypes eCommerce, bool bInvert)
	: BuildingFilterBase(bInvert), m_eYield(eYield), m_eCommerce(eCommerce) {}

bool BuildingFilterIsYieldAndCommerce::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	// Yield (e.g., YIELD_COMMERCE) or commerce type (e.g., COMMERCE_GOLD).
	int aFlatYields[NUM_YIELD_TYPES];
	if (CvBuildListValuation::buildingFlatYields(eBuilding, pPlayer, pCity, aFlatYields)
		&& aFlatYields[m_eYield] > 0)
	{
		return true;
	}
	int aYieldModifiers[NUM_YIELD_TYPES];
	if (CvBuildListValuation::buildingYieldModifiers(eBuilding, pPlayer, pCity, aYieldModifiers)
		&& aYieldModifiers[m_eYield] > 0)
	{
		return true;
	}
	int aFlatCommerce[NUM_COMMERCE_TYPES];
	return CvBuildListValuation::buildingFlatCommerce(eBuilding, pPlayer, pCity, aFlatCommerce)
		&& aFlatCommerce[m_eCommerce] > 0;
}

BuildingFilterIsYield::BuildingFilterIsYield(YieldTypes eYield, bool bInvert) : BuildingFilterBase(bInvert), m_eYield(eYield) {}

bool BuildingFilterIsYield::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	// A building offers the yield if it deposits the channel flat OR raises the city's rate in it -- the two are
	// separate groups because they are separate scales, and either one alone answers this filter's question.
	int aFlatYields[NUM_YIELD_TYPES];
	if (CvBuildListValuation::buildingFlatYields(eBuilding, pPlayer, pCity, aFlatYields)
		&& aFlatYields[m_eYield] > 0)
	{
		return true;
	}
	int aYieldModifiers[NUM_YIELD_TYPES];
	return CvBuildListValuation::buildingYieldModifiers(eBuilding, pPlayer, pCity, aYieldModifiers)
		&& aYieldModifiers[m_eYield] > 0;
}

bool BuildingFilterIsHappiness::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	int iBalance = 0;
	if (!CvBuildListValuation::buildingHappinessBalance(eBuilding, pPlayer, pCity, iBalance))
	{
		return false;
	}
	return iBalance > 0;
}

bool BuildingFilterIsHealth::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	int iBalance = 0;
	if (!CvBuildListValuation::buildingHealthBalance(eBuilding, pPlayer, pCity, iBalance))
	{
		return false;
	}
	return iBalance > 0;
}

bool BuildingFilterIsUnhappiness::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	int iBalance = 0;
	if (!CvBuildListValuation::buildingHappinessBalance(eBuilding, pPlayer, pCity, iBalance))
	{
		return false;
	}
	return iBalance < 0;
}

bool BuildingFilterIsUnhealthiness::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	int iBalance = 0;
	if (!CvBuildListValuation::buildingHealthBalance(eBuilding, pPlayer, pCity, iBalance))
	{
		return false;
	}
	return iBalance < 0;
}

bool BuildingFilterIsMilitary::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);
	// The military BUILD RATE is the tag-filtered `units` read ([modifier.md] §4: military is a PREDICATE on the
	// units target, not a category). The KEYED buildRate targets are still deliberately NOT read: a unit- or
	// domain-keyed build rate names UNIT_TRADE_CARAVAN as readily as a soldier, so it says nothing about military
	// without resolving the target's own nature -- whereas the tag filter names the military class outright.
	{
		const int iUnitsSeg = InfoValuation::keyedTargetSegment("units");
		const int iMilitaryTag = GC.getInfoTypeForString("TAG_MILITARY", true);
		if (InfoValuation::taggedTargetSum(kBuilding.getModifiers(), MODFAM_BUILD_RATE, -1,
				CASC_SCOPE_CITY, CASC_UNIT_PERCENT, iUnitsSeg, iMilitaryTag) != 0
			|| InfoValuation::taggedTargetSum(kBuilding.getModifiers(), MODFAM_BUILD_RATE, -1,
				CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT, iUnitsSeg, iMilitaryTag) != 0)
		{
			return true;
		}
	}
	// Unit EXPERIENCE and COMBAT are military whole-family -- every kind either one authors is about the units
	// the city produces -- so membership answers the filter, and it covers the keyed unitCombat/domain targets
	// the point sums cannot see.
	if (bf_authorsFamily(kBuilding, MODFAM_EXPERIENCE) || bf_authorsFamily(kBuilding, MODFAM_COMBAT))
	{
		return true;
	}
	// A building that hands the units it produces a free promotion is military by the same reading.
	return kBuilding.hasTriggerPromotions();
}

bool BuildingFilterIsCityDefense::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);
	// The DEFENSE family IS the concept, so this asks the whole vocabulary rather than electing kinds: any
	// non-zero defense kind, at a scope a building authors defense at, means the building touches city defense.
	// Each kind's canonical authored unit resolves in the vocabulary (infoDefenseUnit), so no unit is chosen
	// here and no kind carries a weight -- the filter needs presence, not magnitude.
	for (int iDefenseKind = 0; iDefenseKind < NUM_DEFENSE_KINDS; ++iDefenseKind)
	{
		if (kBuilding.getDefense((DefenseKind)iDefenseKind, CASC_SCOPE_CITY) != 0
			|| kBuilding.getDefense((DefenseKind)iDefenseKind, CASC_SCOPE_EMPIRE) != 0)
		{
			return true;
		}
	}
	return false;
}

bool BuildingFilterIsProperty::isFilteredBuilding(const CvPlayer *pPlayer, CvCity *pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	if (m_eProperty == NO_PROPERTY)
	{
		return false;
	}
	// Either SIGN qualifies: a building that suppresses crime belongs in the crime filter exactly as one that
	// breeds it does.
	return bf_propertyAmount(GC.getBuildingInfo(eBuilding), m_eProperty) != 0;
}

BuildingFilterList::BuildingFilterList(CvPlayer *pPlayer, CvCity *pCity)
{
	PROFILE_EXTRA_FUNC();
	m_pPlayer = pPlayer;
	m_pCity = pCity;

	for (int i = 0; i < NUM_BUILDING_FILTERS; i++)
	{
		m_apBuildingFilters[i] = NULL;
	}
}

void BuildingFilterList::init()
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < NUM_BUILDING_FILTERS; i++)
	{
		SAFE_DELETE(m_apBuildingFilters[i]);
	}

	m_apBuildingFilters[BUILDING_FILTER_HIDE_BASIC_INVISIBLE] = new BuildingFilterCanBuild(true);
	if (m_pCity)
		m_apBuildingFilters[BUILDING_FILTER_HIDE_BASIC_INVISIBLE]->Activate();
	m_apBuildingFilters[BUILDING_FILTER_HIDE_BUILDABLE] = new BuildingFilterCanBuild(false, true);
	m_apBuildingFilters[BUILDING_FILTER_HIDE_UNBUILDABLE] = new BuildingFilterCanBuild();
	m_apBuildingFilters[BUILDING_FILTER_HIDE_GREAT_WONDER] = new BuildingFilterIsGreatWonder(true);
	m_apBuildingFilters[BUILDING_FILTER_HIDE_NATIONAL_WONDER] = new BuildingFilterIsNationalWonder(true);
	m_apBuildingFilters[BUILDING_FILTER_HIDE_NORMAL] = new BuildingFilterIsWonder();
	m_apBuildingFilters[BUILDING_FILTER_SHOW_SCIENCE] = new BuildingFilterIsCommerce(COMMERCE_RESEARCH);
	m_apBuildingFilters[BUILDING_FILTER_SHOW_FOOD] = new BuildingFilterIsYield(YIELD_FOOD);
	m_apBuildingFilters[BUILDING_FILTER_SHOW_CULTURE] = new BuildingFilterIsCommerce(COMMERCE_CULTURE);
	m_apBuildingFilters[BUILDING_FILTER_SHOW_ESPIONAGE] = new BuildingFilterIsCommerce(COMMERCE_ESPIONAGE);
	m_apBuildingFilters[BUILDING_FILTER_SHOW_GOLD] = new BuildingFilterIsYieldAndCommerce(YIELD_COMMERCE, COMMERCE_GOLD);
	m_apBuildingFilters[BUILDING_FILTER_SHOW_PRODUCTION] = new BuildingFilterIsYield(YIELD_PRODUCTION);
	m_apBuildingFilters[BUILDING_FILTER_SHOW_HAPPINESS] = new BuildingFilterIsHappiness();
	m_apBuildingFilters[BUILDING_FILTER_SHOW_HEALTH] = new BuildingFilterIsHealth();
	m_apBuildingFilters[BUILDING_FILTER_SHOW_MILITARY] = new BuildingFilterIsMilitary();
	m_apBuildingFilters[BUILDING_FILTER_SHOW_CITY_DEFENSE] = new BuildingFilterIsCityDefense();
	m_apBuildingFilters[BUILDING_FILTER_HIDE_UNHAPPINESS] = new BuildingFilterIsUnhappiness(true);
	m_apBuildingFilters[BUILDING_FILTER_HIDE_UNHEALTHINESS] = new BuildingFilterIsUnhealthiness(true);
	m_apBuildingFilters[BUILDING_FILTER_SHOW_CRIME] = new BuildingFilterIsProperty((PropertyTypes)GC.getInfoTypeForString("PROPERTY_CRIME"));
	m_apBuildingFilters[BUILDING_FILTER_SHOW_FLAMMABILITY] = new BuildingFilterIsProperty((PropertyTypes)GC.getInfoTypeForString("PROPERTY_FLAMMABILITY"));
	m_apBuildingFilters[BUILDING_FILTER_SHOW_EDUCATION] = new BuildingFilterIsProperty((PropertyTypes)GC.getInfoTypeForString("PROPERTY_EDUCATION"));
	m_apBuildingFilters[BUILDING_FILTER_SHOW_DISEASE] = new BuildingFilterIsProperty((PropertyTypes)GC.getInfoTypeForString("PROPERTY_DISEASE"));
	m_apBuildingFilters[BUILDING_FILTER_SHOW_AIR_POLLUTION] = new BuildingFilterIsProperty((PropertyTypes)GC.getInfoTypeForString("PROPERTY_AIR_POLLUTION"));
	m_apBuildingFilters[BUILDING_FILTER_SHOW_WATER_POLLUTION] = new BuildingFilterIsProperty((PropertyTypes)GC.getInfoTypeForString("PROPERTY_WATER_POLLUTION"));
	m_apBuildingFilters[BUILDING_FILTER_SHOW_TOURISM] = new BuildingFilterIsProperty((PropertyTypes)GC.getInfoTypeForString("PROPERTY_TOURISM"));
	m_apBuildingFilters[BUILDING_FILTER_HIDE_UNBUILDABLE]->setActive(getBugOptionBOOL("CityScreen__HideUnconstructableBuildings", false));
}

BuildingFilterList::~BuildingFilterList()
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < NUM_BUILDING_FILTERS; i++)
	{
		SAFE_DELETE(m_apBuildingFilters[i]);
	}
}

bool BuildingFilterList::isFilterActive(BuildingFilterTypes i) const
{
	FASSERT_BOUNDS(0, NUM_BUILDING_FILTERS, i)
	return m_apBuildingFilters[i]->isActive();
}

void BuildingFilterList::setCity(CvCity *pCity)
{
	m_pCity = pCity;
}

void BuildingFilterList::setPlayer(CvPlayer *pPlayer)
{
	m_pPlayer = pPlayer;
}

bool BuildingFilterList::setFilterActive(BuildingFilterTypes i, bool bActive)
{
	FASSERT_BOUNDS(0, NUM_BUILDING_FILTERS, i)
	return m_apBuildingFilters[i]->setActive(bActive);
}

bool BuildingFilterList::isFiltered(BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < NUM_BUILDING_FILTERS; i++)
	{
		if (!m_apBuildingFilters[i]->isFiltered(m_pPlayer, m_pCity, eBuilding))
			return false;
	}
	return true;
}

void BuildingFilterList::setFilterActiveAll(BuildingFilterTypes eFilter, bool bActive)
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < MAX_PC_PLAYERS; ++iI)
	{
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
		if (kLoopPlayer.isAlive())
		{
			kLoopPlayer.setBuildingListFilterActive(eFilter, bActive);

			algo::for_each(kLoopPlayer.cities(), CvCity::fn::setBuildingListFilterActive(eFilter, bActive));
		}
	}
}