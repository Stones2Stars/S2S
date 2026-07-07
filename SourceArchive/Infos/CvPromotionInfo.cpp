//------------------------------------------------------------------------------------------------
//  FILE:    CvPromotionInfo.cpp
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
#include "AI/CvPlayerAI.h"
#include "Infrastructure/CvPython.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvXMLLoadUtilityModTools.h"
#include "Tools/CheckSum.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvPromotionInfo.h"


//======================================================================================================
//					CvPromotionInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvPromotionInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
// All XML-backed fields declared in getDataMembers are initialized by initDataMembers();
// the init list keeps only the hand-written remainder (see getDataMembers for the split).
CvPromotionInfo::CvPromotionInfo() :
m_iTechPrereq(NO_TECH),
m_iCommandType(NO_COMMAND), // runtime field, set post-load via setCommandType (CvXMLLoadUtilitySet)
m_piTerrainAttackPercent(NULL),
m_piTerrainDefensePercent(NULL),
m_piFeatureAttackPercent(NULL),
m_piFeatureDefensePercent(NULL),
m_piUnitCombatModifierPercent(NULL),
m_piDomainModifierPercent(NULL),
//m_piAIWeightbyUnitCombatTypes(NULL),
//ls612: Terrain Work modifiers
m_piTerrainWorkPercent(NULL),
m_piFeatureWorkPercent(NULL)
{
	CvInfoUtil(this).initDataMembers();

	m_zobristValue = GC.getGame().getSorenRand().getInt();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvPromotionInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------

CvPromotionInfo::~CvPromotionInfo()
{
	CvInfoUtil(this).uninitDataMembers();

	SAFE_DELETE_ARRAY(m_piTerrainAttackPercent);
	SAFE_DELETE_ARRAY(m_piTerrainDefensePercent);
	SAFE_DELETE_ARRAY(m_piFeatureAttackPercent);
	SAFE_DELETE_ARRAY(m_piFeatureDefensePercent);
	SAFE_DELETE_ARRAY(m_piUnitCombatModifierPercent);
	SAFE_DELETE_ARRAY(m_piDomainModifierPercent);
	SAFE_DELETE_ARRAY(m_piTerrainWorkPercent);
	SAFE_DELETE_ARRAY(m_piFeatureWorkPercent);

	//GC.removeDelayedResolutionVector(m_vPromotionOverwrites);
	GC.removeDelayedResolutionVector(m_aiAddsBuildTypes);
	GC.removeDelayedResolutionVector(m_aiPrereqLocalBuildingTypes);
	GC.removeDelayedResolutionVector(m_aiTrapSetWithPromotionTypes);
}


int CvPromotionInfo::getLayerAnimationPath() const
{
	return m_iLayerAnimationPath;
}


TechTypes CvPromotionInfo::getTechPrereq() const
{
	if (m_iTechPrereq == NO_TECH)
	{
		if (getPromotionLine() == NO_PROMOTIONLINE)
		{
			return m_iTechPrereq;
		}
		return GC.getPromotionLineInfo(getPromotionLine()).getPrereqTech();
	}
	// Sets up the Tech Prereq on the Promotion Definition as a potential override to the Line Tech Prereq
	return m_iTechPrereq;
}


int CvPromotionInfo::getStateReligionPrereq() const
{
	return m_iStateReligionPrereq;
}


int CvPromotionInfo::getMinEraType() const
{
	return m_iMinEraType;
}


int CvPromotionInfo::getMaxEraType() const
{
	return m_iMaxEraType;
}


int CvPromotionInfo::getVisibilityChange() const
{
	return m_iVisibilityChange;
}


int CvPromotionInfo::getMovesChange() const
{
	return m_iMovesChange;
}


int CvPromotionInfo::getMoveDiscountChange() const
{
	return m_iMoveDiscountChange;
}


int CvPromotionInfo::getAirRangeChange() const
{
	return m_iAirRangeChange;
}


int CvPromotionInfo::getInterceptChange() const
{
	return m_iInterceptChange;
}


int CvPromotionInfo::getEvasionChange() const
{
	return m_iEvasionChange;
}


int CvPromotionInfo::getWithdrawalChange() const
{
	return m_iWithdrawalChange;
}


int CvPromotionInfo::getCargoChange() const
{
	return m_iCargoChange;
}


int CvPromotionInfo::getSMCargoChange() const
{
	return m_iSMCargoChange;
}


int CvPromotionInfo::getSMCargoVolumeChange() const
{
	return m_iSMCargoVolumeChange;
}


int CvPromotionInfo::getSMCargoVolumeModifierChange() const
{
	return m_iSMCargoVolumeModifierChange;
}


int CvPromotionInfo::getCollateralDamageChange() const
{
	return m_iCollateralDamageChange;
}


int CvPromotionInfo::getBombardRateChange() const
{
	return m_iBombardRateChange;
}


int CvPromotionInfo::getFirstStrikesChange() const
{
	return m_iFirstStrikesChange;
}


int CvPromotionInfo::getChanceFirstStrikesChange() const
{
	return m_iChanceFirstStrikesChange;
}


int CvPromotionInfo::getEnemyHealChange() const
{
	return m_iEnemyHealChange;
}


int CvPromotionInfo::getNeutralHealChange() const
{
	return m_iNeutralHealChange;
}


int CvPromotionInfo::getFriendlyHealChange() const
{
	return m_iFriendlyHealChange;
}


int CvPromotionInfo::getSameTileHealChange() const
{
	return m_iSameTileHealChange;
}


int CvPromotionInfo::getAdjacentTileHealChange() const
{
	return m_iAdjacentTileHealChange;
}


int CvPromotionInfo::getCombatPercent() const
{
	return m_iCombatPercent;
}


int CvPromotionInfo::getCityAttackPercent() const
{
	return m_iCityAttackPercent;
}


int CvPromotionInfo::getCityDefensePercent() const
{
	return m_iCityDefensePercent;
}


int CvPromotionInfo::getHillsAttackPercent() const
{
	return m_iHillsAttackPercent;
}


int CvPromotionInfo::getHillsDefensePercent() const
{
	return m_iHillsDefensePercent;
}


int CvPromotionInfo::getHillsWorkPercent() const
{
	return m_iHillsWorkPercent;
}


int CvPromotionInfo::getPeaksWorkPercent() const
{
	return m_iPeaksWorkPercent;
}


int CvPromotionInfo::getWorkRatePercent() const
{
	return m_iWorkRatePercent;
}


int CvPromotionInfo::getCommandType() const
{
	return m_iCommandType;
}


void CvPromotionInfo::setCommandType(int iNewType)
{
	m_iCommandType = iNewType;
}


int CvPromotionInfo::getRevoltProtection() const
{
	return m_iRevoltProtection;
}


int CvPromotionInfo::getCollateralDamageProtection() const
{
	return m_iCollateralDamageProtection;
}


int CvPromotionInfo::getPillageChange() const
{
	return m_iPillageChange;
}


int CvPromotionInfo::getUpgradeDiscount() const
{
	return m_iUpgradeDiscount;
}


int CvPromotionInfo::getExperiencePercent() const
{
	return m_iExperiencePercent;
}


int CvPromotionInfo::getKamikazePercent() const
{
	return m_iKamikazePercent;
}


int CvPromotionInfo::getAirCombatLimitChange() const
{
	return m_iAirCombatLimitChange;
}


int CvPromotionInfo::getCelebrityHappy() const
{
	return m_iCelebrityHappy;
}


int CvPromotionInfo::getCollateralDamageLimitChange() const
{
	return m_iCollateralDamageLimitChange;
}


int CvPromotionInfo::getCollateralDamageMaxUnitsChange() const
{
	return m_iCollateralDamageMaxUnitsChange;
}


int CvPromotionInfo::getCombatLimitChange() const
{
	return m_iCombatLimitChange;
}


int CvPromotionInfo::getExtraDropRange() const
{
	return m_iExtraDropRange;
}


int CvPromotionInfo::getSurvivorChance() const
{
	return m_iSurvivorChance;
}


int CvPromotionInfo::getVictoryAdjacentHeal() const
{
	return m_iVictoryAdjacentHeal;
}


int CvPromotionInfo::getVictoryHeal() const
{
	return m_iVictoryHeal;
}


int CvPromotionInfo::getVictoryStackHeal() const
{
	return m_iVictoryStackHeal;
}


bool CvPromotionInfo::isDefensiveVictoryMove() const
{
	return m_bDefensiveVictoryMove;
}


bool CvPromotionInfo::isFreeDrop() const
{
	return m_bFreeDrop;
}


bool CvPromotionInfo::isOffensiveVictoryMove() const
{
	return m_bOffensiveVictoryMove;
}


bool CvPromotionInfo::isOneUp() const
{
	return m_bOneUp;
}


bool CvPromotionInfo::isPillageEspionage() const
{
	return m_bPillageEspionage;
}


bool CvPromotionInfo::isPillageMarauder() const
{
	return m_bPillageMarauder;
}


bool CvPromotionInfo::isPillageOnMove() const
{
	return m_bPillageOnMove;
}


bool CvPromotionInfo::isPillageOnVictory() const
{
	return m_bPillageOnVictory;
}


bool CvPromotionInfo::isPillageResearch() const
{
	return m_bPillageResearch;
}


bool CvPromotionInfo::isLeader() const
{
	return m_bLeader;
}


bool CvPromotionInfo::isBlitz() const
{
	return m_bBlitz;
}


bool CvPromotionInfo::isAmphib() const
{
	return m_bAmphib;
}


bool CvPromotionInfo::isRiver() const
{
	return m_bRiver;
}


bool CvPromotionInfo::isEnemyRoute() const
{
	return m_bEnemyRoute;
}


bool CvPromotionInfo::isAlwaysHeal() const
{
	return m_bAlwaysHeal;
}


bool CvPromotionInfo::isHillsDoubleMove() const
{
	return m_bHillsDoubleMove;
}


bool CvPromotionInfo::isImmuneToFirstStrikes() const
{
	return m_bImmuneToFirstStrikes;
}


const char* CvPromotionInfo::getSound() const
{
	return m_szSound;
}


bool CvPromotionInfo::changesMoveThroughPlots() const
{
	return (isAmphib() ||
			isCanMovePeaks() ||
			isCanLeadThroughPeaks() ||
			!m_aeTerrainDoubleMove.empty() ||
			!m_aeFeatureDoubleMove.empty() ||
			m_bHillsDoubleMove);
}


int CvPromotionInfo::getTerrainAttackPercent(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return m_piTerrainAttackPercent ? m_piTerrainAttackPercent[i] : 0;
}


bool CvPromotionInfo::isAnyTerrainAttackPercent() const
{
	return m_piTerrainAttackPercent != NULL;
}


int CvPromotionInfo::getTerrainDefensePercent(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return m_piTerrainDefensePercent ? m_piTerrainDefensePercent[i] : 0;
}


bool CvPromotionInfo::isAnyTerrainDefensePercent() const
{
	return m_piTerrainDefensePercent != NULL;
}


int CvPromotionInfo::getFeatureAttackPercent(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return m_piFeatureAttackPercent ? m_piFeatureAttackPercent[i] : 0;
}


bool CvPromotionInfo::isAnyFeatureAttackPercent() const
{
	return m_piFeatureAttackPercent != NULL;
}


int CvPromotionInfo::getFeatureDefensePercent(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return m_piFeatureDefensePercent ? m_piFeatureDefensePercent[i] : 0;
}


bool CvPromotionInfo::isAnyFeatureDefensePercent() const
{
	return m_piFeatureDefensePercent != NULL;
}


int CvPromotionInfo::getUnitCombatModifierPercent(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return m_piUnitCombatModifierPercent ? m_piUnitCombatModifierPercent[i] : 0;
}


bool CvPromotionInfo::isAnyUnitCombatModifierPercent() const
{
	return m_piUnitCombatModifierPercent != NULL;
}


int CvPromotionInfo::getDomainModifierPercent(int i) const
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, i);
	return m_piDomainModifierPercent ? m_piDomainModifierPercent[i] : 0;
}


bool CvPromotionInfo::isAnyDomainModifierPercent() const
{
	return m_piDomainModifierPercent != NULL;
}


bool CvPromotionInfo::getTerrainDoubleMove(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return algo::any_of_equal(m_aeTerrainDoubleMove, static_cast<TerrainTypes>(i));
}


bool CvPromotionInfo::getFeatureDoubleMove(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return algo::any_of_equal(m_aeFeatureDoubleMove, static_cast<FeatureTypes>(i));
}


bool CvPromotionInfo::getUnitCombat(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aeUnitCombat, static_cast<UnitCombatTypes>(i));
}

//ls612: Terrain Work Modifiers
int CvPromotionInfo::getTerrainWorkPercent(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return m_piTerrainWorkPercent ? m_piTerrainWorkPercent[i] : 0;
}


int CvPromotionInfo::getFeatureWorkPercent(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return m_piFeatureWorkPercent ? m_piFeatureWorkPercent[i] : 0;
}


bool CvPromotionInfo::isCanMovePeaks() const
{
	return m_bCanMovePeaks;
}


//	Koshling - enhanced mountaineering mode to differentiate between ability to move through
//	mountains, and ability to lead a stack through mountains
bool CvPromotionInfo::isCanLeadThroughPeaks() const
{
	return m_bCanLeadThroughPeaks;
}


TechTypes CvPromotionInfo::getObsoleteTech() const
{
	if (m_iObsoleteTech == NO_TECH)
	{
		if (getPromotionLine() == NO_PROMOTIONLINE)
		{
			return m_iObsoleteTech;
		}
		return GC.getPromotionLineInfo(getPromotionLine()).getObsoleteTech();
	}
	//Sets up the Tech Prereq on the Promotion Definition as a potential override to the Line Tech Obsoletion
	return m_iObsoleteTech;
}


int CvPromotionInfo::getControlPoints() const
{
	return m_iControlPoints;
}


int CvPromotionInfo::getCommandRange() const
{
	return m_iCommandRange;
}


bool CvPromotionInfo::isZoneOfControl() const
{
	return m_bZoneOfControl;
}


//TB Combat Mods Begin  TB SubCombat Mod begin
const wchar_t* CvPromotionInfo::getRenamesUnitTo() const
{
	return m_szRenamesUnitTo;
}


//Textual References
PromotionLineTypes CvPromotionInfo::getPromotionLine() const
{
	return m_ePromotionLine;
}


UnitCombatTypes CvPromotionInfo::getReplacesUnitCombat() const
{
	return m_eReplacesUnitCombat;
}


DomainTypes CvPromotionInfo::getDomainCargoChange() const
{
	return m_eDomainCargoChange;
}


SpecialUnitTypes CvPromotionInfo::getSpecialCargoChange() const
{
	return m_eSpecialCargoChange;
}


SpecialUnitTypes CvPromotionInfo::getSpecialCargoPrereq() const
{
	return m_eSpecialCargoPrereq;
}


SpecialUnitTypes CvPromotionInfo::getSMNotSpecialCargoChange() const
{
	return m_eSMNotSpecialCargoChange;
}


SpecialUnitTypes CvPromotionInfo::getSMNotSpecialCargoPrereq() const
{
	return m_eSMNotSpecialCargoPrereq;
}


SpecialUnitTypes CvPromotionInfo::setSpecialUnit() const
{
	return m_eSetSpecialUnit;
}


// integers
int CvPromotionInfo::getAttackCombatModifierChange() const
{
	return m_iAttackCombatModifierChange;
}


int CvPromotionInfo::getDefenseCombatModifierChange() const
{
	return m_iDefenseCombatModifierChange;
}


int CvPromotionInfo::getVSBarbsChange() const
{
	return m_iVSBarbsChange;
}






//S&D extended
int CvPromotionInfo::getUnnerveChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iUnnerveChange;
}


int CvPromotionInfo::getEncloseChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iEncloseChange;
}


int CvPromotionInfo::getLungeChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iLungeChange;
}


int CvPromotionInfo::getDynamicDefenseChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iDynamicDefenseChange;
}

//

int CvPromotionInfo::getStrengthChange() const
{
	return m_iStrengthChange;
}


int CvPromotionInfo::getLinePriority() const
{
	return m_iLinePriority;
}


int CvPromotionInfo::getDamageperTurn() const
{
	return m_iDamageperTurn;
}


int CvPromotionInfo::getStrAdjperTurn() const
{
	return m_iStrAdjperTurn;
}


int CvPromotionInfo::getWeakenperTurn() const
{
	return m_iWeakenperTurn;
}







int CvPromotionInfo::getEnduranceChange() const
{
	return m_iEnduranceChange;
}




int CvPromotionInfo::getPoisonProbabilityModifierChange() const
{
	return m_iPoisonProbabilityModifierChange;
}



int CvPromotionInfo::getCaptureProbabilityModifierChange() const
{
	return m_iCaptureProbabilityModifierChange;
}


int CvPromotionInfo::getCaptureResistanceModifierChange() const
{
	return m_iCaptureResistanceModifierChange;
}



int CvPromotionInfo::getBreakdownChanceChange() const
{
	return m_iBreakdownChanceChange;
}


int CvPromotionInfo::getBreakdownDamageChange() const
{
	return m_iBreakdownDamageChange;
}


int CvPromotionInfo::getTauntChange() const
{
	return m_iTauntChange;
}


int CvPromotionInfo::getMaxHPChange() const
{
	return m_iMaxHPChange;
}


int CvPromotionInfo::getStrengthModifier() const
{
	return m_iStrengthModifier;
}


int CvPromotionInfo::getQualityChange() const
{
	return m_iQualityChange;
}


int CvPromotionInfo::getGroupChange() const
{
	return m_iGroupChange;
}


int CvPromotionInfo::getLevelPrereq() const
{
	return m_iLevelPrereq;
}


int CvPromotionInfo::getDamageModifierChange() const
{
	return m_iDamageModifierChange;
}


int CvPromotionInfo::getUpkeepModifier() const
{
	return m_iUpkeepModifier;
}


int CvPromotionInfo::getExtraUpkeep100() const
{
	return m_iExtraUpkeep100;
}


int CvPromotionInfo::getRBombardDamageChange() const
{
	return m_iRBombardDamageChange;
}


int CvPromotionInfo::getRBombardDamageLimitChange() const
{
	return m_iRBombardDamageLimitChange;
}


int CvPromotionInfo::getRBombardDamageMaxUnitsChange() const
{
	return m_iRBombardDamageMaxUnitsChange;
}


int CvPromotionInfo::getDCMBombRangeChange() const
{
	return m_iDCMBombRangeChange;
}


int CvPromotionInfo::getDCMBombAccuracyChange() const
{
	return m_iDCMBombAccuracyChange;
}


int CvPromotionInfo::getCombatModifierPerSizeMoreChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerSizeMoreChange;
}


int CvPromotionInfo::getCombatModifierPerSizeLessChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerSizeLessChange;
}


int CvPromotionInfo::getCombatModifierPerVolumeMoreChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerVolumeMoreChange;
}


int CvPromotionInfo::getCombatModifierPerVolumeLessChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerVolumeLessChange;
}


int CvPromotionInfo::getSelfHealModifier() const
{
	return m_iSelfHealModifier;
}


int CvPromotionInfo::getNumHealSupport() const
{
	return m_iNumHealSupport;
}


int CvPromotionInfo::getExcileChange() const
{
	return m_iExcileChange;
}


int CvPromotionInfo::getPassageChange() const
{
	return m_iPassageChange;
}


int CvPromotionInfo::getNoNonOwnedCityEntryChange() const
{
	return m_iNoNonOwnedCityEntryChange;
}


int CvPromotionInfo::getBarbCoExistChange() const
{
	return m_iBarbCoExistChange;
}


int CvPromotionInfo::getBlendIntoCityChange() const
{
	return m_iBlendIntoCityChange;
}


int CvPromotionInfo::getUpgradeAnywhereChange() const
{
	return m_iUpgradeAnywhereChange;
}


int CvPromotionInfo::getInsidiousnessChange() const
{
	return m_iInsidiousnessChange;
}


int CvPromotionInfo::getInvestigationChange() const
{
	return m_iInvestigationChange;
}


int CvPromotionInfo::getAssassinChange() const
{
	return m_iAssassinChange;
}


int CvPromotionInfo::getStealthStrikesChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	return m_iStealthStrikesChange;
}


int CvPromotionInfo::getStealthCombatModifierChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	return m_iStealthCombatModifierChange;
}


int CvPromotionInfo::getStealthDefenseChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	return m_iStealthDefenseChange;
}


int CvPromotionInfo::getDefenseOnlyChange() const
{
	return m_iDefenseOnlyChange;
}


int CvPromotionInfo::getNoInvisibilityChange() const
{
	return m_iNoInvisibilityChange;
}


int CvPromotionInfo::getTrapDamageMax() const
{
	return m_iTrapDamageMax;
}


int CvPromotionInfo::getTrapDamageMin() const
{
	return m_iTrapDamageMin;
}


int CvPromotionInfo::getTrapComplexity() const
{
	return m_iTrapComplexity;
}


int CvPromotionInfo::getNumTriggers() const
{
	return m_iNumTriggers;
}


int CvPromotionInfo::getTriggerBeforeAttackChange() const
{
	return m_iTriggerBeforeAttackChange;
}


int CvPromotionInfo::getHiddenNationalityChange() const
{
	return m_iHiddenNationalityChange;
}


int CvPromotionInfo::getAnimalIgnoresBordersChange() const
{
	return m_iAnimalIgnoresBordersChange;
}


int CvPromotionInfo::getNoDefensiveBonusChange() const
{
	return m_iNoDefensiveBonusChange;
}


int CvPromotionInfo::getGatherHerdChange() const
{
	return m_iGatherHerdChange;
}


int CvPromotionInfo::getReligiousCombatModifierChange() const
{
	return m_iReligiousCombatModifierChange;
}

// booleans
bool CvPromotionInfo::isStampedeChange() const
{
	return m_bStampedeChange;
}


bool CvPromotionInfo::isRemoveStampede() const
{
	return m_bRemoveStampede;
}


bool CvPromotionInfo::isOnslaughtChange() const
{
	return m_bOnslaughtChange;
}




bool CvPromotionInfo::isParalyze() const
{
	return m_bParalyze;
}




bool CvPromotionInfo::isAttackOnlyCitiesAdd() const
{
	return m_bAttackOnlyCitiesAdd;
}


bool CvPromotionInfo::isAttackOnlyCitiesSubtract() const
{
	return m_bAttackOnlyCitiesSubtract;
}


bool CvPromotionInfo::isIgnoreNoEntryLevelAdd() const
{
	return m_bIgnoreNoEntryLevelAdd;
}


bool CvPromotionInfo::isIgnoreNoEntryLevelSubtract() const
{
	return m_bIgnoreNoEntryLevelSubtract;
}


bool CvPromotionInfo::isIgnoreZoneofControlAdd() const
{
	return m_bIgnoreZoneofControlAdd;
}


bool CvPromotionInfo::isIgnoreZoneofControlSubtract() const
{
	return m_bIgnoreZoneofControlSubtract;
}


bool CvPromotionInfo::isFliesToMoveAdd() const
{
	return m_bFliesToMoveAdd;
}


bool CvPromotionInfo::isFliesToMoveSubtract() const
{
	return m_bFliesToMoveSubtract;
}


bool CvPromotionInfo::isZeroesXP() const
{
	return m_bZeroesXP;
}


bool CvPromotionInfo::isForOffset() const
{
	return m_bForOffset;
}


bool CvPromotionInfo::isCargoPrereq() const
{
	return m_bCargoPrereq;
}


bool CvPromotionInfo::isRBombardPrereq() const
{
	return m_bRBombardPrereq;
}


bool CvPromotionInfo::isNoSelfHeal() const
{
	return m_bNoSelfHeal;
}


bool CvPromotionInfo::isSetOnHNCapture() const
{
	return m_bSetOnHNCapture;
}


bool CvPromotionInfo::isSetOnInvestigated() const
{
	return m_bSetOnInvestigated;
}


bool CvPromotionInfo::isStatus() const
{
	return m_bStatus;
}


bool CvPromotionInfo::isPrereqNormInvisible() const
{
	return m_bPrereqNormInvisible;
}


bool CvPromotionInfo::isPlotPrereqsKeepAfter() const
{
	return m_bPlotPrereqsKeepAfter;
}


bool CvPromotionInfo::isRemoveAfterSet() const
{
	return m_bRemoveAfterSet;
}


bool CvPromotionInfo::isQuick() const
{
	return m_bQuick;
}


// bool vectors without delayed resolution
int CvPromotionInfo::getSubCombatChangeType(int i) const
{
	return m_aiSubCombatChangeTypes[i];
}


int CvPromotionInfo::getNumSubCombatChangeTypes() const
{
	return (int)m_aiSubCombatChangeTypes.size();
}


bool CvPromotionInfo::isSubCombatChangeType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiSubCombatChangeTypes, i);
}


int CvPromotionInfo::getRemovesUnitCombatType(int i) const
{
	return m_aiRemovesUnitCombatTypes[i];
}


int CvPromotionInfo::getNumRemovesUnitCombatTypes() const
{
	return (int)m_aiRemovesUnitCombatTypes.size();
}


bool CvPromotionInfo::isRemovesUnitCombatType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiRemovesUnitCombatTypes, i);
}


int CvPromotionInfo::getOnGameOption(int i) const
{
	return m_aiOnGameOptions[i];
}


int CvPromotionInfo::getNumOnGameOptions() const
{
	return (int)m_aiOnGameOptions.size();
}


bool CvPromotionInfo::isOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiOnGameOptions, i);
}


int CvPromotionInfo::getNotOnGameOption(int i) const
{
	return m_aiNotOnGameOptions[i];
}


int CvPromotionInfo::getNumNotOnGameOptions() const
{
	return (int)m_aiNotOnGameOptions.size();
}


bool CvPromotionInfo::isNotOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiNotOnGameOptions, i);
}


int CvPromotionInfo::getFreetoUnitCombat(int i) const
{
	return m_aiFreetoUnitCombats[i];
}


int CvPromotionInfo::getNumFreetoUnitCombats() const
{
	return (int)m_aiFreetoUnitCombats.size();
}


bool CvPromotionInfo::isFreetoUnitCombat(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiFreetoUnitCombats, i);
}


int CvPromotionInfo::getNotOnDomainType(int i) const
{
	return m_aiNotOnDomainTypes[i];
}


int CvPromotionInfo::getNumNotOnDomainTypes() const
{
	return (int)m_aiNotOnDomainTypes.size();
}


bool CvPromotionInfo::isNotOnDomainType(int i) const
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, i);
	return algo::any_of_equal(m_aiNotOnDomainTypes, i);
}







int CvPromotionInfo::getAddsBuildType(int i) const
{
	return m_aiAddsBuildTypes[i];
}


int CvPromotionInfo::getNumAddsBuildTypes() const
{
	return (int)m_aiAddsBuildTypes.size();
}


bool CvPromotionInfo::isAddsBuildType(int i) const
{
	return algo::any_of_equal(m_aiAddsBuildTypes, i);
}


int CvPromotionInfo::getNegatesInvisibilityType(int i) const
{
	return m_aiNegatesInvisibilityTypes[i];
}


int CvPromotionInfo::getNumNegatesInvisibilityTypes() const
{
	return (int)m_aiNegatesInvisibilityTypes.size();
}


bool CvPromotionInfo::isNegatesInvisibilityType(int i) const
{
	return algo::any_of_equal(m_aiNegatesInvisibilityTypes, i);
}


int CvPromotionInfo::getPrereqTerrainType(int i) const
{
	return m_aiPrereqTerrainTypes[i];
}


int CvPromotionInfo::getNumPrereqTerrainTypes() const
{
	return (int)m_aiPrereqTerrainTypes.size();
}


bool CvPromotionInfo::isPrereqTerrainType(int i) const
{
	return algo::any_of_equal(m_aiPrereqTerrainTypes, i);
}


int CvPromotionInfo::getPrereqFeatureType(int i) const
{
	return m_aiPrereqFeatureTypes[i];
}


int CvPromotionInfo::getNumPrereqFeatureTypes() const
{
	return (int)m_aiPrereqFeatureTypes.size();
}


bool CvPromotionInfo::isPrereqFeatureType(int i) const
{
	return algo::any_of_equal(m_aiPrereqFeatureTypes, i);
}


int CvPromotionInfo::getPrereqImprovementType(int i) const
{
	return m_aiPrereqImprovementTypes[i];
}


int CvPromotionInfo::getNumPrereqImprovementTypes() const
{
	return (int)m_aiPrereqImprovementTypes.size();
}


bool CvPromotionInfo::isPrereqImprovementType(int i) const
{
	return algo::any_of_equal(m_aiPrereqImprovementTypes, i);
}


int CvPromotionInfo::getPrereqPlotBonusType(int i) const
{
	return m_aiPrereqPlotBonusTypes[i];
}


int CvPromotionInfo::getNumPrereqPlotBonusTypes() const
{
	return (int)m_aiPrereqPlotBonusTypes.size();
}


bool CvPromotionInfo::isPrereqPlotBonusType(int i) const
{
	return algo::any_of_equal(m_aiPrereqPlotBonusTypes, i);
}


int CvPromotionInfo::getPrereqLocalBuildingType(int i) const
{
	return m_aiPrereqLocalBuildingTypes[i];
}


int CvPromotionInfo::getNumPrereqLocalBuildingTypes() const
{
	return (int)m_aiPrereqLocalBuildingTypes.size();
}


bool CvPromotionInfo::isPrereqLocalBuildingType(int i) const
{
	return algo::any_of_equal(m_aiPrereqLocalBuildingTypes, i);
}


int CvPromotionInfo::getTrapSetWithPromotionType(int i) const
{
	return m_aiTrapSetWithPromotionTypes[i];
}


int CvPromotionInfo::getNumTrapSetWithPromotionTypes() const
{
	return (int)m_aiTrapSetWithPromotionTypes.size();
}


bool CvPromotionInfo::isTrapSetWithPromotionType(int i) const
{
	return algo::any_of_equal(m_aiTrapSetWithPromotionTypes, i);
}


int CvPromotionInfo::getTrapImmunityUnitCombatType(int i) const
{
	return m_aiTrapImmunityUnitCombatTypes[i];
}


int CvPromotionInfo::getNumTrapImmunityUnitCombatTypes() const
{
	return (int)m_aiTrapImmunityUnitCombatTypes.size();
}


bool CvPromotionInfo::isTrapImmunityUnitCombatType(int i) const
{
	return algo::any_of_equal(m_aiTrapImmunityUnitCombatTypes, i);
}


int CvPromotionInfo::getTargetUnitCombatType(int i) const
{
	return m_aiTargetUnitCombatTypes[i];
}


int CvPromotionInfo::getNumTargetUnitCombatTypes() const
{
	return (int)m_aiTargetUnitCombatTypes.size();
}


bool CvPromotionInfo::isTargetUnitCombatType(int i) const
{
	return algo::any_of_equal(m_aiTargetUnitCombatTypes, i);
}


int CvPromotionInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvPromotionInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvPromotionInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


// int vectors utilizing pairing without delayed resolution
int CvPromotionInfo::getNumFlankingStrikesbyUnitCombatTypesChange() const
{
	return m_aFlankingStrengthbyUnitCombatTypeChange.size();
}


int CvPromotionInfo::getFlankingStrengthbyUnitCombatTypeChange(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aFlankingStrengthbyUnitCombatTypeChange.begin(); it != m_aFlankingStrengthbyUnitCombatTypeChange.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPromotionInfo::isFlankingStrikebyUnitCombatTypeChange(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aFlankingStrengthbyUnitCombatTypeChange.begin(); it != m_aFlankingStrengthbyUnitCombatTypeChange.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return true;
		}
	}
	return false;
}


int CvPromotionInfo::getNumTrapDisableUnitCombatTypes() const
{
	return m_aTrapDisableUnitCombatTypes.size();
}


int CvPromotionInfo::getTrapDisableUnitCombatType(int iUnitCombat) const
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


bool CvPromotionInfo::isTrapDisableUnitCombatType(int iUnitCombat) const
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


int CvPromotionInfo::getNumTrapAvoidanceUnitCombatTypes() const
{
	return m_aTrapAvoidanceUnitCombatTypes.size();
}


int CvPromotionInfo::getTrapAvoidanceUnitCombatType(int iUnitCombat) const
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


bool CvPromotionInfo::isTrapAvoidanceUnitCombatType(int iUnitCombat) const
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


int CvPromotionInfo::getNumTrapTriggerUnitCombatTypes() const
{
	return m_aTrapTriggerUnitCombatTypes.size();
}


int CvPromotionInfo::getTrapTriggerUnitCombatType(int iUnitCombat) const
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


bool CvPromotionInfo::isTrapTriggerUnitCombatType(int iUnitCombat) const
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





int CvPromotionInfo::getNumBuildWorkRateModifierChangeTypes() const
{
	return m_aBuildWorkRateModifierChangeTypes.size();
}


int CvPromotionInfo::getBuildWorkRateModifierChangeType(int iBuild) const
{
	PROFILE_EXTRA_FUNC();
	for (BuildModifierArray::const_iterator it = m_aBuildWorkRateModifierChangeTypes.begin(); it != m_aBuildWorkRateModifierChangeTypes.end(); ++it)
	{
		if ((*it).first == (BuildTypes)iBuild)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPromotionInfo::isBuildWorkRateModifierChangeType(int iBuild) const
{
	PROFILE_EXTRA_FUNC();
	for (BuildModifierArray::const_iterator it = m_aBuildWorkRateModifierChangeTypes.begin(); it != m_aBuildWorkRateModifierChangeTypes.end(); ++it)
	{
		if ((*it).first == (BuildTypes)iBuild)
		{
			return true;
		}
	}
	return false;
}


int CvPromotionInfo::getNumVisibilityIntensityChangeTypes() const
{
	return m_aVisibilityIntensityChangeTypes.size();
}


int CvPromotionInfo::getVisibilityIntensityChangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensityChangeTypes.begin(); it != m_aVisibilityIntensityChangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPromotionInfo::isVisibilityIntensityChangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensityChangeTypes.begin(); it != m_aVisibilityIntensityChangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return true;
		}
	}
	return false;
}


int CvPromotionInfo::getNumInvisibilityIntensityChangeTypes() const
{
	return m_aInvisibilityIntensityChangeTypes.size();
}


int CvPromotionInfo::getInvisibilityIntensityChangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aInvisibilityIntensityChangeTypes.begin(); it != m_aInvisibilityIntensityChangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPromotionInfo::isInvisibilityIntensityChangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aInvisibilityIntensityChangeTypes.begin(); it != m_aInvisibilityIntensityChangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return true;
		}
	}
	return false;
}


int CvPromotionInfo::getNumVisibilityIntensityRangeChangeTypes() const
{
	return m_aVisibilityIntensityRangeChangeTypes.size();
}


int CvPromotionInfo::getVisibilityIntensityRangeChangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensityRangeChangeTypes.begin(); it != m_aVisibilityIntensityRangeChangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPromotionInfo::isVisibilityIntensityRangeChangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensityRangeChangeTypes.begin(); it != m_aVisibilityIntensityRangeChangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return true;
		}
	}
	return false;
}


int CvPromotionInfo::getNumAIWeightbyUnitCombatTypes() const
{
	return (int)m_aAIWeightbyUnitCombatTypes.size();
}


const UnitCombatModifier& CvPromotionInfo::getAIWeightbyUnitCombatType(int iUnitCombat) const
{
	FASSERT_BOUNDS(0, getNumAIWeightbyUnitCombatTypes(), iUnitCombat);
	return m_aAIWeightbyUnitCombatTypes[iUnitCombat];
}






int CvPromotionInfo::getNumInvisibleTerrainChanges() const
{
	return (int)m_aInvisibleTerrainChanges.size();
}


const InvisibleTerrainChanges& CvPromotionInfo::getInvisibleTerrainChange(int iIndex) const
{
	return m_aInvisibleTerrainChanges[iIndex];
}


int CvPromotionInfo::getNumInvisibleFeatureChanges() const
{
	return (int)m_aInvisibleFeatureChanges.size();
}


const InvisibleFeatureChanges& CvPromotionInfo::getInvisibleFeatureChange(int iIndex) const
{
	return m_aInvisibleFeatureChanges[iIndex];
}


int CvPromotionInfo::getNumInvisibleImprovementChanges() const
{
	return (int)m_aInvisibleImprovementChanges.size();
}


const InvisibleImprovementChanges& CvPromotionInfo::getInvisibleImprovementChange(int iIndex) const
{
	return m_aInvisibleImprovementChanges[iIndex];
}


int CvPromotionInfo::getNumVisibleTerrainChanges() const
{
	return (int)m_aVisibleTerrainChanges.size();
}


const InvisibleTerrainChanges& CvPromotionInfo::getVisibleTerrainChange(int iIndex) const
{
	return m_aVisibleTerrainChanges[iIndex];
}


int CvPromotionInfo::getNumVisibleFeatureChanges() const
{
	return (int)m_aVisibleFeatureChanges.size();
}


const InvisibleFeatureChanges& CvPromotionInfo::getVisibleFeatureChange(int iIndex) const
{
	return m_aVisibleFeatureChanges[iIndex];
}


int CvPromotionInfo::getNumVisibleImprovementChanges() const
{
	return (int)m_aVisibleImprovementChanges.size();
}


const InvisibleImprovementChanges& CvPromotionInfo::getVisibleImprovementChange(int iIndex) const
{
	return m_aVisibleImprovementChanges[iIndex];
}


int CvPromotionInfo::getNumVisibleTerrainRangeChanges() const
{
	return (int)m_aVisibleTerrainRangeChanges.size();
}


const InvisibleTerrainChanges& CvPromotionInfo::getVisibleTerrainRangeChange(int iIndex) const
{
	return m_aVisibleTerrainRangeChanges[iIndex];
}


int CvPromotionInfo::getNumVisibleFeatureRangeChanges() const
{
	return (int)m_aVisibleFeatureRangeChanges.size();
}


const InvisibleFeatureChanges& CvPromotionInfo::getVisibleFeatureRangeChange(int iIndex) const
{
	return m_aVisibleFeatureRangeChanges[iIndex];
}


int CvPromotionInfo::getNumVisibleImprovementRangeChanges() const
{
	return (int)m_aVisibleImprovementRangeChanges.size();
}


const InvisibleImprovementChanges& CvPromotionInfo::getVisibleImprovementRangeChange(int iIndex) const
{
	return m_aVisibleImprovementRangeChanges[iIndex];
}


int CvPromotionInfo::getNumHealUnitCombatChangeTypes() const
{
	return (int)m_aHealUnitCombatChangeTypes.size();
}


const HealUnitCombat& CvPromotionInfo::getHealUnitCombatChangeType(int iUnitCombat) const
{
	FASSERT_BOUNDS(0, getNumHealUnitCombatChangeTypes(), iUnitCombat);
	return m_aHealUnitCombatChangeTypes[iUnitCombat];
}


int CvPromotionInfo::getQualifiedUnitCombatType(int i) const
{
	return m_aiQualifiedUnitCombatTypes[i];
}


int CvPromotionInfo::getNumQualifiedUnitCombatTypes() const
{
	return (int)m_aiQualifiedUnitCombatTypes.size();
}


bool CvPromotionInfo::isQualifiedUnitCombatType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiQualifiedUnitCombatTypes, i);
}


void CvPromotionInfo::setQualifiedUnitCombatTypes()
{
	PROFILE_EXTRA_FUNC();
	m_aiQualifiedUnitCombatTypes.clear();
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (getUnitCombat(iI))
		{
			m_aiQualifiedUnitCombatTypes.push_back(iI);
		}
	}
	const PromotionLineTypes ePromotionLine = getPromotionLine();

	if (ePromotionLine > -1)
	{
		for (int iI = 0; iI < GC.getPromotionLineInfo(ePromotionLine).getNumUnitCombatPrereqTypes(); iI++)
		{
			const int iUnitCombat = GC.getPromotionLineInfo(ePromotionLine).getUnitCombatPrereqType(iI);

			if (!isQualifiedUnitCombatType(iUnitCombat))
			{
				m_aiQualifiedUnitCombatTypes.push_back(iUnitCombat);
			}
		}
	}
}


int CvPromotionInfo::getNotOnUnitCombatType(int i) const
{
	return m_aiNotOnUnitCombatTypes[i];
}


int CvPromotionInfo::getNumNotOnUnitCombatTypes() const
{
	return (int)m_aiNotOnUnitCombatTypes.size();
}


bool CvPromotionInfo::isNotOnUnitCombatType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiNotOnUnitCombatTypes, i);
}


int CvPromotionInfo::getDisqualifiedUnitCombatType(int i) const
{
	return m_disqualifiedUnitCombatTypes[i];
}


int CvPromotionInfo::getNumDisqualifiedUnitCombatTypes() const
{
	return (int)m_disqualifiedUnitCombatTypes.size();
}


void CvPromotionInfo::setDisqualifiedUnitCombatTypes()
{
	PROFILE_EXTRA_FUNC();
	m_disqualifiedUnitCombatTypes.clear();
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (isNotOnUnitCombatType(iI))
		{
			if (isQualifiedUnitCombatType(iI))
			{
				std::vector<int>::iterator itr = find(m_aiQualifiedUnitCombatTypes.begin(), m_aiQualifiedUnitCombatTypes.end(), iI);
				if (itr != m_aiQualifiedUnitCombatTypes.end())
				{
					m_aiQualifiedUnitCombatTypes.erase(itr);
				}
			}
			m_disqualifiedUnitCombatTypes.push_back(iI);
		}
	}
	const PromotionLineTypes ePromotionLine = getPromotionLine();

	if (ePromotionLine > -1)
	{
		for (int iI = 0; iI < GC.getPromotionLineInfo(ePromotionLine).getNumNotOnUnitCombatTypes(); iI++)
		{
			const int iUnitCombat = GC.getPromotionLineInfo(ePromotionLine).getNotOnUnitCombatType(iI);

			if (!isNotOnUnitCombatType(iUnitCombat))
			{
				if (isQualifiedUnitCombatType(iUnitCombat))
				{
					std::vector<int>::iterator itr = find(m_aiQualifiedUnitCombatTypes.begin(), m_aiQualifiedUnitCombatTypes.end(), iI);
					if (itr != m_aiQualifiedUnitCombatTypes.end())
					{
						m_aiQualifiedUnitCombatTypes.erase(itr);
					}
				}
				m_disqualifiedUnitCombatTypes.push_back(iUnitCombat);
			}
		}
	}
}


//TB Combat Mods End  TB SubCombat Mod end

bool CvPromotionInfo::hasNegativeEffects() const
{
	return getLungeChange() < 0 ||
		getEnduranceChange() < 0 ||
		getFirstStrikesChange() < 0 ||
		getChanceFirstStrikesChange() < 0 ||
		getVSBarbsChange() < 0 ||
		getStrengthChange() < 0 ||
		getAttackCombatModifierChange() < 0 ||
		getCombatPercent() < 0 ||
		getDefenseOnlyChange() > 0 ||
		getNoInvisibilityChange() > 0 ||
		getHiddenNationalityChange() != 0;
}


// Every wrapper-expressible XML field is declared here (#196/#276). getCheckSum stays
// explicit/legacy-ordered (see the comment there), so declaration order below is free and
// follows the legacy read() order for reviewability.
//
// Kept hand-written in read()/copyNonDefaults() (with the reason at each site):
//  - m_iTechPrereq (copy compares the line-redirecting getter, not the raw member)
//  - m_iCommandType (runtime field, set post-load; checksummed)
//  - the 8 SetVariableListTagPair int* arrays
//  - m_szRenamesUnitTo (CvWString - no wrapper)
//  - the 3 delayed-resolution int vectors (AddsBuildTypes/PrereqLocalBuildingTypes/TrapSetWithPromotionTypes)
//  - the 8 SetOptionalPairVector pair-vectors (no wrapper for plain pair-vectors)
//  - the 11 struct-vectors (CvStructs.h structs lack getDataMembers; VisibleTerrainRangeChanges
//    additionally needs per-element delayed resolution, which addStruct forces to immediate)
void CvPromotionInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.addEnum(m_iPrereqPromotion, L"PromotionPrereq")
		.addEnum(m_iPrereqOrPromotion1, L"PromotionPrereqOr1")
		.addEnum(m_iPrereqOrPromotion2, L"PromotionPrereqOr2")
		.add(m_aeTerrainDoubleMove, L"TerrainDoubleMoves")
		.add(m_aeFeatureDoubleMove, L"FeatureDoubleMoves")
		.add(m_aeUnitCombat, L"UnitCombats")

		.add(m_szSound, L"Sound")
		.addEnumAsInt(m_iLayerAnimationPath, L"LayerAnimationPath")
		.addEnumAsInt(m_iStateReligionPrereq, L"StateReligionPrereq")
		.addEnumAsInt(m_iMinEraType, L"MinEraType")
		.addEnumAsInt(m_iMaxEraType, L"MaxEraType")
		.add(m_bLeader, L"bLeader") // read()/copyNonDefaults() keep the m_bGraphicalOnly side effect
		.add(m_bBlitz, L"bBlitz")
		.add(m_bAmphib, L"bAmphib")
		.add(m_bRiver, L"bRiver")
		.add(m_bEnemyRoute, L"bEnemyRoute")
		.add(m_bAlwaysHeal, L"bAlwaysHeal")
		.add(m_bHillsDoubleMove, L"bHillsDoubleMove")
		.add(m_bImmuneToFirstStrikes, L"bImmuneToFirstStrikes")
		.add(m_iVisibilityChange, L"iVisibilityChange")
		.add(m_iMovesChange, L"iMovesChange")
		.add(m_iMoveDiscountChange, L"iMoveDiscountChange")
		.add(m_iAirRangeChange, L"iAirRangeChange")
		.add(m_iInterceptChange, L"iInterceptChange")
		.add(m_iEvasionChange, L"iEvasionChange")
		.add(m_iWithdrawalChange, L"iWithdrawalChange")
		.add(m_iCargoChange, L"iCargoChange")
		.add(m_iSMCargoChange, L"iSMCargoChange")
		.add(m_iSMCargoVolumeChange, L"iSMCargoVolumeChange")
		.add(m_iSMCargoVolumeModifierChange, L"iSMCargoVolumeModifierChange")
		.add(m_iCollateralDamageChange, L"iCollateralDamageChange")
		.add(m_iBombardRateChange, L"iBombardRateChange")
		.add(m_iFirstStrikesChange, L"iFirstStrikesChange")
		.add(m_iChanceFirstStrikesChange, L"iChanceFirstStrikesChange")
		.add(m_iEnemyHealChange, L"iEnemyHealChange")
		.add(m_iNeutralHealChange, L"iNeutralHealChange")
		.add(m_iFriendlyHealChange, L"iFriendlyHealChange")
		.add(m_iSameTileHealChange, L"iSameTileHealChange")
		.add(m_iAdjacentTileHealChange, L"iAdjacentTileHealChange")
		.add(m_iCombatPercent, L"iCombatPercent")
		.add(m_iCityAttackPercent, L"iCityAttack")
		.add(m_iCityDefensePercent, L"iCityDefense")
		.add(m_iHillsAttackPercent, L"iHillsAttack")
		.add(m_iHillsDefensePercent, L"iHillsDefense")
		.add(m_iHillsWorkPercent, L"iHillsWorkModifier")
		.add(m_iPeaksWorkPercent, L"iPeaksWorkModifier")
		.add(m_iWorkRatePercent, L"iWorkRateModifier")
		.add(m_iRevoltProtection, L"iRevoltProtection")
		.add(m_iCollateralDamageProtection, L"iCollateralDamageProtection")
		.add(m_iPillageChange, L"iPillageChange")
		.add(m_iUpgradeDiscount, L"iUpgradeDiscount")
		.add(m_iExperiencePercent, L"iExperiencePercent")
		.add(m_iKamikazePercent, L"iKamikazePercent")
		.add(m_bDefensiveVictoryMove, L"bDefensiveVictoryMove")
		.add(m_bFreeDrop, L"bFreeDrop")
		.add(m_bOffensiveVictoryMove, L"bOffensiveVictoryMove")
		.add(m_bOneUp, L"bOneUp")
		.add(m_bPillageEspionage, L"bPillageEspionage")
		.add(m_bPillageMarauder, L"bPillageMarauder")
		.add(m_bPillageOnMove, L"bPillageOnMove")
		.add(m_bPillageOnVictory, L"bPillageOnVictory")
		.add(m_bPillageResearch, L"bPillageResearch")
		.add(m_iAirCombatLimitChange, L"iAirCombatLimitChange")
		.add(m_iCelebrityHappy, L"iCelebrityHappy")
		.add(m_iCollateralDamageLimitChange, L"iCollateralDamageLimitChange")
		.add(m_iCollateralDamageMaxUnitsChange, L"iCollateralDamageMaxUnitsChange")
		.add(m_iCombatLimitChange, L"iCombatLimitChange")
		.add(m_iExtraDropRange, L"iExtraDropRange")
		.add(m_iSurvivorChance, L"iSurvivorChance")
		.add(m_iVictoryAdjacentHeal, L"iVictoryAdjacentHeal")
		.add(m_iVictoryHeal, L"iVictoryHeal")
		.add(m_iVictoryStackHeal, L"iVictoryStackHeal")
		.add(m_bCanMovePeaks, L"bCanMovePeaks")
		.add(m_bCanLeadThroughPeaks, L"bCanLeadThroughPeaks")
		.addEnum(m_iObsoleteTech, L"ObsoleteTech")
		.add(m_iControlPoints, L"iControlPoints")
		.add(m_iCommandRange, L"iCommandRange")
		.add(m_bZoneOfControl, L"bZoneOfControl")
		.add(m_PropertyManipulators)
		//TB Combat Mods - textual references
		.addEnum(m_ePromotionLine, L"PromotionLine")
		.addEnum(m_eReplacesUnitCombat, L"ReplacesUnitCombat")
		.addEnum(m_eDomainCargoChange, L"DomainCargoChange")
		.addEnum(m_eSpecialCargoChange, L"SpecialCargoChange")
		.addEnum(m_eSpecialCargoPrereq, L"SpecialCargoPrereq")
		.addEnum(m_eSMNotSpecialCargoChange, L"SMNotSpecialCargoChange")
		.addEnum(m_eSMNotSpecialCargoPrereq, L"SMNotSpecialCargoPrereq")
		.addEnum(m_eSetSpecialUnit, L"SetSpecialUnit")
		//TB Combat Mods - integers
		.add(m_iAttackCombatModifierChange, L"iAttackCombatModifierChange")
		.add(m_iDefenseCombatModifierChange, L"iDefenseCombatModifierChange")
		.add(m_iVSBarbsChange, L"iVSBarbsChange")
		.add(m_iUnnerveChange, L"iUnnerveChange")
		.add(m_iEncloseChange, L"iEncloseChange")
		.add(m_iLungeChange, L"iLungeChange")
		.add(m_iDynamicDefenseChange, L"iDynamicDefenseChange")
		.add(m_iStrengthChange, L"iStrengthChange")
		.add(m_iLinePriority, L"iLinePriority")
		.add(m_iDamageperTurn, L"iDamageperTurn")
		.add(m_iStrAdjperTurn, L"iStrAdjperTurn")
		.add(m_iWeakenperTurn, L"iWeakenperTurn")
		.add(m_iEnduranceChange, L"iEnduranceChange")
		.add(m_iPoisonProbabilityModifierChange, L"iPoisonProbabilityModifierChange")
		.add(m_iCaptureProbabilityModifierChange, L"iCaptureProbabilityModifierChange")
		.add(m_iCaptureResistanceModifierChange, L"iCaptureResistanceModifierChange")
		.add(m_iBreakdownChanceChange, L"iBreakdownChanceChange")
		.add(m_iBreakdownDamageChange, L"iBreakdownDamageChange")
		.add(m_iTauntChange, L"iTauntChange")
		.add(m_iMaxHPChange, L"iMaxHPChange")
		.add(m_iStrengthModifier, L"iStrengthModifier")
		.add(m_iQualityChange, L"iQualityChange")
		.add(m_iGroupChange, L"iGroupChange")
		.add(m_iLevelPrereq, L"iLevelPrereq")
		.add(m_iDamageModifierChange, L"iDamageModifierChange")
		.add(m_iUpkeepModifier, L"iUpkeepModifier")
		.add(m_iExtraUpkeep100, L"iExtraUpkeep100")
		.add(m_iRBombardDamageChange, L"iRBombardDamageChange")
		.add(m_iRBombardDamageLimitChange, L"iRBombardDamageLimitChange")
		.add(m_iRBombardDamageMaxUnitsChange, L"iRBombardDamageMaxUnitsChange")
		.add(m_iDCMBombRangeChange, L"iDCMBombRangeChange")
		.add(m_iDCMBombAccuracyChange, L"iDCMBombAccuracyChange")
		.add(m_iCombatModifierPerSizeMoreChange, L"iCombatModifierPerSizeMoreChange")
		.add(m_iCombatModifierPerSizeLessChange, L"iCombatModifierPerSizeLessChange")
		.add(m_iCombatModifierPerVolumeMoreChange, L"iCombatModifierPerVolumeMoreChange")
		.add(m_iCombatModifierPerVolumeLessChange, L"iCombatModifierPerVolumeLessChange")
		.add(m_iSelfHealModifier, L"iSelfHealModifier")
		.add(m_iNumHealSupport, L"iNumHealSupport")
		.add(m_iExcileChange, L"iExcileChange")
		.add(m_iPassageChange, L"iPassageChange")
		.add(m_iNoNonOwnedCityEntryChange, L"iNoNonOwnedCityEntryChange")
		.add(m_iBarbCoExistChange, L"iBarbCoExistChange")
		.add(m_iBlendIntoCityChange, L"iBlendIntoCityChange")
		.add(m_iUpgradeAnywhereChange, L"iUpgradeAnywhereChange")
		.add(m_iInsidiousnessChange, L"iInsidiousnessChange")
		.add(m_iInvestigationChange, L"iInvestigationChange")
		.add(m_iAssassinChange, L"iAssassinChange")
		.add(m_iStealthStrikesChange, L"iStealthStrikesChange")
		.add(m_iStealthCombatModifierChange, L"iStealthCombatModifierChange")
		.add(m_iStealthDefenseChange, L"iStealthDefenseChange")
		.add(m_iDefenseOnlyChange, L"iDefenseOnlyChange")
		.add(m_iNoInvisibilityChange, L"iNoInvisibilityChange")
		.add(m_iTrapDamageMax, L"iTrapDamageMax")
		.add(m_iTrapDamageMin, L"iTrapDamageMin")
		.add(m_iTrapComplexity, L"iTrapComplexity")
		.add(m_iNumTriggers, L"iNumTriggers")
		.add(m_iTriggerBeforeAttackChange, L"iTriggerBeforeAttackChange")
		.add(m_iHiddenNationalityChange, L"iHiddenNationalityChange")
		.add(m_iAnimalIgnoresBordersChange, L"iAnimalIgnoresBordersChange")
		.add(m_iNoDefensiveBonusChange, L"iNoDefensiveBonusChange")
		.add(m_iGatherHerdChange, L"iGatherHerdChange")
		.add(m_iReligiousCombatModifierChange, L"iReligiousCombatModifierChange")
		//TB Combat Mods - booleans
		.add(m_bStampedeChange, L"bStampedeChange")
		.add(m_bRemoveStampede, L"bRemoveStampede")
		.add(m_bOnslaughtChange, L"bOnslaughtChange")
		.add(m_bParalyze, L"bParalyze")
		.add(m_bAttackOnlyCitiesAdd, L"bAttackOnlyCitiesAdd")
		.add(m_bAttackOnlyCitiesSubtract, L"bAttackOnlyCitiesSubtract")
		.add(m_bIgnoreNoEntryLevelAdd, L"bIgnoreNoEntryLevelAdd")
		.add(m_bIgnoreNoEntryLevelSubtract, L"bIgnoreNoEntryLevelSubtract")
		.add(m_bIgnoreZoneofControlAdd, L"bIgnoreZoneofControlAdd")
		.add(m_bIgnoreZoneofControlSubtract, L"bIgnoreZoneofControlSubtract")
		.add(m_bFliesToMoveAdd, L"bFliesToMoveAdd")
		.add(m_bFliesToMoveSubtract, L"bFliesToMoveSubtract")
		.add(m_bZeroesXP, L"bZeroesXP")
		.add(m_bForOffset, L"bForOffset")
		.add(m_bCargoPrereq, L"bCargoPrereq")
		.add(m_bRBombardPrereq, L"bRBombardPrereq")
		.add(m_bNoSelfHeal, L"bNoSelfHeal")
		.add(m_bSetOnHNCapture, L"bSetOnHNCapture")
		.add(m_bSetOnInvestigated, L"bSetOnInvestigated")
		.add(m_bStatus, L"bStatus")
		.add(m_bPrereqNormInvisible, L"bPrereqNormInvisible")
		.add(m_bPlotPrereqsKeepAfter, L"bPlotPrereqsKeepAfter")
		.add(m_bRemoveAfterSet, L"bRemoveAfterSet")
		.add(m_bQuick, L"bQuick")
		.add(m_bStarsign, L"bStarsign")
		// flat vectors (immediate resolution)
		.add(m_aiSubCombatChangeTypes, L"SubCombatChangeTypes")
		.add(m_aiRemovesUnitCombatTypes, L"RemovesUnitCombatTypes")
		.add(m_aiOnGameOptions, L"OnGameOptions")
		.add(m_aiNotOnGameOptions, L"NotOnGameOptions")
		.add(m_aiFreetoUnitCombats, L"FreetoUnitCombats")
		.add(m_aiNotOnUnitCombatTypes, L"NotOnUnitCombatTypes")
		.add(m_aiNotOnDomainTypes, L"NotOnDomainTypes")
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")
		.add(m_aiPrereqBonusTypes, L"PrereqBonusTypes")
		.add(m_aiNegatesInvisibilityTypes, L"NegatesInvisibilityTypes")
		.add(m_aiPrereqTerrainTypes, L"PrereqTerrainTypes")
		.add(m_aiPrereqFeatureTypes, L"PrereqFeatureTypes")
		.add(m_aiPrereqImprovementTypes, L"PrereqImprovementTypes")
		.add(m_aiPrereqPlotBonusTypes, L"PrereqPlotBonusTypes")
		.add(m_aiTrapImmunityUnitCombatTypes, L"TrapImmunityUnitCombatTypes")
		.add(m_aiTargetUnitCombatTypes, L"TargetUnitCombatTypes")
		.add(m_aiCategories, L"Categories")
	;
}


bool CvPromotionInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvInfoUtil(this).readXml(pXML);

	CvString szTextVal;
	CvString szTextVal2;
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	pXML->GetOptionalTypeEnum(m_iTechPrereq, L"TechPrereq");

	if (m_bLeader) // read by the CvInfoUtil block above; the side effect must run after the base read
	{
		m_bGraphicalOnly = true;  // don't show in Civilopedia list of promotions
	}

	pXML->SetVariableListTagPair(&m_piTerrainAttackPercent, L"TerrainAttacks", GC.getNumTerrainInfos());
	pXML->SetVariableListTagPair(&m_piTerrainDefensePercent, L"TerrainDefenses", GC.getNumTerrainInfos());
	pXML->SetVariableListTagPair(&m_piFeatureAttackPercent, L"FeatureAttacks", GC.getNumFeatureInfos());
	pXML->SetVariableListTagPair(&m_piFeatureDefensePercent, L"FeatureDefenses", GC.getNumFeatureInfos());
	pXML->SetVariableListTagPair(&m_piUnitCombatModifierPercent, L"UnitCombatMods", GC.getNumUnitCombatInfos());
	pXML->SetVariableListTagPair(&m_piDomainModifierPercent, L"DomainMods", NUM_DOMAIN_TYPES);
	//ls612: Terrain Work Modifiers
	pXML->SetVariableListTagPair(&m_piTerrainWorkPercent, L"TerrainWorks", GC.getNumTerrainInfos());
	pXML->SetVariableListTagPair(&m_piFeatureWorkPercent, L"FeatureWorks", GC.getNumFeatureInfos());

	//TB Combat Mods Begin  TB SubCombat Mod begin
	//Text Strings
	pXML->GetOptionalChildXmlValByName(m_szRenamesUnitTo, L"RenamesUnitTo"); // CvWString - no CvInfoUtil wrapper

	// int vectors with delayed resolution (no CvInfoUtil wrapper yet)
	pXML->SetOptionalVectorWithDelayedResolution(m_aiAddsBuildTypes, L"AddsBuildTypes");
	pXML->SetOptionalVectorWithDelayedResolution(m_aiPrereqLocalBuildingTypes, L"PrereqLocalBuildingTypes");
	pXML->SetOptionalVectorWithDelayedResolution(m_aiTrapSetWithPromotionTypes, L"TrapSetWithPromotionTypes");

	// int vector utilizing pairing without delayed resolution (no CvInfoUtil wrapper for plain pair-vectors)
	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aFlankingStrengthbyUnitCombatTypeChange, L"FlankingStrikesbyUnitCombatChange");

	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aTrapDisableUnitCombatTypes, L"TrapDisableUnitCombatTypes");

	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aTrapAvoidanceUnitCombatTypes, L"TrapAvoidanceUnitCombatTypes");

	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aTrapTriggerUnitCombatTypes, L"TrapTriggerUnitCombatTypes");

	//pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(m_aAIWeightbyUnitCombatTypes, L"AIWeightbyUnitCombatTypes");

	pXML->SetOptionalPairVector<BuildModifierArray, BuildTypes, int>(&m_aBuildWorkRateModifierChangeTypes, L"BuildWorkRateModifierChangeTypes");

	pXML->SetOptionalPairVector<InvisibilityArray, InvisibleTypes, int>(&m_aVisibilityIntensityChangeTypes, L"VisibilityIntensityChangeTypes");

	pXML->SetOptionalPairVector<InvisibilityArray, InvisibleTypes, int>(&m_aInvisibilityIntensityChangeTypes, L"InvisibilityIntensityChangeTypes");

	pXML->SetOptionalPairVector<InvisibilityArray, InvisibleTypes, int>(&m_aVisibilityIntensityRangeChangeTypes, L"VisibilityIntensityRangeChangeTypes");

	// int vector utilizing struct with delayed resolution
	if(pXML->TryMoveToXmlFirstChild(L"AIWeightbyUnitCombatTypes"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"AIWeightbyUnitCombatType" );
		m_aAIWeightbyUnitCombatTypes.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"AIWeightbyUnitCombatType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					m_aAIWeightbyUnitCombatTypes[i].eUnitCombat = (UnitCombatTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aAIWeightbyUnitCombatTypes[i].iModifier), L"iAIWeight");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"AIWeightbyUnitCombatType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}


	if(pXML->TryMoveToXmlFirstChild(L"HealUnitCombatChangeTypes"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"HealUnitCombatChangeType" );
		m_aHealUnitCombatChangeTypes.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"HealUnitCombatChangeType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					m_aHealUnitCombatChangeTypes[i].eUnitCombat = (UnitCombatTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aHealUnitCombatChangeTypes[i].iHeal), L"iHeal");
					pXML->GetChildXmlValByName(&(m_aHealUnitCombatChangeTypes[i].iAdjacentHeal), L"iAdjacentHeal");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"HealUnitCombatChangeType"));
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

	//TB Combat Mod End
	//pXML->SetOptionalVectorWithDelayedResolution(PromotionOverwrites, L"PromotionOverwrites");

	return true;
}


void CvPromotionInfo::copyNonDefaults(const CvPromotionInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	int iDefault = 0;
	CvWString wDefault = CvWString::format(L"").GetCString();

	// Hand-written on purpose: compares the EFFECTIVE prereq (the getter redirects through the
	// promotion line), so the wrapper's raw-member compare would change modular-merge behaviour.
	if (getTechPrereq() == NO_TECH) m_iTechPrereq = pClassInfo->getTechPrereq();

	if (m_bLeader) // merged by the CvInfoUtil block above; re-apply the side effect
	{
		m_bGraphicalOnly = true;  // don't show in Civilopedia list of promotions
	}

	for (int j = 0; j < GC.getNumTerrainInfos(); j++)
	{
		if ((m_piTerrainAttackPercent == NULL || m_piTerrainAttackPercent[j] == iDefault) &&
			pClassInfo->getTerrainAttackPercent(j) != iDefault)
		{
			if ( m_piTerrainAttackPercent == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piTerrainAttackPercent,GC.getNumTerrainInfos(),iDefault);
			}
			m_piTerrainAttackPercent[j] = pClassInfo->getTerrainAttackPercent(j);
		}

		if ((m_piTerrainDefensePercent == NULL ||m_piTerrainDefensePercent[j] == iDefault) &&
			pClassInfo->getTerrainDefensePercent(j) != iDefault)
		{
			if ( m_piTerrainDefensePercent == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piTerrainDefensePercent,GC.getNumTerrainInfos(),iDefault);
			}
			m_piTerrainDefensePercent[j] = pClassInfo->getTerrainDefensePercent(j);
		}

		//ls612: Terrain Work Modifiers
		if ((m_piTerrainWorkPercent == NULL || m_piTerrainWorkPercent[j] == iDefault) &&
			pClassInfo->getTerrainWorkPercent(j) != iDefault)
		{
			if ( m_piTerrainWorkPercent == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piTerrainWorkPercent,GC.getNumTerrainInfos(),iDefault);
			}
			m_piTerrainWorkPercent[j] = pClassInfo->getTerrainWorkPercent(j);
		}
	}
	for (int j = 0; j < GC.getNumFeatureInfos(); j++)
	{
		if ((m_piFeatureAttackPercent == NULL || m_piFeatureAttackPercent[j] == iDefault) &&
			pClassInfo->getFeatureAttackPercent(j) != iDefault)
		{
			if ( m_piFeatureAttackPercent == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piFeatureAttackPercent,GC.getNumFeatureInfos(),iDefault);
			}
			m_piFeatureAttackPercent[j] = pClassInfo->getFeatureAttackPercent(j);
		}

		if ((m_piFeatureDefensePercent == NULL ||m_piFeatureDefensePercent[j] == iDefault) &&
			pClassInfo->getFeatureDefensePercent(j) != iDefault)
		{
			if ( m_piFeatureDefensePercent == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piFeatureDefensePercent,GC.getNumFeatureInfos(),iDefault);
			}
			m_piFeatureDefensePercent[j] = pClassInfo->getFeatureDefensePercent(j);
		}

		//ls612: Terrain Work Modifiers
		if ((m_piFeatureWorkPercent == NULL || m_piFeatureWorkPercent[j] == iDefault) &&
			pClassInfo->getFeatureWorkPercent(j) != iDefault)
		{
			if ( m_piFeatureWorkPercent == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piFeatureWorkPercent,GC.getNumFeatureInfos(),iDefault);
			}
			m_piFeatureWorkPercent[j] = pClassInfo->getFeatureWorkPercent(j);
		}
	}
	for (int j = 0; j < GC.getNumUnitCombatInfos(); j++)
	{
		if ((m_piUnitCombatModifierPercent == NULL || m_piUnitCombatModifierPercent[j] == iDefault) &&
			pClassInfo->getUnitCombatModifierPercent(j) != iDefault)
		{
			if ( m_piUnitCombatModifierPercent == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piUnitCombatModifierPercent,GC.getNumUnitCombatInfos(),iDefault);
			}
			m_piUnitCombatModifierPercent[j] = pClassInfo->getUnitCombatModifierPercent(j);
		}


		//if ((m_piAIWeightbyUnitCombatTypes == NULL || m_piAIWeightbyUnitCombatTypes[j] == iDefault) &&
		//	pClassInfo->getAIWeightbyUnitCombatType(j) != iDefault)
		//{
		//	if ( m_piAIWeightbyUnitCombatTypes == NULL )
		//	{
		//		CvXMLLoadUtility::InitList(&m_piAIWeightbyUnitCombatTypes,GC.getNumUnitCombatInfos(),iDefault);
		//	}
		//	m_piAIWeightbyUnitCombatTypes[j] = pClassInfo->getAIWeightbyUnitCombatType(j);
		//}
	}
	for (int j = 0; j < NUM_DOMAIN_TYPES; j++)
	{
		if ((m_piDomainModifierPercent == NULL || m_piDomainModifierPercent[j] == iDefault) &&
			pClassInfo->getDomainModifierPercent(j) != iDefault)
		{
			if ( m_piDomainModifierPercent == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piDomainModifierPercent,NUM_DOMAIN_TYPES,iDefault);
			}
			m_piDomainModifierPercent[j] = pClassInfo->getDomainModifierPercent(j);
		}
	}

	//TB Combat Mods Begin  TB SubCombat Mods Begin
	// Text Strings
	if ( getRenamesUnitTo() == NULL || getRenamesUnitTo() == wDefault )
	{
		m_szRenamesUnitTo = pClassInfo->getRenamesUnitTo();
	}

	if (getNumAddsBuildTypes() == 0)
	{
		const int iNum = pClassInfo->getNumAddsBuildTypes();
		m_aiAddsBuildTypes.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			GC.copyNonDefaultDelayedResolution((int*)&(m_aiAddsBuildTypes[i]), (int*)&(pClassInfo->m_aiAddsBuildTypes[i]));
		}
	}

	if (getNumPrereqLocalBuildingTypes() == 0)
	{
		const int iNum = pClassInfo->getNumPrereqLocalBuildingTypes();
		m_aiPrereqLocalBuildingTypes.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			GC.copyNonDefaultDelayedResolution((int*)&(m_aiPrereqLocalBuildingTypes[i]), (int*)&(pClassInfo->m_aiPrereqLocalBuildingTypes[i]));
		}
	}

	// NOTE (pre-existing quirk, preserved): unlike the two delayed-resolution vectors above, this
	// delayed-resolution vector merges by VALUE (the -1 placeholders), not via
	// GC.copyNonDefaultDelayedResolution per element.
	if (getNumTrapSetWithPromotionTypes() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aiTrapSetWithPromotionTypes, pClassInfo->m_aiTrapSetWithPromotionTypes);
	}

	// int vectors utilizing pairing without delayed resolution
	if (getNumFlankingStrikesbyUnitCombatTypesChange()==0)
	{
		for (int i=0; i < pClassInfo->getNumFlankingStrikesbyUnitCombatTypesChange(); i++)
		{
			UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			int iChange = pClassInfo->getFlankingStrengthbyUnitCombatTypeChange(i);
			m_aFlankingStrengthbyUnitCombatTypeChange.push_back(std::make_pair(eUnitCombat, iChange));
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

	if (getNumBuildWorkRateModifierChangeTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumBuildWorkRateModifierChangeTypes(); i++)
		{
			BuildTypes eBuild = ((BuildTypes)i);
			int iChange = pClassInfo->getBuildWorkRateModifierChangeType(i);
			m_aBuildWorkRateModifierChangeTypes.push_back(std::make_pair(eBuild, iChange));
		}
	}

	if (getNumVisibilityIntensityChangeTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumVisibilityIntensityChangeTypes(); i++)
		{
			InvisibleTypes eInvisible = ((InvisibleTypes)i);
			int iChange = pClassInfo->getVisibilityIntensityChangeType(i);
			m_aVisibilityIntensityChangeTypes.push_back(std::make_pair(eInvisible, iChange));
		}
	}

	if (getNumInvisibilityIntensityChangeTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumInvisibilityIntensityChangeTypes(); i++)
		{
			InvisibleTypes eInvisible = ((InvisibleTypes)i);
			int iChange = pClassInfo->getInvisibilityIntensityChangeType(i);
			m_aInvisibilityIntensityChangeTypes.push_back(std::make_pair(eInvisible, iChange));
		}
	}

	if (getNumVisibilityIntensityRangeChangeTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumVisibilityIntensityRangeChangeTypes(); i++)
		{
			InvisibleTypes eInvisible = ((InvisibleTypes)i);
			int iChange = pClassInfo->getVisibilityIntensityRangeChangeType(i);
			m_aVisibilityIntensityRangeChangeTypes.push_back(std::make_pair(eInvisible, iChange));
		}
	}

	// int vectors utilizing struct with delayed resolution
	if (getNumAIWeightbyUnitCombatTypes() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aAIWeightbyUnitCombatTypes, pClassInfo->m_aAIWeightbyUnitCombatTypes);
	}

	if (getNumHealUnitCombatChangeTypes() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aHealUnitCombatChangeTypes, pClassInfo->m_aHealUnitCombatChangeTypes);
	}

	if (getNumInvisibleTerrainChanges() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aInvisibleTerrainChanges, pClassInfo->m_aInvisibleTerrainChanges);
	}

	if (getNumInvisibleFeatureChanges() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aInvisibleFeatureChanges, pClassInfo->m_aInvisibleFeatureChanges);
	}

	if (getNumInvisibleImprovementChanges() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aInvisibleImprovementChanges, pClassInfo->m_aInvisibleImprovementChanges);
	}

	if (getNumVisibleTerrainChanges() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleTerrainChanges, pClassInfo->m_aVisibleTerrainChanges);
	}

	if (getNumVisibleFeatureChanges() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleFeatureChanges, pClassInfo->m_aVisibleFeatureChanges);
	}

	if (getNumVisibleImprovementChanges() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleImprovementChanges, pClassInfo->m_aVisibleImprovementChanges);
	}

	if (getNumVisibleTerrainRangeChanges() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleTerrainRangeChanges, pClassInfo->m_aVisibleTerrainRangeChanges);
	}

	if (getNumVisibleFeatureRangeChanges() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleFeatureRangeChanges, pClassInfo->m_aVisibleFeatureRangeChanges);
	}

	if (getNumVisibleImprovementRangeChanges() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aVisibleImprovementRangeChanges, pClassInfo->m_aVisibleImprovementRangeChanges);
	}

	//TB Combat Mods End  TB SubCombat Mods end

	//GC.copyNonDefaultDelayedResolutionVector(m_vPromotionOverwrites, pClassInfo->getPromotionOverwrites());
}


// Explicit on purpose - NOT delegated to CvInfoUtil (cf. CvTechInfo): hand-written fields
// (TechPrereq, the runtime CommandType, the SetVariableListTagPair arrays, the pair-vectors
// and the struct-vectors) sit mid-order, and CheckSum folds are order-sensitive (rotate+add),
// so wholesale delegation would change the savegame asset checksum for every existing save.
// The first six folds reproduce the pre-migration CvInfoUtil(this).checkSum(iSum) block
// (the fields that were already declared in getDataMembers); the rest is the legacy order.
// Keep this list in sync with getDataMembers when adding fields.
void CvPromotionInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	CheckSum(iSum, m_iPrereqPromotion);
	CheckSum(iSum, m_iPrereqOrPromotion1);
	CheckSum(iSum, m_iPrereqOrPromotion2);
	CheckSumC(iSum, m_aeTerrainDoubleMove);
	CheckSumC(iSum, m_aeFeatureDoubleMove);
	CheckSumC(iSum, m_aeUnitCombat);

	CheckSum(iSum, m_iTechPrereq);
	CheckSum(iSum, m_iStateReligionPrereq);
	CheckSum(iSum, m_iMinEraType);
	CheckSum(iSum, m_iMaxEraType);
	CheckSum(iSum, m_iVisibilityChange);
	CheckSum(iSum, m_iMovesChange);
	CheckSum(iSum, m_iMoveDiscountChange);
	CheckSum(iSum, m_iAirRangeChange);
	CheckSum(iSum, m_iInterceptChange);
	CheckSum(iSum, m_iEvasionChange);
	CheckSum(iSum, m_iWithdrawalChange);
	CheckSum(iSum, m_iCargoChange);
	CheckSum(iSum, m_iSMCargoChange);
	CheckSum(iSum, m_iSMCargoVolumeChange);
	CheckSum(iSum, m_iSMCargoVolumeModifierChange);
	CheckSum(iSum, m_iCollateralDamageChange);
	CheckSum(iSum, m_iBombardRateChange);
	CheckSum(iSum, m_iFirstStrikesChange);
	CheckSum(iSum, m_iChanceFirstStrikesChange);
	CheckSum(iSum, m_iEnemyHealChange);
	CheckSum(iSum, m_iNeutralHealChange);
	CheckSum(iSum, m_iFriendlyHealChange);
	CheckSum(iSum, m_iSameTileHealChange);
	CheckSum(iSum, m_iAdjacentTileHealChange);
	CheckSum(iSum, m_iCombatPercent);
	CheckSum(iSum, m_iCityAttackPercent);
	CheckSum(iSum, m_iCityDefensePercent);
	CheckSum(iSum, m_iHillsAttackPercent);
	CheckSum(iSum, m_iHillsDefensePercent);
	CheckSum(iSum, m_iHillsWorkPercent);
	CheckSum(iSum, m_iPeaksWorkPercent);
	CheckSum(iSum, m_iWorkRatePercent);
	CheckSum(iSum, m_iCommandType);
	CheckSum(iSum, m_iRevoltProtection);
	CheckSum(iSum, m_iCollateralDamageProtection);
	CheckSum(iSum, m_iPillageChange);
	CheckSum(iSum, m_iUpgradeDiscount);
	CheckSum(iSum, m_iExperiencePercent);
	CheckSum(iSum, m_iKamikazePercent);

	CheckSum(iSum, m_bLeader);
	CheckSum(iSum, m_bBlitz);
	CheckSum(iSum, m_bAmphib);
	CheckSum(iSum, m_bRiver);
	CheckSum(iSum, m_bEnemyRoute);
	CheckSum(iSum, m_bAlwaysHeal);
	CheckSum(iSum, m_bHillsDoubleMove);
	CheckSum(iSum, m_bImmuneToFirstStrikes);

	// Arrays

	CheckSum(iSum, m_piTerrainAttackPercent, GC.getNumTerrainInfos());
	CheckSum(iSum, m_piTerrainDefensePercent, GC.getNumTerrainInfos());
	CheckSum(iSum, m_piFeatureAttackPercent, GC.getNumFeatureInfos());
	CheckSum(iSum, m_piFeatureDefensePercent, GC.getNumFeatureInfos());
	CheckSum(iSum, m_piUnitCombatModifierPercent, GC.getNumUnitCombatInfos());
	CheckSum(iSum, m_piDomainModifierPercent, NUM_DOMAIN_TYPES);
	CheckSum(iSum, m_piTerrainWorkPercent, GC.getNumTerrainInfos());
	CheckSum(iSum, m_piFeatureWorkPercent, GC.getNumFeatureInfos());
	CheckSum(iSum, m_bCanMovePeaks);
	CheckSum(iSum, m_bCanLeadThroughPeaks);
	//CheckSumC(iSum, m_vPromotionOverwrites);
	CheckSum(iSum, m_iObsoleteTech);
	CheckSum(iSum, m_iControlPoints);
	CheckSum(iSum, m_iCommandRange);
	CheckSum(iSum, m_bZoneOfControl);
	m_PropertyManipulators.getCheckSum(iSum);
	CheckSum(iSum, m_bDefensiveVictoryMove);
	CheckSum(iSum, m_bFreeDrop);
	CheckSum(iSum, m_bOffensiveVictoryMove);
	CheckSum(iSum, m_bOneUp);
	CheckSum(iSum, m_bPillageEspionage);
	CheckSum(iSum, m_bPillageMarauder);
	CheckSum(iSum, m_bPillageOnMove);
	CheckSum(iSum, m_bPillageOnVictory);
	CheckSum(iSum, m_bPillageResearch);
	CheckSum(iSum, m_iAirCombatLimitChange);
	CheckSum(iSum, m_iCelebrityHappy);
	CheckSum(iSum, m_iCollateralDamageLimitChange);
	CheckSum(iSum, m_iCollateralDamageMaxUnitsChange);
	CheckSum(iSum, m_iCombatLimitChange);
	CheckSum(iSum, m_iExtraDropRange);
	CheckSum(iSum, m_iSurvivorChance);
	CheckSum(iSum, m_iVictoryAdjacentHeal);
	CheckSum(iSum, m_iVictoryHeal);
	CheckSum(iSum, m_iVictoryStackHeal);

	//TB Combat Mods Begin  TB SubCombat Mod begin
	// Textual References
	CheckSum(iSum, m_ePromotionLine);
	CheckSum(iSum, m_eReplacesUnitCombat);
	CheckSum(iSum, m_eDomainCargoChange);
	CheckSum(iSum, m_eSpecialCargoChange);
	CheckSum(iSum, m_eSpecialCargoPrereq);
	CheckSum(iSum, m_eSMNotSpecialCargoChange);
	CheckSum(iSum, m_eSMNotSpecialCargoPrereq);
	CheckSum(iSum, m_eSetSpecialUnit);
	//integers
	CheckSum(iSum, m_iAttackCombatModifierChange);
	CheckSum(iSum, m_iDefenseCombatModifierChange);
	CheckSum(iSum, m_iVSBarbsChange);
	CheckSum(iSum, m_iUnnerveChange);
	CheckSum(iSum, m_iEncloseChange);
	CheckSum(iSum, m_iLungeChange);
	CheckSum(iSum, m_iDynamicDefenseChange);
	CheckSum(iSum, m_iStrengthChange);
	CheckSum(iSum, m_iLinePriority);
	CheckSum(iSum, m_iDamageperTurn);
	CheckSum(iSum, m_iStrAdjperTurn);
	CheckSum(iSum, m_iWeakenperTurn);
	CheckSum(iSum, m_iEnduranceChange);
	CheckSum(iSum, m_iPoisonProbabilityModifierChange);

	CheckSum(iSum, m_iCaptureProbabilityModifierChange);
	CheckSum(iSum, m_iCaptureResistanceModifierChange);

	CheckSum(iSum, m_iBreakdownChanceChange);
	CheckSum(iSum, m_iBreakdownDamageChange);
	CheckSum(iSum, m_iTauntChange);
	CheckSum(iSum, m_iMaxHPChange);
	CheckSum(iSum, m_iStrengthModifier);
	CheckSum(iSum, m_iQualityChange);
	CheckSum(iSum, m_iGroupChange);
	CheckSum(iSum, m_iLevelPrereq);
	CheckSum(iSum, m_iDamageModifierChange);

	CheckSum(iSum, m_iUpkeepModifier);
	CheckSum(iSum, m_iExtraUpkeep100);

	CheckSum(iSum, m_iRBombardDamageChange);
	CheckSum(iSum, m_iRBombardDamageLimitChange);
	CheckSum(iSum, m_iRBombardDamageMaxUnitsChange);
	CheckSum(iSum, m_iDCMBombRangeChange);
	CheckSum(iSum, m_iDCMBombAccuracyChange);
	CheckSum(iSum, m_iCombatModifierPerSizeMoreChange);
	CheckSum(iSum, m_iCombatModifierPerSizeLessChange);
	CheckSum(iSum, m_iCombatModifierPerVolumeMoreChange);
	CheckSum(iSum, m_iCombatModifierPerVolumeLessChange);
	CheckSum(iSum, m_iSelfHealModifier);
	CheckSum(iSum, m_iNumHealSupport);
	CheckSum(iSum, m_iExcileChange);
	CheckSum(iSum, m_iPassageChange);
	CheckSum(iSum, m_iNoNonOwnedCityEntryChange);
	CheckSum(iSum, m_iBarbCoExistChange);
	CheckSum(iSum, m_iBlendIntoCityChange);
	CheckSum(iSum, m_iUpgradeAnywhereChange);
	CheckSum(iSum, m_iInsidiousnessChange);
	CheckSum(iSum, m_iInvestigationChange);
	CheckSum(iSum, m_iAssassinChange);
	CheckSum(iSum, m_iStealthStrikesChange);
	CheckSum(iSum, m_iStealthCombatModifierChange);
	CheckSum(iSum, m_iStealthDefenseChange);
	CheckSum(iSum, m_iDefenseOnlyChange);
	CheckSum(iSum, m_iNoInvisibilityChange);
	CheckSum(iSum, m_iTrapDamageMax);
	CheckSum(iSum, m_iTrapDamageMin);
	CheckSum(iSum, m_iTrapComplexity);
	CheckSum(iSum, m_iNumTriggers);
	CheckSum(iSum, m_iTriggerBeforeAttackChange);
	CheckSum(iSum, m_iHiddenNationalityChange);
	CheckSum(iSum, m_iAnimalIgnoresBordersChange);
	CheckSum(iSum, m_iNoDefensiveBonusChange);
	CheckSum(iSum, m_iGatherHerdChange);
	CheckSum(iSum, m_iReligiousCombatModifierChange);
	//booleans
	CheckSum(iSum, m_bStampedeChange);
	CheckSum(iSum, m_bRemoveStampede);
	CheckSum(iSum, m_bOnslaughtChange);
	CheckSum(iSum, m_bParalyze);
	CheckSum(iSum, m_bAttackOnlyCitiesAdd);
	CheckSum(iSum, m_bAttackOnlyCitiesSubtract);
	CheckSum(iSum, m_bIgnoreNoEntryLevelAdd);
	CheckSum(iSum, m_bIgnoreNoEntryLevelSubtract);
	CheckSum(iSum, m_bIgnoreZoneofControlAdd);
	CheckSum(iSum, m_bIgnoreZoneofControlSubtract);
	CheckSum(iSum, m_bFliesToMoveAdd);
	CheckSum(iSum, m_bFliesToMoveSubtract);
	CheckSum(iSum, m_bNoSelfHeal);
	CheckSum(iSum, m_bSetOnHNCapture);
	CheckSum(iSum, m_bSetOnInvestigated);
	CheckSum(iSum, m_bStatus);
	CheckSum(iSum, m_bPrereqNormInvisible);
	CheckSum(iSum, m_bPlotPrereqsKeepAfter);
	CheckSum(iSum, m_bRemoveAfterSet);
	CheckSum(iSum, m_bQuick);
	CheckSum(iSum, m_bStarsign);
	CheckSum(iSum, m_bZeroesXP);
	CheckSum(iSum, m_bForOffset);
	CheckSum(iSum, m_bCargoPrereq);
	CheckSum(iSum, m_bRBombardPrereq);
	//Arrays
	//CheckSum(iSum, m_piAIWeightbyUnitCombatTypes, GC.getNumUnitCombatInfos());
	// bool vectors without delayed resolution
	CheckSumC(iSum, m_aiSubCombatChangeTypes);
	CheckSumC(iSum, m_aiRemovesUnitCombatTypes);
	CheckSumC(iSum, m_aiOnGameOptions);
	CheckSumC(iSum, m_aiNotOnGameOptions);
	CheckSumC(iSum, m_aiFreetoUnitCombats);
	CheckSumC(iSum, m_aiNotOnUnitCombatTypes);
	CheckSumC(iSum, m_aiNotOnDomainTypes);
	CheckSumC(iSum, m_aeMapCategoryTypes);
	CheckSumC(iSum, m_aiPrereqBonusTypes);
	CheckSumC(iSum, m_aiAddsBuildTypes);
	CheckSumC(iSum, m_aiNegatesInvisibilityTypes);
	CheckSumC(iSum, m_aiPrereqTerrainTypes);
	CheckSumC(iSum, m_aiPrereqFeatureTypes);
	CheckSumC(iSum, m_aiPrereqImprovementTypes);
	CheckSumC(iSum, m_aiPrereqPlotBonusTypes);
	CheckSumC(iSum, m_aiPrereqLocalBuildingTypes);
	CheckSumC(iSum, m_aiTrapSetWithPromotionTypes);
	CheckSumC(iSum, m_aiTrapImmunityUnitCombatTypes);
	CheckSumC(iSum, m_aiTargetUnitCombatTypes);
	CheckSumC(iSum, m_aiCategories);
	// int vectors utilizing pairing without delayed resolution
	CheckSumC(iSum, m_aFlankingStrengthbyUnitCombatTypeChange);
	CheckSumC(iSum, m_aTrapDisableUnitCombatTypes);
	CheckSumC(iSum, m_aTrapAvoidanceUnitCombatTypes);
	CheckSumC(iSum, m_aTrapTriggerUnitCombatTypes);
//Team Project (4)
		//WorkRateMod
	CheckSumC(iSum, m_aBuildWorkRateModifierChangeTypes);
	CheckSumC(iSum, m_aVisibilityIntensityChangeTypes);
	CheckSumC(iSum, m_aInvisibilityIntensityChangeTypes);
	CheckSumC(iSum, m_aVisibilityIntensityRangeChangeTypes);
	// int vectors utilizing struct with delayed resolution
	int iNumElements;

	iNumElements = m_aAIWeightbyUnitCombatTypes.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aAIWeightbyUnitCombatTypes[i].eUnitCombat);
		CheckSum(iSum, m_aAIWeightbyUnitCombatTypes[i].iModifier);
	}

	iNumElements = m_aHealUnitCombatChangeTypes.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aHealUnitCombatChangeTypes[i].eUnitCombat);
		CheckSum(iSum, m_aHealUnitCombatChangeTypes[i].iHeal);
		CheckSum(iSum, m_aHealUnitCombatChangeTypes[i].iAdjacentHeal);
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

	//TB Combat Mods End  TB SubCombat Mod end
}

