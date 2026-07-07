//------------------------------------------------------------------------------------------------
//  FILE:    CvCivicInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvBuildingInfo.h"
#include "CvHeritageInfo.h"
#include "AI/CvGameAI.h"
#include "UI/CvGameTextMgr.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "Engine/CvMap.h"
#include "AI/CvPlayerAI.h"
#include "Infrastructure/CvPython.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvXMLLoadUtilityModTools.h"
#include "Tools/CheckSum.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvCivicInfo.h"



//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvCivicInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvCivicInfo::CvCivicInfo()
	// Fields declared in getDataMembers() are initialized by initDataMembers();
	// only the hand-written (non-declarative) members remain in this list.
	: m_pszCivicAttitudeReason(NULL)
	, m_bAnyImprovementYieldChange(false)
	, m_piBonusMintedPercent(NULL)
	, m_piImprovementHappinessChanges(NULL)
	, m_piImprovementHealthPercentChanges(NULL)
	, m_piFreeSpecialistCount(NULL)
	, m_piUnitProductionModifier(NULL)
	, m_piFlavorValue(NULL)
	, m_piCivicAttitudeChanges(NULL)
	, m_paiUnitCombatProductionModifier(NULL)
	, m_paiBuildingHappinessChanges(NULL)
	, m_paiBuildingHealthChanges(NULL)
	, m_paiFeatureHappinessChanges(NULL)
	, m_pabHurry(NULL)
	, m_pabSpecialBuildingNotRequired(NULL)
	, m_pabSpecialistValid(NULL)
	, m_ppiSpecialistYieldPercentChanges(NULL)
	, m_ppiSpecialistCommercePercentChanges(NULL)
	, m_ppiTerrainYieldChanges(NULL)
	, m_ppiBuildingCommerceChange(NULL)
	, m_ppiBuildingCommerceModifier(NULL)
	, m_ppiBonusCommerceModifier(NULL)
	, m_ppiImprovementYieldChanges(NULL)
	, m_bSparseListsCached(false)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvCivicInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvCivicInfo::~CvCivicInfo()
{
	// The yield/commerce arrays declared via addYields/addCommerce in getDataMembers()
	// are owned by the wrappers and freed here.
	CvInfoUtil(this).uninitDataMembers();

	SAFE_DELETE_ARRAY(m_paiBuildingHappinessChanges);
	SAFE_DELETE_ARRAY(m_paiBuildingHealthChanges);
	SAFE_DELETE_ARRAY(m_paiFeatureHappinessChanges);
	SAFE_DELETE_ARRAY(m_pabHurry);
	SAFE_DELETE_ARRAY(m_pabSpecialBuildingNotRequired);
	SAFE_DELETE_ARRAY(m_pabSpecialistValid);
	SAFE_DELETE_ARRAY2(m_ppiImprovementYieldChanges, GC.getNumImprovementInfos());
	SAFE_DELETE_ARRAY(m_piFlavorValue);
	SAFE_DELETE_ARRAY(m_piBonusMintedPercent);
	SAFE_DELETE_ARRAY(m_piCivicAttitudeChanges);
	SAFE_DELETE_ARRAY(m_pszCivicAttitudeReason);
	SAFE_DELETE_ARRAY(m_paiUnitCombatProductionModifier);
	SAFE_DELETE_ARRAY(m_piUnitProductionModifier);
	SAFE_DELETE_ARRAY(m_piFreeSpecialistCount);
	SAFE_DELETE_ARRAY(m_piImprovementHappinessChanges);
	SAFE_DELETE_ARRAY(m_piImprovementHealthPercentChanges);
	SAFE_DELETE_ARRAY2(m_ppiBuildingCommerceModifier, GC.getNumBuildingInfos());
	SAFE_DELETE_ARRAY2(m_ppiBuildingCommerceChange, GC.getNumBuildingInfos());
	SAFE_DELETE_ARRAY2(m_ppiBonusCommerceModifier, GC.getNumBonusInfos());
	SAFE_DELETE_ARRAY2(m_ppiTerrainYieldChanges, GC.getNumTerrainInfos());
	SAFE_DELETE_ARRAY2(m_ppiSpecialistYieldPercentChanges, GC.getNumSpecialistInfos());
	SAFE_DELETE_ARRAY2(m_ppiSpecialistCommercePercentChanges, GC.getNumSpecialistInfos());
}


const wchar_t* CvCivicInfo::getWeLoveTheKing() const
{
	return m_szWeLoveTheKingKey;
}


const wchar_t* CvCivicInfo::getWeLoveTheKingKey() const
{
	return m_szWeLoveTheKingKey;
}


int CvCivicInfo::getCivicOptionType() const
{
	return m_iCivicOptionType;
}


int CvCivicInfo::getAnarchyLength() const
{
	return m_iAnarchyLength;
}


int CvCivicInfo::getUpkeep() const
{
	return m_iUpkeep;
}


int CvCivicInfo::getAIWeight() const
{
	return m_iAIWeight;
}


int CvCivicInfo::getGreatPeopleRateModifier() const
{
	return m_iGreatPeopleRateModifier;
}


int CvCivicInfo::getGreatGeneralRateModifier() const
{
	return m_iGreatGeneralRateModifier;
}


int CvCivicInfo::getDomesticGreatGeneralRateModifier() const
{
	return m_iDomesticGreatGeneralRateModifier;
}


int CvCivicInfo::getStateReligionGreatPeopleRateModifier() const
{
	return m_iStateReligionGreatPeopleRateModifier;
}


int CvCivicInfo::getDistanceMaintenanceModifier() const
{
	return m_iDistanceMaintenanceModifier;
}


int CvCivicInfo::getNumCitiesMaintenanceModifier() const
{
	return m_iNumCitiesMaintenanceModifier;
}

//DPII < Maintenance Modifiers >
int CvCivicInfo::getHomeAreaMaintenanceModifier() const
{
	return m_iHomeAreaMaintenanceModifier;
}


int CvCivicInfo::getOtherAreaMaintenanceModifier() const
{
	return m_iOtherAreaMaintenanceModifier;
}

//DPII < Maintenance Modifiers >
int CvCivicInfo::getCorporationMaintenanceModifier() const
{
	return m_iCorporationMaintenanceModifier;
}


int CvCivicInfo::getExtraHealth() const
{
	return m_iExtraHealth;
}


int CvCivicInfo::getFreeExperience() const
{
	return m_iFreeExperience;
}


int CvCivicInfo::getWorkerSpeedModifier() const
{
	return m_iWorkerSpeedModifier;
}


int CvCivicInfo::getImprovementUpgradeRateModifier() const
{
	return m_iImprovementUpgradeRateModifier;
}


int CvCivicInfo::getMilitaryProductionModifier() const
{
	return m_iMilitaryProductionModifier;
}


int CvCivicInfo::getFreeUnitUpkeepCivilian() const
{
	return m_iFreeUnitUpkeepCivilian;
}


int CvCivicInfo::getFreeUnitUpkeepMilitary() const
{
	return m_iFreeUnitUpkeepMilitary;
}


int CvCivicInfo::getFreeUnitUpkeepCivilianPopPercent() const
{
	return m_iFreeUnitUpkeepCivilianPopPercent;
}


int CvCivicInfo::getFreeUnitUpkeepMilitaryPopPercent() const
{
	return m_iFreeUnitUpkeepMilitaryPopPercent;
}


int CvCivicInfo::getCivilianUnitUpkeepMod() const
{
	return m_iCivilianUnitUpkeepMod;
}


int CvCivicInfo::getMilitaryUnitUpkeepMod() const
{
	return m_iMilitaryUnitUpkeepMod;
}


int CvCivicInfo::getHappyPerMilitaryUnit() const
{
	return m_iHappyPerMilitaryUnit;
}


int CvCivicInfo::getLargestCityHappiness() const
{
	return m_iLargestCityHappiness;
}


int CvCivicInfo::getWarWearinessModifier() const
{
	return m_iWarWearinessModifier;
}


int CvCivicInfo::getFreeSpecialist() const
{
	return m_iFreeSpecialist;
}


int CvCivicInfo::getTradeRoutes() const
{
	return m_iTradeRoutes;
}


int CvCivicInfo::getCivicPercentAnger() const
{
	return m_iCivicPercentAnger;
}


int CvCivicInfo::getMaxConscript() const
{
	return m_iMaxConscript;
}


int CvCivicInfo::getStateReligionHappiness() const
{
	return m_iStateReligionHappiness;
}


int CvCivicInfo::getNonStateReligionHappiness() const
{
	return m_iNonStateReligionHappiness;
}


int CvCivicInfo::getStateReligionUnitProductionModifier() const
{
	return m_iStateReligionUnitProductionModifier;
}


int CvCivicInfo::getStateReligionBuildingProductionModifier() const
{
	return m_iStateReligionBuildingProductionModifier;
}


int CvCivicInfo::getStateReligionFreeExperience() const
{
	return m_iStateReligionFreeExperience;
}


int CvCivicInfo::getExpInBorderModifier() const
{
	return m_iExpInBorderModifier;
}


int CvCivicInfo::getRevIdxLocal() const
{
	return m_iRevIdxLocal;
}


int CvCivicInfo::getRevIdxNational() const
{
	return m_iRevIdxNational;
}


int CvCivicInfo::getRevIdxDistanceModifier() const
{
	return m_iRevIdxDistanceModifier;
}


int CvCivicInfo::getRevIdxHolyCityGood() const
{
	return m_iRevIdxHolyCityGood;
}


int CvCivicInfo::getRevIdxHolyCityBad() const
{
	return m_iRevIdxHolyCityBad;
}


int CvCivicInfo::getRevIdxSwitchTo() const
{
	return m_iRevIdxSwitchTo;
}


int CvCivicInfo::getRevReligiousFreedom() const
{
	return m_iRevReligiousFreedom;
}


int CvCivicInfo::getRevLaborFreedom() const
{
	return m_iRevLaborFreedom;
}


int CvCivicInfo::getRevEnvironmentalProtection() const
{
	return m_iRevEnvironmentalProtection;
}


int CvCivicInfo::getRevDemocracyLevel() const
{
	return m_iRevDemocracyLevel;
}


int CvCivicInfo::getAttitudeShareMod() const
{
	return m_iAttitudeShareMod;
}


int CvCivicInfo::getEnslavementChance() const
{
	return m_iEnslavementChance;
}


int CvCivicInfo::getForeignerUnhappyPercent() const
{
	return m_iForeignerUnhappyPercent;
}


int CvCivicInfo::getCityOverLimitUnhappy() const
{
	return m_iCityOverLimitUnhappy;
}


int CvCivicInfo::getLandmarkHappiness() const
{
	return m_iLandmarkHappiness;
}


int CvCivicInfo::getForeignTradeRouteModifier() const
{
	return m_iForeignTradeRouteModifier;
}


int CvCivicInfo::getTaxRateUnhappiness() const
{
	return m_iTaxRateUnhappiness;
}


int CvCivicInfo::getPopulationgrowthratepercentage() const
{
	return m_iPopulationgrowthratepercentage;
}


int CvCivicInfo::getReligionSpreadRate() const
{
	return m_iReligionSpreadRate;
}


int CvCivicInfo::getCivicHappiness() const
{
	return m_iCivicHappiness;
}


int CvCivicInfo::getDistantUnitSupportCostModifier() const
{
	return m_iDistantUnitSupportCostModifier;
}


int CvCivicInfo::getExtraCityDefense() const
{
	return m_iExtraCityDefense;
}


int CvCivicInfo::getNationalCaptureProbabilityModifier() const
{
	return m_iNationalCaptureProbabilityModifier;
}


int CvCivicInfo::getNationalCaptureResistanceModifier() const
{
	return m_iNationalCaptureResistanceModifier;
}


int CvCivicInfo::getInflationModifier() const
{
	return m_iInflationModifier;
}


int CvCivicInfo::getHurryInflationModifier() const
{
	return m_iHurryInflationModifier;
}


int CvCivicInfo::getHurryCostModifier() const
{
	return m_iHurryCostModifier;
}


int CvCivicInfo::getSharedCivicTradeRouteModifier() const
{
	return m_iSharedCivicTradeRouteModifier;
}


int CvCivicInfo::getCorporationSpreadRate() const
{
	return m_iCorporationSpreadRate;
}


int CvCivicInfo::getFreedomFighterChange() const
{
	return m_iFreedomFighterChange;
}



float CvCivicInfo::getRevIdxNationalityMod() const
{
	return m_fRevIdxNationalityMod;
}


float CvCivicInfo::getRevIdxBadReligionMod() const
{
	return m_fRevIdxBadReligionMod;
}


float CvCivicInfo::getRevIdxGoodReligionMod() const
{
	return m_fRevIdxGoodReligionMod;
}


float CvCivicInfo::getRevViolentMod() const
{
	return m_fRevViolentMod;
}



bool CvCivicInfo::isUpgradeAnywhere() const
{
	return m_bUpgradeAnywhere;
}


bool CvCivicInfo::isAllowInquisitions() const
{
	return m_bAllowInquisitions;
}


bool CvCivicInfo::isDisallowInquisitions() const
{
	return m_bDisallowInquisitions;
}


bool CvCivicInfo::isCommunism() const
{
	return m_bCommunism;
}


bool CvCivicInfo::isFreeSpeech() const
{
	return m_bFreeSpeech;
}


bool CvCivicInfo::isCanDoElection() const
{
	return m_bCanDoElection;
}


bool CvCivicInfo::isMilitaryFoodProduction() const
{
	return m_bMilitaryFoodProduction;
}


bool CvCivicInfo::isNoUnhealthyPopulation() const
{
	return m_bNoUnhealthyPopulation;
}


bool CvCivicInfo::isBuildingOnlyHealthy() const
{
	return m_bBuildingOnlyHealthy;
}


bool CvCivicInfo::isNoForeignTrade() const
{
	return m_bNoForeignTrade;
}


bool CvCivicInfo::isNoCorporations() const
{
	return m_bNoCorporations;
}


bool CvCivicInfo::isNoForeignCorporations() const
{
	return m_bNoForeignCorporations;
}


bool CvCivicInfo::isStateReligion() const
{
	return m_bStateReligion;
}


bool CvCivicInfo::isNoNonStateReligionSpread() const
{
	return m_bNoNonStateReligionSpread;
}


bool CvCivicInfo::IsFixedBorders() const
{
	return m_bFixedBorders;
}


bool CvCivicInfo::isNoCapitalUnhappiness() const
{
	return m_bNoCapitalUnhappiness;
}


bool CvCivicInfo::isNoLandmarkAnger() const
{
	return m_bNoLandmarkAnger;
}


bool CvCivicInfo::isAllReligionsActive() const
{
	return m_bAllReligionsActive;
}


bool CvCivicInfo::isBansNonStateReligions() const
{
	return m_bBansNonStateReligions;
}


bool CvCivicInfo::isFreedomFighter() const
{
	return m_bFreedomFighter;
}


bool CvCivicInfo::isPolicy() const
{
	return m_bPolicy;
}



// Arrays
int CvCivicInfo::getYieldModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldModifier ? m_piYieldModifier[i] : 0;
}


int* CvCivicInfo::getYieldModifierArray() const
{
	return m_piYieldModifier;
}


int CvCivicInfo::getCapitalYieldModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piCapitalYieldModifier ? m_piCapitalYieldModifier[i] : 0;
}


int* CvCivicInfo::getCapitalYieldModifierArray() const
{
	return m_piCapitalYieldModifier;
}


int CvCivicInfo::getTradeYieldModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piTradeYieldModifier ? m_piTradeYieldModifier[i] : 0;
}


int* CvCivicInfo::getTradeYieldModifierArray() const
{
	return m_piTradeYieldModifier;
}


int CvCivicInfo::getCommerceModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCommerceModifier ? m_piCommerceModifier[i] : 0;
}


int* CvCivicInfo::getCommerceModifierArray() const
{
	return m_piCommerceModifier;
}


int CvCivicInfo::getCapitalCommerceModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCapitalCommerceModifier ? m_piCapitalCommerceModifier[i] : 0;
}


int* CvCivicInfo::getCapitalCommerceModifierArray() const
{
	return m_piCapitalCommerceModifier;
}


int CvCivicInfo::getSpecialistExtraCommerce(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piSpecialistExtraCommerce ? m_piSpecialistExtraCommerce[i] : 0;
}


int* CvCivicInfo::getSpecialistExtraCommerceArray() const
{
	return m_piSpecialistExtraCommerce;
}


int CvCivicInfo::getCivicAttitudeChange(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCivicInfos(), i);
	return m_piCivicAttitudeChanges ? m_piCivicAttitudeChanges[i] : 0;
}


int* CvCivicInfo::getCivicAttitudeChanges() const
{
	return m_piCivicAttitudeChanges;
}


int CvCivicInfo::getLandmarkYieldChanges(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piLandmarkYieldChanges ? m_piLandmarkYieldChanges[i] : 0;
}


int* CvCivicInfo::getLandmarkYieldChangesArray() const
{
	return m_piLandmarkYieldChanges;
}



int CvCivicInfo::getBonusCommerceModifier(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, j);
	return (m_ppiBonusCommerceModifier && m_ppiBonusCommerceModifier[i]) ? m_ppiBonusCommerceModifier[i][j] : 0;
}


int* CvCivicInfo::getBonusCommerceModifierArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_ppiBonusCommerceModifier ? m_ppiBonusCommerceModifier[i] : NULL;
}



int CvCivicInfo::getBuildingHappinessChanges(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), i);
	return m_paiBuildingHappinessChanges ? m_paiBuildingHappinessChanges[i] : 0;
}


int CvCivicInfo::getBuildingHealthChanges(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), i);
	return m_paiBuildingHealthChanges ? m_paiBuildingHealthChanges[i] : 0;
}


// Walk the dense arrays once and record (building id, value) pairs for the
// non-zero entries. The dense arrays may be NULL when no modifier was loaded
// for this civic, in which case the corresponding sparse list stays empty.
// Called only from the public sparse accessors below; safe to call from a
// const context because the cache fields are mutable.
//
// Invariant: cache is valid until invalidateSparseLists() is called, which
// happens whenever copyNonDefaults() writes to one of the dense arrays.
void CvCivicInfo::cacheSparseListsIfNeeded() const
{
	PROFILE_EXTRA_FUNC();
	if (m_bSparseListsCached)
	{
		return;
	}
	m_vBuildingHappinessChangesSparse.clear();
	m_vBuildingHealthChangesSparse.clear();
	m_vFeatureHappinessChangesSparse.clear();

	const int iNumBuildings = GC.getNumBuildingInfos();
	for (int i = 0; i < iNumBuildings; ++i)
	{
		const int iHappy = m_paiBuildingHappinessChanges ? m_paiBuildingHappinessChanges[i] : 0;
		if (iHappy != 0)
		{
			m_vBuildingHappinessChangesSparse.push_back(BuildingModifier2((BuildingTypes)i, iHappy));
		}
		const int iHealth = m_paiBuildingHealthChanges ? m_paiBuildingHealthChanges[i] : 0;
		if (iHealth != 0)
		{
			m_vBuildingHealthChangesSparse.push_back(BuildingModifier2((BuildingTypes)i, iHealth));
		}
	}

	const int iNumFeatures = GC.getNumFeatureInfos();
	for (int i = 0; i < iNumFeatures; ++i)
	{
		const int iHappy = m_paiFeatureHappinessChanges ? m_paiFeatureHappinessChanges[i] : 0;
		if (iHappy != 0)
		{
			m_vFeatureHappinessChangesSparse.push_back(std::pair<FeatureTypes, int>((FeatureTypes)i, iHappy));
		}
	}

	m_bSparseListsCached = true;
}


const std::vector<BuildingModifier2>& CvCivicInfo::getBuildingHappinessChangesSparse() const
{
	cacheSparseListsIfNeeded();
	return m_vBuildingHappinessChangesSparse;
}


const std::vector<BuildingModifier2>& CvCivicInfo::getBuildingHealthChangesSparse() const
{
	cacheSparseListsIfNeeded();
	return m_vBuildingHealthChangesSparse;
}


const std::vector<std::pair<FeatureTypes, int> >& CvCivicInfo::getFeatureHappinessChangesSparse() const
{
	cacheSparseListsIfNeeded();
	return m_vFeatureHappinessChangesSparse;
}


void CvCivicInfo::invalidateSparseLists()
{
	m_bSparseListsCached = false;
}


int CvCivicInfo::getFeatureHappinessChanges(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return m_paiFeatureHappinessChanges ? m_paiFeatureHappinessChanges[i] : 0;
}


int CvCivicInfo::getBonusMintedPercent(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_piBonusMintedPercent ? m_piBonusMintedPercent[i] : 0;
}


int CvCivicInfo::getFreeSpecialistCount(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	return m_piFreeSpecialistCount ? m_piFreeSpecialistCount[i] : 0;
}


int CvCivicInfo::getFlavorValue(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFlavorTypes(), i);
	return m_piFlavorValue ? m_piFlavorValue[i] : 0;
}


int CvCivicInfo::getUnitCombatProductionModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return m_paiUnitCombatProductionModifier ? m_paiUnitCombatProductionModifier[i] : 0;
}


int CvCivicInfo::getUnitProductionModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), i);
	return m_piUnitProductionModifier ? m_piUnitProductionModifier[i] : 0;
}


int CvCivicInfo::getBuildingProductionModifier(BuildingTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), e);
	return m_aBuildingProductionModifier.getValue(e);
}


int CvCivicInfo::getImprovementHappinessChanges(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumImprovementInfos(), i);
	return m_piImprovementHappinessChanges ? m_piImprovementHappinessChanges[i] : 0;
}


int CvCivicInfo::getImprovementHealthPercentChanges(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumImprovementInfos(), i);
	return m_piImprovementHealthPercentChanges ? m_piImprovementHealthPercentChanges[i] : 0;
}



bool CvCivicInfo::isHurry(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumHurryInfos(), i);
	return m_pabHurry ? m_pabHurry[i] : false;
}


bool CvCivicInfo::isSpecialBuildingNotRequired(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialBuildingInfos(), i);
	return m_pabSpecialBuildingNotRequired ? m_pabSpecialBuildingNotRequired[i] : false;
}


bool CvCivicInfo::isSpecialistValid(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	return m_pabSpecialistValid ? m_pabSpecialistValid[i] : false;
}



int CvCivicInfo::getImprovementYieldChanges(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumImprovementInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return (m_ppiImprovementYieldChanges && m_ppiImprovementYieldChanges[i]) ? m_ppiImprovementYieldChanges[i][j] : 0;
}


int CvCivicInfo::getTerrainYieldChanges(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return (m_ppiTerrainYieldChanges && m_ppiTerrainYieldChanges[i]) ? m_ppiTerrainYieldChanges[i][j] : 0;
}


int CvCivicInfo::getBuildingCommerceModifier(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), i);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, j);
	return (m_ppiBuildingCommerceModifier && m_ppiBuildingCommerceModifier[i]) ? m_ppiBuildingCommerceModifier[i][j] : 0;
}


int CvCivicInfo::getBuildingCommerceChange(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), i);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, j);
	return (m_ppiBuildingCommerceChange && m_ppiBuildingCommerceChange[i]) ? m_ppiBuildingCommerceChange[i][j] : 0;
}


int CvCivicInfo::getSpecialistYieldPercentChanges(int i, int j ) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return (m_ppiSpecialistYieldPercentChanges && m_ppiSpecialistYieldPercentChanges[i]) ? m_ppiSpecialistYieldPercentChanges[i][j] : 0;
}


int CvCivicInfo::getSpecialistCommercePercentChanges(int i, int j ) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, j);
	return (m_ppiSpecialistCommercePercentChanges && m_ppiSpecialistCommercePercentChanges[i]) ? m_ppiSpecialistCommercePercentChanges[i][j] : 0;
}



int CvCivicInfo::getCityLimit(PlayerTypes ePlayer) const
{
	if (ePlayer > NO_PLAYER && GC.getGame().isOption(GAMEOPTION_EXP_OVEREXPANSION_PENALTIES))
	{
		// Larger maps allow more cities before overexpansion penalties kick in.
		return m_iCityLimit * GC.getWorldInfo(GC.getMap().getWorldSize()).getCityLimitsScalePercent() / 100;
	}
	return 0;
}



bool CvCivicInfo::isAnyBuildingHappinessChange() const
{
	return (m_paiBuildingHappinessChanges != NULL);
}


bool CvCivicInfo::isAnyBuildingHealthChange() const
{
	return (m_paiBuildingHealthChanges != NULL);
}


bool CvCivicInfo::isAnyFeatureHappinessChange() const
{
	return (m_paiFeatureHappinessChanges != NULL);
}


bool CvCivicInfo::isAnySpecialistValid() const
{
	return (m_pabSpecialistValid != NULL);
}


bool CvCivicInfo::isAnyImprovementYieldChange() const
{
	return m_bAnyImprovementYieldChange;
}


CvString CvCivicInfo::getCivicAttitudeReason(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCivicInfos(), i);
	return m_pszCivicAttitudeReason[i];
}


int CvCivicInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvCivicInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvCivicInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


int CvCivicInfo::getCivicAttitudeVectorSize() const						{ return m_aszCivicAttitudeforPass3.size(); }

CvString CvCivicInfo::getCivicAttitudeNamesVectorElement(int i) const	{ return m_aszCivicAttitudeforPass3[i]; }

int CvCivicInfo::getCivicAttitudeValuesVectorElement(int i) const		{ return m_aiCivicAttitudeforPass3[i]; }


int CvCivicInfo::getCivicAttitudeReasonVectorSize() const						{ return m_aszCivicAttitudeReasonforPass3.size(); }

CvString CvCivicInfo::getCivicAttitudeReasonNamesVectorElement(int i) const		{ return m_aszCivicAttitudeReasonforPass3[i]; }

CvString CvCivicInfo::getCivicAttitudeReasonValuesVectorElement(int i) const	{ return m_aszCivicAttitudeReasonValueforPass3[i]; }



void CvCivicInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order for readability. The checksum is NOT delegated
	// to these wrappers (see the explicit getCheckSum below), so this order carries no
	// checksum significance; readXml order is irrelevant to loaded values.
	util
		.addEnumAsInt(m_iCivicOptionType, L"CivicOptionType")
		.add(m_iAnarchyLength, L"iAnarchyLength")
		.addEnumAsInt(m_iUpkeep, L"Upkeep")
		.add(m_iAIWeight, L"iAIWeight")
		.add(m_iGreatPeopleRateModifier, L"iGreatPeopleRateModifier")
		.add(m_iGreatGeneralRateModifier, L"iGreatGeneralRateModifier")
		.add(m_iDomesticGreatGeneralRateModifier, L"iDomesticGreatGeneralRateModifier")
		.add(m_iStateReligionGreatPeopleRateModifier, L"iStateReligionGreatPeopleRateModifier")
		.add(m_iDistanceMaintenanceModifier, L"iDistanceMaintenanceModifier")
		.add(m_iNumCitiesMaintenanceModifier, L"iNumCitiesMaintenanceModifier")
		.add(m_iHomeAreaMaintenanceModifier, L"iHomeAreaMaintenanceModifier")
		.add(m_iOtherAreaMaintenanceModifier, L"iOtherAreaMaintenanceModifier")
		.add(m_iCorporationMaintenanceModifier, L"iCorporationMaintenanceModifier")
		.add(m_iExtraHealth, L"iExtraHealth")
		.add(m_iFreeExperience, L"iFreeExperience")
		.add(m_iWorkerSpeedModifier, L"iWorkerSpeedModifier")
		.add(m_iImprovementUpgradeRateModifier, L"iImprovementUpgradeRateModifier")
		.add(m_iMilitaryProductionModifier, L"iMilitaryProductionModifier")
		.add(m_iFreeUnitUpkeepCivilian, L"iFreeUnitUpkeepCivilian")
		.add(m_iFreeUnitUpkeepMilitary, L"iFreeUnitUpkeepMilitary")
		.add(m_iFreeUnitUpkeepCivilianPopPercent, L"iFreeUnitUpkeepCivilianPopPercent")
		.add(m_iFreeUnitUpkeepMilitaryPopPercent, L"iFreeUnitUpkeepMilitaryPopPercent")
		.add(m_iCivilianUnitUpkeepMod, L"iCivilianUnitUpkeepMod")
		.add(m_iMilitaryUnitUpkeepMod, L"iMilitaryUnitUpkeepMod")
		.add(m_iHappyPerMilitaryUnit, L"iHappyPerMilitaryUnit")
		.add(m_iLargestCityHappiness, L"iLargestCityHappiness")
		.add(m_iWarWearinessModifier, L"iWarWearinessModifier")
		.add(m_iFreeSpecialist, L"iFreeSpecialist")
		.add(m_iTradeRoutes, L"iTradeRoutes")
		.addEnum(m_iTechPrereq, L"TechPrereq")
		.add(m_iCivicPercentAnger, L"iCivicPercentAnger")
		.add(m_iMaxConscript, L"iMaxConscript")
		.add(m_iStateReligionHappiness, L"iStateReligionHappiness")
		.add(m_iNonStateReligionHappiness, L"iNonStateReligionHappiness")
		.add(m_iStateReligionUnitProductionModifier, L"iStateReligionUnitProductionModifier")
		.add(m_iStateReligionBuildingProductionModifier, L"iStateReligionBuildingProductionModifier")
		.add(m_iStateReligionFreeExperience, L"iStateReligionFreeExperience")
		.add(m_iExpInBorderModifier, L"iExpInBorderModifier")
		.add(m_iRevIdxLocal, L"iRevIdxLocal")
		.add(m_iRevIdxNational, L"iRevIdxNational")
		.add(m_iRevIdxDistanceModifier, L"iRevIdxDistanceModifier")
		.add(m_iRevIdxHolyCityGood, L"iRevIdxHolyCityGood")
		.add(m_iRevIdxHolyCityBad, L"iRevIdxHolyCityBad")
		.add(m_iRevIdxSwitchTo, L"iRevIdxSwitchTo")
		.add(m_iRevReligiousFreedom, L"iRevReligiousFreedom")
		.add(m_iRevLaborFreedom, L"iRevLaborFreedom")
		.add(m_iRevEnvironmentalProtection, L"iRevEnvironmentalProtection")
		.add(m_iRevDemocracyLevel, L"iRevDemocracyLevel")
		.add(m_iAttitudeShareMod, L"iAttitudeShareMod")
		.add(m_iEnslavementChance, L"iEnslavementChance")
		.add(m_iPopulationgrowthratepercentage, L"iPopulationgrowthratepercentage")
		.add(m_iReligionSpreadRate, L"iReligionSpreadRate")
		.add(m_iCivicHappiness, L"iCivicHappiness")
		.add(m_iDistantUnitSupportCostModifier, L"iDistantUnitSupportCostModifier")
		.add(m_iExtraCityDefense, L"iExtraCityDefense")
		.add(m_iForeignTradeRouteModifier, L"iForeignTradeRouteModifier")
		.add(m_iTaxRateUnhappiness, L"iTaxRateUnhappiness")
		.add(m_iInflationModifier, L"iInflation")
		.add(m_iHurryInflationModifier, L"iHurryInflationModifier")
		.add(m_iHurryCostModifier, L"iHurryCostModifier")
		.add(m_iSharedCivicTradeRouteModifier, L"iSharedCivicTradeRouteModifier")
		.add(m_iLandmarkHappiness, L"iLandmarkHappiness")
		// Legacy read() loaded <iCorporationSpreadRate> into m_iLandmarkHappiness (copy-paste bug)
		// and never filled m_iCorporationSpreadRate. Declaring it correctly fixes that; no current
		// XML uses the tag, so loaded data and the asset checksum are unchanged today.
		.add(m_iCorporationSpreadRate, L"iCorporationSpreadRate")
		.add(m_iForeignerUnhappyPercent, L"iForeignerUnhappyPercent")
		.add(m_iCityLimit, L"iCityLimit")
		.add(m_iCityOverLimitUnhappy, L"iCityOverLimitUnhappy") // not in the legacy checksum — one reason getCheckSum stays explicit
		.add(m_iNationalCaptureProbabilityModifier, L"iNationalCaptureProbabilityModifier")
		.add(m_iNationalCaptureResistanceModifier, L"iNationalCaptureResistanceModifier")
		.add(m_iFreedomFighterChange, L"iFreedomFighterChange")

		.add(m_fRevIdxNationalityMod, L"fRevIdxNationalityMod")
		.add(m_fRevIdxBadReligionMod, L"fRevIdxBadReligionMod")
		.add(m_fRevIdxGoodReligionMod, L"fRevIdxGoodReligionMod")
		.add(m_fRevViolentMod, L"fRevViolentMod")

		.add(m_bUpgradeAnywhere, L"bUpgradeAnywhere")
		.add(m_bAllowInquisitions, L"bAllowInquisitions")
		.add(m_bDisallowInquisitions, L"bDisallowInquisitions")
		.add(m_bCommunism, L"bCommunism")
		.add(m_bFreeSpeech, L"bFreeSpeech")
		.add(m_bCanDoElection, L"bCanDoElection")
		.add(m_bMilitaryFoodProduction, L"bMilitaryFoodProduction")
		.add(m_bNoUnhealthyPopulation, L"bNoUnhealthyPopulation")
		.add(m_bBuildingOnlyHealthy, L"bBuildingOnlyHealthy")
		.add(m_bNoForeignTrade, L"bNoForeignTrade")
		.add(m_bNoCorporations, L"bNoCorporations")
		.add(m_bNoForeignCorporations, L"bNoForeignCorporations")
		.add(m_bStateReligion, L"bStateReligion")
		.add(m_bNoNonStateReligionSpread, L"bNoNonStateReligionSpread")
		.add(m_bFixedBorders, L"bFixedBorders")
		.add(m_bNoCapitalUnhappiness, L"bNoCapitalUnhappiness")
		.add(m_bNoLandmarkAnger, L"bNoLandmarkAnger")
		.add(m_bAllReligionsActive, L"bAllReligionsActive")
		.add(m_bBansNonStateReligions, L"bBansNonStateReligions")
		.add(m_bFreedomFighter, L"bFreedomFighter")
		.add(m_bPolicy, L"bPolicy")

		.addYields(m_piYieldModifier, L"YieldModifiers")
		.addYields(m_piCapitalYieldModifier, L"CapitalYieldModifiers")
		.addYields(m_piTradeYieldModifier, L"TradeYieldModifiers")
		.addCommerce(m_piCommerceModifier, L"CommerceModifiers")
		.addCommerce(m_piCapitalCommerceModifier, L"CapitalCommerceModifiers")
		.addCommerce(m_piSpecialistExtraCommerce, L"SpecialistExtraCommerces")
		.addYields(m_piLandmarkYieldChanges, L"LandmarkYieldChanges")

		.add(m_aBuildingProductionModifier, L"BuildingProductionModifiers")
		.add(m_aiCategories, L"Categories")
		.add(m_PropertyManipulators)
	;
}


// Kept explicit (NOT delegated to CvInfoUtil) for byte-identical asset-checksum parity:
//  - hand-written fields (SetVariableListTagPair dense arrays, the int** 2D tables, the
//    readPass3 civic-attitude/unit-production arrays) interleave mid-order with the
//    declarative fields, and
//  - the legacy sum omits m_iCityOverLimitUnhappy even though it is read and copied.
// Delegating would reorder/extend the fold and change every existing save's checksum.
void CvCivicInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	CheckSum(iSum, m_iCivicOptionType);
	CheckSum(iSum, m_iAnarchyLength);
	CheckSum(iSum, m_iUpkeep);
	CheckSum(iSum, m_iAIWeight);
	CheckSum(iSum, m_iGreatPeopleRateModifier);
	CheckSum(iSum, m_iGreatGeneralRateModifier);
	CheckSum(iSum, m_iDomesticGreatGeneralRateModifier);
	CheckSum(iSum, m_iStateReligionGreatPeopleRateModifier);
	CheckSum(iSum, m_iDistanceMaintenanceModifier);
	CheckSum(iSum, m_iNumCitiesMaintenanceModifier);
	CheckSum(iSum, m_iHomeAreaMaintenanceModifier);
	CheckSum(iSum, m_iOtherAreaMaintenanceModifier);
	CheckSum(iSum, m_iCorporationMaintenanceModifier);
	CheckSum(iSum, m_iExtraHealth);
	CheckSum(iSum, m_iFreeExperience);
	CheckSum(iSum, m_iWorkerSpeedModifier);
	CheckSum(iSum, m_iImprovementUpgradeRateModifier);
	CheckSum(iSum, m_iMilitaryProductionModifier);
	CheckSum(iSum, m_iFreeUnitUpkeepCivilian);
	CheckSum(iSum, m_iFreeUnitUpkeepMilitary);
	CheckSum(iSum, m_iFreeUnitUpkeepCivilianPopPercent);
	CheckSum(iSum, m_iFreeUnitUpkeepMilitaryPopPercent);
	CheckSum(iSum, m_iCivilianUnitUpkeepMod);
	CheckSum(iSum, m_iMilitaryUnitUpkeepMod);
	CheckSum(iSum, m_iHappyPerMilitaryUnit);
	CheckSum(iSum, m_iLargestCityHappiness);
	CheckSum(iSum, m_iWarWearinessModifier);
	CheckSum(iSum, m_iFreeSpecialist);
	CheckSum(iSum, m_iTradeRoutes);
	CheckSum(iSum, m_iTechPrereq);
	CheckSum(iSum, m_iCivicPercentAnger);
	CheckSum(iSum, m_iMaxConscript);
	CheckSum(iSum, m_iStateReligionHappiness);
	CheckSum(iSum, m_iNonStateReligionHappiness);
	CheckSum(iSum, m_iStateReligionUnitProductionModifier);
	CheckSum(iSum, m_iStateReligionBuildingProductionModifier);
	CheckSum(iSum, m_iStateReligionFreeExperience);
	CheckSum(iSum, m_iExpInBorderModifier);
	CheckSum(iSum, m_iRevIdxLocal);
	CheckSum(iSum, m_iRevIdxNational);
	CheckSum(iSum, m_iRevIdxDistanceModifier);
	CheckSum(iSum, m_iRevIdxHolyCityGood);
	CheckSum(iSum, m_iRevIdxHolyCityBad);
	CheckSum(iSum, m_iRevIdxSwitchTo);
	CheckSum(iSum, m_iRevReligiousFreedom);
	CheckSum(iSum, m_iRevLaborFreedom);
	CheckSum(iSum, m_iRevEnvironmentalProtection);
	CheckSum(iSum, m_iRevDemocracyLevel);
	CheckSum(iSum, m_iAttitudeShareMod);
	CheckSum(iSum, m_iEnslavementChance);
	CheckSum(iSum, m_iPopulationgrowthratepercentage);
	CheckSum(iSum, m_iReligionSpreadRate);
	CheckSum(iSum, m_iCivicHappiness);
	CheckSum(iSum, m_iDistantUnitSupportCostModifier);
	CheckSum(iSum, m_iExtraCityDefense);
	CheckSum(iSum, m_iForeignTradeRouteModifier);
	CheckSum(iSum, m_iTaxRateUnhappiness);
	CheckSum(iSum, m_iInflationModifier);
	CheckSum(iSum, m_iHurryInflationModifier);
	CheckSum(iSum, m_iHurryCostModifier);
	CheckSum(iSum, m_iSharedCivicTradeRouteModifier);
	CheckSum(iSum, m_iLandmarkHappiness);
	CheckSum(iSum, m_iCorporationSpreadRate);
	CheckSum(iSum, m_iForeignerUnhappyPercent);
	CheckSum(iSum, m_iCityLimit);
	CheckSum(iSum, m_iNationalCaptureProbabilityModifier);
	CheckSum(iSum, m_iNationalCaptureResistanceModifier);
	CheckSum(iSum, m_iFreedomFighterChange);

	CheckSum(iSum, m_fRevIdxNationalityMod);
	CheckSum(iSum, m_fRevIdxBadReligionMod);
	CheckSum(iSum, m_fRevIdxGoodReligionMod);
	CheckSum(iSum, m_fRevViolentMod);

	CheckSum(iSum, m_bUpgradeAnywhere);
	CheckSum(iSum, m_bAllowInquisitions);
	CheckSum(iSum, m_bDisallowInquisitions);
	CheckSum(iSum, m_bCommunism);
	CheckSum(iSum, m_bFreeSpeech);
	CheckSum(iSum, m_bCanDoElection);
	CheckSum(iSum, m_bMilitaryFoodProduction);
	CheckSum(iSum, m_bNoUnhealthyPopulation);
	CheckSum(iSum, m_bBuildingOnlyHealthy);
	CheckSum(iSum, m_bNoForeignTrade);
	CheckSum(iSum, m_bNoCorporations);
	CheckSum(iSum, m_bNoForeignCorporations);
	CheckSum(iSum, m_bStateReligion);
	CheckSum(iSum, m_bNoNonStateReligionSpread);
	CheckSum(iSum, m_bFixedBorders);
	CheckSum(iSum, m_bNoCapitalUnhappiness);
	CheckSum(iSum, m_bNoLandmarkAnger);
	CheckSum(iSum, m_bAllReligionsActive);
	CheckSum(iSum, m_bBansNonStateReligions);
	CheckSum(iSum, m_bFreedomFighter);
	CheckSum(iSum, m_bPolicy);

	// Arrays

	CheckSumI(iSum, NUM_YIELD_TYPES, m_piYieldModifier);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piCapitalYieldModifier);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piTradeYieldModifier);
	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_piCommerceModifier);
	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_piCapitalCommerceModifier);
	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_piSpecialistExtraCommerce);
	CheckSumI(iSum, GC.getNumBuildingInfos(), m_paiBuildingHappinessChanges);
	CheckSumI(iSum, GC.getNumBuildingInfos(), m_paiBuildingHealthChanges);
	CheckSumI(iSum, GC.getNumFeatureInfos(), m_paiFeatureHappinessChanges);
	CheckSumI(iSum, GC.getNumHurryInfos(), m_pabHurry);
	CheckSumI(iSum, GC.getNumSpecialBuildingInfos(), m_pabSpecialBuildingNotRequired);
	CheckSumI(iSum, GC.getNumSpecialistInfos(), m_pabSpecialistValid);

	int i;
	if (m_ppiImprovementYieldChanges)
		for(i=0;i<GC.getNumImprovementInfos();i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppiImprovementYieldChanges[i]);
		}

	CheckSumI(iSum, GC.getNumBonusInfos(), m_piBonusMintedPercent);
	CheckSumI(iSum, GC.getNumUnitCombatInfos(), m_paiUnitCombatProductionModifier);
	CheckSumI(iSum, GC.getNumUnitInfos(), m_piUnitProductionModifier);
	CheckSumI(iSum, GC.getNumFlavorTypes(), m_piFlavorValue);
	CheckSumI(iSum, GC.getNumCivicInfos(), m_piCivicAttitudeChanges);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piLandmarkYieldChanges);
	CheckSumI(iSum, GC.getNumSpecialistInfos(), m_piFreeSpecialistCount);
	CheckSumI(iSum, GC.getNumImprovementInfos(), m_piImprovementHappinessChanges);
	CheckSumI(iSum, GC.getNumImprovementInfos(), m_piImprovementHealthPercentChanges);

	if (m_ppiSpecialistYieldPercentChanges)
	{
		for(i=0;i<GC.getNumSpecialistInfos();i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppiSpecialistYieldPercentChanges[i]);
		}
	}

	if (m_ppiSpecialistCommercePercentChanges)
	{
		for(i=0;i<GC.getNumSpecialistInfos();i++)
		{
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppiSpecialistCommercePercentChanges[i]);
		}
	}

	if (m_ppiTerrainYieldChanges)
	{
		for(i=0;i<GC.getNumTerrainInfos();i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppiTerrainYieldChanges[i]);
		}
	}

	if (m_ppiBuildingCommerceModifier)
	{
		for(i=0;i<GC.getNumBuildingInfos();i++)
		{
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppiBuildingCommerceModifier[i]);
		}
	}

	if (m_ppiBuildingCommerceChange)
	{
		for(i=0;i<GC.getNumBuildingInfos();i++)
		{
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppiBuildingCommerceChange[i]);
		}
	}

	if (m_ppiBonusCommerceModifier)
	{
		for(i=0;i<GC.getNumBonusInfos();i++)
		{
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppiBonusCommerceModifier[i]);
		}
	}

	CheckSumC(iSum, m_aBuildingProductionModifier);
	CheckSumC(iSum, m_aiCategories);

	m_PropertyManipulators.getCheckSum(iSum);
}


bool CvCivicInfo::read(CvXMLLoadUtility* pXML)
{

	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	// Scalars, FK enums, yield/commerce arrays, BuildingProductionModifiers, Categories and the
	// property manipulators are declared in getDataMembers() and read here. NOTE: this fixes a
	// legacy copy-paste bug — read() used to load <iCorporationSpreadRate> into m_iLandmarkHappiness
	// (stomping <iLandmarkHappiness>) and never filled m_iCorporationSpreadRate at all. No current
	// XML uses the tag, so loaded data is unchanged today.
	CvInfoUtil(this).readXml(pXML);

	int j;
	int iNumSibs=0;				// the number of siblings the current xml node has
	int iIndex;

	pXML->SetVariableListTagPair(&m_pabHurry, L"Hurrys", GC.getNumHurryInfos());
	pXML->SetVariableListTagPair(&m_pabSpecialBuildingNotRequired, L"SpecialBuildingNotRequireds",  GC.getNumSpecialBuildingInfos());
	pXML->SetVariableListTagPair(&m_pabSpecialistValid, L"SpecialistValids", GC.getNumSpecialistInfos());
	pXML->SetVariableListTagPair(&m_paiBuildingHappinessChanges, L"BuildingHappinessChanges", GC.getNumBuildingInfos());
	pXML->SetVariableListTagPair(&m_paiBuildingHealthChanges, L"BuildingHealthChanges", GC.getNumBuildingInfos());
	pXML->SetVariableListTagPair(&m_paiFeatureHappinessChanges, L"FeatureHappinessChanges", GC.getNumFeatureInfos());

	m_bAnyImprovementYieldChange = false;
	if (pXML->TryMoveToXmlFirstChild(L"ImprovementYieldChanges"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				pXML->Init2DList(&m_ppiImprovementYieldChanges, GC.getNumImprovementInfos(), NUM_YIELD_TYPES);
				for (j=0;j<iNumSibs;j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"ImprovementType");
					iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppiImprovementYieldChanges[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"ImprovementYields"))
						{
							// call the function that sets the yield change variable
							pXML->SetYields(&m_ppiImprovementYieldChanges[iIndex]);
							pXML->MoveToXmlParent();
						}
						else
						{
							SAFE_DELETE_ARRAY(m_ppiImprovementYieldChanges[iIndex]);
						}
					}

					if (!pXML->TryMoveToXmlNextSibling())
					{
						break;
					}
				}

				for(int ii=0;(!m_bAnyImprovementYieldChange) && ii<GC.getNumImprovementInfos();ii++)
				{
					for(int ij=0; ij < NUM_YIELD_TYPES; ij++ )
					{
						if( getImprovementYieldChanges(ii, ij) != 0 )
						{
							m_bAnyImprovementYieldChange = true;
							break;
						}
					}
				}
			}

			pXML->MoveToXmlParent();
		}

		pXML->MoveToXmlParent();
	}

	pXML->GetOptionalChildXmlValByName(m_szWeLoveTheKingKey, L"WeLoveTheKing");

	pXML->SetVariableListTagPair(&m_piBonusMintedPercent, L"BonusMintedPercents", GC.getNumBonusInfos());
	pXML->SetVariableListTagPair(&m_piImprovementHappinessChanges, L"ImprovementHappinessChanges", GC.getNumImprovementInfos());
	pXML->SetVariableListTagPair(&m_piImprovementHealthPercentChanges, L"ImprovementHealthPercentChanges", GC.getNumImprovementInfos());
	pXML->SetVariableListTagPair(&m_paiUnitCombatProductionModifier, L"UnitCombatProductionModifiers", GC.getNumUnitCombatInfos());

	if (pXML->TryMoveToXmlFirstChild(L"UnitProductionModifiers"))
	{
		const int iNumSibs = pXML->GetXmlChildrenNumber();
		int iTemp = false;
		if (iNumSibs > 0)
		{
			if (pXML->TryMoveToXmlFirstChild())
			{
				for (int i = 0; i < iNumSibs; i++)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						m_aszUnitProdModforPass3.push_back(szTextVal);
						pXML->GetNextXmlVal(&iTemp);
						m_aiUnitProdModforPass3.push_back(iTemp);
						pXML->MoveToXmlParent();
					}
					if (!pXML->TryMoveToXmlNextSibling())
					{
						break;
					}
				}
				pXML->MoveToXmlParent();
			}
		}
		pXML->MoveToXmlParent();
	}

	pXML->SetVariableListTagPair(&m_piFlavorValue, L"Flavors", GC.getNumFlavorTypes());
	pXML->SetVariableListTagPair(&m_piFreeSpecialistCount, L"FreeSpecialistCounts", GC.getNumSpecialistInfos());

	if (pXML->TryMoveToXmlFirstChild(L"SpecialistYieldPercentChanges"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				pXML->Init2DList(&m_ppiSpecialistYieldPercentChanges, GC.getNumSpecialistInfos(), NUM_YIELD_TYPES);
				for (j=0;j<iNumSibs;j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
					iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppiSpecialistYieldPercentChanges[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"SpecialistYieldPercents"))
						{
							// call the function that sets the yield change variable
							pXML->SetYields(&m_ppiSpecialistYieldPercentChanges[iIndex]);
							pXML->MoveToXmlParent();
						}
					}

					if (!pXML->TryMoveToXmlNextSibling())
					{
						break;
					}
				}
			}

			pXML->MoveToXmlParent();
		}

		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"SpecialistCommercePercentChanges"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				pXML->Init2DList(&m_ppiSpecialistCommercePercentChanges, GC.getNumSpecialistInfos(), NUM_COMMERCE_TYPES);
				for (j=0;j<iNumSibs;j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
					iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppiSpecialistCommercePercentChanges[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"SpecialistCommercePercents"))
						{
							// call the function that sets the commerce change variable
							pXML->SetCommerce(&m_ppiSpecialistCommercePercentChanges[iIndex]);
							pXML->MoveToXmlParent();
						}
					}

					if (!pXML->TryMoveToXmlNextSibling())
					{
						break;
					}
				}
			}

			pXML->MoveToXmlParent();
		}

		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"TerrainYieldChanges"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				pXML->Init2DList(&m_ppiTerrainYieldChanges, GC.getNumTerrainInfos(), NUM_YIELD_TYPES);
				for (j=0;j<iNumSibs;j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"TerrainType");
					iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppiTerrainYieldChanges[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"TerrainYields"))
						{
							// call the function that sets the yield change variable
							pXML->SetYields(&m_ppiTerrainYieldChanges[iIndex]);
							pXML->MoveToXmlParent();
						}
					}

					if (!pXML->TryMoveToXmlNextSibling())
					{
						break;
					}
				}
			}
			else
			{
				SAFE_DELETE_ARRAY(m_ppiTerrainYieldChanges);
			}

			pXML->MoveToXmlParent();
		}

		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"BuildingCommerceModifiers"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				pXML->Init2DList(&m_ppiBuildingCommerceModifier, GC.getNumBuildingInfos(), NUM_COMMERCE_TYPES);
				for (j=0;j<iNumSibs;j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"BuildingType");
					iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppiBuildingCommerceModifier[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"CommerceModifiers"))
						{
							// call the function that sets the yield change variable
							pXML->SetCommerce(&m_ppiBuildingCommerceModifier[iIndex]);
							pXML->MoveToXmlParent();
						}
					}

					if (!pXML->TryMoveToXmlNextSibling())
					{
						break;
					}
				}
			}

			pXML->MoveToXmlParent();
		}

		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"BuildingCommerceChanges"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				pXML->Init2DList(&m_ppiBuildingCommerceChange, GC.getNumBuildingInfos(), NUM_COMMERCE_TYPES);
				for (j=0;j<iNumSibs;j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"BuildingType");
					iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppiBuildingCommerceChange[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"CommerceChanges"))
						{
							// call the function that sets the yield change variable
							pXML->SetCommerce(&m_ppiBuildingCommerceChange[iIndex]);
							pXML->MoveToXmlParent();
						}
					}

					if (!pXML->TryMoveToXmlNextSibling())
					{
						break;
					}
				}
			}

			pXML->MoveToXmlParent();
		}

		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"BonusCommerceModifiers"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				pXML->Init2DList(&m_ppiBonusCommerceModifier, GC.getNumBonusInfos(), NUM_COMMERCE_TYPES);
				for (j=0;j<iNumSibs;j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"BonusType");
					iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppiBonusCommerceModifier[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"CommerceModifiers"))
						{
							// call the function that sets the yield change variable
							pXML->SetCommerce(&m_ppiBonusCommerceModifier[iIndex]);
							pXML->MoveToXmlParent();
						}
					}

					if (!pXML->TryMoveToXmlNextSibling())
					{
						break;
					}
				}
			}

			pXML->MoveToXmlParent();
		}

		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"CivicAttitudeChanges"))
	{
			int iTemp = 0;
			CvString szTemp;
			int iNumSibs = pXML->GetXmlChildrenNumber();
			if (pXML->TryMoveToXmlFirstChild())
			{
				if (iNumSibs > 0)
				{
					for (int i=0;i<iNumSibs;i++)
					{
						if (pXML->GetChildXmlVal(szTextVal))
						{
							m_aszCivicAttitudeforPass3.push_back(szTextVal);
							pXML->GetNextXmlVal(&iTemp);
							m_aiCivicAttitudeforPass3.push_back(iTemp);

							m_aszCivicAttitudeReasonforPass3.push_back(szTextVal);
							pXML->GetNextXmlVal(szTemp);
							m_aszCivicAttitudeReasonValueforPass3.push_back(szTemp);

							pXML->MoveToXmlParent();
						}

						if (!pXML->TryMoveToXmlNextSibling())
						{
							break;
						}
					}

					pXML->MoveToXmlParent();
				}
			}

		pXML->MoveToXmlParent();
	}

	return true;
}


bool CvCivicInfo::readPass3()
{
	PROFILE_EXTRA_FUNC();
	m_piCivicAttitudeChanges = new int[GC.getNumCivicInfos()];
	m_pszCivicAttitudeReason = new CvString[GC.getNumCivicInfos()];
	for (int iI = 0; iI < GC.getNumCivicInfos(); iI++)
	{
		m_piCivicAttitudeChanges[iI] = 0;
		m_pszCivicAttitudeReason[iI] = "";
	}
	if (!m_aiCivicAttitudeforPass3.empty() && !m_aszCivicAttitudeforPass3.empty())
	{
		const int iNumLoad = m_aiCivicAttitudeforPass3.size();
		for(int iI = 0; iI < iNumLoad; iI++)
		{
			const int iTempIndex = GC.getInfoTypeForString(m_aszCivicAttitudeforPass3[iI]);
			if (iTempIndex >= 0 && iTempIndex < GC.getNumCivicInfos())
				m_piCivicAttitudeChanges[iTempIndex] = m_aiCivicAttitudeforPass3[iI];
		}
		m_aszCivicAttitudeforPass3.clear();
		m_aiCivicAttitudeforPass3.clear();
	}
	if (!m_aszCivicAttitudeReasonValueforPass3.empty() && !m_aszCivicAttitudeReasonforPass3.empty())
	{
		const int iNumLoad = m_aszCivicAttitudeReasonValueforPass3.size();
		for(int iI = 0; iI < iNumLoad; iI++)
		{
			const int iTempIndex = GC.getInfoTypeForString(m_aszCivicAttitudeReasonforPass3[iI]);
			if (iTempIndex >= 0 && iTempIndex < GC.getNumCivicInfos())
				m_pszCivicAttitudeReason[iTempIndex] = m_aszCivicAttitudeReasonValueforPass3[iI];
		}
		m_aszCivicAttitudeReasonforPass3.clear();
		m_aszCivicAttitudeReasonValueforPass3.clear();
	}

	m_piUnitProductionModifier = new int[GC.getNumUnitInfos()];
    for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		m_piUnitProductionModifier[iI] = 0;
	}
	if (!m_aiUnitProdModforPass3.empty() && !m_aszUnitProdModforPass3.empty())
	{
		int iNumLoad = m_aiUnitProdModforPass3.size();
		for(int iI = 0; iI < iNumLoad; iI++)
		{
			int iTempIndex = GC.getInfoTypeForString(m_aszUnitProdModforPass3[iI]);
			if (iTempIndex >= 0 && iTempIndex < GC.getNumUnitInfos())
			{
				m_piUnitProductionModifier[iTempIndex] = m_aiUnitProdModforPass3[iI];
			}
		}
		m_aszUnitProdModforPass3.clear();
		m_aiUnitProdModforPass3.clear();
	}

	return true;
}


void CvCivicInfo::copyNonDefaults(const CvCivicInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	bool bDefault = false;
	int iDefault = 0;
	CvWString wDefault = CvWString::format(L"").GetCString();

	CvInfoBase::copyNonDefaults(pClassInfo);

	// Scalars, FK enums, yield/commerce arrays, BuildingProductionModifiers, Categories and the
	// property manipulators merge via the field wrappers. NOTE: this fixes the legacy m_iCityLimit
	// merge bug — the old code compared getCityLimit(NO_PLAYER), which always returns 0 (and calls
	// GC.getGame() during XML load), so a modular override unconditionally stomped m_iCityLimit
	// with 0. The wrapper compares the member against its default like every other field. No
	// modular civic XML exists today, so behaviour is unchanged for current assets.
	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	for ( int i = 0; i < GC.getNumHurryInfos(); i++ )
	{
		if ( isHurry(i) == bDefault && pClassInfo->isHurry(i) != bDefault)
		{
			if ( NULL == m_pabHurry )
			{
				CvXMLLoadUtility::InitList(&m_pabHurry,GC.getNumHurryInfos(),bDefault);
			}
			m_pabHurry[i] = pClassInfo->isHurry(i);
		}
	}
	for ( int i = 0; i < GC.getNumSpecialBuildingInfos(); i++ )
	{
		if ( isSpecialBuildingNotRequired(i) == bDefault && pClassInfo->isSpecialBuildingNotRequired(i) != bDefault)
		{
			if ( NULL == m_pabSpecialBuildingNotRequired)
			{
				CvXMLLoadUtility::InitList(&m_pabSpecialBuildingNotRequired,GC.getNumSpecialBuildingInfos(),bDefault);
			}
			m_pabSpecialBuildingNotRequired[i] = pClassInfo->isSpecialBuildingNotRequired(i);
		}
	}
	for ( int i = 0; i < GC.getNumSpecialistInfos(); i++ )
	{
		if ( isSpecialistValid(i) == bDefault && pClassInfo->isSpecialistValid(i) != bDefault)
		{
			if ( NULL == m_pabSpecialistValid )
			{
				CvXMLLoadUtility::InitList(&m_pabSpecialistValid,GC.getNumSpecialistInfos(),bDefault);
			}
			m_pabSpecialistValid[i] = pClassInfo->isSpecialistValid(i);
		}
	}

	// Note: any write into m_paiBuildingHappinessChanges /
	// m_paiBuildingHealthChanges below must be followed by
	// invalidateSparseLists() so the next sparse-view access rebuilds from the
	// updated dense storage. The two single calls below cover the only mutating
	// branches in this block.
	for ( int i = 0; i < GC.getNumBuildingInfos(); i++ )
	{
		if ( getBuildingHappinessChanges(i) == iDefault && pClassInfo->getBuildingHappinessChanges(i) != iDefault)
		{
			if ( NULL == m_paiBuildingHappinessChanges )
			{
				CvXMLLoadUtility::InitList(&m_paiBuildingHappinessChanges,GC.getNumBuildingInfos(),iDefault);
			}
			m_paiBuildingHappinessChanges[i] = pClassInfo->getBuildingHappinessChanges(i);
			invalidateSparseLists();
		}
		if ( getBuildingHealthChanges(i) == iDefault && pClassInfo->getBuildingHealthChanges(i) != iDefault)
		{
			if ( NULL == m_paiBuildingHealthChanges )
			{
				CvXMLLoadUtility::InitList(&m_paiBuildingHealthChanges,GC.getNumBuildingInfos(),iDefault);
			}
			m_paiBuildingHealthChanges[i] = pClassInfo->getBuildingHealthChanges(i);
			invalidateSparseLists();
		}
	}
	for ( int i = 0; i < GC.getNumFeatureInfos(); i++ )
	{
		if ( getFeatureHappinessChanges(i) == iDefault && pClassInfo->getFeatureHappinessChanges(i) != iDefault)
		{
			if ( NULL == m_paiFeatureHappinessChanges )
			{
				CvXMLLoadUtility::InitList(&m_paiFeatureHappinessChanges,GC.getNumFeatureInfos(),iDefault);
			}
			m_paiFeatureHappinessChanges[i] = pClassInfo->getFeatureHappinessChanges(i);
			invalidateSparseLists();
		}
	}

	for ( int i = 0; i < GC.getNumImprovementInfos(); i++ )
	{
		for ( int j = 0; j < NUM_YIELD_TYPES; j++ )
		{
			if ( getImprovementYieldChanges(i, j) == iDefault && pClassInfo->getImprovementYieldChanges(i,j) != iDefault)
			{
				if ( NULL == m_ppiImprovementYieldChanges )
				{
					CvXMLLoadUtility::Init2DList(&m_ppiImprovementYieldChanges, GC.getNumImprovementInfos(), NUM_YIELD_TYPES);
				}
				else if ( NULL == m_ppiImprovementYieldChanges[i] )
				{
					CvXMLLoadUtility::InitList(&m_ppiImprovementYieldChanges[i],NUM_YIELD_TYPES,iDefault);
				}
				m_ppiImprovementYieldChanges[i][j] = pClassInfo->getImprovementYieldChanges(i,j);
			}
		}
	}

	if (getWeLoveTheKingKey() == wDefault)
	{
		m_szWeLoveTheKingKey = pClassInfo->getWeLoveTheKingKey();
	}

	for ( int i = 0; i < GC.getNumImprovementInfos(); i++ )
	{
		if ( getImprovementHealthPercentChanges(i) == iDefault && pClassInfo->getImprovementHealthPercentChanges(i) != iDefault)
		{
			if ( NULL == m_piImprovementHealthPercentChanges )
			{
				CvXMLLoadUtility::InitList(&m_piImprovementHealthPercentChanges,GC.getNumImprovementInfos(),iDefault);
			}
			m_piImprovementHealthPercentChanges[i] = pClassInfo->getImprovementHealthPercentChanges(i);
		}

		if ( getImprovementHappinessChanges(i) == iDefault && pClassInfo->getImprovementHappinessChanges(i) != iDefault)
		{
			if ( NULL == m_piImprovementHappinessChanges )
			{
				CvXMLLoadUtility::InitList(&m_piImprovementHappinessChanges,GC.getNumImprovementInfos(),iDefault);
			}
			m_piImprovementHappinessChanges[i] = pClassInfo->getImprovementHappinessChanges(i);
		}
	}


	for ( int i = 0; i < GC.getNumSpecialistInfos(); i++ )
	{
		if ( getFreeSpecialistCount(i) == iDefault && pClassInfo->getFreeSpecialistCount(i) != iDefault)
		{
			if ( NULL == m_piFreeSpecialistCount)
			{
				CvXMLLoadUtility::InitList(&m_piFreeSpecialistCount,GC.getNumSpecialistInfos(),iDefault);
			}
			m_piFreeSpecialistCount[i] = pClassInfo->getFreeSpecialistCount(i);
		}

		for ( int j = 0; j < NUM_YIELD_TYPES; j++ )
		{
			if ( getSpecialistYieldPercentChanges(i,j) == iDefault && pClassInfo->getSpecialistYieldPercentChanges(i,j) != iDefault)
			{
				if ( NULL == m_ppiSpecialistYieldPercentChanges )
				{
					CvXMLLoadUtility::Init2DList(&m_ppiSpecialistYieldPercentChanges, GC.getNumSpecialistInfos(), NUM_YIELD_TYPES);
				}
				else if ( NULL == m_ppiSpecialistYieldPercentChanges[i] )
				{
					CvXMLLoadUtility::InitList(&m_ppiSpecialistYieldPercentChanges[i],NUM_YIELD_TYPES,iDefault);
				}
				m_ppiSpecialistYieldPercentChanges[i][j] = pClassInfo->getSpecialistYieldPercentChanges(i,j);
			}
		}

		for ( int j = 0; j < NUM_COMMERCE_TYPES; j++ )
		{
			if ( getSpecialistCommercePercentChanges(i,j) == iDefault && pClassInfo->getSpecialistCommercePercentChanges(i,j) != iDefault)
			{
				if ( NULL == m_ppiSpecialistCommercePercentChanges )
				{
					CvXMLLoadUtility::Init2DList(&m_ppiSpecialistCommercePercentChanges, GC.getNumSpecialistInfos(), NUM_COMMERCE_TYPES);
				}
				else if ( NULL == m_ppiSpecialistCommercePercentChanges[i] )
				{
					CvXMLLoadUtility::InitList(&m_ppiSpecialistCommercePercentChanges[i],NUM_COMMERCE_TYPES,iDefault);
				}
				m_ppiSpecialistCommercePercentChanges[i][j] = pClassInfo->getSpecialistCommercePercentChanges(i,j);
			}
		}
	}

	for ( int i = 0; i < GC.getNumUnitCombatInfos(); i++ )
	{
		if ( getUnitCombatProductionModifier(i) == iDefault && pClassInfo->getUnitCombatProductionModifier(i) != iDefault)
		{
			if ( NULL == m_paiUnitCombatProductionModifier )
			{
				CvXMLLoadUtility::InitList(&m_paiUnitCombatProductionModifier,GC.getNumUnitCombatInfos(),iDefault);
			}
			m_paiUnitCombatProductionModifier[i] = pClassInfo->getUnitCombatProductionModifier(i);
		}
	}

	for ( int i = 0; i < GC.getNumBuildingInfos(); i++ )
	{
		for ( int j = 0; j < NUM_COMMERCE_TYPES; j++ )
		{
			if ( getBuildingCommerceChange(i,j) == iDefault && pClassInfo->getBuildingCommerceChange(i,j) != iDefault)
			{
				if ( NULL == m_ppiBuildingCommerceChange )
				{
					CvXMLLoadUtility::Init2DList(&m_ppiBuildingCommerceChange, GC.getNumBuildingInfos(), NUM_COMMERCE_TYPES);
				}
				else if ( NULL == m_ppiBuildingCommerceChange[i] )
				{
					CvXMLLoadUtility::InitList(&m_ppiBuildingCommerceChange[i],NUM_COMMERCE_TYPES,iDefault);
				}
				m_ppiBuildingCommerceChange[i][j] = pClassInfo->getBuildingCommerceChange(i,j);
			}
		}
	}

	for ( int i = 0; i < GC.getNumTerrainInfos(); i++ )
	{
		for ( int j = 0; j < NUM_YIELD_TYPES; j++ )
		{
			if ( getTerrainYieldChanges(i,j) == iDefault && pClassInfo->getTerrainYieldChanges(i,j) != iDefault)
			{
				if ( NULL == m_ppiTerrainYieldChanges)
				{
					CvXMLLoadUtility::Init2DList(&m_ppiTerrainYieldChanges, GC.getNumTerrainInfos(), NUM_YIELD_TYPES);
				}
				else if ( NULL == m_ppiTerrainYieldChanges[i] )
				{
					CvXMLLoadUtility::InitList(&m_ppiTerrainYieldChanges[i],NUM_YIELD_TYPES,iDefault);
				}
				m_ppiTerrainYieldChanges[i][j] = pClassInfo->getTerrainYieldChanges(i,j);
			}
		}
	}
	for ( int i = 0; i < GC.getNumBuildingInfos(); i++ )
	{
		for ( int j = 0; j < NUM_COMMERCE_TYPES; j++ )
		{
			if ( getBuildingCommerceModifier(i,j) == iDefault && pClassInfo->getBuildingCommerceModifier(i,j) != iDefault)
			{
				if ( NULL == m_ppiBuildingCommerceModifier)
				{
					CvXMLLoadUtility::Init2DList(&m_ppiBuildingCommerceModifier, GC.getNumBuildingInfos(), NUM_COMMERCE_TYPES);
				}
				else if ( NULL == m_ppiBuildingCommerceModifier[i] )
				{
					CvXMLLoadUtility::InitList(&m_ppiBuildingCommerceModifier[i],NUM_COMMERCE_TYPES,iDefault);
				}
				m_ppiBuildingCommerceModifier[i][j] = pClassInfo->getBuildingCommerceModifier(i,j);
			}
		}
	}
	for ( int i = 0; i < GC.getNumBonusInfos(); i++ )
	{
		if (getBonusMintedPercent(i) == iDefault && pClassInfo->getBonusMintedPercent(i) != iDefault)
		{
			if ( NULL == m_piBonusMintedPercent )
			{
				CvXMLLoadUtility::InitList(&m_piBonusMintedPercent,GC.getNumBonusInfos(),iDefault);
			}
			m_piBonusMintedPercent[i] = pClassInfo->getBonusMintedPercent(i);
		}
		for ( int j = 0; j < NUM_COMMERCE_TYPES; j++ )
		{
			if ( getBonusCommerceModifier(i,j) == iDefault && pClassInfo->getBonusCommerceModifier(i,j) != iDefault)
			{
				if ( NULL == m_ppiBonusCommerceModifier )
				{
					CvXMLLoadUtility::Init2DList(&m_ppiBonusCommerceModifier, GC.getNumBonusInfos(), NUM_COMMERCE_TYPES);
				}
				else if ( NULL == m_ppiBonusCommerceModifier[i] )
				{
					CvXMLLoadUtility::InitList(&m_ppiBonusCommerceModifier[i],NUM_COMMERCE_TYPES,iDefault);
				}
				m_ppiBonusCommerceModifier[i][j] = pClassInfo->getBonusCommerceModifier(i,j);
			}
		}
	}
	for ( int i = 0; i < pClassInfo->getCivicAttitudeVectorSize(); i++ )
	{
		m_aiCivicAttitudeforPass3.push_back(pClassInfo->getCivicAttitudeValuesVectorElement(i));
		m_aszCivicAttitudeforPass3.push_back(pClassInfo->getCivicAttitudeNamesVectorElement(i));
	}
}

