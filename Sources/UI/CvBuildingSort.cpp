//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:	CvBuildingSort.cpp
//
//  PURPOSE: Sorting classes for buildings
//
//------------------------------------------------------------------------------------------------

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "CvBuildListValuation.h"
#include "CvBuildingInfo.h"
#include "CvInfoKinds.h"
#include "CvModifiers.h"
#include "Engine/CvCity.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"

bool BuildingSortBase::isLesserBuilding(const CvPlayer* pPlayer, CvCity* pCity, BuildingTypes eBuilding1, BuildingTypes eBuilding2)
{
	int iVal1;
	int iVal2;

	stdext::hash_map<BuildingTypes, int>::iterator it = m_mapValueCache.find(eBuilding1);
	if (it == m_mapValueCache.end())
	{
		iVal1 = getBuildingValue(pPlayer, pCity, eBuilding1);
		m_mapValueCache[eBuilding1] = iVal1;
	}
	else
		iVal1 = it->second;

	it = m_mapValueCache.find(eBuilding2);
	if (it == m_mapValueCache.end())
	{
		iVal2 = getBuildingValue(pPlayer, pCity, eBuilding2);
		m_mapValueCache[eBuilding2] = iVal2;
	}
	else
		iVal2 = it->second;

	// To keep the strict weak ordering for sorting, the result of the comparison cannot just be inverted, equal must always be false
	if (m_bInvert)
		return iVal1 < iVal2;
	else
		return iVal1 > iVal2;
}

bool BuildingSortBase::isInverse() const
{
	return m_bInvert;
}

bool BuildingSortBase::setInverse(bool bInvert)
{
	const bool bChanged = bInvert != m_bInvert;
	m_bInvert = bInvert;
	return bChanged;
}

void BuildingSortBase::deleteCache()
{
	m_mapValueCache.clear();
}

BuildingSortBase::~BuildingSortBase()
{

}

BuildingSortCommerce::BuildingSortCommerce(CommerceTypes eCommerce, bool bInvert) : BuildingSortBase(bInvert)
{
	m_eCommerce = eCommerce;
}

int BuildingSortCommerce::getBuildingValue(const CvPlayer* pPlayer, CvCity* pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	int aFlatCommerce[NUM_COMMERCE_TYPES];
	if (!CvBuildListValuation::buildingFlatCommerce(eBuilding, pPlayer, pCity, aFlatCommerce))
	{
		return 0;
	}
	int iCommerce = aFlatCommerce[m_eCommerce];
	// The commerce YIELD is split across the commerce types by the player's rates, so the share this criterion's
	// type receives counts toward it. The flat yield is x100 and getCommercePercent is a human percent, so the
	// /100 here is that percent's reduction and the sum stays x100.
	int aFlatYields[NUM_YIELD_TYPES];
	if (pPlayer != NULL
		&& CvBuildListValuation::buildingFlatYields(eBuilding, pPlayer, pCity, aFlatYields))
	{
		iCommerce += aFlatYields[YIELD_COMMERCE] * pPlayer->getCommercePercent(m_eCommerce) / 100;
	}
	return iCommerce;
}

BuildingSortYield::BuildingSortYield(YieldTypes eYield, bool bInvert) : BuildingSortBase(bInvert)
{
	m_eYield = eYield;
}

int BuildingSortYield::getBuildingValue(const CvPlayer* pPlayer, CvCity* pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	// The FLAT channel is what an ordering can be built from. The yield MODIFIER is a percent: it only becomes a
	// comparable amount once resolved against the city's current rate in the channel, which is a valuation the
	// seam owns (contexts in, delta out) -- adding the raw percent here would rank two different scales as one.
	int aFlatYields[NUM_YIELD_TYPES];
	if (!CvBuildListValuation::buildingFlatYields(eBuilding, pPlayer, pCity, aFlatYields))
	{
		return 0;
	}
	return aFlatYields[m_eYield];
}

int BuildingSortHappiness::getBuildingValue(const CvPlayer* pPlayer, CvCity* pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	int iBalance = 0;
	if (!CvBuildListValuation::buildingHappinessBalance(eBuilding, pPlayer, pCity, iBalance))
	{
		return 0;
	}
	return iBalance;
}

int BuildingSortHealth::getBuildingValue(const CvPlayer* pPlayer, CvCity* pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	int iBalance = 0;
	if (!CvBuildListValuation::buildingHealthBalance(eBuilding, pPlayer, pCity, iBalance))
	{
		return 0;
	}
	return iBalance;
}

int BuildingSortCost::getBuildingValue(const CvPlayer* pPlayer, CvCity* pCity, BuildingTypes eBuilding) const
{
	if (pCity)
	{
		return pCity->getProductionNeeded(eBuilding) - pCity->getProgressOnBuilding(eBuilding);
	}
	else
	{
		return pPlayer->getProductionNeeded(eBuilding);
	}
}

int BuildingSortName::getBuildingValue(const CvPlayer* pPlayer, CvCity* pCity, BuildingTypes eBuilding) const
{
	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);
	// Get the localized name/description of the building
	CvWString szName = kBuilding.getDescription();

	if (szName.empty())
		return 0;

	// Encode the first few characters of the name into an integer
	// This way we can return an int value that represents the alphabetical order
	int value = 0;
	for (int i = 0; i < std::min((int)szName.length(), 3); i++)
	{
		// Convert character to uppercase so sorting is case-insensitive
		wchar_t c = towupper(szName[i]);
		// Shift previous value and add current character code
		value = value * 256 + (int)c;
	}
	return -value;
}

// name sorting defaults to A first
bool BuildingSortName::isLesserBuilding(const CvPlayer* pPlayer, CvCity* pCity, BuildingTypes eBuilding1, BuildingTypes eBuilding2) const
{
	if (m_bInvert)
		return wcscmp(GC.getBuildingInfo(eBuilding1).getDescription(), GC.getBuildingInfo(eBuilding2).getDescription()) > 0;
	else
		return wcscmp(GC.getBuildingInfo(eBuilding1).getDescription(), GC.getBuildingInfo(eBuilding2).getDescription()) < 0;
}

int BuildingSortProperty::getBuildingValue(const CvPlayer* pPlayer, CvCity* pCity, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	if (m_eProperty == NO_PROPERTY)
	{
		return 0;
	}
	const CvModifiers* pModifiers = GC.getBuildingInfo(eBuilding).getModifiers();
	if (pModifiers == NULL)
	{
		return 0;
	}
	// The building's own compiled contribution to the property, over the scopes a building authors the open
	// per-property plane at (PROPERTY_X.city.flat, PROPERTY_X.empire.flat). Signed, and x100 -- an ordering is
	// scale-invariant, so it is never reduced here.
	return pModifiers->propertySum((int)m_eProperty, CASC_SCOPE_CITY, CASC_UNIT_FLAT)
		+ pModifiers->propertySum((int)m_eProperty, CASC_SCOPE_EMPIRE, CASC_UNIT_FLAT);
}

BuildingSortList::BuildingSortList(CvPlayer *pPlayer, CvCity *pCity)
{
	PROFILE_EXTRA_FUNC();
	m_pPlayer = pPlayer;
	m_pCity = pCity;

	for (int i = 0; i < NUM_BUILDING_SORT; i++)
	{
		m_apBuildingSort[i] = NULL;
	}

	m_eActiveSort = BUILDING_SORT_COST;
}

BuildingSortList::~BuildingSortList()
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < NUM_BUILDING_SORT; i++)
	{
		SAFE_DELETE(m_apBuildingSort[i]);
	}
}

void BuildingSortList::init()
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < NUM_BUILDING_SORT; i++)
	{
		SAFE_DELETE(m_apBuildingSort[i]);
	}

	m_apBuildingSort[BUILDING_SORT_NAME] = new BuildingSortName();
	m_apBuildingSort[BUILDING_SORT_COST] = new BuildingSortCost(true);
	m_apBuildingSort[BUILDING_SORT_SCIENCE] = new BuildingSortCommerce(COMMERCE_RESEARCH);
	m_apBuildingSort[BUILDING_SORT_CULTURE] = new BuildingSortCommerce(COMMERCE_CULTURE);
	m_apBuildingSort[BUILDING_SORT_ESPIONAGE] = new BuildingSortCommerce(COMMERCE_ESPIONAGE);
	m_apBuildingSort[BUILDING_SORT_GOLD] = new BuildingSortCommerce(COMMERCE_GOLD);
	m_apBuildingSort[BUILDING_SORT_FOOD] = new BuildingSortYield(YIELD_FOOD);
	m_apBuildingSort[BUILDING_SORT_PRODUCTION] = new BuildingSortYield(YIELD_PRODUCTION);
	m_apBuildingSort[BUILDING_SORT_HAPPINESS] = new BuildingSortHappiness();
	m_apBuildingSort[BUILDING_SORT_HEALTH] = new BuildingSortHealth();
	m_apBuildingSort[BUILDING_SORT_CRIME] = new BuildingSortProperty((PropertyTypes)GC.getInfoTypeForString("PROPERTY_CRIME"), true);
	m_apBuildingSort[BUILDING_SORT_FLAMMABILITY] = new BuildingSortProperty((PropertyTypes)GC.getInfoTypeForString("PROPERTY_FLAMMABILITY"), true);
}

BuildingSortTypes BuildingSortList::getActiveSort() const
{
	return m_eActiveSort;
}

bool BuildingSortList::setActiveSort(BuildingSortTypes eActiveSort)
{
	FASSERT_BOUNDS(0, NUM_BUILDING_SORT, eActiveSort);
	const bool bChanged = m_eActiveSort != eActiveSort;
	m_eActiveSort = eActiveSort;
	return bChanged;
}

void BuildingSortList::setCity(CvCity *pCity)
{
	m_pCity = pCity;
}

void BuildingSortList::setPlayer(CvPlayer *pPlayer)
{
	m_pPlayer = pPlayer;
}

bool BuildingSortList::operator ()(BuildingTypes eBuilding1, BuildingTypes eBuilding2)
{
	return m_apBuildingSort[m_eActiveSort]->isLesserBuilding(m_pPlayer, m_pCity, eBuilding1, eBuilding2);
}

void BuildingSortList::deleteCache()
{
	m_apBuildingSort[m_eActiveSort]->deleteCache();
}