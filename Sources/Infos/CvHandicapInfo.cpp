//------------------------------------------------------------------------------------------------
//  FILE:    CvHandicapInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvJsonBuildingInfo.h"
#include "CvJsonHeritageInfo.h"
#include "AI/CvGameAI.h"
#include "UI/CvGameTextMgr.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "AI/CvPlayerAI.h"
#include "Infrastructure/CvPython.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvXMLLoadUtilityModTools.h"
#include "Tools/CheckSum.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvHandicapInfo.h"


//======================================================================================================
//					CvHandicapInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvHandicapInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvHandicapInfo::CvHandicapInfo()
{
	CvInfoUtil(this).initDataMembers();
}


// Every XML-backed field is declared here (#196); read/copy derive from it.
// getCheckSum stays explicit: the legacy checksum omits m_iSubdueAnimalBonusAI (a read field),
// so delegating would change the savegame asset checksum.
void CvHandicapInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iFreeWinsVsBarbs, L"iFreeWinsVsBarbs")
		.add(m_iAnimalAttackProb, L"iAnimalAttackProb")
		.add(m_iAdvancedStartPointsMod, L"iAdvancedStartPointsMod")
		.add(m_iStartingGold, L"iGold")
		.add(m_iUnitUpkeepPercent, L"iUnitUpkeepPercent")
		.add(m_iDistanceMaintenancePercent, L"iDistanceMaintenancePercent")
		.add(m_iNumCitiesMaintenancePercent, L"iNumCitiesMaintenancePercent")
		.add(m_iColonyMaintenancePercent, L"iColonyMaintenancePercent")
		.add(m_iMaxColonyMaintenance, L"iMaxColonyMaintenance")
		.add(m_iCorporationMaintenancePercent, L"iCorporationMaintenancePercent")
		.add(m_iCivicUpkeepPercent, L"iCivicUpkeepPercent")
		.add(m_iInflationPercent, L"iInflationPercent")
		.add(m_iHealthBonus, L"iHealthBonus")
		.add(m_iHappyBonus, L"iHappyBonus")
		.add(m_iAttitudeChange, L"iAttitudeChange")
		.add(m_iNoTechTradeModifier, L"iNoTechTradeModifier")
		.add(m_iTechTradeKnownModifier, L"iTechTradeKnownModifier")
		.add(m_iUnownedWaterTilesPerBarbarianUnit, L"iUnownedWaterTilesPerBarbarianUnit")
		.add(m_iUnownedTilesPerBarbarianCity, L"iUnownedTilesPerBarbarianCity")
		.add(m_iBarbarianCityCreationTurnsElapsed, L"iBarbarianCityCreationTurnsElapsed")
		.add(m_iBarbarianCityCreationProb, L"iBarbarianCityCreationProb")
		.add(m_iAnimalCombatModifier, L"iAnimalBonus")
		.add(m_iBarbarianCombatModifier, L"iBarbarianBonus")
		.add(m_iAIAnimalCombatModifier, L"iAIAnimalBonus")
		.add(m_iSubdueAnimalBonusAI, L"iSubdueAnimalBonusAI")
		.add(m_iAIBarbarianCombatModifier, L"iAIBarbarianBonus")
		.add(m_iStartingDefenseUnits, L"iStartingDefenseUnits")
		.add(m_iStartingWorkerUnits, L"iStartingWorkerUnits")
		.add(m_iStartingExploreUnits, L"iStartingExploreUnits")
		.add(m_iAIStartingDefenseUnits, L"iAIStartingDefenseUnits")
		.add(m_iAIStartingWorkerUnits, L"iAIStartingWorkerUnits")
		.add(m_iAIStartingExploreUnits, L"iAIStartingExploreUnits")
		.add(m_iBarbarianInitialDefenders, L"iBarbarianDefenders")
		.add(m_iAIDeclareWarProb, L"iAIDeclareWarProb")
		.add(m_iAIWorkRateModifier, L"iAIWorkRateModifier")
		.add(m_iAIGrowthPercent, L"iAIGrowthPercent")
		.add(m_iAITrainPercent, L"iAITrainPercent")
		.add(m_iAIWorldTrainPercent, L"iAIWorldTrainPercent")
		.add(m_iAIConstructPercent, L"iAIConstructPercent")
		.add(m_iAIWorldConstructPercent, L"iAIWorldConstructPercent")
		.add(m_iAICreatePercent, L"iAICreatePercent")
		.add(m_iAIResearchPercent, L"iAIResearchPercent")
		.add(m_iAIWorldCreatePercent, L"iAIWorldCreatePercent")
		.add(m_iAICivicUpkeepPercent, L"iAICivicUpkeepPercent")
		.add(m_iAIUnitUpkeepPercent, L"iAIUnitUpkeepPercent")
		.add(m_iAIUnitSupplyPercent, L"iAIUnitSupplyPercent")
		.add(m_iAIUnitUpgradePercent, L"iAIUnitUpgradePercent")
		.add(m_iAIInflationPercent, L"iAIInflationPercent")
		.add(m_iAIWarWearinessPercent, L"iAIWarWearinessPercent")
		.add(m_iAIPerEraModifier, L"iAIPerEraModifier")
		.add(m_iAIAdvancedStartPercent, L"iAIAdvancedStartPercent")
		.add(m_piGoodies, L"Goodies")
		.add(m_iRevolutionIndexPercent, L"iRevolutionIndexPercent")
		.add(m_PropertyManipulators)
	;
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvHandicapInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvHandicapInfo::~CvHandicapInfo()
{
}


int CvHandicapInfo::getFreeWinsVsBarbs() const
{
	return m_iFreeWinsVsBarbs;
}


int CvHandicapInfo::getAnimalAttackProb() const
{
	return m_iAnimalAttackProb;
}


int CvHandicapInfo::getAdvancedStartPointsMod() const
{
	return m_iAdvancedStartPointsMod;
}


int CvHandicapInfo::getStartingGold() const
{
	return m_iStartingGold;
}


int CvHandicapInfo::getUnitUpkeepPercent() const
{
	return m_iUnitUpkeepPercent;
}


int CvHandicapInfo::getDistanceMaintenancePercent() const
{
	return m_iDistanceMaintenancePercent;
}


int CvHandicapInfo::getNumCitiesMaintenancePercent() const
{
	return m_iNumCitiesMaintenancePercent;
}


int CvHandicapInfo::getColonyMaintenancePercent() const
{
	return m_iColonyMaintenancePercent;
}


int CvHandicapInfo::getMaxColonyMaintenance() const
{
	return m_iMaxColonyMaintenance;
}


int CvHandicapInfo::getCorporationMaintenancePercent() const
{
	return m_iCorporationMaintenancePercent;
}


int CvHandicapInfo::getCivicUpkeepPercent() const
{
	return m_iCivicUpkeepPercent;
}


int CvHandicapInfo::getInflationPercent() const
{
	return m_iInflationPercent;
}


int CvHandicapInfo::getHealthBonus() const
{
	return m_iHealthBonus;
}


int CvHandicapInfo::getHappyBonus() const
{
	return m_iHappyBonus;
}


int CvHandicapInfo::getAttitudeChange() const
{
	return m_iAttitudeChange;
}


int CvHandicapInfo::getNoTechTradeModifier() const
{
	return m_iNoTechTradeModifier;
}


int CvHandicapInfo::getTechTradeKnownModifier() const
{
	return m_iTechTradeKnownModifier;
}


int CvHandicapInfo::getUnownedWaterTilesPerBarbarianUnit() const
{
	return m_iUnownedWaterTilesPerBarbarianUnit;
}


int CvHandicapInfo::getUnownedTilesPerBarbarianCity() const
{
	return m_iUnownedTilesPerBarbarianCity;
}


int CvHandicapInfo::getBarbarianCityCreationTurnsElapsed() const
{
	return m_iBarbarianCityCreationTurnsElapsed;
}


int CvHandicapInfo::getBarbarianCityCreationProb() const
{
	return m_iBarbarianCityCreationProb;
}


int CvHandicapInfo::getAnimalCombatModifier() const
{
	return m_iAnimalCombatModifier;
}


int CvHandicapInfo::getBarbarianCombatModifier() const
{
	return m_iBarbarianCombatModifier;
}


int CvHandicapInfo::getAIAnimalCombatModifier() const
{
	return m_iAIAnimalCombatModifier;
}


int CvHandicapInfo::getAIBarbarianCombatModifier() const
{
	return m_iAIBarbarianCombatModifier;
}


int CvHandicapInfo::getStartingDefenseUnits() const
{
	return m_iStartingDefenseUnits;
}


int CvHandicapInfo::getStartingWorkerUnits() const
{
	return m_iStartingWorkerUnits;
}


int CvHandicapInfo::getStartingExploreUnits() const
{
	return m_iStartingExploreUnits;
}


int CvHandicapInfo::getAIStartingDefenseUnits() const
{
	return m_iAIStartingDefenseUnits;
}


int CvHandicapInfo::getAIStartingWorkerUnits() const
{
	return m_iAIStartingWorkerUnits;
}


int CvHandicapInfo::getAIStartingExploreUnits() const
{
	return m_iAIStartingExploreUnits;
}


int CvHandicapInfo::getBarbarianInitialDefenders() const
{
	return m_iBarbarianInitialDefenders;
}


int CvHandicapInfo::getAIDeclareWarProb() const
{
	return m_iAIDeclareWarProb;
}


int CvHandicapInfo::getAIWorkRateModifier() const
{
	return m_iAIWorkRateModifier;
}


int CvHandicapInfo::getAIGrowthPercent() const
{
	return m_iAIGrowthPercent;
}


int CvHandicapInfo::getAITrainPercent() const
{
	return m_iAITrainPercent;
}


int CvHandicapInfo::getAIWorldTrainPercent() const
{
	return m_iAIWorldTrainPercent;
}


int CvHandicapInfo::getAIConstructPercent() const
{
	return m_iAIConstructPercent;
}


int CvHandicapInfo::getAIWorldConstructPercent() const
{
	return m_iAIWorldConstructPercent;
}


int CvHandicapInfo::getAICreatePercent() const
{
	return m_iAICreatePercent;
}


int CvHandicapInfo::getAIResearchPercent() const
{
	return m_iAIResearchPercent;
}


int CvHandicapInfo::getAIWorldCreatePercent() const
{
	return m_iAIWorldCreatePercent;
}


int CvHandicapInfo::getAICivicUpkeepPercent() const
{
	return m_iAICivicUpkeepPercent;
}


int CvHandicapInfo::getAIUnitUpkeepPercent() const
{
	return m_iAIUnitUpkeepPercent;
}


int CvHandicapInfo::getAIUnitSupplyPercent() const
{
	return m_iAIUnitSupplyPercent;
}


int CvHandicapInfo::getAIUnitUpgradePercent() const
{
	return m_iAIUnitUpgradePercent;
}


int CvHandicapInfo::getAIInflationPercent() const
{
	return m_iAIInflationPercent;
}


int CvHandicapInfo::getAIWarWearinessPercent() const
{
	return m_iAIWarWearinessPercent;
}


int CvHandicapInfo::getAIPerEraModifier() const
{
	return m_iAIPerEraModifier;
}


int CvHandicapInfo::getAIAdvancedStartPercent() const
{
	return m_iAIAdvancedStartPercent;
}


int CvHandicapInfo::getNumGoodies() const
{
	return m_piGoodies.size();
}


int CvHandicapInfo::getGoodies(int i) const
{
	FASSERT_BOUNDS(0, getNumGoodies(), i);
	return m_piGoodies[i];
}


int CvHandicapInfo::getRevolutionIndexPercent() const
{
	return m_iRevolutionIndexPercent;
}



// Explicit, NOT delegated to CvInfoUtil (#196): the legacy checksum omits m_iSubdueAnimalBonusAI
// (which IS read from XML and declared in getDataMembers); delegating would fold it in and
// change the savegame asset checksum. Order reproduces the legacy composition exactly.
void CvHandicapInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_iFreeWinsVsBarbs);
	CheckSum(iSum, m_iAnimalAttackProb);
	CheckSum(iSum, m_iAdvancedStartPointsMod);
	CheckSum(iSum, m_iStartingGold);
	CheckSum(iSum, m_iUnitUpkeepPercent);
	CheckSum(iSum, m_iDistanceMaintenancePercent);
	CheckSum(iSum, m_iNumCitiesMaintenancePercent);
	CheckSum(iSum, m_iColonyMaintenancePercent);
	CheckSum(iSum, m_iMaxColonyMaintenance);
	CheckSum(iSum, m_iCorporationMaintenancePercent);
	CheckSum(iSum, m_iCivicUpkeepPercent);
	CheckSum(iSum, m_iInflationPercent);
	CheckSum(iSum, m_iHealthBonus);
	CheckSum(iSum, m_iHappyBonus);
	CheckSum(iSum, m_iAttitudeChange);
	CheckSum(iSum, m_iNoTechTradeModifier);
	CheckSum(iSum, m_iTechTradeKnownModifier);
	CheckSum(iSum, m_iUnownedWaterTilesPerBarbarianUnit);
	CheckSum(iSum, m_iUnownedTilesPerBarbarianCity);
	CheckSum(iSum, m_iBarbarianCityCreationTurnsElapsed);
	CheckSum(iSum, m_iBarbarianCityCreationProb);
	CheckSum(iSum, m_iAnimalCombatModifier);
	CheckSum(iSum, m_iBarbarianCombatModifier);
	CheckSum(iSum, m_iAIAnimalCombatModifier);
	CheckSum(iSum, m_iAIBarbarianCombatModifier);

	CheckSum(iSum, m_iStartingDefenseUnits);
	CheckSum(iSum, m_iStartingWorkerUnits);
	CheckSum(iSum, m_iStartingExploreUnits);
	CheckSum(iSum, m_iAIStartingDefenseUnits);
	CheckSum(iSum, m_iAIStartingWorkerUnits);
	CheckSum(iSum, m_iAIStartingExploreUnits);
	CheckSum(iSum, m_iBarbarianInitialDefenders);
	CheckSum(iSum, m_iAIDeclareWarProb);
	CheckSum(iSum, m_iAIWorkRateModifier);
	CheckSum(iSum, m_iAIGrowthPercent);
	CheckSum(iSum, m_iAITrainPercent);
	CheckSum(iSum, m_iAIWorldTrainPercent);
	CheckSum(iSum, m_iAIConstructPercent);
	CheckSum(iSum, m_iAIWorldConstructPercent);
	CheckSum(iSum, m_iAICreatePercent);
	CheckSum(iSum, m_iAIResearchPercent);
	CheckSum(iSum, m_iAIWorldCreatePercent);
	CheckSum(iSum, m_iAICivicUpkeepPercent);
	CheckSum(iSum, m_iAIUnitUpkeepPercent);
	CheckSum(iSum, m_iAIUnitSupplyPercent);
	CheckSum(iSum, m_iAIUnitUpgradePercent);
	CheckSum(iSum, m_iAIInflationPercent);
	CheckSum(iSum, m_iAIWarWearinessPercent);
	CheckSum(iSum, m_iAIPerEraModifier);
	CheckSum(iSum, m_iAIAdvancedStartPercent);

	m_PropertyManipulators.getCheckSum(iSum);

	CheckSumC(iSum, m_piGoodies);

	CheckSum(iSum, m_iRevolutionIndexPercent);
}


bool CvHandicapInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvHandicapInfo::copyNonDefaults(const CvHandicapInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

