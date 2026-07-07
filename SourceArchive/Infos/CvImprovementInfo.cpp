#include "CvGameCoreDLL.h"
#include "Tools/FProfiler.h"

#include "UI/CvArtFileMgr.h"
#include "Defines/CvDefines.h"
#include "CvImprovementInfo.h"
#include "CvInfoUtil.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Tools/CheckSum.h"
#include "Infrastructure/BoolExpr.h"
//#include "Infrastructure/IntExpr.h"
#include "Infrastructure/IDValueMap.h"

CvImprovementInfo::CvImprovementInfo() :
	// Only non-declared members are initialized here; everything in getDataMembers is
	// initialized by initDataMembers() below.
	m_iWorldSoundscapeScriptId(0),
	m_ppiTechYieldChanges(NULL),
	m_ppiRouteYieldChanges(NULL),
	//m_ppiTraitYieldChanges(NULL),
	m_paImprovementBonus(NULL)
	//,m_iHighestCost(0)
{
	CvInfoUtil(this).initDataMembers();
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvImprovementInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvImprovementInfo::~CvImprovementInfo()
{
	// Frees the declared yield arrays (addYields => wrapper-owned) and unregisters the declared
	// delayed-resolution enums (ImprovementPillage/ImprovementUpgrade/BonusChange).
	CvInfoUtil(this).uninitDataMembers();

	SAFE_DELETE_ARRAY(m_paImprovementBonus); // XXX make sure this isn't leaking memory...
	SAFE_DELETE_ARRAY2(m_ppiTechYieldChanges, GC.getNumTechInfos());
	SAFE_DELETE_ARRAY2(m_ppiRouteYieldChanges, GC.getNumRouteInfos());
	//	SAFE_DELETE_ARRAY2(m_ppiTraitYieldChanges, GC.getNumTraitInfos());
	GC.removeDelayedResolutionVector(m_aiAlternativeImprovementUpgradeTypes);
}

int CvImprovementInfo::getAdvancedStartCost() const
{
	return m_iAdvancedStartCost;
}

int CvImprovementInfo::getTilesPerGoody() const
{
	return m_iTilesPerGoody;
}

int CvImprovementInfo::getGoodyUniqueRange() const
{
	return m_iGoodyUniqueRange;
}

int CvImprovementInfo::getFeatureGrowthProbability() const
{
	return m_iFeatureGrowthProbability;
}

int CvImprovementInfo::getUpgradeTime() const
{
	return m_iUpgradeTime;
}

int CvImprovementInfo::getAirBombDefense() const
{
	return m_iAirBombDefense;
}

int CvImprovementInfo::getDefenseModifier() const
{
	return m_iDefenseModifier;
}

int CvImprovementInfo::getHappiness() const
{
	return m_iHappiness;
}

int CvImprovementInfo::getPillageGold() const
{
	return m_iPillageGold;
}

bool CvImprovementInfo::isOutsideBorders() const
{
	return m_bOutsideBorders;
}

// Super Forts begin *XML*
int CvImprovementInfo::getCulture() const
{
	return m_iCulture;
}

int CvImprovementInfo::getCultureRange() const
{
	return m_iCultureRange;
}

int CvImprovementInfo::getVisibilityChange() const
{
	return m_iVisibilityChange;
}

int CvImprovementInfo::getSeeFrom() const
{
	return m_iSeeFrom;
}

int CvImprovementInfo::getUniqueRange() const
{
	return m_iUniqueRange;
}

bool CvImprovementInfo::isBombardable() const
{
	return m_bBombardable;
}

bool CvImprovementInfo::isUpgradeRequiresFortify() const
{
	return m_bUpgradeRequiresFortify;
}

bool CvImprovementInfo::isZOCSource() const
{
	return m_bIsZOCSource;
}

bool CvImprovementInfo::isActsAsCity() const
{
	return m_bActsAsCity;
}

bool CvImprovementInfo::isHillsMakesValid() const
{
	return m_bHillsMakesValid;
}

bool CvImprovementInfo::isFreshWaterMakesValid() const
{
	return m_bFreshWaterMakesValid;
}

bool CvImprovementInfo::isRiverSideMakesValid() const
{
	return m_bRiverSideMakesValid;
}

bool CvImprovementInfo::isNoFreshWater() const
{
	return m_bNoFreshWater;
}

bool CvImprovementInfo::isRequiresFlatlands() const
{
	return m_bRequiresFlatlands;
}

bool CvImprovementInfo::isRequiresRiverSide() const
{
	return m_bRequiresRiverSide;
}

bool CvImprovementInfo::isRequiresIrrigation() const
{
	return m_bRequiresIrrigation;
}

bool CvImprovementInfo::isCarriesIrrigation() const
{
	return m_bCarriesIrrigation;
}

bool CvImprovementInfo::isRequiresFeature() const
{
	return m_bRequiresFeature;
}

bool CvImprovementInfo::isPeakImprovement() const
{
	return m_bPeakImprovement;
}

bool CvImprovementInfo::isWaterImprovement() const
{
	return m_bWaterImprovement;
}

bool CvImprovementInfo::isGoody() const
{
	return m_bGoody;
}

const char* CvImprovementInfo::getArtDefineTag() const
{
	return m_szArtDefineTag;
}

int CvImprovementInfo::getWorldSoundscapeScriptId() const
{
	return m_iWorldSoundscapeScriptId;
}

// Arrays

int CvImprovementInfo::getPrereqNatureYield(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piPrereqNatureYield ? m_piPrereqNatureYield[i] : 0;
}

int CvImprovementInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldChange ? m_piYieldChange[i] : 0;
}

int* CvImprovementInfo::getYieldChangeArray() const
{
	return m_piYieldChange;
}

int CvImprovementInfo::getRiverSideYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piRiverSideYieldChange ? m_piRiverSideYieldChange[i] : 0;
}

int* CvImprovementInfo::getRiverSideYieldChangeArray() const
{
	return m_piRiverSideYieldChange;
}

int CvImprovementInfo::getIrrigatedYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piIrrigatedChange ? m_piIrrigatedChange[i] : 0;
}

int* CvImprovementInfo::getIrrigatedYieldChangeArray() const
{
	return m_piIrrigatedChange;
}

bool CvImprovementInfo::getTerrainMakesValid(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return algo::any_of_equal(m_aeTerrainMakesValid, static_cast<TerrainTypes>(i));
}

bool CvImprovementInfo::getFeatureMakesValid(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return algo::any_of_equal(m_aeFeatureMakesValid, static_cast<FeatureTypes>(i));
}

int CvImprovementInfo::getTechYieldChanges(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumTechInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return (m_ppiTechYieldChanges && m_ppiTechYieldChanges[i]) ? m_ppiTechYieldChanges[i][j] : 0;
}

int* CvImprovementInfo::getTechYieldChangesArray(int i) const
{
	return m_ppiTechYieldChanges ? m_ppiTechYieldChanges[i] : NULL;
}

int CvImprovementInfo::getRouteYieldChanges(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumRouteInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return (m_ppiRouteYieldChanges && m_ppiRouteYieldChanges[i]) ? m_ppiRouteYieldChanges[i][j] : 0;
}

int* CvImprovementInfo::getRouteYieldChangesArray(int i) const
{
	return m_ppiRouteYieldChanges ? m_ppiRouteYieldChanges[i] : NULL;
}

int CvImprovementInfo::getImprovementBonusYield(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return m_paImprovementBonus[i].m_piYieldChange ? m_paImprovementBonus[i].getYieldChange(j) : 0;
}

bool CvImprovementInfo::isImprovementBonusMakesValid(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_paImprovementBonus[i].m_bBonusMakesValid;
}

bool CvImprovementInfo::isImprovementObsoleteBonusMakesValid(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_paImprovementBonus[i].m_bObsoleteBonusMakesValid;
}

bool CvImprovementInfo::isImprovementBonusTrade(int iBonus) const
{
	if (iBonus < 0)
	{
		return m_bIsUniversalTradeBonusProvider;
	}
	return m_bIsUniversalTradeBonusProvider || m_paImprovementBonus[iBonus].m_bBonusTrade;
}

int CvImprovementInfo::getImprovementBonusDiscoverRand(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_paImprovementBonus[i].m_iDiscoverRand;
}

const char* CvImprovementInfo::getButton() const
{
	const CvArtInfoImprovement* pImprovementArtInfo = getArtInfo();
	return pImprovementArtInfo ? pImprovementArtInfo->getButton() : NULL;
}

const CvArtInfoImprovement* CvImprovementInfo::getArtInfo() const
{
	return ARTFILEMGR.getImprovementArtInfo(getArtDefineTag());
}

const char* CvArtInfoImprovement::getShaderNIF() const
{
	return m_szShaderNIF;
}

void CvArtInfoImprovement::setShaderNIF(const char* szDesc)
{
	m_szShaderNIF = szDesc;
}


bool CvImprovementInfo::isPeakMakesValid() const
{
	return m_bPeakMakesValid;
}

int CvImprovementInfo::getHealthPercent() const
{
	return m_iHealthPercent;
}

int CvImprovementInfo::getImprovementBonusDepletionRand(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_paImprovementBonus[i].m_iDepletionRand;
}

//int CvImprovementInfo::getTraitYieldChanges(int i, int j) const
//{
//	FASSERT_BOUNDS(0, GC.getNumTraitInfos(), i);
//	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
//	return (m_ppiTraitYieldChanges && m_ppiTraitYieldChanges[i]) ? m_ppiTraitYieldChanges[i][j] : 0;
//}

//int* CvImprovementInfo::getTraitYieldChangesArray(int i) const
//{
//	return m_ppiTraitYieldChanges ? m_ppiTraitYieldChanges[i] : NULL;
//}

bool CvImprovementInfo::isCanMoveSeaUnits() const
{
	return m_bCanMoveSeaUnits;
}

bool CvImprovementInfo::isChangeRemove() const
{
	return m_bChangeRemove;
}

bool CvImprovementInfo::isNotOnAnyBonus() const
{
	return m_bNotOnAnyBonus;
}

bool CvImprovementInfo::isNational() const
{
	return m_bNational;
}

bool CvImprovementInfo::isGlobal() const
{
	return m_bGlobal;
}

int CvImprovementInfo::getAlternativeImprovementUpgradeType(int i) const
{
	return m_aiAlternativeImprovementUpgradeTypes[i];
}

int CvImprovementInfo::getNumAlternativeImprovementUpgradeTypes() const
{
	return (int)m_aiAlternativeImprovementUpgradeTypes.size();
}

bool CvImprovementInfo::isAlternativeImprovementUpgradeType(int i) const
{
	return algo::any_of_equal(m_aiAlternativeImprovementUpgradeTypes, i);
}

int CvImprovementInfo::getFeatureChangeType(int i) const
{
	return m_aiFeatureChangeTypes[i];
}

int CvImprovementInfo::getNumFeatureChangeTypes() const
{
	return (int)m_aiFeatureChangeTypes.size();
}

bool CvImprovementInfo::isFeatureChangeType(int i) const
{
	return algo::any_of_equal(m_aiFeatureChangeTypes, i);
}

int CvImprovementInfo::getCategory(int i) const
{
	return (m_aiCategories[i]);
}

int CvImprovementInfo::getNumCategories() const
{
	return (int)(m_aiCategories.size());
}

bool CvImprovementInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}

//Post Load functions
//void CvImprovementInfo::setHighestCost()
//{
//	BuildTypes eHighestCostBuild = NO_BUILD;
//	int iHighestCost = 0;
//	for (int iI = 0; iI < GC.getNumBuildInfos(); iI++)
//	{
//		if (GC.getBuildInfo((BuildTypes)iI).getImprovement() == GC.getInfoTypeForString(m_szType))
//		{
//			if (GC.getBuildInfo((BuildTypes)iI).getCost() > iHighestCost)
//			{
//				eHighestCostBuild = (BuildTypes)iI;
//				iHighestCost = GC.getBuildInfo((BuildTypes)iI).getCost();
//			}
//		}
//	}
//	m_iHighestCost = iHighestCost;
//}

//int CvImprovementInfo::getHighestCost() const
//{
//	return m_iHighestCost;
//}

void CvImprovementInfo::getDataMembers(CvInfoUtil& util)
{
	// Kept hand-written:
	// - m_ppiTechYieldChanges / m_ppiRouteYieldChanges: 2D int** arrays (the known m_ppi blocker).
	// - m_paImprovementBonus: bespoke per-bonus CvImprovementBonusInfo array (SetImprovementBonuses).
	// - m_aiAlternativeImprovementUpgradeTypes: delayed-resolution int vector (no wrapper yet).
	// - m_iWorldSoundscapeScriptId: resolved through gDLL->getAudioTagIndex at read, and its
	//   absent-tag default is -1 while the ctor default is 0 (no wrapper default reproduces that).
	// - the <iHealth> component of m_iHealthPercent: composite tag, folded in read().
	// m_improvementBuildTypes is a runtime cache (doPostLoadCaching), not XML.
	// getCheckSum stays explicit: the hand-written fields sit mid-order in the legacy checksum
	// stream, so delegating would reorder it. Declaration order below mirrors that stream.
	util
		.add(m_aeTerrainMakesValid, L"TerrainMakesValids")
		.add(m_aeFeatureMakesValid, L"FeatureMakesValids")
		.add(m_iAdvancedStartCost, L"iAdvancedStartCost", 100)
		.add(m_iTilesPerGoody, L"iTilesPerGoody")
		.add(m_iGoodyUniqueRange, L"iGoodyRange")
		.add(m_iFeatureGrowthProbability, L"iFeatureGrowth")
		.add(m_iUpgradeTime, L"iUpgradeTime")
		.add(m_iAirBombDefense, L"iAirBombDefense")
		.add(m_iDefenseModifier, L"iDefenseModifier")
		.add(m_iHappiness, L"iHappiness")
		.add(m_iPillageGold, L"iPillageGold")
		.addEnum(m_iImprovementPillage, L"ImprovementPillage") // self-FK => delayed resolution
		.addEnum(m_iImprovementUpgrade, L"ImprovementUpgrade") // self-FK => delayed resolution
		// Super Forts begin *XML*
		.add(m_iCulture, L"iCulture")
		.add(m_iCultureRange, L"iCultureRange")
		.add(m_iVisibilityChange, L"iVisibilityChange")
		.add(m_iSeeFrom, L"iSeeFrom")
		.add(m_iUniqueRange, L"iUniqueRange")
		.add(m_bBombardable, L"bBombardable")
		.add(m_bUpgradeRequiresFortify, L"bUpgradeRequiresFortify")
		// Super Forts end
		.add(m_bIsUniversalTradeBonusProvider, L"bIsUniversalTradeBonusProvider")
		.add(m_bIsZOCSource, L"bIsZOCSource")
		.add(m_bActsAsCity, L"bActsAsCity")
		.add(m_bHillsMakesValid, L"bHillsMakesValid")
		.add(m_bFreshWaterMakesValid, L"bFreshWaterMakesValid")
		.add(m_bRiverSideMakesValid, L"bRiverSideMakesValid")
		.add(m_bNoFreshWater, L"bNoFreshWater")
		.add(m_bRequiresFlatlands, L"bRequiresFlatlands")
		.add(m_bRequiresRiverSide, L"bRequiresRiverSide")
		.add(m_bRequiresIrrigation, L"bRequiresIrrigation")
		.add(m_bCarriesIrrigation, L"bCarriesIrrigation")
		.add(m_bRequiresFeature, L"bRequiresFeature")
		.add(m_bPeakImprovement, L"bPeakImprovement")
		.add(m_bWaterImprovement, L"bWaterImprovement")
		.add(m_bGoody, L"bGoody")
		.add(m_bOutsideBorders, L"bOutsideBorders")
		.add(m_bMilitaryStructure, L"bMilitaryStructure")
		.add(m_bPlacesBonus, L"bPlacesBonus")
		.add(m_bPlacesFeature, L"bPlacesFeature")
		.add(m_bPlacesTerrain, L"bPlacesTerrain")
		.add(m_bExtraterresial, L"bExtraterresial")
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")
		.addYields(m_piPrereqNatureYield, L"PrereqNatureYields")
		.addYields(m_piYieldChange, L"YieldChanges")
		.addYields(m_piRiverSideYieldChange, L"RiverSideYieldChange")
		.addYields(m_piIrrigatedChange, L"IrrigatedYieldChange")
		.add(m_iHealthPercent, L"iHealthPercent") // read() folds <iHealth>*100 in afterwards
		.add(m_bPeakMakesValid, L"bPeakMakesValid")
		// NOTE: the legacy copyNonDefaults never copied m_iDepletionRand (dead field: read and
		// checksummed, but no getter); the wrapper now also merges it on modular redefinition.
		.add(m_iDepletionRand, L"iDepletionRand")
		.addEnum(m_iPrereqTech, L"PrereqTech")
		// NOTE: the legacy copyNonDefaults OMITTED the property-manipulator merge (unlike every
		// other manipulator-bearing info class), so a modular redefinition silently dropped the
		// base definition's manipulators; the wrapper merges them (sanctioned fix).
		.add(m_PropertyManipulators)
		// Legacy read used delayed resolution here; bonuses load before improvements, so the
		// wrapper resolves it immediately - same resolved value either way.
		.addEnum(m_iBonusChange, L"BonusChange")
		// Effective legacy read-default was false (the old ctor 'true' was always overwritten by
		// the read default), and the legacy copy compared against false; false it is.
		.add(m_bCanMoveSeaUnits, L"bCanMoveSeaUnits")
		.add(m_bChangeRemove, L"bChangeRemove")
		.add(m_bNotOnAnyBonus, L"bNotOnAnyBonus")
		.add(m_bNational, L"bNational")
		.add(m_bGlobal, L"bGlobal")
		.add(m_aiFeatureChangeTypes, L"FeatureChangeTypes")
		.add(m_aiCategories, L"Categories")
		.add(m_szArtDefineTag, L"ArtDefineTag")
	;
}

void CvImprovementInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	// NOTE: kept explicit (not delegated to CvInfoUtil) to preserve the exact legacy checksum:
	// CheckSum is order-sensitive and the hand-written fields (m_paImprovementBonus, the int**
	// yield matrices, m_aiAlternativeImprovementUpgradeTypes) sit mid-order between declared
	// fields. Do not reorder.
	CheckSumC(iSum, m_aeTerrainMakesValid);
	CheckSumC(iSum, m_aeFeatureMakesValid);

	CheckSum(iSum, m_iAdvancedStartCost);

	CheckSum(iSum, m_iTilesPerGoody);
	CheckSum(iSum, m_iGoodyUniqueRange);
	CheckSum(iSum, m_iFeatureGrowthProbability);
	CheckSum(iSum, m_iUpgradeTime);
	CheckSum(iSum, m_iAirBombDefense);
	CheckSum(iSum, m_iDefenseModifier);
	CheckSum(iSum, m_iHappiness);
	CheckSum(iSum, m_iPillageGold);
	CheckSum(iSum, m_iImprovementPillage);
	CheckSum(iSum, m_iImprovementUpgrade);

	// Super Forts begin *XML*
	CheckSum(iSum, m_iCulture);
	CheckSum(iSum, m_iCultureRange);
	CheckSum(iSum, m_iVisibilityChange);
	CheckSum(iSum, m_iSeeFrom);
	CheckSum(iSum, m_iUniqueRange);
	CheckSum(iSum, m_bBombardable);
	CheckSum(iSum, m_bUpgradeRequiresFortify);
	// Super Forts end
	// Super forts C2C adaptation
	CheckSum(iSum, m_bIsUniversalTradeBonusProvider);
	CheckSum(iSum, m_bIsZOCSource);
	// Super forts C2C adaptation end

	CheckSum(iSum, m_bActsAsCity);
	CheckSum(iSum, m_bHillsMakesValid);
	CheckSum(iSum, m_bFreshWaterMakesValid);
	CheckSum(iSum, m_bRiverSideMakesValid);
	CheckSum(iSum, m_bNoFreshWater);
	CheckSum(iSum, m_bRequiresFlatlands);
	CheckSum(iSum, m_bRequiresRiverSide);
	CheckSum(iSum, m_bRequiresIrrigation);
	CheckSum(iSum, m_bCarriesIrrigation);
	CheckSum(iSum, m_bRequiresFeature);
	CheckSum(iSum, m_bPeakImprovement);
	CheckSum(iSum, m_bWaterImprovement);
	CheckSum(iSum, m_bGoody);
	CheckSum(iSum, m_bOutsideBorders);
	CheckSum(iSum, m_bMilitaryStructure);
	CheckSum(iSum, m_bPlacesBonus);
	CheckSum(iSum, m_bPlacesFeature);
	CheckSum(iSum, m_bPlacesTerrain);
	CheckSum(iSum, m_bExtraterresial);
	CheckSumC(iSum, m_aeMapCategoryTypes);

	// Arrays

	CheckSumI(iSum, NUM_YIELD_TYPES, m_piPrereqNatureYield);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piYieldChange);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piRiverSideYieldChange);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piIrrigatedChange);

	 int i;
	if (m_paImprovementBonus)
		for (i = 0; i < GC.getNumBonusInfos(); i++)
		{
			m_paImprovementBonus[i].getCheckSum(iSum);
		}

	if (m_ppiTechYieldChanges)
		for (i = 0; i < GC.getNumTechInfos(); i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppiTechYieldChanges[i]);
		}

	if (m_ppiRouteYieldChanges)
		for (i = 0; i < GC.getNumRouteInfos(); i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppiRouteYieldChanges[i]);
		}

	CheckSum(iSum, m_iHealthPercent);
	CheckSum(iSum, m_bPeakMakesValid);
	CheckSum(iSum, m_iDepletionRand);
	CheckSum(iSum, m_iPrereqTech);

	//if (m_ppiTraitYieldChanges)
	//	for(i=0;i<GC.getNumTraitInfos();i++)
	//	{
	//		CheckSumI(iSum, NUM_YIELD_TYPES, m_ppiTraitYieldChanges[i]);
	//	}

	m_PropertyManipulators.getCheckSum(iSum);
	//TB Improvements
	//Object Indexes
	CheckSum(iSum, m_iBonusChange);
	//Booleans
	CheckSum(iSum, m_bCanMoveSeaUnits);
	CheckSum(iSum, m_bChangeRemove);
	CheckSum(iSum, m_bNotOnAnyBonus);
	CheckSum(iSum, m_bNational);
	CheckSum(iSum, m_bGlobal);
	CheckSumC(iSum, m_aiAlternativeImprovementUpgradeTypes);
	CheckSumC(iSum, m_aiFeatureChangeTypes);
	CheckSumC(iSum, m_aiCategories);
}

bool CvImprovementInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	int iIndex, j, iNumSibs;

	// Composite tag: <iHealth> folds into the declared m_iHealthPercent (1 health = 100 percent).
	// Must stay after the CvInfoUtil readXml above, which reads <iHealthPercent>.
	int iHealth = 0;
	pXML->GetOptionalChildXmlValByName(&iHealth, L"iHealth");
	m_iHealthPercent += iHealth * 100;

	if (pXML->TryMoveToXmlFirstChild(L"BonusTypeStructs"))
	{
		// call the function that sets the bonus booleans
		pXML->SetImprovementBonuses(&m_paImprovementBonus);
		pXML->MoveToXmlParent();
	}
	else
	{
		// initialize the boolean list to the correct size and all the booleans to false
		pXML->InitImprovementBonusList(&m_paImprovementBonus, GC.getNumBonusInfos());
	}

	// initialize the boolean list to the correct size and all the booleans to false
	FAssertMsg(GC.getNumTechInfos() > 0, "the number of tech infos is zero or less");
	if (pXML->TryMoveToXmlFirstChild(L"TechYieldChanges"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				pXML->Init2DList(&m_ppiTechYieldChanges, GC.getNumTechInfos(), NUM_YIELD_TYPES);
				for (j = 0; j < iNumSibs; j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"PrereqTech");
					iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppiTechYieldChanges[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"TechYields"))
						{
							// call the function that sets the yield change variable
							pXML->SetYields(&m_ppiTechYieldChanges[iIndex]);
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

	// initialize the boolean list to the correct size and all the booleans to false
	FAssertMsg(GC.getNumRouteInfos() > 0, "the number of route infos is zero or less");
	if (pXML->TryMoveToXmlFirstChild(L"RouteYieldChanges"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				pXML->Init2DList(&m_ppiRouteYieldChanges, GC.getNumRouteInfos(), NUM_YIELD_TYPES);
				for (j = 0; j < iNumSibs; j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"RouteType");
					iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppiRouteYieldChanges[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"RouteYields"))
						{
							// call the function that sets the yield change variable
							pXML->SetYields(&m_ppiRouteYieldChanges[iIndex]);
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

	pXML->GetOptionalChildXmlValByName(szTextVal, L"WorldSoundscapeAudioScript");
	if (szTextVal.GetLength() > 0)
		m_iWorldSoundscapeScriptId = gDLL->getAudioTagIndex(szTextVal.GetCString(), AUDIOTAG_SOUNDSCAPE);
	else
		m_iWorldSoundscapeScriptId = -1;

	// initialize the boolean list to the correct size and all the booleans to false
	//FAssertMsg((GC.getNumTraitInfos() > 0) && (NUM_YIELD_TYPES > 0),"either the number of trait infos is zero or less or the number of yield types is zero or less");
	//if (pXML->TryMoveToXmlFirstChild(L"TraitYieldChanges"))
	//{
	//	if (pXML->SkipToNextVal())
	//	{
	//		iNumSibs = pXML->GetXmlChildrenNumber();
	//		if (pXML->TryMoveToXmlFirstChild())
	//		{
	//			if (0 < iNumSibs)
	//			{
	//				pXML->Init2DList(&m_ppiTraitYieldChanges, GC.getNumTraitInfos(), NUM_YIELD_TYPES);
	//				for (j=0;j<iNumSibs;j++)
	//				{
	//					pXML->GetChildXmlValByName(szTextVal, L"TraitType");
	//					iIndex = pXML->GetInfoClass(szTextVal);

	//					if (iIndex > -1)
	//					{
	//						// delete the array since it will be reallocated
	//						SAFE_DELETE_ARRAY(m_ppiTraitYieldChanges[iIndex]);
	//						// if we can set the current xml node to it's next sibling
	//						if (pXML->TryMoveToXmlFirstChild(L"TraitYields"))
	//						{
	//							// call the function that sets the yield change variable
	//							pXML->SetYields(&m_ppiTraitYieldChanges[iIndex]);
	//							pXML->MoveToXmlParent();
	//						}
	//					}

	//					if (!pXML->TryMoveToXmlNextSibling())
	//					{
	//						break;
	//					}
	//				}
	//			}

	//			pXML->MoveToXmlParent();
	//		}
	//	}

	//	pXML->MoveToXmlParent();
	//}

	pXML->SetOptionalVectorWithDelayedResolution(m_aiAlternativeImprovementUpgradeTypes, L"AlternativeImprovementUpgradeTypes");

	return true;
}

void CvImprovementInfo::copyNonDefaults(const CvImprovementInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const bool bDefault = false;
	const int iDefault = 0;
	const int iTextDefault = -1;  //all integers which are TEXT_KEYS in the xml are -1 by default

	// The art tag must merge BEFORE the base copy: CvInfoBase::copyNonDefaults calls the virtual
	// getButton(), which resolves through getArtDefineTag(). The declared StringWrapper copy
	// then no-ops.
	if (m_szArtDefineTag.empty()) m_szArtDefineTag = pClassInfo->getArtDefineTag();

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	for (int i = 0; i < GC.getNumBonusInfos(); i++)
	{
		if (m_paImprovementBonus[i].m_bBonusMakesValid == bDefault)
		{
			m_paImprovementBonus[i].m_bBonusMakesValid = pClassInfo->isImprovementBonusMakesValid(i);
		}
		if (m_paImprovementBonus[i].m_bObsoleteBonusMakesValid == bDefault)
		{
			m_paImprovementBonus[i].m_bObsoleteBonusMakesValid = pClassInfo->isImprovementObsoleteBonusMakesValid(i);
		}
		if (m_paImprovementBonus[i].m_bBonusTrade == bDefault)
		{
			m_paImprovementBonus[i].m_bBonusTrade = pClassInfo->isImprovementBonusTrade(i);
		}
		if (m_paImprovementBonus[i].m_iDiscoverRand == iDefault)
		{
			m_paImprovementBonus[i].m_iDiscoverRand = pClassInfo->getImprovementBonusDiscoverRand(i);
		}
		if (m_paImprovementBonus[i].m_iDepletionRand == 0)
		{
			m_paImprovementBonus[i].m_iDepletionRand = pClassInfo->getImprovementBonusDepletionRand(i);
		}

		for (int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if (m_paImprovementBonus[i].m_piYieldChange[j] == iDefault)
			{
				m_paImprovementBonus[i].m_piYieldChange[j] = pClassInfo->getImprovementBonusYield(i, j);
			}
		}
	}
	for (int i = 0; i < GC.getNumTechInfos(); i++)
	{
		for (int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if (getTechYieldChanges(i, j) == iDefault && pClassInfo->getTechYieldChanges(i, j) != iDefault)
			{
				if (NULL == m_ppiTechYieldChanges)
				{
					CvXMLLoadUtility::Init2DList(&m_ppiTechYieldChanges, GC.getNumTechInfos(), NUM_YIELD_TYPES);
				}
				else if (NULL == m_ppiTechYieldChanges[i])
				{
					CvXMLLoadUtility::InitList(&m_ppiTechYieldChanges[i], NUM_YIELD_TYPES, iDefault);
				}
				m_ppiTechYieldChanges[i][j] = pClassInfo->getTechYieldChanges(i, j);
			}
		}
	}
	for (int i = 0; i < GC.getNumRouteInfos(); i++)
	{
		for (int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if (getRouteYieldChanges(i, j) == iDefault && pClassInfo->getRouteYieldChanges(i, j) != iDefault)
			{
				if (NULL == m_ppiRouteYieldChanges)
				{
					CvXMLLoadUtility::Init2DList(&m_ppiRouteYieldChanges, GC.getNumRouteInfos(), NUM_YIELD_TYPES);
				}
				else if (NULL == m_ppiRouteYieldChanges[i])
				{
					CvXMLLoadUtility::InitList(&m_ppiRouteYieldChanges[i], NUM_YIELD_TYPES, iDefault);
				}
				m_ppiRouteYieldChanges[i][j] = pClassInfo->getRouteYieldChanges(i, j);
			}
		}
	}

	if (m_iWorldSoundscapeScriptId == iTextDefault) m_iWorldSoundscapeScriptId = pClassInfo->getWorldSoundscapeScriptId();

	//for ( int i = 0; i < GC.getNumTraitInfos(); i++)
	//{
	//	for ( int j = 0; j < NUM_YIELD_TYPES; j++)
	//	{
	//		if ( getTraitYieldChanges(i, j) == iDefault && pClassInfo->getTraitYieldChanges(i, j) != iDefault)
	//		{
	//			if ( NULL == m_ppiTraitYieldChanges )
	//			{
	//				pXML->Init2DList(&m_ppiTraitYieldChanges, GC.getNumTraitInfos(), NUM_YIELD_TYPES);
	//			}
	//			else if ( NULL == m_ppiTraitYieldChanges[i] )
	//			{
	//				CvXMLLoadUtility::InitList(&m_ppiTraitYieldChanges[i],NUM_YIELD_TYPES,iDefault);
	//			}
	//			m_ppiTraitYieldChanges[i][j] = pClassInfo->getTraitYieldChanges(i, j);
	//		}
	//	}
	//}

	if (getNumAlternativeImprovementUpgradeTypes() == 0)
	{
		const int iNum = pClassInfo->getNumAlternativeImprovementUpgradeTypes();
		m_aiAlternativeImprovementUpgradeTypes.resize(iNum);
		for (int i = 0; i < iNum; i++)
		{
			GC.copyNonDefaultDelayedResolution((int*)&(m_aiAlternativeImprovementUpgradeTypes[i]), (int*)&(pClassInfo->m_aiAlternativeImprovementUpgradeTypes[i]));
		}
	}
}

void CvImprovementInfo::doPostLoadCaching(uint32_t iThis)
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0, num = GC.getNumBuildInfos(); i < num; i++)
	{
		if (GC.getBuildInfo((BuildTypes)i).getImprovement() == iThis)
		{
			m_improvementBuildTypes.push_back((BuildTypes)i);
		}
	}
}