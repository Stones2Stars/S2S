//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:	CvUnitInfo.cpp
//
//  PURPOSE: Info class for units
//
//------------------------------------------------------------------------------------------------
//  Copyright (c) 2003 Firaxis Games, Inc. All rights reserved.
//------------------------------------------------------------------------------------------------

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvBonusInfo.h"
#include "CvInfos.h"
#include "CvUnitCombatInfo.h"
#include "CvInfoUtil.h"
#include "AI/CvPlayerAI.h"
#include "Infrastructure/CvPython.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvXMLLoadUtilityModTools.h"
#include "Tools/CheckSum.h"

//======================================================================================================
//					CvUnitInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvUnitInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
// Fields declared in getDataMembers() are initialized by initDataMembers() in the body;
// the init list below covers only the hand-written XML remainder and runtime/derived fields.
CvUnitInfo::CvUnitInfo() :
m_iWithdrawalProbability(0),
m_iBaseUpkeep(0),
m_iDefaultUnitAIType(NO_UNITAI),
m_iGroupSize(0),
m_iGroupDefinitions(0),
m_iUnitMeleeWaveSize(0),
m_iUnitRangedWaveSize(0),
m_iNumUnitNames(0),
m_iCommandType(NO_COMMAND),
m_fUnitMaxSpeed(0.0f),
m_fUnitPadTime(0.0f),
m_piReligionSpreads(NULL),
m_piCorporationSpreads(NULL),
m_piTerrainPassableTech(NULL),
m_piFeaturePassableTech(NULL),
m_piFlavorValue(NULL),
m_piTerrainAttackModifier(NULL),
m_piTerrainDefenseModifier(NULL),
m_piFeatureAttackModifier(NULL),
m_piFeatureDefenseModifier(NULL),
m_piUnitCombatModifier(NULL),
m_piUnitCombatCollateralImmune(NULL),
m_piDomainModifier(NULL),
m_piBonusProductionModifier(NULL),
m_piUnitGroupRequired(NULL),
m_paszEarlyArtDefineTags(NULL),
m_paszLateArtDefineTags(NULL),
m_paszMiddleArtDefineTags(NULL),
m_paszUnitNames(NULL),
m_abHasCombatType(NULL),
m_paszClassicalArtDefineTags(NULL),
m_paszRennArtDefineTags(NULL),
m_paszIndustrialArtDefineTags(NULL),
m_paszFutureArtDefineTags(NULL),
m_paszCivilizationNames(NULL),
m_iTotalCombatStrengthModifierBase(0),
m_iTotalCombatStrengthChangeBase(0),
m_iBaseCargoVolume(0),
m_iBaseGroupRank(0),
m_bCanAnimalIgnoresBorders(false),
m_bCanAnimalIgnoresImprovements(false),
m_bCanAnimalIgnoresCities(false),
m_bCanMergeSplit(true)
{
	CvInfoUtil(this).initDataMembers();

	m_zobristValue = GC.getGame().getSorenRand().getInt();
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvUnitInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvUnitInfo::~CvUnitInfo()
{
	PROFILE_EXTRA_FUNC();
	CvInfoUtil(this).uninitDataMembers();

	SAFE_DELETE_ARRAY(m_piReligionSpreads);
	SAFE_DELETE_ARRAY(m_piCorporationSpreads);
	SAFE_DELETE_ARRAY(m_piTerrainPassableTech);
	SAFE_DELETE_ARRAY(m_piFeaturePassableTech);
	//SAFE_DELETE_ARRAY(m_pbTerrainImpassable);
	//SAFE_DELETE_ARRAY(m_pbFeatureImpassable);
	SAFE_DELETE_ARRAY(m_piFlavorValue);
	SAFE_DELETE_ARRAY(m_piTerrainAttackModifier);
	SAFE_DELETE_ARRAY(m_piTerrainDefenseModifier);
	SAFE_DELETE_ARRAY(m_piFeatureAttackModifier);
	SAFE_DELETE_ARRAY(m_piFeatureDefenseModifier);
	SAFE_DELETE_ARRAY(m_piUnitCombatModifier);
	SAFE_DELETE_ARRAY(m_piUnitCombatCollateralImmune);
	SAFE_DELETE_ARRAY(m_piDomainModifier);
	SAFE_DELETE_ARRAY(m_piBonusProductionModifier);
	SAFE_DELETE_ARRAY(m_piUnitGroupRequired);
	SAFE_DELETE_ARRAY(m_paszEarlyArtDefineTags);
	SAFE_DELETE_ARRAY(m_paszLateArtDefineTags);
	SAFE_DELETE_ARRAY(m_paszMiddleArtDefineTags);
	SAFE_DELETE_ARRAY(m_paszUnitNames);
	SAFE_DELETE_ARRAY(m_paszClassicalArtDefineTags);
	SAFE_DELETE_ARRAY(m_paszRennArtDefineTags);
	SAFE_DELETE_ARRAY(m_paszIndustrialArtDefineTags);
	SAFE_DELETE_ARRAY(m_paszFutureArtDefineTags);
	SAFE_DELETE_ARRAY(m_paszCivilizationNames);
	// m_pExprTrainCondition is owned by its BoolExprWrapper (getDataMembers);
	// uninitDataMembers() above deletes it.

	GC.removeDelayedResolutionVector(m_piPrereqOrBonuses);
	GC.removeDelayedResolutionVector(m_piPrereqOrVicinityBonuses);

	foreach_(const CvOutcomeMission* outcomeMission, m_aOutcomeMissions)
	{
		SAFE_DELETE(outcomeMission);
	}

	//Struct Vector
	for (int i=0; i<(int)m_aEnabledCivilizationTypes.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aEnabledCivilizationTypes[i]));
	}

	for (int i=0; i<(int)m_aiSupersedingUnits.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aiSupersedingUnits[i]));
	}

	for (int i=0; i<(int)m_aiUnitUpgrades.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aiUnitUpgrades[i]));
	}

	for (int i=0; i<(int)m_aiDefendAgainstUnit.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aiDefendAgainstUnit[i]));
	}

	for (int i=0; i<(int)m_aiTargetUnit.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aiTargetUnit[i]));
	}
}

const wchar_t* CvUnitInfo::getExtraHoverText() const
{
	return m_szExtraHoverTextKey.empty() ? L"" : gDLL->getText(m_szExtraHoverTextKey);
}


int CvUnitInfo::getMaxGlobalInstances() const
{
	return m_iMaxGlobalInstances;
}
int CvUnitInfo::getMaxPlayerInstances() const
{
	return m_iMaxPlayerInstances;
}
bool CvUnitInfo::isUnlimitedException() const
{
	return m_bUnlimitedException;
}


// When a player is specified as argument, only civ units not specific to said player returns false, else true.
// Thus regular units will also return true as they are not specific to another civ than this player.
// When NO_PLAYER - Any units requiring a civ specific building are considered civ units, all others are not.
bool CvUnitInfo::isCivilizationUnit(const PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	// Not the most elegant solution for exluding or including neanderthal units for starting unit selection,
	// nor the best way to stop barbarians from spawning neanderthal units. But good enough for now.
	const bool bCivUnit = ePlayer != NO_PLAYER;

	for (int iI = 0; iI < getNumPrereqAndBuildings(); ++iI)
	{
		const int iBuilding = getPrereqAndBuilding(iI);

		for (int iCiv = 0; iCiv < GC.getNumCivilizationInfos(); iCiv++)
		{
			// Civ specific building prereq?
			if(GC.getCivilizationInfo((CivilizationTypes)iCiv).isPlayable()
			&& GC.getCivilizationInfo((CivilizationTypes)iCiv).isCivilizationBuilding(iBuilding))
			{
				if (!bCivUnit) // NO_PLAYER
				{
					return true; // A civ specific unit.
				}
				// Most likely a native or active culture prereq
				if (!GC.getCivilizationInfo(GET_PLAYER(ePlayer).getCivilizationType()).isCivilizationBuilding(iBuilding))
				{
					return false; // Not specific to ePlayer civ.
				}
				break;
			}
		}
	}
	return bCivUnit; // Unit is valid for ePlayer civilization.
}

int CvUnitInfo::getInstanceCostModifier() const
{
	return m_iInstanceCostModifier;
}

// Dale - RB: Field Bombard START
int CvUnitInfo::getDCMBombRange() const
{
	return m_iDCMBombRange;
}

int CvUnitInfo::getDCMBombAccuracy() const
{
	return m_iDCMBombAccuracy;
}
// Dale - RB: Field Bombard END

// Dale - AB: Bombing START
bool CvUnitInfo::getDCMAirBomb1() const
{
	return m_bDCMAirBomb1;
}

bool CvUnitInfo::getDCMAirBomb2() const
{
	return m_bDCMAirBomb2;
}

bool CvUnitInfo::getDCMAirBomb3() const
{
	return m_bDCMAirBomb3;
}

bool CvUnitInfo::getDCMAirBomb4() const
{
	return m_bDCMAirBomb4;
}

bool CvUnitInfo::getDCMAirBomb5() const
{
	return m_bDCMAirBomb5;
}
// Dale - AB: Bombing END
// Dale - FE: Fighters START
bool CvUnitInfo::getDCMFighterEngage() const
{
	return m_bDCMFighterEngage;
}
// Dale - FE: Fighters END


int CvUnitInfo::getAIWeight() const
{
	return m_iAIWeight;
}

int CvUnitInfo::getProductionCost() const
{
	return m_iProductionCost;
}

int CvUnitInfo::getHurryCostModifier() const
{
	return m_iHurryCostModifier;
}

int CvUnitInfo::getAdvancedStartCost() const
{
	return m_iAdvancedStartCost;
}

int CvUnitInfo::getMinAreaSize() const
{
	return m_iMinAreaSize;
}

int CvUnitInfo::getMoves() const
{
	return m_iMoves;
}

int CvUnitInfo::getAirRange() const
{
	return m_iAirRange;
}

int CvUnitInfo::getAirUnitCap() const
{
	return m_iAirUnitCap;
}

int CvUnitInfo::getDropRange() const
{
	return m_iDropRange;
}

int CvUnitInfo::getNukeRange() const
{
	return m_iNukeRange;
}

int CvUnitInfo::getWorkRate() const
{
	return m_iWorkRate;
}

int CvUnitInfo::getBaseDiscover() const
{
	return m_iBaseDiscover;
}

int CvUnitInfo::getDiscoverMultiplier() const
{
	return m_iDiscoverMultiplier;
}

int CvUnitInfo::getBaseHurry() const
{
	return m_iBaseHurry;
}

int CvUnitInfo::getHurryMultiplier() const
{
	return m_iHurryMultiplier;
}

int CvUnitInfo::getBaseTrade() const
{
	return m_iBaseTrade;
}

int CvUnitInfo::getTradeMultiplier() const
{
	return m_iTradeMultiplier;
}

int CvUnitInfo::getGreatWorkCulture() const
{
	return m_iGreatWorkCulture;
}

int CvUnitInfo::getEspionagePoints() const
{
	return m_iEspionagePoints;
}

int CvUnitInfo::getCombat() const
{
	return m_iCombat;
}

int CvUnitInfo::getCombatLimit() const
{
	return m_iCombatLimit;
}

int CvUnitInfo::getAirCombat() const
{
	return m_iAirCombat;
}

int CvUnitInfo::getAirCombatLimit() const
{
	return m_iAirCombatLimit;
}

int CvUnitInfo::getXPValueAttack() const
{
	return m_iXPValueAttack;
}

int CvUnitInfo::getXPValueDefense() const
{
	return m_iXPValueDefense;
}

int CvUnitInfo::getFirstStrikes() const
{
	return m_iFirstStrikes;
}

int CvUnitInfo::getChanceFirstStrikes() const
{
	return m_iChanceFirstStrikes;
}

int CvUnitInfo::getInterceptionProbability() const
{
	return m_iInterceptionProbability;
}

//ls612: Advanced Nuke Interception
//int CvUnitInfo::getNukeInterceptionProbability() const
//{
//	return m_iNukeInterceptionProbability;
//}
//
//int CvUnitInfo::getNukeInterceptionRange() const
//{
//	return m_iNukeInterceptionRange;
//}

int CvUnitInfo::getEvasionProbability() const
{
	return m_iEvasionProbability;
}

int CvUnitInfo::getWithdrawalProbability() const
{
	return m_iWithdrawalProbability;
}

int CvUnitInfo::getCollateralDamage() const
{
	return m_iCollateralDamage;
}

int CvUnitInfo::getCollateralDamageLimit() const
{
	return m_iCollateralDamageLimit;
}

int CvUnitInfo::getCollateralDamageMaxUnits() const
{
	return m_iCollateralDamageMaxUnits;
}

int CvUnitInfo::getCityAttackModifier() const
{
	return m_iCityAttackModifier;
}

int CvUnitInfo::getCityDefenseModifier() const
{
	return m_iCityDefenseModifier;
}

int CvUnitInfo::getAnimalCombatModifier() const
{
	return m_iAnimalCombatModifier;
}

int CvUnitInfo::getHillsAttackModifier() const
{
	return m_iHillsAttackModifier;
}

int CvUnitInfo::getHillsDefenseModifier() const
{
	return m_iHillsDefenseModifier;
}

int CvUnitInfo::getBombRate() const
{
	return m_iBombRate;
}

int CvUnitInfo::getBombardRate() const
{
	return m_iBombardRate;
}

int CvUnitInfo::getSpecialCargo() const
{
	return m_iSpecialCargo;
}

int CvUnitInfo::getSMNotSpecialCargo() const
{
	return m_iSMNotSpecialCargo;
}

int CvUnitInfo::getDomainCargo() const
{
	return m_iDomainCargo;
}

int CvUnitInfo::getCargoSpace() const
{
	return m_iCargoSpace;
}

int CvUnitInfo::getSMCargoSpace() const
{
	return m_iSMCargoSpace;
}

int CvUnitInfo::getSMCargoVolume() const
{
	return m_iSMCargoVolume;
}

int CvUnitInfo::getConscriptionValue() const
{
	return m_iConscriptionValue;
}

int CvUnitInfo::getCultureGarrisonValue() const
{
	return m_iCultureGarrisonValue;
}

int CvUnitInfo::getBaseUpkeep() const
{
	return m_iBaseUpkeep;
}

int CvUnitInfo::getAssetValue() const
{
	return m_iAssetValue * 100;
}

int CvUnitInfo::getPowerValue() const
{
	return m_iPowerValue * 100;
}

int CvUnitInfo::getSpecialUnitType() const
{
	return m_iSpecialUnitType;
}

int CvUnitInfo::getUnitCombatType() const
{
	return m_iUnitCombatType;
}

DomainTypes CvUnitInfo::getDomainType() const
{
	return m_iDomainType;
}

UnitAITypes CvUnitInfo::getDefaultUnitAIType() const
{
	return m_iDefaultUnitAIType;
}

int CvUnitInfo::getInvisibleType() const
{
	return m_iInvisibleType;
}

int CvUnitInfo::getSeeInvisibleType(int i) const
{
	FASSERT_BOUNDS(0, getNumSeeInvisibleTypes(), i);

	return m_aiSeeInvisibleTypes[i];
}

int CvUnitInfo::getNumSeeInvisibleTypes() const
{
	return (int)m_aiSeeInvisibleTypes.size();
}

int CvUnitInfo::getAdvisorType() const
{
	return m_iAdvisorType;
}

int CvUnitInfo::getMaxStartEra() const
{
	return m_iMaxStartEra;
}

int CvUnitInfo::getObsoleteTech() const
{
	return m_iObsoleteTech;
}

bool CvUnitInfo::isStateReligion() const
{
	return m_bStateReligion;
}

int CvUnitInfo::getPrereqGameOption() const
{
	return m_iPrereqGameOption;
}

int CvUnitInfo::getNotGameOption() const
{
	return m_iNotGameOption;
}

int CvUnitInfo::getHolyCity() const
{
	return m_iHolyCity;
}

int CvUnitInfo::getReligionType() const
{
	return m_iReligionType;
}

int CvUnitInfo::getStateReligion() const
{
	return m_iStateReligion;
}

int CvUnitInfo::getPrereqReligion() const
{
	return m_iPrereqReligion;
}

int CvUnitInfo::getPrereqCorporation() const
{
	return m_iPrereqCorporation;
}

int CvUnitInfo::getPrereqOrBuildingsNum() const
{
	return m_aiPrereqOrBuildings.size();
}

BuildingTypes CvUnitInfo::getPrereqOrBuilding(int i) const
{
	FASSERT_BOUNDS(0, getPrereqOrBuildingsNum(), i);
	return (BuildingTypes)m_aiPrereqOrBuildings[i];
}

int CvUnitInfo::getPrereqAndTech() const
{
	return m_iPrereqAndTech;
}

int CvUnitInfo::getPrereqAndBonus() const
{
	return m_iPrereqAndBonus;
}

int CvUnitInfo::getGroupSize() const// the initial number of individuals in the unit group
{
	return m_iGroupSize;
}

int CvUnitInfo::getGroupDefinitions() const// the number of UnitMeshGroups for this unit
{
	return m_iGroupDefinitions;
}

int CvUnitInfo::getMeleeWaveSize() const
{
	return m_iUnitMeleeWaveSize;
}

int CvUnitInfo::getRangedWaveSize() const
{
	return m_iUnitRangedWaveSize;
}

int CvUnitInfo::getNumUnitNames() const
{
	return m_iNumUnitNames;
}

bool CvUnitInfo::isFoodProduction() const
{
	return m_bFoodProduction;
}

bool CvUnitInfo::isNoBadGoodies() const
{
	return m_bNoBadGoodies;
}

bool CvUnitInfo::isOnlyDefensive() const
{
	return m_bOnlyDefensive;
}

bool CvUnitInfo::isNoCapture() const
{
	return (m_bNoCapture);
}

bool CvUnitInfo::isRivalTerritory() const
{
	return m_bRivalTerritory;
}

bool CvUnitInfo::isMilitaryHappiness() const
{
	return m_bMilitaryHappiness;
}

bool CvUnitInfo::isMilitarySupport() const
{
	return m_bMilitarySupport;
}

bool CvUnitInfo::isMilitaryProduction() const
{
	return m_bMilitaryProduction;
}

bool CvUnitInfo::isPillage() const
{
	return m_bPillage;
}

bool CvUnitInfo::isSpy() const
{
	return m_bSpy;
}

bool CvUnitInfo::isSabotage() const
{
	return m_bSabotage;
}

bool CvUnitInfo::isDestroy() const
{
	return m_bDestroy;
}

bool CvUnitInfo::isStealPlans() const
{
	return m_bStealPlans;
}

bool CvUnitInfo::isInvestigate() const
{
	return m_bInvestigate;
}

bool CvUnitInfo::isCounterSpy() const
{
	return m_bCounterSpy;
}

bool CvUnitInfo::isFound() const
{
	return m_bFound;
}

bool CvUnitInfo::isGoldenAge() const
{
	return m_bGoldenAge;
}

bool CvUnitInfo::isInvisible() const
{
	return m_bInvisible;
}

void CvUnitInfo::setInvisible(bool bEnable)
{
	m_bInvisible = bEnable;
}

bool CvUnitInfo::isFirstStrikeImmune() const
{
	return m_bFirstStrikeImmune;
}

bool CvUnitInfo::isNoDefensiveBonus() const
{
	return m_bNoDefensiveBonus;
}

bool CvUnitInfo::isIgnoreBuildingDefense() const
{
	return m_bIgnoreBuildingDefense;
}

bool CvUnitInfo::isCanMoveImpassable() const
{
	return m_bCanMoveImpassable;
}

bool CvUnitInfo::isCanMoveAllTerrain() const
{
	return m_bCanMoveAllTerrain;
}

bool CvUnitInfo::isFlatMovementCost() const
{
	return m_bFlatMovementCost;
}

bool CvUnitInfo::isIgnoreTerrainCost() const
{
	return m_bIgnoreTerrainCost;
}

bool CvUnitInfo::isNukeImmune() const
{
	return m_bNukeImmune;
}

bool CvUnitInfo::isMechUnit() const
{
	return m_bMechanized;
}

bool CvUnitInfo::isRenderBelowWater() const
{
	return m_bRenderBelowWater;
}

bool CvUnitInfo::isRenderAlways() const
{
	return m_bRenderAlways;
}

bool CvUnitInfo::isSuicide() const
{
	return m_bSuicide;
}

bool CvUnitInfo::isLineOfSight() const
{
	return m_bLineOfSight;
}

bool CvUnitInfo::isHiddenNationality() const
{
	return m_bHiddenNationality;
}

bool CvUnitInfo::isAlwaysHostile() const
{
	return m_bAlwaysHostile;
}

bool CvUnitInfo::isFreeDrop() const
{
	return m_bFreeDrop;
}

bool CvUnitInfo::isNoRevealMap() const
{
	return m_bNoRevealMap;
}

bool CvUnitInfo::isInquisitor() const
{
	return m_bInquisitor;
}

//ls612: Can't enter non-Owned cities
bool CvUnitInfo::isNoNonOwnedEntry() const
{
	return m_bNoNonOwnedEntry;
}

float CvUnitInfo::getUnitMaxSpeed() const
{
	return m_fUnitMaxSpeed;
}

float CvUnitInfo::getUnitPadTime() const
{
	return m_fUnitPadTime;
}

int CvUnitInfo::getCommandType() const
{
	return m_iCommandType;
}

void CvUnitInfo::setCommandType(int iNewType)
{
	m_iCommandType = iNewType;
}

const BoolExpr* CvUnitInfo::getTrainCondition() const
{
	return m_pExprTrainCondition;
}

// BUG - Unit Experience - start
/*
 * Returns true if this unit type is eligible to receive experience points.
 */
bool CvUnitInfo::canAcquireExperience() const
{
	PROFILE_EXTRA_FUNC();
	if (m_iUnitCombatType != NO_UNITCOMBAT)
	{
		for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
		{
			if (GC.getPromotionInfo((PromotionTypes)iI).getUnitCombat(m_iUnitCombatType))
			{
				return true;
			}
		}
	}

	return false;
}
// BUG - Unit Experience - end


// Arrays

bool CvUnitInfo::isPrereqOrCivics(int i) const
{
	FASSERT_BOUNDS(NO_CIVIC, GC.getNumCivicInfos(), i);

	if (i == NO_CIVIC)
	{
		return !m_aePrereqOrCivics.empty();
	}
	return algo::any_of_equal(m_aePrereqOrCivics, static_cast<CivicTypes>(i));
}


BuildTypes CvUnitInfo::getBuild(int i) const
{
	return m_workerBuilds[i];
}
short CvUnitInfo::getNumBuilds() const
{
	return m_workerBuilds.size();
}
bool CvUnitInfo::hasBuild(BuildTypes e) const
{
	return algo::any_of_equal(m_workerBuilds, e);
}

int CvUnitInfo::getNumPrereqAndBuildings() const
{
	return (int)m_aiPrereqAndBuildings.size();
}
int CvUnitInfo::getPrereqAndBuilding(int i) const
{
	return m_aiPrereqAndBuildings[i];
}
bool CvUnitInfo::isPrereqAndBuilding(int i) const
{
	return algo::any_of_equal(m_aiPrereqAndBuildings, i);
}
bool CvUnitInfo::isPrereqOrBuilding(int i) const
{
	return algo::any_of_equal(m_aiPrereqOrBuildings, i);
}

//Struct Vector
int CvUnitInfo::getTargetUnit(int i) const
{
	return m_aiTargetUnit[i];
}
int CvUnitInfo::getNumTargetUnits() const
{
	return (int)m_aiTargetUnit.size();
}
bool CvUnitInfo::isTargetUnit(int i) const
{
	return algo::any_of_equal(m_aiTargetUnit, i);
}


int CvUnitInfo::getDefendAgainstUnit(int i) const
{
	return m_aiDefendAgainstUnit[i];
}
int CvUnitInfo::getNumDefendAgainstUnits() const
{
	return (int)m_aiDefendAgainstUnit.size();
}
bool CvUnitInfo::isDefendAgainstUnit(int i) const
{
	return algo::any_of_equal(m_aiDefendAgainstUnit, i);
}


int CvUnitInfo::getSupersedingUnit(int i) const
{
	return m_aiSupersedingUnits[i];
}
short CvUnitInfo::getNumSupersedingUnits() const
{
	return m_aiSupersedingUnits.size();
}
bool CvUnitInfo::isSupersedingUnit(int i) const
{
	return algo::any_of_equal(m_aiSupersedingUnits, i);
}


int CvUnitInfo::getUnitUpgrade(int i) const
{
	return m_aiUnitUpgrades[i];
}
int CvUnitInfo::getNumUnitUpgrades() const
{
	return (int)m_aiUnitUpgrades.size();
}
bool CvUnitInfo::isUnitUpgrade(int i) const
{
	return algo::any_of_equal(m_aiUnitUpgrades, i);
}


std::vector<int> CvUnitInfo::getUnitUpgradeChain() const
{
	return m_aiUnitUpgradeChain;
}
void CvUnitInfo::addUnitToUpgradeChain(int i)
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), i);
	if (algo::none_of_equal(m_aiUnitUpgradeChain, i))
	{
		m_aiUnitUpgradeChain.push_back(i);
	}
}


const std::vector<TechTypes>& CvUnitInfo::getPrereqAndTechs() const
{
	return m_piPrereqAndTechs;
}

const std::vector<BonusTypes>& CvUnitInfo::getPrereqOrBonuses() const
{
	return m_piPrereqOrBonuses;
}

int CvUnitInfo::getFlavorValue(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFlavorTypes(), i);
	return m_piFlavorValue ? m_piFlavorValue[i] : 0;
}

int CvUnitInfo::getTerrainAttackModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return m_piTerrainAttackModifier ? m_piTerrainAttackModifier[i] : 0;
}

int CvUnitInfo::getTerrainDefenseModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return m_piTerrainDefenseModifier ? m_piTerrainDefenseModifier[i] : 0;
}

int CvUnitInfo::getFeatureAttackModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return m_piFeatureAttackModifier ? m_piFeatureAttackModifier[i] : 0;
}

int CvUnitInfo::getFeatureDefenseModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return m_piFeatureDefenseModifier ? m_piFeatureDefenseModifier[i] : 0;
}

int CvUnitInfo::getUnitCombatModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return m_piUnitCombatModifier ? m_piUnitCombatModifier[i] : 0;
}

int CvUnitInfo::getUnitCombatCollateralImmune(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return m_piUnitCombatCollateralImmune ? m_piUnitCombatCollateralImmune[i] : 0;
}

int CvUnitInfo::getDomainModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, i);
	return m_piDomainModifier ? m_piDomainModifier[i] : 0;
}

int CvUnitInfo::getBonusProductionModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_piBonusProductionModifier ? m_piBonusProductionModifier[i] : 0;
}

int CvUnitInfo::getUnitGroupRequired(int i) const
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);
	return m_piUnitGroupRequired ? m_piUnitGroupRequired[i] : NULL;
}

bool CvUnitInfo::getTargetUnitCombat(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aeTargetUnitCombat, static_cast<UnitCombatTypes>(i));
}

bool CvUnitInfo::getDefenderUnitCombat(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aeDefenderUnitCombat, static_cast<UnitCombatTypes>(i));
}

bool CvUnitInfo::getUnitAIType(int i) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, i);
	return algo::any_of_equal(m_aiUnitAIs, static_cast<UnitAITypes>(i));
}

bool CvUnitInfo::getNotUnitAIType(int i) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, i);
	return algo::any_of_equal(m_aiNotUnitAIs, static_cast<UnitAITypes>(i));
}

int CvUnitInfo::getReligionSpreads(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumReligionInfos(), i);
	return m_piReligionSpreads ? m_piReligionSpreads[i] : -1;
}

int CvUnitInfo::getCorporationSpreads(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCorporationInfos(), i);
	return m_piCorporationSpreads ? m_piCorporationSpreads[i] : -1;
}

int CvUnitInfo::getTerrainPassableTech(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return m_piTerrainPassableTech ? m_piTerrainPassableTech[i] : -1;
}

int CvUnitInfo::getFeaturePassableTech(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return m_piFeaturePassableTech ? m_piFeaturePassableTech[i] : -1;
}

bool CvUnitInfo::getGreatPeoples(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	return algo::any_of_equal(m_aeGreatPeoples, static_cast<SpecialistTypes>(i));
}

int CvUnitInfo::getBuildings(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), i);
	return m_pbBuildings[i];
}

bool CvUnitInfo::getHasBuilding(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), i);
	return algo::any_of_equal(m_pbBuildings, i);
}

int CvUnitInfo::getNumBuildings() const
{
	return m_pbBuildings.size();
}

int CvUnitInfo::getHeritage(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumHeritageInfos(), i);
	return m_addHeritage[i];
}

bool CvUnitInfo::getHasHeritage(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumHeritageInfos(), i);
	return algo::any_of_equal(m_addHeritage, i);
}

int CvUnitInfo::getNumHeritage() const
{
	return m_addHeritage.size();
}

//
//bool CvUnitInfo::getTerrainImpassable(int i) const
//{
//	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
//	return m_pbTerrainImpassable ? m_pbTerrainImpassable[i] : false;
//}
//
//bool CvUnitInfo::getFeatureImpassable(int i) const
//{
//	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
//	return m_pbFeatureImpassable ? m_pbFeatureImpassable[i] : false;
//}

bool CvUnitInfo::getTerrainNative(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return algo::any_of_equal(m_aeTerrainNative, static_cast<TerrainTypes>(i));
}

bool CvUnitInfo::getFeatureNative(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return algo::any_of_equal(m_aeFeatureNative, static_cast<FeatureTypes>(i));
}

bool CvUnitInfo::getFreePromotions(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), i);
	return algo::any_of_equal(m_aeFreePromotions, static_cast<PromotionTypes>(i));
}

int CvUnitInfo::getLeaderPromotion() const
{
	return m_iLeaderPromotion;
}

int CvUnitInfo::getLeaderExperience() const
{
	return m_iLeaderExperience;
}

const CvOutcomeList* CvUnitInfo::getKillOutcomeList() const
{
	return &m_KillOutcomeList;
}

int CvUnitInfo::getNumActionOutcomes() const
{
	return m_aOutcomeMissions.size();
}

MissionTypes CvUnitInfo::getActionOutcomeMission(int index) const
{
	return m_aOutcomeMissions[index]->getMission();
}

const CvOutcomeList* CvUnitInfo::getActionOutcomeList(int index) const
{
	return m_aOutcomeMissions[index]->getOutcomeList();
}

const CvOutcomeList* CvUnitInfo::getActionOutcomeListByMission(MissionTypes eMission) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const CvOutcomeMission* outcomeMission, m_aOutcomeMissions)
	{
		if (outcomeMission->getMission() == eMission)
		{
			return outcomeMission->getOutcomeList();
		}
	}
	return NULL;
}

const CvOutcomeMission* CvUnitInfo::getOutcomeMission(int index) const
{
	return m_aOutcomeMissions[index];
}

const CvOutcomeMission* CvUnitInfo::getOutcomeMissionByMission(MissionTypes eMission) const
{
	return algo::find_if(m_aOutcomeMissions, bind(CvOutcomeMission::getMission, _1) == eMission).get_value_or(NULL);
}

const char* CvUnitInfo::getEarlyArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);

	if (NO_UNIT_ARTSTYLE != eStyle)
	{
		const int iIndex = GC.getInfoTypeForString(getType());
		if (-1 != iIndex)
		{
			const char* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getEarlyArtDefineTag(i, iIndex);
			if (NULL != pcTag)
			{
				return pcTag;
			}
		}
	}

	return (m_paszEarlyArtDefineTags) ? m_paszEarlyArtDefineTags[i] : NULL;
}

void CvUnitInfo::setEarlyArtDefineTag(int i, const char* szVal)
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);
	m_paszEarlyArtDefineTags[i] = szVal;
}

const char* CvUnitInfo::getLateArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);

	if (NO_UNIT_ARTSTYLE != eStyle)
	{
		const int iIndex = GC.getInfoTypeForString(getType());
		if (-1 != iIndex)
		{
			const char* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getLateArtDefineTag(i, iIndex);
			if (NULL != pcTag)
			{
				return pcTag;
			}
		}
	}

	return (m_paszLateArtDefineTags) ? m_paszLateArtDefineTags[i] : NULL;
}

void CvUnitInfo::setLateArtDefineTag(int i, const char* szVal)
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);
	m_paszLateArtDefineTags[i] = szVal;
}

const char* CvUnitInfo::getMiddleArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);

	if (NO_UNIT_ARTSTYLE != eStyle)
	{
		const int iIndex = GC.getInfoTypeForString(getType());
		if (-1 != iIndex)
		{
			const char* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getMiddleArtDefineTag(i, iIndex);
			if (NULL != pcTag)
			{
				return pcTag;
			}
		}
	}

	return (m_paszMiddleArtDefineTags) ? m_paszMiddleArtDefineTags[i] : NULL;
}

void CvUnitInfo::setMiddleArtDefineTag(int i, const char* szVal)
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);
	m_paszMiddleArtDefineTags[i] = szVal;
}

const char* CvUnitInfo::getUnitNames(int i) const
{
	FASSERT_BOUNDS(0, getNumUnitNames(), i);
	return (m_paszUnitNames) ? m_paszUnitNames[i] : NULL;
}

const char* CvUnitInfo::getFormationType() const
{
	return m_szFormationType;
}

const char* CvUnitInfo::getButton() const
{
	return m_szArtDefineButton;
}

void CvUnitInfo::updateArtDefineButton()
{
	m_szArtDefineButton = getArtInfo(0, NO_ERA, NO_UNIT_ARTSTYLE)->getButton();
}

const char* CvUnitInfo::getClassicalArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);

	if (NO_UNIT_ARTSTYLE != eStyle)
	{
		const int iIndex = GC.getInfoTypeForString(getType());
		if (-1 != iIndex)
		{
			const char* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getClassicalArtDefineTag(i, iIndex);
			if (NULL != pcTag)
			{
				return pcTag;
			}
		}
	}

	return (m_paszClassicalArtDefineTags) ? m_paszClassicalArtDefineTags[i] : NULL;
}

void CvUnitInfo::setClassicalArtDefineTag(int i, const char* szVal)
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);
	m_paszClassicalArtDefineTags[i] = szVal;
}

const char* CvUnitInfo::getRennArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);

	if (NO_UNIT_ARTSTYLE != eStyle)
	{
		const int iIndex = GC.getInfoTypeForString(getType());
		if (-1 != iIndex)
		{
			const char* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getRennArtDefineTag(i, iIndex);
			if (NULL != pcTag)
			{
				return pcTag;
			}
		}
	}

	return (m_paszRennArtDefineTags) ? m_paszRennArtDefineTags[i] : NULL;
}

void CvUnitInfo::setRennArtDefineTag(int i, const char* szVal)
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);
	m_paszRennArtDefineTags[i] = szVal;
}

const char* CvUnitInfo::getIndustrialArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);

	if (NO_UNIT_ARTSTYLE != eStyle)
	{
		const int iIndex = GC.getInfoTypeForString(getType());
		if (-1 != iIndex)
		{
			const char* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getIndustrialArtDefineTag(i, iIndex);
			if (NULL != pcTag)
			{
				return pcTag;
			}
		}
	}

	return (m_paszIndustrialArtDefineTags) ? m_paszIndustrialArtDefineTags[i] : NULL;
}

void CvUnitInfo::setIndustrialArtDefineTag(int i, const char* szVal)
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);
	m_paszIndustrialArtDefineTags[i] = szVal;
}

const char* CvUnitInfo::getFutureArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);

	if (NO_UNIT_ARTSTYLE != eStyle)
	{
		const int iIndex = GC.getInfoTypeForString(getType());
		if (-1 != iIndex)
		{
			const char* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getFutureArtDefineTag(i, iIndex);
			if (NULL != pcTag)
			{
				return pcTag;
			}
		}
	}

	return (m_paszFutureArtDefineTags) ? m_paszFutureArtDefineTags[i] : NULL;
}

void CvUnitInfo::setFutureArtDefineTag(int i, const char* szVal)
{
	FASSERT_BOUNDS(0, getGroupDefinitions(), i);
	m_paszFutureArtDefineTags[i] = szVal;
}

const CvArtInfoUnit* CvUnitInfo::getArtInfo(int i, EraTypes eEra, UnitArtStyleTypes eStyle) const
{
	if ((eEra > 8) && (getFutureArtDefineTag(i, eStyle) != NULL) && !CvString(getFutureArtDefineTag(i, eStyle)).empty())
	{
		return ARTFILEMGR.getUnitArtInfo(getFutureArtDefineTag(i, eStyle));
	}
	else if ((eEra > 5) && (getLateArtDefineTag(i, eStyle) != NULL) && !CvString(getLateArtDefineTag(i, eStyle)).empty())
	{
		return ARTFILEMGR.getUnitArtInfo(getLateArtDefineTag(i, eStyle));
	}
	else if ((eEra > 4) && (getIndustrialArtDefineTag(i, eStyle) != NULL) && !CvString(getIndustrialArtDefineTag(i, eStyle)).empty())
	{
		return ARTFILEMGR.getUnitArtInfo(getIndustrialArtDefineTag(i, eStyle));
	}
	else if ((eEra > 3) && (getRennArtDefineTag(i, eStyle) != NULL) && !CvString(getRennArtDefineTag(i, eStyle)).empty())
	{
		return ARTFILEMGR.getUnitArtInfo(getRennArtDefineTag(i, eStyle));
	}
	else if ((eEra > 2) && (getMiddleArtDefineTag(i, eStyle) != NULL) && !CvString(getMiddleArtDefineTag(i, eStyle)).empty())
	{
		return ARTFILEMGR.getUnitArtInfo(getMiddleArtDefineTag(i, eStyle));
	}
	else if ((eEra > 1) && (getClassicalArtDefineTag(i, eStyle) != NULL) && !CvString(getClassicalArtDefineTag(i, eStyle)).empty())
	{
		return ARTFILEMGR.getUnitArtInfo(getClassicalArtDefineTag(i, eStyle));
	}
	else
	{
		return ARTFILEMGR.getUnitArtInfo(getEarlyArtDefineTag(i, eStyle));
	}
}

void CvUnitInfo::setPowerValue(int iNewValue)
{
	m_iPowerValue = iNewValue;
}

int CvUnitInfo::getPrereqVicinityBonus() const
{
	return m_iPrereqVicinityBonus;
}

int CvUnitInfo::getBaseFoodChange() const
{
	return m_iBaseFoodChange;
}

bool CvUnitInfo::isWorkerTrade() const
{
	return m_bWorkerTrade;
}

bool CvUnitInfo::isMilitaryTrade() const
{
	return m_bMilitaryTrade;
}

bool CvUnitInfo::isForceUpgrade() const
{
	return m_bForceUpgrade;
}

bool CvUnitInfo::isGreatGeneral() const
{
	return m_bGreatGeneral;
}

bool CvUnitInfo::isSlave() const
{
	return m_bSlave;
}

int CvUnitInfo::getControlPoints() const
{
	return m_iControlPoints;
}

int CvUnitInfo::getCommandRange() const
{
	return m_iCommandRange;
}

bool CvUnitInfo::isRequiresStateReligionInCity() const
{
	return m_bRequiresStateReligionInCity;
}

bool CvUnitInfo::getPassableRouteNeeded(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumRouteInfos(), i);
	return algo::any_of_equal(m_aePassableRouteNeeded, static_cast<RouteTypes>(i));
}

const std::vector<BonusTypes>& CvUnitInfo::getPrereqOrVicinityBonuses() const
{
	return m_piPrereqOrVicinityBonuses;
}

int CvUnitInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}

int CvUnitInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}

bool CvUnitInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}

CvWString CvUnitInfo::getCivilizationName(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCivilizationInfos(), i);
	return m_paszCivilizationNames[i];
}

int CvUnitInfo::getCivilizationNamesVectorSize() const					{return m_aszCivilizationNamesforPass3.size();}
CvWString CvUnitInfo::getCivilizationNamesNamesVectorElement(int i) const	{return m_aszCivilizationNamesforPass3[i];}
CvWString CvUnitInfo::getCivilizationNamesValuesVectorElement(int i) const		{return m_aszCivilizationNamesValueforPass3[i];}


//TB Combat Mods Start  TB SubCombat Mod begin
//Functions
int CvUnitInfo::getEraInfo() const
{
	PROFILE_EXTRA_FUNC();
	if (getPrereqAndTech() != NO_TECH)
	{
		return GC.getTechInfo((TechTypes)getPrereqAndTech()).getEra();
	}
	TechTypes eHighestTech = NO_TECH;
	foreach_(const TechTypes ePrereqTech, getPrereqAndTechs())
	{
		if (eHighestTech == NO_TECH
		|| GC.getTechInfo(ePrereqTech).getEra() > GC.getTechInfo(eHighestTech).getEra())
		{
			eHighestTech = ePrereqTech;
		}
	}
	if (eHighestTech != NO_TECH)
	{
		return GC.getTechInfo(eHighestTech).getEra();
	}
	return NO_ERA;
}

//integers
int CvUnitInfo::getAttackCombatModifier() const
{
	return m_iAttackCombatModifier;
}

int CvUnitInfo::getDefenseCombatModifier() const
{
	return m_iDefenseCombatModifier;
}

int CvUnitInfo::getVSBarbs() const
{
	return m_iVSBarbs;
}


int CvUnitInfo::getUnnerve() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iUnnerve;
}

int CvUnitInfo::getEnclose() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iEnclose;
}

int CvUnitInfo::getLunge() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iLunge;
}

int CvUnitInfo::getDynamicDefense() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iDynamicDefense;
}

int CvUnitInfo::getEndurance() const
{
	return m_iEndurance;
}


int CvUnitInfo::getPoisonProbabilityModifier() const
{
	return m_iPoisonProbabilityModifier;
}

int CvUnitInfo::getCaptureProbabilityModifier() const
{
	return m_iCaptureProbabilityModifier;
}

int CvUnitInfo::getCaptureResistanceModifier() const
{
	return m_iCaptureResistanceModifier;
}

int CvUnitInfo::getHillsWorkModifier() const
{
	return m_iHillsWorkModifier;
}

int CvUnitInfo::getPeaksWorkModifier() const
{
	return m_iPeaksWorkModifier;
}

int CvUnitInfo::getBreakdownChance() const
{
	return m_iBreakdownChance;
}

int CvUnitInfo::getBreakdownDamage() const
{
	return m_iBreakdownDamage;
}

int CvUnitInfo::getTaunt() const
{
	return m_iTaunt;
}

int CvUnitInfo::getMaxHP(bool bForLoad) const
{
	if (!bForLoad && !GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 100;
	}
	if (m_iMaxHP == 0)
	{
		return 100;
	}
	return m_iMaxHP;
}

int CvUnitInfo::getDamageModifier() const
{
	return m_iDamageModifier;
}

int CvUnitInfo::getRBombardDamage() const
{
	return m_iRBombardDamage;
}

int CvUnitInfo::getRBombardDamageLimit() const
{
	return m_iRBombardDamageLimit;
}

int CvUnitInfo::getRBombardDamageMaxUnits() const
{
	return m_iRBombardDamageMaxUnits;
}

int CvUnitInfo::getCombatModifierPerSizeMore() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerSizeMore;
}

int CvUnitInfo::getCombatModifierPerSizeLess() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerSizeLess;
}

int CvUnitInfo::getCombatModifierPerVolumeMore() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerVolumeMore;
}

int CvUnitInfo::getCombatModifierPerVolumeLess() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerVolumeLess;
}

int CvUnitInfo::getSelfHealModifier() const
{
	return m_iSelfHealModifier;
}

int CvUnitInfo::getNumHealSupport() const
{
	return m_iNumHealSupport;
}

int CvUnitInfo::getInsidiousness() const
{
	return m_iInsidiousness;
}

int CvUnitInfo::getInvestigation() const
{
	return m_iInvestigation;
}

int CvUnitInfo::getStealthStrikes() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	return m_iStealthStrikes;
}

int CvUnitInfo::getStealthCombatModifier() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	return m_iStealthCombatModifier;
}

int CvUnitInfo::getTrapDamageMax() const
{
	return m_iTrapDamageMax;
}

int CvUnitInfo::getTrapDamageMin() const
{
	return m_iTrapDamageMin;
}

int CvUnitInfo::getTrapComplexity() const
{
	return m_iTrapComplexity;
}

int CvUnitInfo::getNumTriggers() const
{
	return m_iNumTriggers;
}

int CvUnitInfo::getAggression() const
{
	return m_iAggression;
}

int CvUnitInfo::getAnimalIgnoresBorders() const
{
	return m_iAnimalIgnoresBorders;
}

int CvUnitInfo::getReligiousCombatModifier() const
{
	return m_iReligiousCombatModifier;
}

//booleans
bool CvUnitInfo::isStampede() const
{
	return m_bStampede;
}

bool CvUnitInfo::isOnslaught() const
{
	return m_bOnslaught;
}


bool CvUnitInfo::isAttackOnlyCities() const
{
	return m_bAttackOnlyCities;
}

bool CvUnitInfo::isIgnoreNoEntryLevel() const
{
	return m_bIgnoreNoEntryLevel;
}

bool CvUnitInfo::isIgnoreZoneofControl() const
{
	return m_bIgnoreZoneofControl;
}

bool CvUnitInfo::isFliesToMove() const
{
	return m_bFliesToMove;
}

bool CvUnitInfo::isRBombardForceAbility() const
{
	return m_bRBombardForceAbility;
}

bool CvUnitInfo::isNoSelfHeal() const
{
	return m_bNoSelfHeal;
}

bool CvUnitInfo::isExcile() const
{
	return m_bExcile;
}

bool CvUnitInfo::isPassage() const
{
	return m_bPassage;
}

bool CvUnitInfo::isNoNonOwnedCityEntry() const
{
	return m_bNoNonOwnedCityEntry;
}

bool CvUnitInfo::isBarbCoExist() const
{
	return m_bBarbCoExist;
}

bool CvUnitInfo::isBlendIntoCity() const
{
	return m_bBlendIntoCity;
}

bool CvUnitInfo::isUpgradeAnywhere() const
{
	return m_bUpgradeAnywhere;
}

bool CvUnitInfo::isAssassin() const
{
	return m_bAssassin;
}

bool CvUnitInfo::isStealthDefense() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	return m_bStealthDefense;
}

bool CvUnitInfo::isNoInvisibility() const
{
	return m_bNoInvisibility;
}

bool CvUnitInfo::isTriggerBeforeAttack() const
{
	return m_bTriggerBeforeAttack;
}

bool CvUnitInfo::isAnimal() const
{
	return hasUnitCombat(GC.getUNITCOMBAT_ANIMAL());
}

bool CvUnitInfo::isWildAnimal() const
{
	return hasUnitCombat(GC.getUNITCOMBAT_WILD());
}

bool CvUnitInfo::canAnimalIgnoresBorders() const
{
	return m_bCanAnimalIgnoresBorders;
}

bool CvUnitInfo::canAnimalIgnoresImprovements() const
{
	return m_bCanAnimalIgnoresImprovements;
}

bool CvUnitInfo::canAnimalIgnoresCities() const
{
	return m_bCanAnimalIgnoresCities;
}

bool CvUnitInfo::isNoNonTypeProdMods() const
{
	return m_bNoNonTypeProdMods;
}

bool CvUnitInfo::isGatherHerd() const
{
	return m_bGatherHerd;
}

//boolean vectors without delayed resolution
UnitCombatTypes CvUnitInfo::getSubCombatType(int i) const
{
	return m_aiSubCombatTypes[i];
}

int CvUnitInfo::getNumSubCombatTypes() const
{
	return (int)m_aiSubCombatTypes.size();
}

bool CvUnitInfo::isSubCombatType(UnitCombatTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), e);
	return algo::any_of_equal(m_aiSubCombatTypes, e);
}

const std::vector<UnitCombatTypes>& CvUnitInfo::getSubCombatTypes() const
{
	return m_aiSubCombatTypes;
}

int CvUnitInfo::getHealAsType(int i) const
{
	return m_aiHealAsTypes[i];
}

int CvUnitInfo::getNumHealAsTypes() const
{
	return (int)m_aiHealAsTypes.size();
}

bool CvUnitInfo::isHealAsType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiHealAsTypes, i);
}

bool CvUnitInfo::isTerrainImpassableType(TerrainTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), e);
	return algo::any_of_equal(m_vTerrainImpassableTypes, e);
}

bool CvUnitInfo::isFeatureImpassableType(FeatureTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), e);
	return algo::any_of_equal(m_vFeatureImpassableTypes, e);
}

int CvUnitInfo::getTrapSetWithPromotionType(int i) const
{
	return m_aiTrapSetWithPromotionTypes[i];
}

int CvUnitInfo::getNumTrapSetWithPromotionTypes() const
{
	return (int)m_aiTrapSetWithPromotionTypes.size();
}

bool CvUnitInfo::isTrapSetWithPromotionType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), i);
	return algo::any_of_equal(m_aiTrapSetWithPromotionTypes, i);
}

int CvUnitInfo::getTrapImmunityUnitCombatType(int i) const
{
	return m_aiTrapImmunityUnitCombatTypes[i];
}

int CvUnitInfo::getNumTrapImmunityUnitCombatTypes() const
{
	return (int)m_aiTrapImmunityUnitCombatTypes.size();
}

bool CvUnitInfo::isTrapImmunityUnitCombatType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiTrapImmunityUnitCombatTypes, i);
}
//struct vectors
int CvUnitInfo::getNumHealUnitCombatTypes() const
{
	return (int)m_aHealUnitCombatTypes.size();
}


const HealUnitCombat& CvUnitInfo::getHealUnitCombatType(int iUnitCombat) const
{
	FASSERT_BOUNDS(0, getNumHealUnitCombatTypes(), iUnitCombat);
	return m_aHealUnitCombatTypes[iUnitCombat];
}

int CvUnitInfo::getNumGroupSpawnUnitCombatTypes() const
{
	return (int)m_aGroupSpawnUnitCombatTypes.size();
}

const GroupSpawnUnitCombat& CvUnitInfo::getGroupSpawnUnitCombatType(int iIndex) const
{
	return m_aGroupSpawnUnitCombatTypes[iIndex];
}

int CvUnitInfo::getNumInvisibleTerrainChanges() const
{
	return (int)m_aInvisibleTerrainChanges.size();
}

const InvisibleTerrainChanges& CvUnitInfo::getInvisibleTerrainChange(int iIndex) const
{
	return m_aInvisibleTerrainChanges[iIndex];
}

int CvUnitInfo::getNumInvisibleFeatureChanges() const
{
	return (int)m_aInvisibleFeatureChanges.size();
}

const InvisibleFeatureChanges& CvUnitInfo::getInvisibleFeatureChange(int iIndex) const
{
	return m_aInvisibleFeatureChanges[iIndex];
}

int CvUnitInfo::getNumInvisibleImprovementChanges() const
{
	return (int)m_aInvisibleImprovementChanges.size();
}

const InvisibleImprovementChanges& CvUnitInfo::getInvisibleImprovementChange(int iIndex) const
{
	return m_aInvisibleImprovementChanges[iIndex];
}

int CvUnitInfo::getNumVisibleTerrainChanges() const
{
	return (int)m_aVisibleTerrainChanges.size();
}

const InvisibleTerrainChanges& CvUnitInfo::getVisibleTerrainChange(int iIndex) const
{
	return m_aVisibleTerrainChanges[iIndex];
}

int CvUnitInfo::getNumVisibleFeatureChanges() const
{
	return (int)m_aVisibleFeatureChanges.size();
}

const InvisibleFeatureChanges& CvUnitInfo::getVisibleFeatureChange(int iIndex) const
{
	return m_aVisibleFeatureChanges[iIndex];
}

int CvUnitInfo::getNumVisibleImprovementChanges() const
{
	return (int)m_aVisibleImprovementChanges.size();
}

const InvisibleImprovementChanges& CvUnitInfo::getVisibleImprovementChange(int iIndex) const
{
	return m_aVisibleImprovementChanges[iIndex];
}

int CvUnitInfo::getNumVisibleTerrainRangeChanges() const
{
	return (int)m_aVisibleTerrainRangeChanges.size();
}

const InvisibleTerrainChanges& CvUnitInfo::getVisibleTerrainRangeChange(int iIndex) const
{
	return m_aVisibleTerrainRangeChanges[iIndex];
}

int CvUnitInfo::getNumVisibleFeatureRangeChanges() const
{
	return (int)m_aVisibleFeatureRangeChanges.size();
}

const InvisibleFeatureChanges& CvUnitInfo::getVisibleFeatureRangeChange(int iIndex) const
{
	return m_aVisibleFeatureRangeChanges[iIndex];
}

int CvUnitInfo::getNumVisibleImprovementRangeChanges() const
{
	return (int)m_aVisibleImprovementRangeChanges.size();
}

const InvisibleImprovementChanges& CvUnitInfo::getVisibleImprovementRangeChange(int iIndex) const
{
	return m_aVisibleImprovementRangeChanges[iIndex];
}

// bool vector utilizing delayed resolution
int CvUnitInfo::getNumEnabledCivilizationTypes() const
{
	return (int)m_aEnabledCivilizationTypes.size();
}

const EnabledCivilizations& CvUnitInfo::getEnabledCivilizationType(int iIndex) const
{
	return m_aEnabledCivilizationTypes[iIndex];
}

// int vectors utilizing pairing without delayed resolution
int CvUnitInfo::getNumFlankingStrikesbyUnitCombatTypes() const
{
	return m_aFlankingStrengthbyUnitCombatType.size();
}

int CvUnitInfo::getFlankingStrengthbyUnitCombatType(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aFlankingStrengthbyUnitCombatType.begin(); it != m_aFlankingStrengthbyUnitCombatType.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isFlankingStrikebyUnitCombatType(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aFlankingStrengthbyUnitCombatType.begin(); it != m_aFlankingStrengthbyUnitCombatType.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return true;
		}
	}
	return false;
}

int CvUnitInfo::getNumTrapDisableUnitCombatTypes() const
{
	return m_aTrapDisableUnitCombatTypes.size();
}

int CvUnitInfo::getTrapDisableUnitCombatType(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aTrapDisableUnitCombatTypes.begin(); it != m_aTrapDisableUnitCombatTypes.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isTrapDisableUnitCombatType(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aTrapDisableUnitCombatTypes.begin(); it != m_aTrapDisableUnitCombatTypes.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return true;
		}
	}
	return false;
}

const UnitCombatModifierArray& CvUnitInfo::getTrapDisableUnitCombatTypes() const
{
	return m_aTrapDisableUnitCombatTypes;
}

int CvUnitInfo::getNumTrapAvoidanceUnitCombatTypes() const
{
	return m_aTrapAvoidanceUnitCombatTypes.size();
}

int CvUnitInfo::getTrapAvoidanceUnitCombatType(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aTrapAvoidanceUnitCombatTypes.begin(); it != m_aTrapAvoidanceUnitCombatTypes.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isTrapAvoidanceUnitCombatType(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aTrapAvoidanceUnitCombatTypes.begin(); it != m_aTrapAvoidanceUnitCombatTypes.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return true;
		}
	}
	return false;
}

const UnitCombatModifierArray& CvUnitInfo::getTrapAvoidanceUnitCombatTypes() const
{
	return m_aTrapAvoidanceUnitCombatTypes;
}

int CvUnitInfo::getNumTrapTriggerUnitCombatTypes() const
{
	return m_aTrapTriggerUnitCombatTypes.size();
}

int CvUnitInfo::getTrapTriggerUnitCombatType(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aTrapTriggerUnitCombatTypes.begin(); it != m_aTrapTriggerUnitCombatTypes.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isTrapTriggerUnitCombatType(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aTrapTriggerUnitCombatTypes.begin(); it != m_aTrapTriggerUnitCombatTypes.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return true;
		}
	}
	return false;
}

const UnitCombatModifierArray& CvUnitInfo::getTrapTriggerUnitCombatTypes() const
{
	return m_aTrapTriggerUnitCombatTypes;
}

int CvUnitInfo::getNumVisibilityIntensityTypes() const
{
	return m_aVisibilityIntensityTypes.size();
}

int CvUnitInfo::getVisibilityIntensityType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensityTypes.begin(); it != m_aVisibilityIntensityTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isVisibilityIntensityType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensityTypes.begin(); it != m_aVisibilityIntensityTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return true;
		}
	}
	return false;
}

const InvisibilityArray& CvUnitInfo::getVisibilityIntensityTypes() const
{
	return m_aVisibilityIntensityTypes;
}

int CvUnitInfo::getNumInvisibilityIntensityTypes() const
{
	return m_aInvisibilityIntensityTypes.size();
}

int CvUnitInfo::getInvisibilityIntensityType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aInvisibilityIntensityTypes.begin(); it != m_aInvisibilityIntensityTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isInvisibilityIntensityType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aInvisibilityIntensityTypes.begin(); it != m_aInvisibilityIntensityTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return true;
		}
	}
	return false;
}

const InvisibilityArray& CvUnitInfo::getInvisibilityIntensityTypes() const
{
	return m_aInvisibilityIntensityTypes;
}

int CvUnitInfo::getNumVisibilityIntensityRangeTypes() const
{
	return m_aVisibilityIntensityRangeTypes.size();
}

int CvUnitInfo::getVisibilityIntensityRangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensityRangeTypes.begin(); it != m_aVisibilityIntensityRangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isVisibilityIntensityRangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensityRangeTypes.begin(); it != m_aVisibilityIntensityRangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return true;
		}
	}
	return false;
}

const InvisibilityArray& CvUnitInfo::getVisibilityIntensityRangeTypes() const
{
	return m_aVisibilityIntensityRangeTypes;
}

int CvUnitInfo::getNumTerrainWorkRateModifierTypes() const
{
	return m_aTerrainWorkRateModifierTypes.size();
}

int CvUnitInfo::getTerrainWorkRateModifierType(int iTerrain) const
{
	PROFILE_EXTRA_FUNC();
	for (TerrainModifierArray::const_iterator it = m_aTerrainWorkRateModifierTypes.begin(); it != m_aTerrainWorkRateModifierTypes.end(); ++it)
	{
		if ((*it).first == (TerrainTypes)iTerrain)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isTerrainWorkRateModifierType(int iTerrain) const
{
	PROFILE_EXTRA_FUNC();
	for (TerrainModifierArray::const_iterator it = m_aTerrainWorkRateModifierTypes.begin(); it != m_aTerrainWorkRateModifierTypes.end(); ++it)
	{
		if ((*it).first == (TerrainTypes)iTerrain)
		{
			return true;
		}
	}
	return false;
}

int CvUnitInfo::getNumFeatureWorkRateModifierTypes() const
{
	return m_aFeatureWorkRateModifierTypes.size();
}

int CvUnitInfo::getFeatureWorkRateModifierType(int iFeature) const
{
	PROFILE_EXTRA_FUNC();
	for (FeatureModifierArray::const_iterator it = m_aFeatureWorkRateModifierTypes.begin(); it != m_aFeatureWorkRateModifierTypes.end(); ++it)
	{
		if ((*it).first == (FeatureTypes)iFeature)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isFeatureWorkRateModifierType(int iFeature) const
{
	PROFILE_EXTRA_FUNC();
	for (FeatureModifierArray::const_iterator it = m_aFeatureWorkRateModifierTypes.begin(); it != m_aFeatureWorkRateModifierTypes.end(); ++it)
	{
		if ((*it).first == (FeatureTypes)iFeature)
		{
			return true;
		}
	}
	return false;
}

int CvUnitInfo::getNumBuildWorkRateModifierTypes() const
{
	return m_aBuildWorkRateModifierTypes.size();
}

int CvUnitInfo::getBuildWorkRateModifierType(int iBuild) const
{
	PROFILE_EXTRA_FUNC();
	for (BuildModifierArray::const_iterator it = m_aBuildWorkRateModifierTypes.begin(); it != m_aBuildWorkRateModifierTypes.end(); ++it)
	{
		if ((*it).first == (BuildTypes)iBuild)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitInfo::isBuildWorkRateModifierType(int iBuild) const
{
	PROFILE_EXTRA_FUNC();
	for (BuildModifierArray::const_iterator it = m_aBuildWorkRateModifierTypes.begin(); it != m_aBuildWorkRateModifierTypes.end(); ++it)
	{
		if ((*it).first == (BuildTypes)iBuild)
		{
			return true;
		}
	}
	return false;
}

//TB Combat Mods End  TB SubCombat Mod end

// #304 (#196): every wrapper-expressible XML field is declared here; init/read/copyNonDefaults
// are derived from this declaration. getCheckSum() is deliberately NOT derived from it -- see
// the comment in getCheckSum(). Any field added here MUST also get an explicit CheckSum line there.
//
// The hand-written remainder in read()/copyNonDefaults(), grouped by blocking reason:
//  - CvWString scalar (no wrapper):                m_szExtraHoverTextKey
//  - load-bearing non-wrapper default triple:      m_iDefaultUnitAIType (ctor NO_UNITAI / read
//    "UNITAI_UNKNOWN" / copy-compare UNITAI_UNKNOWN), m_szFormationType (copy-compare
//    "FORMATION_TYPE_DEFAULT")
//  - post-read clamp:                              m_iWithdrawalProbability, m_iBaseUpkeep
//  - bespoke parse (comma-list in one tag):        m_aiSeeInvisibleTypes
//  - SetVariableListTagPair dynamic int* arrays:   ReligionSpreads, CorporationSpreads, Flavors,
//    Terrain/Feature Attack+Defense, UnitCombatMods, UnitCombatCollateralImmunes, DomainMods,
//    BonusProductionModifiers, Terrain/FeaturePassableTechs (string->tech post-walk)
//  - delayed-resolution int FK vectors:            UnitTargets, DefendAgainstUnit,
//    SupersedingUnits, UnitUpgrades
//  - pair-vectors (SetOptionalPairVector):         FlankingStrikesbyUnitCombat, TrapDisable/
//    Avoidance/Trigger, Visibility/InvisibilityIntensity, Terrain/Feature/BuildWorkRateModifiers
//  - struct-vectors (CvStructs.h structs have no getDataMembers; some delayed-res / partial
//    checksum / CvWString member):                 HealUnitCombatTypes, GroupSpawnUnitCombatTypes,
//    the 9 In/Visible*Change(s) lists, EnabledCivilizationTypes
//  - order-sensitive bespoke art/audio walks:      UnitMeshGroups block (group sizes, waves,
//    floats, 7 art-define-tag arrays), UniqueNames
//  - readPass3 (leave alone):                      CivilizationNames
//  - outcome system (no wrapper):                  KillOutcomes, Actions (m_aOutcomeMissions)
void CvUnitInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.addEnum(m_eUnitCaptureType, L"Capture")
		.add(m_piFlankingStrikeUnit, L"FlankingStrikes")
		.add(m_piUnitAttackModifier, L"UnitAttackMods")
		.add(m_piUnitDefenseModifier, L"UnitDefenseMods")
		.add(m_aiUnitAIs, L"UnitAIs")
		.add(m_aiNotUnitAIs, L"NotUnitAIs")
		.add(m_aeTargetUnitCombat, L"UnitCombatTargets")
		.add(m_aeDefenderUnitCombat, L"UnitCombatDefenders")
		.add(m_aeGreatPeoples, L"GreatPeoples")
		.add(m_aePrereqOrCivics, L"PrereqOrCivics")
		.add(m_aeTerrainNative, L"TerrainNatives")
		.add(m_aeFeatureNative, L"FeatureNatives")
		.add(m_aeFreePromotions, L"FreePromotions")
		.add(m_aePassableRouteNeeded, L"PassableRouteNeededs")

		// --- #304 remainder migration: instance limits ---
		.add(m_iMaxGlobalInstances, L"iMaxGlobalInstances", -1)
		.add(m_iMaxPlayerInstances, L"iMaxPlayerInstances", -1)
		.add(m_bUnlimitedException, L"bUnlimitedException")
		.add(m_iInstanceCostModifier, L"iInstanceCostModifier")

		// --- immediate FK type indices (legacy int members; GetInfoClass reads) ---
		.addEnumAsInt(m_iSpecialUnitType, L"Special")
		.addEnumAsInt(m_iUnitCombatType, L"Combat")
		.addEnum(m_iDomainType, L"Domain")
		.addEnumAsInt(m_iInvisibleType, L"Invisible")
		.addEnumAsInt(m_iAdvisorType, L"Advisor")
		.addEnumAsInt(m_iMaxStartEra, L"MaxStartEra")
		.addEnumAsInt(m_iObsoleteTech, L"ObsoleteTech")
		.addEnumAsInt(m_iPrereqGameOption, L"PrereqGameOption")
		.addEnumAsInt(m_iNotGameOption, L"NotGameOption")
		.addEnumAsInt(m_iHolyCity, L"HolyCity")
		.addEnumAsInt(m_iReligionType, L"ReligionType")
		.addEnumAsInt(m_iStateReligion, L"StateReligion")
		.addEnumAsInt(m_iPrereqReligion, L"PrereqReligion")
		.addEnumAsInt(m_iPrereqCorporation, L"PrereqCorporation")
		.addEnumAsInt(m_iPrereqAndTech, L"PrereqTech")
		.addEnumAsInt(m_iPrereqAndBonus, L"BonusType")
		.addEnumAsInt(m_iSpecialCargo, L"SpecialCargo")
		.addEnumAsInt(m_iSMNotSpecialCargo, L"SMNotSpecialCargo")
		.addEnumAsInt(m_iDomainCargo, L"DomainCargo")
		.addEnumAsInt(m_iLeaderPromotion, L"LeaderPromotion")
		.addEnumAsInt(m_iPrereqVicinityBonus, L"VicinityBonusType")

		// --- bool abilities ---
		.add(m_bFoodProduction, L"bFood")
		.add(m_bNoBadGoodies, L"bNoBadGoodies")
		.add(m_bOnlyDefensive, L"bOnlyDefensive")
		.add(m_bNoCapture, L"bNoCapture")
		.add(m_bRivalTerritory, L"bRivalTerritory")
		.add(m_bMilitaryHappiness, L"bMilitaryHappiness")
		.add(m_bMilitarySupport, L"bMilitarySupport")
		.add(m_bMilitaryProduction, L"bMilitaryProduction")
		.add(m_bPillage, L"bPillage")
		.add(m_bSpy, L"bSpy")
		.add(m_bSabotage, L"bSabotage")
		.add(m_bDestroy, L"bDestroy")
		.add(m_bStealPlans, L"bStealPlans")
		.add(m_bInvestigate, L"bInvestigate")
		.add(m_bCounterSpy, L"bCounterSpy")
		.add(m_bFound, L"bFound")
		.add(m_bGoldenAge, L"bGoldenAge")
		.add(m_bInvisible, L"bInvisible")
		.add(m_bFirstStrikeImmune, L"bFirstStrikeImmune")
		.add(m_bNoDefensiveBonus, L"bNoDefensiveBonus")
		.add(m_bIgnoreBuildingDefense, L"bIgnoreBuildingDefense")
		.add(m_bCanMoveImpassable, L"bCanMoveImpassable")
		.add(m_bCanMoveAllTerrain, L"bCanMoveAllTerrain")
		.add(m_bFlatMovementCost, L"bFlatMovementCost")
		.add(m_bIgnoreTerrainCost, L"bIgnoreTerrainCost")
		.add(m_bNukeImmune, L"bNukeImmune")
		.add(m_bMechanized, L"bMechanized")
		.add(m_bRenderBelowWater, L"bRenderBelowWater")
		.add(m_bRenderAlways, L"bRenderAlways")
		.add(m_bSuicide, L"bSuicide")
		.add(m_bLineOfSight, L"bLineOfSight")
		.add(m_bHiddenNationality, L"bHiddenNationality")
		.add(m_bAlwaysHostile, L"bAlwaysHostile")
		.add(m_bFreeDrop, L"bFreeDrop")
		.add(m_bNoRevealMap, L"bNoRevealMap")
		.add(m_bInquisitor, L"bInquisitor")
		.add(m_bNoNonOwnedEntry, L"bOnlyFriendlyEntry")

		// --- prerequisite / membership vectors ---
		.add(m_pbBuildings, L"Buildings")
		.add(m_addHeritage, L"Heritage")
		.add(m_workerBuilds, L"Builds")
		.add(m_prereqOrHeritage, L"PrereqOrHeritage")
		.add(m_prereqAndHeritage, L"PrereqAndHeritage")
		.add(m_aiPrereqAndBuildings, L"PrereqAndBuildings")
		.add(m_aiPrereqOrBuildings, L"PrereqOrBuildings")
		.add(m_piPrereqAndTechs, L"TechTypes")
		.add(m_piPrereqOrBonuses, L"PrereqBonuses")
		.add(m_piPrereqOrVicinityBonuses, L"PrereqVicinityBonuses")
		.add(m_aiCategories, L"Categories")
		.add(m_bStateReligion, L"bStateReligion")

		// --- int scalars ---
		.add(m_iAIWeight, L"iAIWeight")
		.add(m_iProductionCost, L"iCost")
		.add(m_iHurryCostModifier, L"iHurryCostModifier")
		.add(m_iAdvancedStartCost, L"iAdvancedStartCost", 100)
		.add(m_iMinAreaSize, L"iMinAreaSize")
		.add(m_iMoves, L"iMoves")
		.add(m_iAirRange, L"iAirRange")
		.add(m_iAirUnitCap, L"iAirUnitCap")
		.add(m_iDropRange, L"iDropRange")
		.add(m_iNukeRange, L"iNukeRange", -1)
		.add(m_iWorkRate, L"iWorkRate")
		.add(m_iBaseDiscover, L"iBaseDiscover")
		.add(m_iDiscoverMultiplier, L"iDiscoverMultiplier")
		.add(m_iBaseHurry, L"iBaseHurry")
		.add(m_iHurryMultiplier, L"iHurryMultiplier")
		.add(m_iBaseTrade, L"iBaseTrade")
		.add(m_iTradeMultiplier, L"iTradeMultiplier")
		.add(m_iGreatWorkCulture, L"iGreatWorkCulture")
		.add(m_iEspionagePoints, L"iEspionagePoints")
		.add(m_iCombat, L"iCombat")
		.add(m_iCombatLimit, L"iCombatLimit", 100)
		.add(m_iAirCombat, L"iAirCombat")
		.add(m_iAirCombatLimit, L"iAirCombatLimit")
		.add(m_iXPValueAttack, L"iXPValueAttack")
		.add(m_iXPValueDefense, L"iXPValueDefense")
		.add(m_iFirstStrikes, L"iFirstStrikes")
		.add(m_iChanceFirstStrikes, L"iChanceFirstStrikes")
		.add(m_iInterceptionProbability, L"iInterceptionProbability")
		.add(m_iEvasionProbability, L"iEvasionProbability")
		.add(m_iCollateralDamage, L"iCollateralDamage")
		.add(m_iCollateralDamageLimit, L"iCollateralDamageLimit")
		.add(m_iCollateralDamageMaxUnits, L"iCollateralDamageMaxUnits")
		.add(m_iCityAttackModifier, L"iCityAttack")
		.add(m_iCityDefenseModifier, L"iCityDefense")
		.add(m_iAnimalCombatModifier, L"iAnimalCombat")
		.add(m_iHillsAttackModifier, L"iHillsAttack")
		.add(m_iHillsDefenseModifier, L"iHillsDefense")
		.add(m_iBombRate, L"iBombRate")
		.add(m_iBombardRate, L"iBombardRate")
		.add(m_iCargoSpace, L"iCargo")
		.add(m_iSMCargoSpace, L"iSMCargo")
		.add(m_iSMCargoVolume, L"iSMCargoVolume")
		.add(m_iConscriptionValue, L"iConscription")
		.add(m_iCultureGarrisonValue, L"iCultureGarrison")
		.add(m_iAssetValue, L"iAsset")
		.add(m_iPowerValue, L"iPower")
		.add(m_iLeaderExperience, L"iLeaderExperience")
		.add(m_iBaseFoodChange, L"iBaseFoodChange")
		.add(m_iControlPoints, L"iControlPoints")
		.add(m_iCommandRange, L"iCommandRange")

		// --- DCM ---
		.add(m_iDCMBombRange, L"iDCMBombRange")
		.add(m_iDCMBombAccuracy, L"iDCMBombAccuracy")
		.add(m_bDCMAirBomb1, L"bDCMAirBomb1")
		.add(m_bDCMAirBomb2, L"bDCMAirBomb2")
		.add(m_bDCMAirBomb3, L"bDCMAirBomb3")
		.add(m_bDCMAirBomb4, L"bDCMAirBomb4")
		.add(m_bDCMAirBomb5, L"bDCMAirBomb5")
		.add(m_bDCMFighterEngage, L"bDCMFighterEngage")

		// --- trade / role flags ---
		.add(m_bWorkerTrade, L"bWorkerTrade")
		.add(m_bMilitaryTrade, L"bMilitaryTrade")
		.add(m_bForceUpgrade, L"bForceUpgrade")
		.add(m_bGreatGeneral, L"bGreatGeneral")
		.add(m_bSlave, L"bSlave")
		.add(m_bRequiresStateReligionInCity, L"bRequiresStateReligionInCity")

		// --- self-contained sub-objects ---
		.add(m_PropertyManipulators)
		.addBoolExpr(m_pExprTrainCondition, L"TrainCondition")

		// --- TB Combat Mods ints ---
		.add(m_iAttackCombatModifier, L"iAttackCombatModifier")
		.add(m_iDefenseCombatModifier, L"iDefenseCombatModifier")
		.add(m_iVSBarbs, L"iVSBarbs")
		.add(m_iUnnerve, L"iUnnerve")
		.add(m_iEnclose, L"iEnclose")
		.add(m_iLunge, L"iLunge")
		.add(m_iDynamicDefense, L"iDynamicDefense")
		.add(m_iEndurance, L"iEndurance")
		.add(m_iPoisonProbabilityModifier, L"iPoisonProbabilityModifier")
		.add(m_iCaptureProbabilityModifier, L"iCaptureProbabilityModifier")
		.add(m_iCaptureResistanceModifier, L"iCaptureResistanceModifier")
		.add(m_iHillsWorkModifier, L"iHillsWorkModifier")
		.add(m_iPeaksWorkModifier, L"iPeaksWorkModifier")
		.add(m_iBreakdownChance, L"iBreakdownChance")
		.add(m_iBreakdownDamage, L"iBreakdownDamage")
		.add(m_iTaunt, L"iTaunt")
		.add(m_iMaxHP, L"iMaxHP", 100)
		.add(m_iDamageModifier, L"iDamageModifier")
		.add(m_iRBombardDamage, L"iRBombardDamage")
		.add(m_iRBombardDamageLimit, L"iRBombardDamageLimit")
		.add(m_iRBombardDamageMaxUnits, L"iRBombardDamageMaxUnits")
		.add(m_iCombatModifierPerSizeMore, L"iCombatModifierPerSizeMore")
		.add(m_iCombatModifierPerSizeLess, L"iCombatModifierPerSizeLess")
		.add(m_iCombatModifierPerVolumeMore, L"iCombatModifierPerVolumeMore")
		.add(m_iCombatModifierPerVolumeLess, L"iCombatModifierPerVolumeLess")
		.add(m_iSelfHealModifier, L"iSelfHealModifier")
		.add(m_iNumHealSupport, L"iNumHealSupport")
		.add(m_iInsidiousness, L"iInsidiousness")
		.add(m_iInvestigation, L"iInvestigation")
		.add(m_iStealthStrikes, L"iStealthStrikes")
		.add(m_iStealthCombatModifier, L"iStealthCombatModifier")
		.add(m_iTrapDamageMax, L"iTrapDamageMax")
		.add(m_iTrapDamageMin, L"iTrapDamageMin")
		.add(m_iTrapComplexity, L"iTrapComplexity")
		.add(m_iNumTriggers, L"iNumTriggers")
		.add(m_iAggression, L"iAggression", 5)
		.add(m_iAnimalIgnoresBorders, L"iAnimalIgnoresBorders")
		.add(m_iReligiousCombatModifier, L"iReligiousCombatModifier")

		// --- TB Combat Mods bools ---
		.add(m_bStampede, L"bStampede")
		.add(m_bOnslaught, L"bOnslaught")
		.add(m_bAttackOnlyCities, L"bAttackOnlyCities")
		.add(m_bIgnoreNoEntryLevel, L"bIgnoreNoEntryLevel")
		.add(m_bIgnoreZoneofControl, L"bIgnoreZoneofControl")
		.add(m_bFliesToMove, L"bFliesToMove")
		.add(m_bRBombardForceAbility, L"bRBombardForceAbility")
		.add(m_bNoSelfHeal, L"bNoSelfHeal")
		.add(m_bExcile, L"bExcile")
		.add(m_bPassage, L"bPassage")
		.add(m_bNoNonOwnedCityEntry, L"bNoNonOwnedCityEntry")
		.add(m_bBarbCoExist, L"bBarbCoExist")
		.add(m_bBlendIntoCity, L"bBlendIntoCity")
		.add(m_bUpgradeAnywhere, L"bUpgradeAnywhere")
		.add(m_bAssassin, L"bAssassin")
		.add(m_bStealthDefense, L"bStealthDefense")
		.add(m_bNoInvisibility, L"bNoInvisibility")
		.add(m_bTriggerBeforeAttack, L"bTriggerBeforeAttack")
		.add(m_bNoNonTypeProdMods, L"bNoNonTypeProdMods")
		.add(m_bGatherHerd, L"bGatherHerd")

		// --- TB Combat Mods membership vectors ---
		.add(m_aiSubCombatTypes, L"SubCombatTypes")
		.add(m_vTerrainImpassableTypes, L"TerrainImpassableTypes")
		.add(m_vFeatureImpassableTypes, L"FeatureImpassableTypes")
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")
		.add(m_aiTrapSetWithPromotionTypes, L"TrapSetWithPromotionTypes")
		.add(m_aiTrapImmunityUnitCombatTypes, L"TrapImmunityUnitCombatTypes")
	;
}

void CvUnitInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	// #304: this checksum deliberately stays FULLY hand-written instead of delegating to
	// CvInfoUtil(this).checkSum(). CheckSum() is a rotating (order-dependent) fold and the
	// legacy field order below interleaves declared and hand-written fields; delegating would
	// reorder the fold and break savegame asset-checksum parity for every existing save.
	// RULE: every field declared in getDataMembers() MUST keep an explicit CheckSum/CheckSumC
	// line here (CvString fields excepted - their wrapper checksum is a no-op).
	// The next block reproduces, in declaration order, the wrapper checksums of the members
	// that were already declarative before #304 (their fold position was the head of the sum).
	CheckSum(iSum, m_eUnitCaptureType);
	CheckSumC(iSum, m_piFlankingStrikeUnit);
	CheckSumC(iSum, m_piUnitAttackModifier);
	CheckSumC(iSum, m_piUnitDefenseModifier);
	CheckSumC(iSum, m_aiUnitAIs);
	CheckSumC(iSum, m_aiNotUnitAIs);
	CheckSumC(iSum, m_aeTargetUnitCombat);
	CheckSumC(iSum, m_aeDefenderUnitCombat);
	CheckSumC(iSum, m_aeGreatPeoples);
	CheckSumC(iSum, m_aePrereqOrCivics);
	CheckSumC(iSum, m_aeTerrainNative);
	CheckSumC(iSum, m_aeFeatureNative);
	CheckSumC(iSum, m_aeFreePromotions);
	CheckSumC(iSum, m_aePassableRouteNeeded);

	CheckSum(iSum, m_iMaxGlobalInstances);
	CheckSum(iSum, m_iMaxPlayerInstances);
	CheckSum(iSum, m_bUnlimitedException);
	CheckSum(iSum, m_iInstanceCostModifier);
	CheckSum(iSum, m_iDCMBombRange);
	CheckSum(iSum, m_iDCMBombAccuracy);
	CheckSum(iSum, m_bDCMAirBomb1);
	CheckSum(iSum, m_bDCMAirBomb2);
	CheckSum(iSum, m_bDCMAirBomb3);
	CheckSum(iSum, m_bDCMAirBomb4);
	CheckSum(iSum, m_bDCMAirBomb5);
	CheckSum(iSum, m_bDCMFighterEngage);

	CheckSum(iSum, m_iAIWeight);
	CheckSum(iSum, m_iProductionCost);
	CheckSum(iSum, m_iHurryCostModifier);
	CheckSum(iSum, m_iAdvancedStartCost);
	CheckSum(iSum, m_iMinAreaSize);
	CheckSum(iSum, m_iMoves);
	CheckSum(iSum, m_iAirRange);
	CheckSum(iSum, m_iAirUnitCap);
	CheckSum(iSum, m_iDropRange);
	CheckSum(iSum, m_iNukeRange);
	CheckSum(iSum, m_iWorkRate);
	CheckSum(iSum, m_iBaseDiscover);
	CheckSum(iSum, m_iDiscoverMultiplier);
	CheckSum(iSum, m_iBaseHurry);
	CheckSum(iSum, m_iHurryMultiplier);
	CheckSum(iSum, m_iBaseTrade);
	CheckSum(iSum, m_iTradeMultiplier);
	CheckSum(iSum, m_iGreatWorkCulture);
	CheckSum(iSum, m_iEspionagePoints);
	CheckSum(iSum, m_iCombat);
	CheckSum(iSum, m_iCombatLimit);
	CheckSum(iSum, m_iAirCombat);
	CheckSum(iSum, m_iAirCombatLimit);
	CheckSum(iSum, m_iXPValueAttack);
	CheckSum(iSum, m_iXPValueDefense);
	CheckSum(iSum, m_iFirstStrikes);
	CheckSum(iSum, m_iChanceFirstStrikes);
	CheckSum(iSum, m_iInterceptionProbability);
	//ls612: Advanced Nuke Interception
	//CheckSum(iSum, m_iNukeInterceptionProbability);
	//CheckSum(iSum, m_iNukeInterceptionRange);
	CheckSum(iSum, m_iEvasionProbability);
	CheckSum(iSum, m_iWithdrawalProbability);
	CheckSum(iSum, m_iCollateralDamage);
	CheckSum(iSum, m_iCollateralDamageLimit);
	CheckSum(iSum, m_iCollateralDamageMaxUnits);
	CheckSum(iSum, m_iCityAttackModifier);
	CheckSum(iSum, m_iCityDefenseModifier);
	CheckSum(iSum, m_iAnimalCombatModifier);
	CheckSum(iSum, m_iHillsAttackModifier);
	CheckSum(iSum, m_iHillsDefenseModifier);
	CheckSum(iSum, m_iBombRate);
	CheckSum(iSum, m_iBombardRate);
	CheckSum(iSum, m_iSpecialCargo);
	CheckSum(iSum, m_iSMNotSpecialCargo);
	CheckSum(iSum, m_iDomainCargo);
	CheckSum(iSum, m_iCargoSpace);
	CheckSum(iSum, m_iSMCargoSpace);
	CheckSum(iSum, m_iSMCargoVolume);
	CheckSum(iSum, m_iConscriptionValue);
	CheckSum(iSum, m_iCultureGarrisonValue);
	CheckSum(iSum, m_iBaseUpkeep);
	CheckSum(iSum, m_iAssetValue);
	CheckSum(iSum, m_iPowerValue);
	CheckSum(iSum, m_iSpecialUnitType);
	CheckSum(iSum, m_iUnitCombatType);
	CheckSum(iSum, m_iDomainType);
	CheckSum(iSum, m_iDefaultUnitAIType);
	CheckSum(iSum, m_iInvisibleType);

	CheckSumC(iSum, m_aiSeeInvisibleTypes);

	CheckSum(iSum, m_iAdvisorType);

	CheckSum(iSum, m_iMaxStartEra);
	CheckSum(iSum, m_iObsoleteTech);
	CheckSum(iSum, m_bStateReligion);
	CheckSum(iSum, m_iPrereqGameOption);
	CheckSum(iSum, m_iNotGameOption);

	CheckSum(iSum, m_iHolyCity);
	CheckSum(iSum, m_iReligionType);
	CheckSum(iSum, m_iStateReligion);
	CheckSum(iSum, m_iPrereqReligion);
	CheckSum(iSum, m_iPrereqCorporation);

	CheckSum(iSum, m_iPrereqAndTech);
	CheckSum(iSum, m_iPrereqAndBonus);
	CheckSum(iSum, m_iGroupSize);
	CheckSum(iSum, m_iGroupDefinitions);
	CheckSum(iSum, m_iUnitMeleeWaveSize);
	CheckSum(iSum, m_iUnitRangedWaveSize);
	CheckSum(iSum, m_iNumUnitNames);
	CheckSum(iSum, m_iCommandType);

	CheckSum(iSum, m_bFoodProduction);
	CheckSum(iSum, m_bNoBadGoodies);
	CheckSum(iSum, m_bOnlyDefensive);
	CheckSum(iSum, m_bNoCapture);
	CheckSum(iSum, m_bRivalTerritory);
	CheckSum(iSum, m_bMilitaryHappiness);
	CheckSum(iSum, m_bMilitarySupport);
	CheckSum(iSum, m_bMilitaryProduction);
	CheckSum(iSum, m_bPillage);
	CheckSum(iSum, m_bSpy);
	CheckSum(iSum, m_bSabotage);
	CheckSum(iSum, m_bDestroy);
	CheckSum(iSum, m_bStealPlans);
	CheckSum(iSum, m_bInvestigate);
	CheckSum(iSum, m_bCounterSpy);
	CheckSum(iSum, m_bFound);
	CheckSum(iSum, m_bGoldenAge);
	CheckSum(iSum, m_bInvisible);
	CheckSum(iSum, m_bFirstStrikeImmune);
	CheckSum(iSum, m_bNoDefensiveBonus);
	CheckSum(iSum, m_bIgnoreBuildingDefense);
	CheckSum(iSum, m_bCanMoveImpassable);
	CheckSum(iSum, m_bCanMoveAllTerrain);
	CheckSum(iSum, m_bFlatMovementCost);
	CheckSum(iSum, m_bIgnoreTerrainCost);
	CheckSum(iSum, m_bNukeImmune);
	CheckSum(iSum, m_bMechanized);
	CheckSum(iSum, m_bRenderBelowWater);
	CheckSum(iSum, m_bRenderAlways);
	CheckSum(iSum, m_bSuicide);
	CheckSum(iSum, m_bLineOfSight);
	CheckSum(iSum, m_bHiddenNationality);
	CheckSum(iSum, m_bAlwaysHostile);
	CheckSum(iSum, m_bFreeDrop);
	CheckSum(iSum, m_bNoRevealMap);
	CheckSum(iSum, m_bInquisitor);

	CheckSum(iSum, m_bNoNonOwnedEntry);

	//CheckSum(iSum, m_fUnitMaxSpeed);
	//CheckSum(iSum, m_fUnitPadTime);

	CheckSumC(iSum, m_piPrereqAndTechs);
	CheckSumC(iSum, m_piPrereqOrBonuses);
	CheckSumI(iSum, GC.getNumFlavorTypes(), m_piFlavorValue);
	CheckSumI(iSum, GC.getNumTerrainInfos(), m_piTerrainAttackModifier);
	CheckSumI(iSum, GC.getNumTerrainInfos(), m_piTerrainDefenseModifier);
	CheckSumI(iSum, GC.getNumFeatureInfos(), m_piFeatureAttackModifier);
	CheckSumI(iSum, GC.getNumFeatureInfos(), m_piFeatureDefenseModifier);
	CheckSumI(iSum, GC.getNumUnitCombatInfos(), m_piUnitCombatModifier);
	CheckSumI(iSum, GC.getNumUnitCombatInfos(), m_piUnitCombatCollateralImmune);
	CheckSumI(iSum, NUM_DOMAIN_TYPES, m_piDomainModifier);
	CheckSumI(iSum, GC.getNumBonusInfos(), m_piBonusProductionModifier);
	//CheckSumI(iSum, m_iGroupDefinitions, m_piUnitGroupRequired);

	CheckSumC(iSum, m_workerBuilds);
	CheckSumC(iSum, m_prereqOrHeritage);
	CheckSumC(iSum, m_prereqAndHeritage);
	CheckSumC(iSum, m_aiPrereqAndBuildings);
	CheckSumC(iSum, m_aiPrereqOrBuildings);

	CheckSumC(iSum, m_aiTargetUnit);
	CheckSumC(iSum, m_aiDefendAgainstUnit);
	CheckSumC(iSum, m_aiSupersedingUnits);
	CheckSumC(iSum, m_aiUnitUpgrades);
	CheckSumC(iSum, m_aiUnitUpgradeChain);

	CheckSumI(iSum, GC.getNumReligionInfos(), m_piReligionSpreads);
	CheckSumI(iSum, GC.getNumCorporationInfos(), m_piCorporationSpreads);
	CheckSumI(iSum, GC.getNumTerrainInfos(), m_piTerrainPassableTech);
	CheckSumI(iSum, GC.getNumFeatureInfos(), m_piFeaturePassableTech);

	CheckSumC(iSum, m_pbBuildings);
	CheckSumC(iSum, m_addHeritage);

	//CheckSumI(iSum, GC.getNumTerrainInfos(), m_pbTerrainImpassable);
	//CheckSumI(iSum, GC.getNumFeatureInfos(), m_pbFeatureImpassable);

	CheckSum(iSum, m_iLeaderPromotion);
	CheckSum(iSum, m_iLeaderExperience);

	CheckSum(iSum, m_bRequiresStateReligionInCity);
	CheckSum(iSum, m_bWorkerTrade);
	CheckSum(iSum, m_bMilitaryTrade);
	CheckSum(iSum, m_bForceUpgrade);
	CheckSum(iSum, m_bGreatGeneral);
	CheckSum(iSum, m_bSlave);
	CheckSum(iSum, m_iPrereqVicinityBonus);
	CheckSum(iSum, m_iBaseFoodChange);
	CheckSum(iSum, m_iControlPoints);
	CheckSum(iSum, m_iCommandRange);

	CheckSumC(iSum, m_piPrereqOrVicinityBonuses);
	CheckSumC(iSum, m_aiCategories);

	m_KillOutcomeList.getCheckSum(iSum);

	foreach_(const CvOutcomeMission* outcomeMission, m_aOutcomeMissions)
	{
		outcomeMission->getCheckSum(iSum);
	}

	m_PropertyManipulators.getCheckSum(iSum);

	if (m_pExprTrainCondition)
	{
		m_pExprTrainCondition->getCheckSum(iSum);
	}
	//TB Combat Mods Begin  TB SubCombat Mod begin
	//integers
	CheckSum(iSum, m_iAttackCombatModifier);
	CheckSum(iSum, m_iDefenseCombatModifier);
	CheckSum(iSum, m_iVSBarbs);
	CheckSum(iSum, m_iUnnerve);
	CheckSum(iSum, m_iEnclose);
	CheckSum(iSum, m_iLunge);
	CheckSum(iSum, m_iDynamicDefense);
	CheckSum(iSum, m_iEndurance);
	CheckSum(iSum, m_iPoisonProbabilityModifier);

	CheckSum(iSum, m_iCaptureProbabilityModifier);
	CheckSum(iSum, m_iCaptureResistanceModifier);

	CheckSum(iSum, m_iHillsWorkModifier);
	CheckSum(iSum, m_iPeaksWorkModifier);

	CheckSum(iSum, m_iBreakdownChance);
	CheckSum(iSum, m_iBreakdownDamage);
	CheckSum(iSum, m_iTaunt);
	CheckSum(iSum, m_iMaxHP);
	CheckSum(iSum, m_iDamageModifier);
	CheckSum(iSum, m_iTotalCombatStrengthModifierBase);
	CheckSum(iSum, m_iTotalCombatStrengthChangeBase);
	CheckSum(iSum, m_iBaseCargoVolume);
	CheckSum(iSum, m_iRBombardDamage);
	CheckSum(iSum, m_iRBombardDamageLimit);
	CheckSum(iSum, m_iRBombardDamageMaxUnits);
	CheckSum(iSum, m_iBaseGroupRank);
	CheckSum(iSum, m_iCombatModifierPerSizeMore);
	CheckSum(iSum, m_iCombatModifierPerSizeLess);
	CheckSum(iSum, m_iCombatModifierPerVolumeMore);
	CheckSum(iSum, m_iCombatModifierPerVolumeLess);
	CheckSum(iSum, m_iSelfHealModifier);
	CheckSum(iSum, m_iNumHealSupport);
	CheckSum(iSum, m_iInsidiousness);
	CheckSum(iSum, m_iInvestigation);
	CheckSum(iSum, m_iStealthStrikes);
	CheckSum(iSum, m_iStealthCombatModifier);
	CheckSum(iSum, m_iTrapDamageMax);
	CheckSum(iSum, m_iTrapDamageMin);
	CheckSum(iSum, m_iTrapComplexity);
	CheckSum(iSum, m_iNumTriggers);
	CheckSum(iSum, m_iAggression);
	CheckSum(iSum, m_iAnimalIgnoresBorders);
	CheckSum(iSum, m_iReligiousCombatModifier);
	//booleans
	CheckSum(iSum, m_bStampede);
	CheckSum(iSum, m_bOnslaught);
	CheckSum(iSum, m_bAttackOnlyCities);
	CheckSum(iSum, m_bIgnoreNoEntryLevel);
	CheckSum(iSum, m_bIgnoreZoneofControl);
	CheckSum(iSum, m_bFliesToMove);
	CheckSum(iSum, m_bRBombardForceAbility);
	CheckSum(iSum, m_bNoSelfHeal);
	CheckSum(iSum, m_bExcile);
	CheckSum(iSum, m_bPassage);
	CheckSum(iSum, m_bNoNonOwnedCityEntry);
	CheckSum(iSum, m_bBarbCoExist);
	CheckSum(iSum, m_bBlendIntoCity);
	CheckSum(iSum, m_bUpgradeAnywhere);
	CheckSum(iSum, m_bAssassin);
	CheckSum(iSum, m_bStealthDefense);
	CheckSum(iSum, m_bNoInvisibility);
	CheckSum(iSum, m_bTriggerBeforeAttack);
	CheckSum(iSum, m_bNoNonTypeProdMods);
	CheckSum(iSum, m_bGatherHerd);
	CheckSum(iSum, m_bCanMergeSplit);
	//boolean vectors without delayed resolution
	CheckSumC(iSum, m_aiSubCombatTypes);
	CheckSumC(iSum, m_aiHealAsTypes);
	CheckSumC(iSum, m_vTerrainImpassableTypes);
	CheckSumC(iSum, m_vFeatureImpassableTypes);
	CheckSumC(iSum, m_aeMapCategoryTypes);
	CheckSumC(iSum, m_aiTrapSetWithPromotionTypes);
	CheckSumC(iSum, m_aiTrapImmunityUnitCombatTypes);
	// int vectors utilizing struct with delayed resolution
	int iNumElements;
	iNumElements = m_aHealUnitCombatTypes.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aHealUnitCombatTypes[i].eUnitCombat);
		CheckSum(iSum, m_aHealUnitCombatTypes[i].iHeal);
	}

	iNumElements = m_aGroupSpawnUnitCombatTypes.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aGroupSpawnUnitCombatTypes[i].eUnitCombat);
		CheckSum(iSum, m_aGroupSpawnUnitCombatTypes[i].iChance);
	}

	iNumElements = m_aInvisibleTerrainChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aInvisibleTerrainChanges[i].eInvisible);
		CheckSum(iSum, m_aInvisibleTerrainChanges[i].eTerrain);
		CheckSum(iSum, m_aInvisibleTerrainChanges[i].iIntensity);
	}

	iNumElements = m_aInvisibleFeatureChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aInvisibleFeatureChanges[i].eInvisible);
		CheckSum(iSum, m_aInvisibleFeatureChanges[i].eFeature);
		CheckSum(iSum, m_aInvisibleFeatureChanges[i].iIntensity);
	}

	iNumElements = m_aInvisibleImprovementChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aInvisibleImprovementChanges[i].eInvisible);
		CheckSum(iSum, m_aInvisibleImprovementChanges[i].eImprovement);
		CheckSum(iSum, m_aInvisibleImprovementChanges[i].iIntensity);
	}

	iNumElements = m_aVisibleTerrainChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aVisibleTerrainChanges[i].eInvisible);
		CheckSum(iSum, m_aVisibleTerrainChanges[i].eTerrain);
		CheckSum(iSum, m_aVisibleTerrainChanges[i].iIntensity);
	}

	iNumElements = m_aVisibleFeatureChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aVisibleFeatureChanges[i].eInvisible);
		CheckSum(iSum, m_aVisibleFeatureChanges[i].eFeature);
		CheckSum(iSum, m_aVisibleFeatureChanges[i].iIntensity);
	}

	iNumElements = m_aVisibleImprovementChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aVisibleImprovementChanges[i].eInvisible);
		CheckSum(iSum, m_aVisibleImprovementChanges[i].eImprovement);
		CheckSum(iSum, m_aVisibleImprovementChanges[i].iIntensity);
	}

	iNumElements = m_aVisibleTerrainRangeChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aVisibleTerrainRangeChanges[i].eInvisible);
		CheckSum(iSum, m_aVisibleTerrainRangeChanges[i].eTerrain);
		CheckSum(iSum, m_aVisibleTerrainRangeChanges[i].iIntensity);
	}

	iNumElements = m_aVisibleFeatureRangeChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aVisibleFeatureRangeChanges[i].eInvisible);
		CheckSum(iSum, m_aVisibleFeatureRangeChanges[i].eFeature);
		CheckSum(iSum, m_aVisibleFeatureRangeChanges[i].iIntensity);
	}

	iNumElements = m_aVisibleImprovementRangeChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aVisibleImprovementRangeChanges[i].eInvisible);
		CheckSum(iSum, m_aVisibleImprovementRangeChanges[i].eImprovement);
		CheckSum(iSum, m_aVisibleImprovementRangeChanges[i].iIntensity);
	}

	iNumElements = m_aEnabledCivilizationTypes.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aEnabledCivilizationTypes[i].eCivilization);
	}
	// int vectors utilizing pairing without delayed resolution
	CheckSumC(iSum, m_aFlankingStrengthbyUnitCombatType);
	CheckSumC(iSum, m_aTrapDisableUnitCombatTypes);
	CheckSumC(iSum, m_aTrapAvoidanceUnitCombatTypes);
	CheckSumC(iSum, m_aTrapTriggerUnitCombatTypes);
	CheckSumC(iSum, m_aVisibilityIntensityTypes);
	CheckSumC(iSum, m_aInvisibilityIntensityTypes);
//Team Project (4)
		//WorkRateMod
	CheckSumC(iSum, m_aTerrainWorkRateModifierTypes);
	CheckSumC(iSum, m_aFeatureWorkRateModifierTypes);
	CheckSumC(iSum, m_aBuildWorkRateModifierTypes);
	//TB Combat Mods End  TB SubCombat Mod end

	CheckSum(iSum, m_szExtraHoverTextKey);
}

//
// read from xml
//
bool CvUnitInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	CvString szTextVal;
	CvString szTextVal2;

	int iIndexVal;

	// EXTRA HOVER TEXT - hand-written: CvWString member, no wrapper.
	pXML->GetOptionalChildXmlValByName(m_szExtraHoverTextKey, L"ExtraHoverText");

	// Hand-written: ctor/read/copy default triple (NO_UNITAI / "UNITAI_UNKNOWN" / UNITAI_UNKNOWN)
	// is not expressible by the enum wrapper (which defaults all three to -1).
	pXML->GetOptionalChildXmlValByName(szTextVal, L"DefaultUnitAI", "UNITAI_UNKNOWN");
	m_iDefaultUnitAIType = static_cast<UnitAITypes>(pXML->GetInfoClass(szTextVal));

	// Hand-written: bespoke comma-separated list inside a single tag.
	pXML->GetOptionalChildXmlValByName(szTextVal, L"SeeInvisible");
	std::vector<CvString> tokens;
	szTextVal.getTokens(",", tokens);
	for (int i=0;i<(int)tokens.size();i++)
	{
		const int iInvisibleType = pXML->GetInfoClass(tokens[i]);
		if(iInvisibleType != NO_INVISIBLE)
		{
			m_aiSeeInvisibleTypes.push_back(iInvisibleType);
		}
	}

	pXML->SetVariableListTagPair(&m_piReligionSpreads, L"ReligionSpreads", GC.getNumReligionInfos(),-1);
	pXML->SetVariableListTagPair(&m_piCorporationSpreads, L"CorporationSpreads", GC.getNumCorporationInfos(), -1);

	CvString* pszTemp = NULL;
	pXML->SetVariableListTagPair(&pszTemp, L"TerrainPassableTechs", GC.getNumTerrainInfos());
	if ( pszTemp != NULL )
	{
		m_piTerrainPassableTech = new int[GC.getNumTerrainInfos()];
		for (int i = 0; i < GC.getNumTerrainInfos(); ++i)
		{
			m_piTerrainPassableTech[i] = pszTemp[i].IsEmpty() ? NO_TECH : pXML->GetInfoClass(pszTemp[i]);
		}
		SAFE_DELETE_ARRAY(pszTemp);
	}
	else
	{
		m_piTerrainPassableTech = NULL;
	}

	pXML->SetVariableListTagPair(&pszTemp, L"FeaturePassableTechs", GC.getNumFeatureInfos());
	if ( pszTemp != NULL )
	{
		m_piFeaturePassableTech = new int[GC.getNumFeatureInfos()];
		for (int i = 0; i < GC.getNumFeatureInfos(); ++i)
		{
			m_piFeaturePassableTech[i] = pszTemp[i].IsEmpty() ? NO_TECH : pXML->GetInfoClass(pszTemp[i]);
		}
		SAFE_DELETE_ARRAY(pszTemp);
	}
	else
	{
		m_piFeaturePassableTech = NULL;
	}

	pXML->SetOptionalVectorWithDelayedResolution(m_aiTargetUnit, L"UnitTargets");
	pXML->SetOptionalVectorWithDelayedResolution(m_aiDefendAgainstUnit, L"DefendAgainstUnit");
	pXML->SetOptionalVectorWithDelayedResolution(m_aiSupersedingUnits, L"SupersedingUnits");
	pXML->SetOptionalVectorWithDelayedResolution(m_aiUnitUpgrades, L"UnitUpgrades");

	pXML->SetVariableListTagPair(&m_piFlavorValue, L"Flavors", GC.getNumFlavorTypes());

	//pXML->SetVariableListTagPair(&m_pbTerrainImpassable, L"TerrainImpassables", GC.getNumTerrainInfos(), false);
	//pXML->SetVariableListTagPair(&m_pbFeatureImpassable, L"FeatureImpassables", GC.getNumFeatureInfos(), false);

	//ls612: Advanced Nuke Interception
	//pXML->GetChildXmlValByName(&m_iNukeInterceptionProbability, L"iNukeInterceptionProbability");
	//pXML->GetChildXmlValByName(&m_iNukeInterceptionRange, L"iNukeInterceptionRange");

	// Hand-written: post-read clamp not expressible by the int wrapper.
	pXML->GetOptionalChildXmlValByName(&m_iWithdrawalProbability, L"iWithdrawalProb");
	if (m_iWithdrawalProbability < 0) m_iWithdrawalProbability = 0;

	pXML->SetVariableListTagPair(&m_piTerrainAttackModifier, L"TerrainAttacks", GC.getNumTerrainInfos());
	pXML->SetVariableListTagPair(&m_piTerrainDefenseModifier, L"TerrainDefenses", GC.getNumTerrainInfos());
	pXML->SetVariableListTagPair(&m_piFeatureAttackModifier, L"FeatureAttacks", GC.getNumFeatureInfos());
	pXML->SetVariableListTagPair(&m_piFeatureDefenseModifier, L"FeatureDefenses", GC.getNumFeatureInfos());

	pXML->SetVariableListTagPair(&m_piUnitCombatModifier, L"UnitCombatMods", GC.getNumUnitCombatInfos());
	pXML->SetVariableListTagPair(&m_piUnitCombatCollateralImmune, L"UnitCombatCollateralImmunes", GC.getNumUnitCombatInfos());
	pXML->SetVariableListTagPair(&m_piDomainModifier, L"DomainMods", NUM_DOMAIN_TYPES);

	pXML->SetVariableListTagPair(&m_piBonusProductionModifier, L"BonusProductionModifiers", GC.getNumBonusInfos());

	// Hand-written: post-read clamp not expressible by the int wrapper.
	pXML->GetOptionalChildXmlValByName(&m_iBaseUpkeep, L"iBaseUpkeep");
	if (m_iBaseUpkeep < 0) m_iBaseUpkeep = 0;

	// Read the mesh groups elements
	if ( pXML->TryMoveToXmlFirstChild(L"UnitMeshGroups") )
	{
		pXML->GetChildXmlValByName( &m_iGroupSize, L"iGroupSize");
		m_iGroupDefinitions = iIndexVal = pXML->GetXmlChildrenNumber(L"UnitMeshGroup");
		m_piUnitGroupRequired = new int[ iIndexVal ];
		pXML->GetChildXmlValByName( &m_iUnitMeleeWaveSize, L"iMeleeWaveSize" );
		pXML->GetChildXmlValByName( &m_iUnitRangedWaveSize, L"iRangedWaveSize" );
		pXML->GetChildXmlValByName( &m_fUnitMaxSpeed, L"fMaxSpeed");
		pXML->GetChildXmlValByName( &m_fUnitPadTime, L"fPadTime");

		m_paszEarlyArtDefineTags = new CvString[ iIndexVal ];
		m_paszClassicalArtDefineTags = new CvString[ iIndexVal ];
		m_paszMiddleArtDefineTags = new CvString[ iIndexVal ];
		m_paszRennArtDefineTags = new CvString[ iIndexVal ];
		m_paszIndustrialArtDefineTags = new CvString[ iIndexVal ];
		m_paszLateArtDefineTags = new CvString[ iIndexVal ];
		m_paszFutureArtDefineTags = new CvString[ iIndexVal ];

		if (pXML->TryMoveToXmlFirstChild(L"UnitMeshGroup"))
		{
			for (int k = 0; k < iIndexVal; k++ )
			{
				pXML->GetChildXmlValByName( &m_piUnitGroupRequired[k], L"iRequired");
				if (pXML->GetOptionalChildXmlValByName(szTextVal, L"EarlyArtDefineTag"))
					GC.getInfoTypeForString(szTextVal);
				setEarlyArtDefineTag(k, szTextVal);
				if (pXML->GetOptionalChildXmlValByName(szTextVal, L"ClassicalArtDefineTag"))
					GC.getInfoTypeForString(szTextVal);
				setClassicalArtDefineTag(k, szTextVal);
				if (pXML->GetOptionalChildXmlValByName(szTextVal, L"MiddleArtDefineTag"))
					GC.getInfoTypeForString(szTextVal);
				setMiddleArtDefineTag(k, szTextVal);
				if (pXML->GetOptionalChildXmlValByName(szTextVal, L"RennArtDefineTag"))
					GC.getInfoTypeForString(szTextVal);
				setRennArtDefineTag(k, szTextVal);
				if (pXML->GetOptionalChildXmlValByName(szTextVal, L"IndustrialArtDefineTag"))
					GC.getInfoTypeForString(szTextVal);
				setIndustrialArtDefineTag(k, szTextVal);
				if (pXML->GetOptionalChildXmlValByName(szTextVal, L"LateArtDefineTag"))
					GC.getInfoTypeForString(szTextVal);
				setLateArtDefineTag(k, szTextVal);
				if (pXML->GetOptionalChildXmlValByName(szTextVal, L"FutureArtDefineTag"))
					GC.getInfoTypeForString(szTextVal);
				setFutureArtDefineTag(k, szTextVal);
				pXML->TryMoveToXmlNextSibling();
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	pXML->GetOptionalChildXmlValByName(m_szFormationType, L"FormationType");

	if (pXML->TryMoveToXmlFirstChild(L"UniqueNames"))
	{
		pXML->SetStringList(&m_paszUnitNames, &m_iNumUnitNames);
		pXML->MoveToXmlParent();
	}


//	pXML->SetVariableListTagPair(&m_paszCivilizationNames, L"", sizeof(GC.getCivilizationInfo((CivilizationTypes)0)), GC.getNumCivilizationInfos(), L"");

	if (pXML->TryMoveToXmlFirstChild(L"CivilizationNames"))
	{
		CvString szTemp;
		const int iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (iNumSibs > 0)
			{
				for (int i=0;i<iNumSibs;i++)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						m_aszCivilizationNamesforPass3.push_back(szTextVal);
						pXML->GetNextXmlVal(szTemp);
						m_aszCivilizationNamesValueforPass3.push_back(szTemp);

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

	if(pXML->TryMoveToXmlFirstChild(L"HealUnitCombatTypes"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"HealUnitCombatType" );
		m_aHealUnitCombatTypes.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"HealUnitCombatType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					m_aHealUnitCombatTypes[i].eUnitCombat = (UnitCombatTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aHealUnitCombatTypes[i].iHeal), L"iHeal");
					pXML->GetChildXmlValByName(&(m_aHealUnitCombatTypes[i].iAdjacentHeal), L"iAdjacentHeal");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"HealUnitCombatType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"GroupSpawnUnitCombatTypes"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"GroupSpawnUnitCombatType" );
		m_aGroupSpawnUnitCombatTypes.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"GroupSpawnUnitCombatType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					m_aGroupSpawnUnitCombatTypes[i].eUnitCombat = (UnitCombatTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aGroupSpawnUnitCombatTypes[i].iChance), L"iChance");
					pXML->GetOptionalChildXmlValByName(m_aGroupSpawnUnitCombatTypes[i].m_szTitle, L"Title");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"GroupSpawnUnitCombatType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"InvisibleTerrainChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"InvisibleTerrainChange" );
		m_aInvisibleTerrainChanges.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"InvisibleTerrainChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"InvisibleType");
					m_aInvisibleTerrainChanges[i].eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(szTextVal, L"TerrainType");
					m_aInvisibleTerrainChanges[i].eTerrain = (TerrainTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aInvisibleTerrainChanges[i].iIntensity), L"iIntensity");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"InvisibleTerrainChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"InvisibleFeatureChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"InvisibleFeatureChange" );
		m_aInvisibleFeatureChanges.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"InvisibleFeatureChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"InvisibleType");
					m_aInvisibleFeatureChanges[i].eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(szTextVal, L"FeatureType");
					m_aInvisibleFeatureChanges[i].eFeature = (FeatureTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aInvisibleFeatureChanges[i].iIntensity), L"iIntensity");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"InvisibleFeatureChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"InvisibleImprovementChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"InvisibleImprovementChange" );
		m_aInvisibleImprovementChanges.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"InvisibleImprovementChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"InvisibleType");
					m_aInvisibleImprovementChanges[i].eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(szTextVal, L"ImprovementType");
					m_aInvisibleImprovementChanges[i].eImprovement = (ImprovementTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aInvisibleImprovementChanges[i].iIntensity), L"iIntensity");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"InvisibleImprovementChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"VisibleTerrainChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"VisibleTerrainChange" );
		m_aVisibleTerrainChanges.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"VisibleTerrainChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"InvisibleType");
					m_aVisibleTerrainChanges[i].eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(szTextVal, L"TerrainType");
					m_aVisibleTerrainChanges[i].eTerrain = (TerrainTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aVisibleTerrainChanges[i].iIntensity), L"iIntensity");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"VisibleTerrainChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"VisibleFeatureChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"VisibleFeatureChange" );
		m_aVisibleFeatureChanges.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"VisibleFeatureChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"InvisibleType");
					m_aVisibleFeatureChanges[i].eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(szTextVal, L"FeatureType");
					m_aVisibleFeatureChanges[i].eFeature = (FeatureTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aVisibleFeatureChanges[i].iIntensity), L"iIntensity");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"VisibleFeatureChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"VisibleImprovementChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"VisibleImprovementChange" );
		m_aVisibleImprovementChanges.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"VisibleImprovementChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"InvisibleType");
					m_aVisibleImprovementChanges[i].eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(szTextVal, L"ImprovementType");
					m_aVisibleImprovementChanges[i].eImprovement = (ImprovementTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aVisibleImprovementChanges[i].iIntensity), L"iIntensity");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"VisibleImprovementChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"VisibleTerrainRangeChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"VisibleTerrainRangeChange" );
		m_aVisibleTerrainRangeChanges.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"VisibleTerrainRangeChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"InvisibleType");
					pXML->GetChildXmlValByName(szTextVal2, L"TerrainType");
					m_aVisibleTerrainRangeChanges[i].eTerrain = (TerrainTypes)pXML->GetInfoClass(szTextVal2);
					pXML->GetChildXmlValByName(&(m_aVisibleTerrainRangeChanges[i].iIntensity), L"iIntensity");
					GC.addDelayedResolution((int*)&(m_aVisibleTerrainRangeChanges[i].eInvisible), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"VisibleTerrainRangeChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"VisibleFeatureRangeChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"VisibleFeatureRangeChange" );
		m_aVisibleFeatureRangeChanges.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"VisibleFeatureRangeChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"InvisibleType");
					m_aVisibleFeatureRangeChanges[i].eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(szTextVal, L"FeatureType");
					m_aVisibleFeatureRangeChanges[i].eFeature = (FeatureTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aVisibleFeatureRangeChanges[i].iIntensity), L"iIntensity");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"VisibleFeatureRangeChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"VisibleImprovementRangeChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"VisibleImprovementRangeChange" );
		m_aVisibleImprovementRangeChanges.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"VisibleImprovementRangeChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"InvisibleType");
					m_aVisibleImprovementRangeChanges[i].eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(szTextVal, L"ImprovementType");
					m_aVisibleImprovementRangeChanges[i].eImprovement = (ImprovementTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aVisibleImprovementRangeChanges[i].iIntensity), L"iIntensity");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"VisibleImprovementRangeChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}
	if(pXML->TryMoveToXmlFirstChild(L"EnabledCivilizationTypes"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"EnabledCivilizationType" );
		m_aEnabledCivilizationTypes.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"EnabledCivilizationType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"CivilizationType");
					GC.addDelayedResolution((int*)&(m_aEnabledCivilizationTypes[i].eCivilization), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"EnabledCivilizationType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	// int vector utilizing pairing without delayed resolution
	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aFlankingStrengthbyUnitCombatType, L"FlankingStrikesbyUnitCombat");

	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aTrapDisableUnitCombatTypes, L"TrapDisableUnitCombatTypes");

	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aTrapAvoidanceUnitCombatTypes, L"TrapAvoidanceUnitCombatTypes");

	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aTrapTriggerUnitCombatTypes, L"TrapTriggerUnitCombatTypes");

	pXML->SetOptionalPairVector<InvisibilityArray, InvisibleTypes, int>(&m_aVisibilityIntensityTypes, L"VisibilityIntensityTypes");

	pXML->SetOptionalPairVector<InvisibilityArray, InvisibleTypes, int>(&m_aInvisibilityIntensityTypes, L"InvisibilityIntensityTypes");

	pXML->SetOptionalPairVector<TerrainModifierArray, TerrainTypes, int>(&m_aTerrainWorkRateModifierTypes, L"TerrainWorkRateModifierTypes");

	pXML->SetOptionalPairVector<FeatureModifierArray, FeatureTypes, int>(&m_aFeatureWorkRateModifierTypes, L"FeatureWorkRateModifierTypes");

	pXML->SetOptionalPairVector<BuildModifierArray, BuildTypes, int>(&m_aBuildWorkRateModifierTypes, L"BuildWorkRateModifierTypes");

	//TB Combat Mods End  TB SubCombat Mod end

	m_KillOutcomeList.read(pXML, L"KillOutcomes");

	if(pXML->TryMoveToXmlFirstChild(L"Actions"))
	{
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"Action"))
			{
				do
				{
					CvOutcomeMission* pOutcomeMission = new CvOutcomeMission();
					pOutcomeMission->read(pXML);
					m_aOutcomeMissions.push_back(pOutcomeMission);
				} while(pXML->TryMoveToXmlNextSibling());
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

/************************************************************************************************/
/* XMLCOPY								 10/10/07								MRGENIE	  */
/*																							  */
/* if we're using a module, assuming the meshgroup isn't set, we update first after			 */
/* the copyNonDefaults method																   */
/************************************************************************************************/
/*
	updateArtDefineButton();
*/
	if (m_iGroupSize != 0)
	{
		updateArtDefineButton();
	}
	return true;
}

void CvUnitInfo::copyNonDefaults(CvUnitInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	const int iDefault = 0;

	// Hand-written: default is UNITAI_UNKNOWN, not the wrapper's -1.
	if ( m_iDefaultUnitAIType == UNITAI_UNKNOWN )	m_iDefaultUnitAIType = pClassInfo->getDefaultUnitAIType();

	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aiSeeInvisibleTypes, pClassInfo->m_aiSeeInvisibleTypes);

	for ( int i = 0; i < GC.getNumUnitCombatInfos(); i++)
	{
		if ( getUnitCombatModifier(i) == iDefault && pClassInfo->getUnitCombatModifier(i) != iDefault )
		{
			if ( NULL == m_piUnitCombatModifier )
			{
				CvXMLLoadUtility::InitList(&m_piUnitCombatModifier,GC.getNumUnitCombatInfos(),iDefault);
			}
			m_piUnitCombatModifier[i] = pClassInfo->getUnitCombatModifier(i);
		}
		if ( getUnitCombatCollateralImmune(i) == iDefault && pClassInfo->getUnitCombatCollateralImmune(i) != iDefault)
		{
			if ( NULL == m_piUnitCombatCollateralImmune )
			{
				CvXMLLoadUtility::InitList(&m_piUnitCombatCollateralImmune,GC.getNumUnitCombatInfos(),iDefault);
			}
			m_piUnitCombatCollateralImmune[i] = pClassInfo->getUnitCombatCollateralImmune(i);
		}
	}

	for ( int i = 0; i < GC.getNumReligionInfos(); i++)
	{
		if ( getReligionSpreads(i) == -1 && pClassInfo->getReligionSpreads(i) != -1)
		{
			if ( NULL == m_piReligionSpreads )
			{
				CvXMLLoadUtility::InitList(&m_piReligionSpreads,GC.getNumReligionInfos(),-1);
			}
			m_piReligionSpreads[i] = pClassInfo->getReligionSpreads(i);
		}
	}

	for ( int i = 0; i < GC.getNumCorporationInfos(); i++)
	{
		if ( getCorporationSpreads(i) == -1 && pClassInfo->getCorporationSpreads(i) != -1 )
		{
			if ( NULL == m_piCorporationSpreads)
			{
				CvXMLLoadUtility::InitList(&m_piCorporationSpreads,GC.getNumCorporationInfos(),-1);
			}
			m_piCorporationSpreads[i] = pClassInfo->getCorporationSpreads(i);
		}
	}

	for ( int i = 0; i < GC.getNumFlavorTypes(); i++)
	{
		if ( getFlavorValue(i) == iDefault && pClassInfo->getFlavorValue(i) != iDefault)
		{
			if ( NULL == m_piFlavorValue )
			{
				CvXMLLoadUtility::InitList(&m_piFlavorValue,GC.getNumFlavorTypes(),iDefault);
			}
			m_piFlavorValue[i] = pClassInfo->getFlavorValue(i);
		}
	}


	for ( int i = 0; i < GC.getNumTerrainInfos(); i++)
	{
/*		if ( getTerrainImpassable(i) == bDefault && pClassInfo->getTerrainImpassable(i) != bDefault)
		{
			if ( NULL == m_pbTerrainImpassable )
			{
				CvXMLLoadUtility::InitList(&m_pbTerrainImpassable,GC.getNumTerrainInfos(),bDefault);
			}
			m_pbTerrainImpassable[i] = pClassInfo->getTerrainImpassable(i);
		}*/
		if ( getTerrainAttackModifier(i) == iDefault && pClassInfo->getTerrainAttackModifier(i) != iDefault)
		{
			if ( NULL == m_piTerrainAttackModifier )
			{
				CvXMLLoadUtility::InitList(&m_piTerrainAttackModifier,GC.getNumTerrainInfos(),iDefault);
			}
			m_piTerrainAttackModifier[i] = pClassInfo->getTerrainAttackModifier(i);
		}
		if ( getTerrainDefenseModifier(i) == iDefault && pClassInfo->getTerrainDefenseModifier(i) != iDefault)
		{
			if ( NULL == m_piTerrainDefenseModifier )
			{
				CvXMLLoadUtility::InitList(&m_piTerrainDefenseModifier,GC.getNumTerrainInfos(),iDefault);
			}
			m_piTerrainDefenseModifier[i] = pClassInfo->getTerrainDefenseModifier(i);
		}
		if ( getTerrainPassableTech(i) == NO_TECH && pClassInfo->getTerrainPassableTech(i) != NO_TECH )
		{
			if ( NULL == m_piTerrainPassableTech )
			{
				CvXMLLoadUtility::InitList(&m_piTerrainPassableTech,GC.getNumTerrainInfos(),(int)NO_TECH);
			}
			m_piTerrainPassableTech[i] = pClassInfo->getTerrainPassableTech(i);
		}
	}

	for ( int i = 0; i < GC.getNumFeatureInfos(); i++)
	{
		//if ( getFeatureImpassable(i) == bDefault && pClassInfo->getFeatureImpassable(i) != bDefault)
		//{
		//	if (m_pbFeatureImpassable == NULL)
		//	{
		//		CvXMLLoadUtility::InitList(&m_pbFeatureImpassable,GC.getNumFeatureInfos(),bDefault);
		//	}
		//	m_pbFeatureImpassable[i] = pClassInfo->getFeatureImpassable(i);
		//}
		if ( getFeatureAttackModifier(i) == iDefault && pClassInfo->getFeatureAttackModifier(i) != iDefault)
		{
			if (m_piFeatureAttackModifier == NULL)
			{
				CvXMLLoadUtility::InitList(&m_piFeatureAttackModifier,GC.getNumFeatureInfos(),iDefault);
			}
			m_piFeatureAttackModifier[i] = pClassInfo->getFeatureAttackModifier(i);
		}
		if ( getFeatureDefenseModifier(i) == iDefault && pClassInfo->getFeatureDefenseModifier(i) != iDefault)
		{
			if (m_piFeatureDefenseModifier == NULL)
			{
				CvXMLLoadUtility::InitList(&m_piFeatureDefenseModifier,GC.getNumFeatureInfos(),iDefault);
			}
			m_piFeatureDefenseModifier[i] = pClassInfo->getFeatureDefenseModifier(i);
		}
		if ( getFeaturePassableTech(i) == NO_TECH && pClassInfo->getFeaturePassableTech(i) != NO_TECH )
		{
			if ( NULL == m_piFeaturePassableTech )
			{
				CvXMLLoadUtility::InitList(&m_piFeaturePassableTech,GC.getNumFeatureInfos(),(int)NO_TECH);
			}
			m_piFeaturePassableTech[i] = pClassInfo->getFeaturePassableTech(i);
		}
	}

	for ( int i = 0; i < NUM_DOMAIN_TYPES; i++)
	{
		if ( getDomainModifier(i) == iDefault && pClassInfo->getDomainModifier(i) != iDefault)
		{
			if ( NULL == m_piDomainModifier )
			{
				CvXMLLoadUtility::InitList(&m_piDomainModifier,NUM_DOMAIN_TYPES,iDefault);
			}
			m_piDomainModifier[i] = pClassInfo->getDomainModifier(i);
		}
	}

	for ( int i = 0; i < GC.getNumBonusInfos(); i++)
	{
		if ( getBonusProductionModifier(i) == iDefault && pClassInfo->getBonusProductionModifier(i) != iDefault)
		{
			if ( NULL == m_piBonusProductionModifier )
			{
				CvXMLLoadUtility::InitList(&m_piBonusProductionModifier,GC.getNumBonusInfos(),iDefault);
			}
			m_piBonusProductionModifier[i] = pClassInfo->getBonusProductionModifier(i);
		}
	}


	//Struct Vector
	GC.copyNonDefaultDelayedResolutionVector(m_aiTargetUnit, pClassInfo->m_aiTargetUnit);
	GC.copyNonDefaultDelayedResolutionVector(m_aiDefendAgainstUnit, pClassInfo->m_aiDefendAgainstUnit);
	GC.copyNonDefaultDelayedResolutionVector(m_aiSupersedingUnits, pClassInfo->m_aiSupersedingUnits);
	GC.copyNonDefaultDelayedResolutionVector(m_aiUnitUpgrades, pClassInfo->m_aiUnitUpgrades);

	// Hand-written: post-copy clamp not expressible by the int wrapper.
	if ( m_iWithdrawalProbability == iDefault ) m_iWithdrawalProbability = pClassInfo->m_iWithdrawalProbability;
	if (m_iWithdrawalProbability < 0) m_iWithdrawalProbability = 0;

	// Hand-written: read clamps negatives, so the copy stays paired with the read.
	if ( m_iBaseUpkeep == iDefault ) m_iBaseUpkeep = pClassInfo->getBaseUpkeep();

	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aHealUnitCombatTypes, pClassInfo->m_aHealUnitCombatTypes);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aGroupSpawnUnitCombatTypes, pClassInfo->m_aGroupSpawnUnitCombatTypes);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aInvisibleTerrainChanges, pClassInfo->m_aInvisibleTerrainChanges);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aInvisibleFeatureChanges, pClassInfo->m_aInvisibleFeatureChanges);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aInvisibleImprovementChanges, pClassInfo->m_aInvisibleImprovementChanges);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleTerrainChanges, pClassInfo->m_aVisibleTerrainChanges);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleFeatureChanges, pClassInfo->m_aVisibleFeatureChanges);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleImprovementChanges, pClassInfo->m_aVisibleImprovementChanges);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleTerrainRangeChanges, pClassInfo->m_aVisibleTerrainRangeChanges);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleFeatureRangeChanges, pClassInfo->m_aVisibleFeatureRangeChanges);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleImprovementRangeChanges, pClassInfo->m_aVisibleImprovementRangeChanges);

	if (getNumEnabledCivilizationTypes() == 0)
	{
		int iNum = pClassInfo->getNumEnabledCivilizationTypes();
		m_aEnabledCivilizationTypes.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			GC.copyNonDefaultDelayedResolution((int*)&(m_aEnabledCivilizationTypes[i].eCivilization), (int*)&(pClassInfo->m_aEnabledCivilizationTypes[i].eCivilization));
		}
	}
	// int vectors utilizing pairing without delayed resolution
	if (getNumFlankingStrikesbyUnitCombatTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumFlankingStrikesbyUnitCombatTypes(); i++)
		{
			UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			int iChange = pClassInfo->getFlankingStrengthbyUnitCombatType(i);
			m_aFlankingStrengthbyUnitCombatType.push_back(std::make_pair(eUnitCombat, iChange));
		}
	}

	if (getNumTrapDisableUnitCombatTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumTrapDisableUnitCombatTypes(); i++)
		{
			UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			int iChange = pClassInfo->getTrapDisableUnitCombatType(i);
			m_aTrapDisableUnitCombatTypes.push_back(std::make_pair(eUnitCombat, iChange));
		}
	}

	if (getNumTrapAvoidanceUnitCombatTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumTrapAvoidanceUnitCombatTypes(); i++)
		{
			UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			int iChange = pClassInfo->getTrapAvoidanceUnitCombatType(i);
			m_aTrapAvoidanceUnitCombatTypes.push_back(std::make_pair(eUnitCombat, iChange));
		}
	}

	if (getNumTrapTriggerUnitCombatTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumTrapTriggerUnitCombatTypes(); i++)
		{
			UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			int iChange = pClassInfo->getTrapTriggerUnitCombatType(i);
			m_aTrapTriggerUnitCombatTypes.push_back(std::make_pair(eUnitCombat, iChange));
		}
	}

	if (getNumVisibilityIntensityTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumVisibilityIntensityTypes(); i++)
		{
			InvisibleTypes eInvisible = ((InvisibleTypes)i);
			int iChange = pClassInfo->getVisibilityIntensityType(i);
			m_aVisibilityIntensityTypes.push_back(std::make_pair(eInvisible, iChange));
		}
	}

	if (getNumInvisibilityIntensityTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumInvisibilityIntensityTypes(); i++)
		{
			InvisibleTypes eInvisible = ((InvisibleTypes)i);
			int iChange = pClassInfo->getInvisibilityIntensityType(i);
			m_aInvisibilityIntensityTypes.push_back(std::make_pair(eInvisible, iChange));
		}
	}

	if (getNumTerrainWorkRateModifierTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumTerrainWorkRateModifierTypes(); i++)
		{
			TerrainTypes eTerrain = ((TerrainTypes)i);
			int iChange = pClassInfo->getTerrainWorkRateModifierType(i);
			m_aTerrainWorkRateModifierTypes.push_back(std::make_pair(eTerrain, iChange));
		}
	}

	if (getNumFeatureWorkRateModifierTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumFeatureWorkRateModifierTypes(); i++)
		{
			FeatureTypes eFeature = ((FeatureTypes)i);
			int iChange = pClassInfo->getFeatureWorkRateModifierType(i);
			m_aFeatureWorkRateModifierTypes.push_back(std::make_pair(eFeature, iChange));
		}
	}

	if (getNumBuildWorkRateModifierTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumBuildWorkRateModifierTypes(); i++)
		{
			BuildTypes eBuild = ((BuildTypes)i);
			int iChange = pClassInfo->getBuildWorkRateModifierType(i);
			m_aBuildWorkRateModifierTypes.push_back(std::make_pair(eBuild, iChange));
		}
	}

	//TB Combat Mods End  TB SubCombat Mod end

	m_KillOutcomeList.copyNonDefaults(&pClassInfo->m_KillOutcomeList);

	if (m_aOutcomeMissions.empty())
	{
		const int num = pClassInfo->getNumActionOutcomes();
		for (int index = 0; index < num; index++)
		{
			m_aOutcomeMissions.push_back(pClassInfo->m_aOutcomeMissions[index]);
			pClassInfo->m_aOutcomeMissions[index] = NULL;
		}
	}

	// For the Meshgroups I assume the XML holding the largest GroupSize is the most completed(most fancy)
	// and we want to keep that one
	if ( m_iGroupSize < pClassInfo->getGroupSize() )
	{
		m_iGroupSize = pClassInfo->getGroupSize();
		m_iGroupDefinitions = pClassInfo->getGroupDefinitions();

		m_iUnitMeleeWaveSize = pClassInfo->getMeleeWaveSize();
		m_iUnitRangedWaveSize = pClassInfo->getRangedWaveSize();
		m_fUnitMaxSpeed = pClassInfo->getUnitMaxSpeed();
		m_fUnitPadTime = pClassInfo->getUnitPadTime();

		//Delete old Arrays for initializing a new one
		SAFE_DELETE_ARRAY(m_piUnitGroupRequired)
		SAFE_DELETE_ARRAY(m_paszEarlyArtDefineTags)
		SAFE_DELETE_ARRAY(m_paszLateArtDefineTags)
		SAFE_DELETE_ARRAY(m_paszMiddleArtDefineTags)
		SAFE_DELETE_ARRAY(m_paszClassicalArtDefineTags)
		SAFE_DELETE_ARRAY(m_paszRennArtDefineTags)
		SAFE_DELETE_ARRAY(m_paszIndustrialArtDefineTags)
		SAFE_DELETE_ARRAY(m_paszFutureArtDefineTags)

		m_piUnitGroupRequired = new int[ m_iGroupDefinitions ];
		m_paszEarlyArtDefineTags = new CvString[ m_iGroupDefinitions ];
		m_paszLateArtDefineTags = new CvString[ m_iGroupDefinitions ];
		m_paszMiddleArtDefineTags = new CvString[ m_iGroupDefinitions ];
		m_paszClassicalArtDefineTags = new CvString[ m_iGroupDefinitions ];
		m_paszRennArtDefineTags = new CvString[ m_iGroupDefinitions ];
		m_paszIndustrialArtDefineTags = new CvString[ m_iGroupDefinitions ];
		m_paszFutureArtDefineTags = new CvString[ m_iGroupDefinitions ];

		for ( int i = 0; i < m_iGroupDefinitions; i++ )
		{
			m_piUnitGroupRequired[i] = pClassInfo->getUnitGroupRequired(i);
			setEarlyArtDefineTag(i, pClassInfo->getEarlyArtDefineTag(i, NO_UNIT_ARTSTYLE));
			setLateArtDefineTag(i, pClassInfo->getLateArtDefineTag(i, NO_UNIT_ARTSTYLE));
			setClassicalArtDefineTag(i, pClassInfo->getClassicalArtDefineTag(i, NO_UNIT_ARTSTYLE));
			setMiddleArtDefineTag(i, pClassInfo->getMiddleArtDefineTag(i, NO_UNIT_ARTSTYLE));
			setRennArtDefineTag(i, pClassInfo->getRennArtDefineTag(i, NO_UNIT_ARTSTYLE));
			setIndustrialArtDefineTag(i, pClassInfo->getIndustrialArtDefineTag(i, NO_UNIT_ARTSTYLE));
			setFutureArtDefineTag(i, pClassInfo->getFutureArtDefineTag(i, NO_UNIT_ARTSTYLE));
		}
	}

	if ( m_szFormationType == "FORMATION_TYPE_DEFAULT" ) m_szFormationType = pClassInfo->getFormationType();

	// First we check if there are different Unique Unitnames in the Modules(we want to keep all of them)
	// So we have to set the Arraysize properly, knowing the amount of Unique Unitnames
	if ( pClassInfo->getNumUnitNames() != 0 )
	{
		CvString* m_paszNewNames = new CvString[pClassInfo->getNumUnitNames()];
		for ( int i = 0; i < pClassInfo->getNumUnitNames(); i++)
		{
			m_paszNewNames[i] = pClassInfo->getUnitNames(i);
		}

		CvXMLLoadUtilityModTools::StringArrayExtend(&m_paszUnitNames, &m_iNumUnitNames, &m_paszNewNames, pClassInfo->getNumUnitNames());
		SAFE_DELETE_ARRAY(m_paszNewNames)
	}

	if (m_iGroupSize != 0)
	{
		updateArtDefineButton();
	}
}

bool CvUnitInfo::readPass3()
{
	PROFILE_EXTRA_FUNC();
	m_paszCivilizationNames = new CvWString[GC.getNumCivilizationInfos()];
	for (int iI = 0; iI < GC.getNumCivilizationInfos(); iI++)
	{
		m_paszCivilizationNames[iI] = "";
	}

	if (!m_aszCivilizationNamesValueforPass3.empty() && !m_aszCivilizationNamesforPass3.empty())
	{
		const int iNumLoad = m_aszCivilizationNamesValueforPass3.size();
		for(int iI = 0; iI < iNumLoad; iI++)
		{
			const int iTempIndex = GC.getInfoTypeForString(m_aszCivilizationNamesforPass3[iI]);
			if (iTempIndex >= 0 && iTempIndex < GC.getNumCivilizationInfos())
				m_paszCivilizationNames[iTempIndex] = m_aszCivilizationNamesValueforPass3[iI];
		}
		m_aszCivilizationNamesforPass3.clear();
		m_aszCivilizationNamesValueforPass3.clear();
	}
	return true;
}

// #195 Phase 2: aggregate the GOM-expressible typed Prereq* fields into one introspectable
// requirement list (the train-side analogue of CvBuildingInfo::buildConstructRequirements).
// Read-only description; canTrain still evaluates. Non-GOM and vicinity-bonus prereqs keep
// their bespoke handling and are not modelled here yet.
void CvUnitInfo::buildTrainRequirements()
{
	PROFILE_EXTRA_FUNC();
	m_trainRequirements.clear();

	// --- GOM_BUILDING ---
	{
		ConstructRequirement req(GOM_BUILDING, REQOP_REQUIRE_ALL);
		for (int i = 0, n = getNumPrereqAndBuildings(); i < n; i++)
		{
			req.aiIds.push_back(getPrereqAndBuilding(i));
		}
		if (!req.aiIds.empty()) m_trainRequirements.push_back(req);
	}
	{
		ConstructRequirement req(GOM_BUILDING, REQOP_REQUIRE_ANY);
		for (int i = 0, n = getPrereqOrBuildingsNum(); i < n; i++)
		{
			req.aiIds.push_back(getPrereqOrBuilding(i));
		}
		if (!req.aiIds.empty()) m_trainRequirements.push_back(req);
	}

	// --- GOM_TECH (single PrereqAndTech + any PrereqAndTechs, all required) ---
	{
		ConstructRequirement req(GOM_TECH, REQOP_REQUIRE_ALL);
		if (getPrereqAndTech() != NO_TECH)
		{
			req.aiIds.push_back(getPrereqAndTech());
		}
		foreach_(const TechTypes eTech, getPrereqAndTechs())
		{
			if (eTech != NO_TECH && algo::none_of_equal(req.aiIds, (int)eTech))
			{
				req.aiIds.push_back(eTech);
			}
		}
		if (!req.aiIds.empty()) m_trainRequirements.push_back(req);
	}

	// --- GOM_BONUS ---
	if (getPrereqAndBonus() != NO_BONUS)
	{
		m_trainRequirements.push_back(ConstructRequirement(GOM_BONUS, REQOP_REQUIRE_ALL, getPrereqAndBonus()));
	}
	{
		ConstructRequirement req(GOM_BONUS, REQOP_REQUIRE_ANY);
		foreach_(const BonusTypes eBonus, getPrereqOrBonuses())
		{
			req.aiIds.push_back(eBonus);
		}
		if (!req.aiIds.empty()) m_trainRequirements.push_back(req);
	}

	// --- GOM_RELIGION ---
	if (getPrereqReligion() != NO_RELIGION)
	{
		m_trainRequirements.push_back(ConstructRequirement(GOM_RELIGION, REQOP_REQUIRE_ALL, getPrereqReligion()));
	}

	// --- GOM_CORPORATION ---
	if (getPrereqCorporation() != NO_CORPORATION)
	{
		m_trainRequirements.push_back(ConstructRequirement(GOM_CORPORATION, REQOP_REQUIRE_ALL, getPrereqCorporation()));
	}

	// --- GOM_CIVIC (per-civic OR membership; units have no AND-civic prereq) ---
	{
		ConstructRequirement reqOr(GOM_CIVIC, REQOP_REQUIRE_ANY);
		for (int i = 0, n = GC.getNumCivicInfos(); i < n; i++)
		{
			if (isPrereqOrCivics(i)) reqOr.aiIds.push_back(i);
		}
		if (!reqOr.aiIds.empty()) m_trainRequirements.push_back(reqOr);
	}
}

void CvUnitInfo::doPostLoadCaching(uint32_t iThis)
{
	PROFILE_EXTRA_FUNC();
	buildTrainRequirements();
	const int iNumUnitCombatInfos = GC.getNumUnitCombatInfos();
	{
		bool bCheck = true;
		foreach_(const UnitCombatTypes eSubCombat, getSubCombatTypes())
		{
			if (GC.getUnitCombatInfo(eSubCombat).getReligion() != NO_RELIGION)
			{
				bCheck = false;
				break;
			}
		}
		if (bCheck)
		{
			const int iReligion = getReligionType();
			const int iReligionState = getStateReligion();
			const int iReligionPrereq = getPrereqReligion();

			if (iReligion > -1 || iReligionState > -1 || iReligionPrereq > -1)
			{
				for (int iI = 0; iI < iNumUnitCombatInfos; iI++)
				{
					const int iType = GC.getUnitCombatInfo((UnitCombatTypes)iI).getReligion();

					if (iType > -1 && (iType == iReligion || iType == iReligionState || iType == iReligionPrereq))
					{
						m_aiSubCombatTypes.push_back((UnitCombatTypes)iI);
						break;
					}
				}
			}
		}
	}
	{
		bool bCheck = true;
		foreach_(const UnitCombatTypes eSubCombat, getSubCombatTypes())
		{
			if (GC.getUnitCombatInfo(eSubCombat).getCulture() != NO_BONUS)
			{
				bCheck = false;
				break;
			}
		}
		if (bCheck)
		{
			const BonusTypes eBonus = (BonusTypes)getPrereqAndBonus();

			if (eBonus != NO_BONUS && GC.getBonusInfo(eBonus).getBonusClassType() == GC.getInfoTypeForString("BONUSCLASS_CULTURE"))
			{
				for (int iI = 0; iI < iNumUnitCombatInfos; iI++)
				{
					if (GC.getUnitCombatInfo((UnitCombatTypes)iI).getCulture() == eBonus)
					{
						m_aiSubCombatTypes.push_back((UnitCombatTypes)iI);
						break;
					}
				}
			}
		}
	}
	{
		if (getEraInfo() != NO_ERA)
		{
			bool bCheck = true;
			foreach_(const UnitCombatTypes eSubCombat, getSubCombatTypes())
			{
				if (GC.getUnitCombatInfo(eSubCombat).getEra() != NO_ERA)
				{
					bCheck = false;
					break;
				}
			}
			if (bCheck)
			{
				for (int iI = 0; iI < iNumUnitCombatInfos; iI++)
				{
					if (GC.getUnitCombatInfo((UnitCombatTypes)iI).getEra() == getEraInfo())
					{
						m_aiSubCombatTypes.push_back((UnitCombatTypes)iI);
						break;
					}
				}
			}
		}
	}

	// Toffer - Size Matters
	{
		int iOffset = 0;
		m_iBaseGroupRank = 0;
		m_iTotalCombatStrengthChangeBase = 0;
		m_iTotalCombatStrengthModifierBase = 0;

		for (int iI = -1; iI < getNumSubCombatTypes(); iI++)
		{
			const UnitCombatTypes eUnitCombat = (
				(UnitCombatTypes)
				(
					iI > -1 ? getSubCombatType(iI) : getUnitCombatType()
				)
			);
			if (eUnitCombat == NO_UNITCOMBAT) continue;

			const CvUnitCombatInfo& info = GC.getUnitCombatInfo(eUnitCombat);

			m_iTotalCombatStrengthChangeBase += info.getStrengthChange();

			if (info.getQualityBase() > -10)
			{
				m_iTotalCombatStrengthModifierBase += info.getQualityBase() - 5;
			}
			if (info.getSizeBase() > -10)
			{
				m_iTotalCombatStrengthModifierBase += info.getSizeBase() - 5;

				iOffset += info.getSizeBase();
			}
			if (info.getGroupBase() > -10)
			{
				m_iTotalCombatStrengthModifierBase += info.getGroupBase() - 5;

				iOffset += info.getGroupBase();
				m_iBaseGroupRank += info.getGroupBase();
			}
		}
		const int iSMMultiplier = GC.getSIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER();
		int iBase = 10000;
		if (iOffset > 0)
		{
			for (int iI = 0; iI < iOffset; iI++)
			{
				iBase = iBase * iSMMultiplier / 100;
			}
		}
		else
		{
			for (int iI = 0; iI < -iOffset; iI++)
			{
				iBase = iBase * 100 / iSMMultiplier;
			}
		}
		iBase /= 100;
		m_iBaseCargoVolume = std::max(1, iBase);
	}

	m_aiHealAsTypes.clear();
	for (int iI = 0; iI < iNumUnitCombatInfos; iI++)
	{
		if (getUnitCombatType() == iI || isSubCombatType((UnitCombatTypes)iI))
		{
			if (GC.getUnitCombatInfo((UnitCombatTypes)iI).isHealsAs())
			{
				m_aiHealAsTypes.push_back((UnitCombatTypes)iI);
			}

			if (m_bCanMergeSplit && GC.getUnitCombatInfo((UnitCombatTypes)iI).isCannotMergeSplit())
			{
				m_bCanMergeSplit = false;
			}
		}
	}
	// ! Size Matters
	{
		UnitTypes eUnit = static_cast<UnitTypes>(iThis);

		std::vector<UnitTypes> aUpgradeUnits;
		do
		{
			if (eUnit != NO_UNIT)
			{
				for (int iI = 0; iI < GC.getUnitInfo(eUnit).getNumUnitUpgrades(); iI++)
				{
					addUnitToUpgradeChain(GC.getUnitInfo(eUnit).getUnitUpgrade(iI));
					aUpgradeUnits.push_back(static_cast<UnitTypes>(GC.getUnitInfo(eUnit).getUnitUpgrade(iI)));
				}
			}
			if (aUpgradeUnits.empty())
			{
				break;
			}
			eUnit = aUpgradeUnits.front();
			aUpgradeUnits.erase(aUpgradeUnits.begin());
		}
		while (true);
	}
}


bool CvUnitInfo::hasUnitCombat(UnitCombatTypes eUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eUnitCombat);

	if (m_abHasCombatType == NULL)
	{
		m_abHasCombatType = new bool[GC.getNumUnitCombatInfos()];
		memset(m_abHasCombatType, 0, GC.getNumUnitCombatInfos());

		m_abHasCombatType[getUnitCombatType()] = true;

		foreach_(const UnitCombatTypes eSubCombat, getSubCombatTypes())
		{
			m_abHasCombatType[eSubCombat] = true;
		}
	}
	return m_abHasCombatType[eUnitCombat];
}


int CvUnitInfo::getTotalModifiedCombatStrength100(const bool bSizeMatters) const
{
	const int iStr = 100 * ((getDomainType() == DOMAIN_AIR ? m_iAirCombat : m_iCombat) + m_iTotalCombatStrengthChangeBase);

	if (iStr < 1)
	{
		return 0;
	}
	if (!bSizeMatters || m_iTotalCombatStrengthModifierBase == 0)
	{
		return iStr;
	}
	return std::max(1, applySMRank(iStr, m_iTotalCombatStrengthModifierBase, GC.getSIZE_MATTERS_MOST_MULTIPLIER()));
}


int CvUnitInfo::getBaseGroupRank() const
{
	return m_iBaseGroupRank;
}

int CvUnitInfo::getBaseCargoVolume() const
{
	return m_iBaseCargoVolume;
}


bool CvUnitInfo::isQualifiedPromotionType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), i);
	return algo::any_of_equal(m_aiQualifiedPromotionTypes, i);
}

bool CvUnitInfo::setQualifiedPromotionType(const int iPromo, std::vector<int>& checklist)
{
	PROFILE_EXTRA_FUNC();
	if (iPromo == -1 || isQualifiedPromotionType(iPromo))
	{
		return true;
	}
	// infinite recursion sanity valve - Allow a couple revisits.
	if (std::count(checklist.begin(), checklist.end(), iPromo) > 2)
	{
		return false; // A sensible conclusion at this point.
	}
	checklist.push_back(iPromo);

	const CvPromotionInfo& promo = GC.getPromotionInfo(static_cast<PromotionTypes>(iPromo));

	if (setQualifiedPromotionType((int)promo.getPrereqPromotion(), checklist)
	&& (setQualifiedPromotionType((int)promo.getPrereqOrPromotion1(), checklist)
	||  setQualifiedPromotionType((int)promo.getPrereqOrPromotion2(), checklist)))
	{
		// There is a theoretical possibility that this promo got qualified in any of the three above recursion calls..
		if (isQualifiedPromotionType(iPromo))
		{
			return true;
		}
		for (int iI = 0; iI < promo.getNumDisqualifiedUnitCombatTypes(); iI++)
		{
			if (hasUnitCombat((UnitCombatTypes)promo.getDisqualifiedUnitCombatType(iI)))
			{
				return false;
			}
		}
		for (int iI = 0; iI < promo.getNumQualifiedUnitCombatTypes(); iI++)
		{
			if (hasUnitCombat((UnitCombatTypes)promo.getQualifiedUnitCombatType(iI)))
			{
				const PromotionLineTypes ePromotionline = promo.getPromotionLine();
				if (ePromotionline != NO_PROMOTIONLINE)
				{
					for (int iK = 0; iK < GC.getPromotionLineInfo(ePromotionline).getNumNotOnDomainTypes(); iK++)
					{
						if (m_iDomainType == GC.getPromotionLineInfo(ePromotionline).getNotOnDomainType(iK))
						{
							return false;
						}
					}
				}
				for (int iK = 0; iK < promo.getNumNotOnDomainTypes(); iK++)
				{
					if (m_iDomainType == promo.getNotOnDomainType(iK))
					{
						return false;
					}
				}
				m_aiQualifiedPromotionTypes.push_back(iPromo);
				return true;
			}
		}
	}
	return false;
}
void CvUnitInfo::setQualifiedPromotionTypes()
{
	PROFILE_EXTRA_FUNC();
	m_aiQualifiedPromotionTypes.clear();

	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		std::vector<int> checklist;
		checklist.clear();
		setQualifiedPromotionType(iI, checklist);
	}
}


void CvUnitInfo::setCanAnimalIgnores()
{
	PROFILE_EXTRA_FUNC();
	int iCount = getAnimalIgnoresBorders();

	const UnitCombatTypes eUnitCombat = (UnitCombatTypes)getUnitCombatType();
	if (eUnitCombat != NO_UNITCOMBAT)
	{
		iCount += GC.getUnitCombatInfo(eUnitCombat).getAnimalIgnoresBordersChange();
	}
	foreach_(const UnitCombatTypes eSubCombat, getSubCombatTypes())
	{
		iCount += GC.getUnitCombatInfo(eSubCombat).getAnimalIgnoresBordersChange();
	}
	m_bCanAnimalIgnoresBorders = (iCount > 0);
	m_bCanAnimalIgnoresImprovements = (iCount > 1);
	m_bCanAnimalIgnoresCities = (iCount > 2);
}

