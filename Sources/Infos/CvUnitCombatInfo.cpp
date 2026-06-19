#include "CvGameCoreDLL.h"
#include "CvUnitCombatInfo.h"
#include "AI/CvGameAI.h"
#include "CvInfoUtil.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvUnitCombatInfo
//
//  DESC:   Contains unit combat types
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

CvUnitCombatInfo::CvUnitCombatInfo()
	: m_piDomainModifierPercent(NULL)
{
	CvInfoUtil(this).initDataMembers();

	// Non-XML runtime field; never read, copied or checksummed.
	m_zobristValue = GC.getGame().getSorenRand().getInt();
}

CvUnitCombatInfo::~CvUnitCombatInfo()
{
	PROFILE_EXTRA_FUNC();
	// Clears any pending delayed-resolution entries registered by the declared FK wrappers
	// (addEnum may pick the delayed path depending on XML load order).
	CvInfoUtil(this).uninitDataMembers();

	SAFE_DELETE_ARRAY(m_piDomainModifierPercent);

	foreach_(const CvOutcomeMission* outcomeMission, m_aOutcomeMissions)
	{
		SAFE_DELETE(outcomeMission);
	}

	for (int i=0; i<(int)m_aBuildWorkChangeModifiers.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aBuildWorkChangeModifiers[i]));
	}

	for (int i=0; i<(int)m_aUnitCombatChangeModifiers.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aUnitCombatChangeModifiers[i]));
	}

	for (int i=0; i<(int)m_aFlankingStrengthbyUnitCombatTypeChange.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aFlankingStrengthbyUnitCombatTypeChange[i]));
	}

	for (int i=0; i<(int)m_aTrapAvoidanceUnitCombatTypes.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aTrapAvoidanceUnitCombatTypes[i]));
	}

}

// Textual References
ReligionTypes CvUnitCombatInfo::getReligion() const
{
	return m_eReligion;
}

BonusTypes CvUnitCombatInfo::getCulture() const
{
	return m_eCulture;
}

EraTypes CvUnitCombatInfo::getEra() const
{
	return m_eEra;
}
//Integers
int CvUnitCombatInfo::getVisibilityChange() const
{
	return m_iVisibilityChange;
}

int CvUnitCombatInfo::getMovesChange() const
{
	return m_iMovesChange;
}

int CvUnitCombatInfo::getMoveDiscountChange() const
{
	return m_iMoveDiscountChange;
}

int CvUnitCombatInfo::getAirRangeChange() const
{
	return m_iAirRangeChange;
}

int CvUnitCombatInfo::getInterceptChange() const
{
	return m_iInterceptChange;
}

int CvUnitCombatInfo::getEvasionChange() const
{
	return m_iEvasionChange;
}

int CvUnitCombatInfo::getWithdrawalChange() const
{
	return m_iWithdrawalChange;
}

int CvUnitCombatInfo::getCargoChange() const
{
	return m_iCargoChange;
}

int CvUnitCombatInfo::getSMCargoChange() const
{
	return m_iSMCargoChange;
}

int CvUnitCombatInfo::getSMCargoVolumeChange() const
{
	return m_iSMCargoVolumeChange;
}

int CvUnitCombatInfo::getSMCargoVolumeModifierChange() const
{
	return m_iSMCargoVolumeModifierChange;
}

int CvUnitCombatInfo::getCollateralDamageChange() const
{
	return m_iCollateralDamageChange;
}

int CvUnitCombatInfo::getBombardRateChange() const
{
	return m_iBombardRateChange;
}

int CvUnitCombatInfo::getFirstStrikesChange() const
{
	return m_iFirstStrikesChange;
}

int CvUnitCombatInfo::getChanceFirstStrikesChange() const
{
	return m_iChanceFirstStrikesChange;
}

int CvUnitCombatInfo::getEnemyHealChange() const
{
	return m_iEnemyHealChange;
}

int CvUnitCombatInfo::getNeutralHealChange() const
{
	return m_iNeutralHealChange;
}

int CvUnitCombatInfo::getFriendlyHealChange() const
{
	return m_iFriendlyHealChange;
}

int CvUnitCombatInfo::getSameTileHealChange() const
{
	return m_iSameTileHealChange;
}

int CvUnitCombatInfo::getAdjacentTileHealChange() const
{
	return m_iAdjacentTileHealChange;
}

int CvUnitCombatInfo::getCombatPercent() const
{
	return m_iCombatPercent;
}

int CvUnitCombatInfo::getCityAttackPercent() const
{
	return m_iCityAttackPercent;
}

int CvUnitCombatInfo::getCityDefensePercent() const
{
	return m_iCityDefensePercent;
}

int CvUnitCombatInfo::getHillsAttackPercent() const
{
	return m_iHillsAttackPercent;
}

int CvUnitCombatInfo::getHillsDefensePercent() const
{
	return m_iHillsDefensePercent;
}

int CvUnitCombatInfo::getHillsWorkPercent() const
{
	return m_iHillsWorkPercent;
}

int CvUnitCombatInfo::getPeaksWorkPercent() const
{
	return m_iPeaksWorkPercent;
}

int CvUnitCombatInfo::getWorkRatePercent() const
{
	return m_iWorkRatePercent;
}

int CvUnitCombatInfo::getRevoltProtection() const
{
	return m_iRevoltProtection;
}

int CvUnitCombatInfo::getCollateralDamageProtection() const
{
	return m_iCollateralDamageProtection;
}

int CvUnitCombatInfo::getPillageChange() const
{
	return m_iPillageChange;
}

int CvUnitCombatInfo::getUpgradeDiscount() const
{
	return m_iUpgradeDiscount;
}

int CvUnitCombatInfo::getExperiencePercent() const
{
	return m_iExperiencePercent;
}

int CvUnitCombatInfo::getKamikazePercent() const
{
	return m_iKamikazePercent;
}

int CvUnitCombatInfo::getAirCombatLimitChange() const
{
	return m_iAirCombatLimitChange;
}

int CvUnitCombatInfo::getCelebrityHappy() const
{
	return m_iCelebrityHappy;
}

int CvUnitCombatInfo::getCollateralDamageLimitChange() const
{
	return m_iCollateralDamageLimitChange;
}

int CvUnitCombatInfo::getCollateralDamageMaxUnitsChange() const
{
	return m_iCollateralDamageMaxUnitsChange;
}

int CvUnitCombatInfo::getCombatLimitChange() const
{
	return m_iCombatLimitChange;
}

int CvUnitCombatInfo::getExtraDropRange() const
{
	return m_iExtraDropRange;
}

int CvUnitCombatInfo::getSurvivorChance() const
{
	return m_iSurvivorChance;
}

int CvUnitCombatInfo::getVictoryAdjacentHeal() const
{
	return m_iVictoryAdjacentHeal;
}

int CvUnitCombatInfo::getVictoryHeal() const
{
	return m_iVictoryHeal;
}

int CvUnitCombatInfo::getVictoryStackHeal() const
{
	return m_iVictoryStackHeal;
}

int CvUnitCombatInfo::getAttackCombatModifierChange() const
{
	return m_iAttackCombatModifierChange;
}

int CvUnitCombatInfo::getDefenseCombatModifierChange() const
{
	return m_iDefenseCombatModifierChange;
}

int CvUnitCombatInfo::getVSBarbsChange() const
{
	return m_iVSBarbsChange;
}


int CvUnitCombatInfo::getUnnerveChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iUnnerveChange;
}

int CvUnitCombatInfo::getEncloseChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iEncloseChange;
}

int CvUnitCombatInfo::getLungeChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iLungeChange;
}

int CvUnitCombatInfo::getDynamicDefenseChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	return m_iDynamicDefenseChange;
}

int CvUnitCombatInfo::getStrengthChange() const
{
	return m_iStrengthChange;
}

int CvUnitCombatInfo::getEnduranceChange() const
{
	return m_iEnduranceChange;
}


int CvUnitCombatInfo::getPoisonProbabilityModifierChange() const
{
	return m_iPoisonProbabilityModifierChange;
}

int CvUnitCombatInfo::getCaptureProbabilityModifierChange() const
{
	return m_iCaptureProbabilityModifierChange;
}

int CvUnitCombatInfo::getCaptureResistanceModifierChange() const
{
	return m_iCaptureResistanceModifierChange;
}

int CvUnitCombatInfo::getBreakdownChanceChange() const
{
	return m_iBreakdownChanceChange;
}

int CvUnitCombatInfo::getBreakdownDamageChange() const
{
	return m_iBreakdownDamageChange;
}

int CvUnitCombatInfo::getTauntChange() const
{
	return m_iTauntChange;
}

int CvUnitCombatInfo::getMaxHPChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iMaxHPChange;
}

int CvUnitCombatInfo::getStrengthModifier() const
{
	return m_iStrengthModifier;
}

int CvUnitCombatInfo::getQualityBase() const
{
	return m_iQualityBase;
}

int CvUnitCombatInfo::getGroupBase() const
{
	return m_iGroupBase;
}

int CvUnitCombatInfo::getSizeBase() const
{
	return m_iSizeBase;
}

int CvUnitCombatInfo::getDamageModifierChange() const
{
	return m_iDamageModifierChange;
}

int CvUnitCombatInfo::getUpkeepModifier() const
{
	return m_iUpkeepModifier;
}

int CvUnitCombatInfo::getExtraUpkeep100() const
{
	return m_iExtraUpkeep100;
}

int CvUnitCombatInfo::getRBombardDamageBase() const
{
	return m_iRBombardDamageBase;
}

int CvUnitCombatInfo::getRBombardDamageLimitBase() const
{
	return m_iRBombardDamageLimitBase;
}

int CvUnitCombatInfo::getRBombardDamageMaxUnitsBase() const
{
	return m_iRBombardDamageMaxUnitsBase;
}

int CvUnitCombatInfo::getDCMBombRangeBase() const
{
	return m_iDCMBombRangeBase;
}

int CvUnitCombatInfo::getDCMBombAccuracyBase() const
{
	return m_iDCMBombAccuracyBase;
}

int CvUnitCombatInfo::getCombatModifierPerSizeMoreChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerSizeMoreChange;
}

int CvUnitCombatInfo::getCombatModifierPerSizeLessChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerSizeLessChange;
}

int CvUnitCombatInfo::getCombatModifierPerVolumeMoreChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerVolumeMoreChange;
}

int CvUnitCombatInfo::getCombatModifierPerVolumeLessChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 0;
	}
	return m_iCombatModifierPerVolumeLessChange;
}

int CvUnitCombatInfo::getSelfHealModifier() const
{
	return m_iSelfHealModifier;
}

int CvUnitCombatInfo::getNumHealSupport() const
{
	return m_iNumHealSupport;
}

int CvUnitCombatInfo::getExcileChange() const
{
	return m_iExcileChange;
}

int CvUnitCombatInfo::getPassageChange() const
{
	return m_iPassageChange;
}

int CvUnitCombatInfo::getNoNonOwnedCityEntryChange() const
{
	return m_iNoNonOwnedCityEntryChange;
}

int CvUnitCombatInfo::getBarbCoExistChange() const
{
	return m_iBarbCoExistChange;
}

int CvUnitCombatInfo::getBlendIntoCityChange() const
{
	return m_iBlendIntoCityChange;
}

int CvUnitCombatInfo::getInsidiousnessChange() const
{
	return m_iInsidiousnessChange;
}

int CvUnitCombatInfo::getInvestigationChange() const
{
	return m_iInvestigationChange;
}

int CvUnitCombatInfo::getStealthStrikesChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	return m_iStealthStrikesChange;
}

int CvUnitCombatInfo::getStealthCombatModifierChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	return m_iStealthCombatModifierChange;
}

int CvUnitCombatInfo::getStealthDefenseChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	return m_iStealthDefenseChange;
}

int CvUnitCombatInfo::getDefenseOnlyChange() const
{
	return m_iDefenseOnlyChange;
}

int CvUnitCombatInfo::getNoInvisibilityChange() const
{
	return m_iNoInvisibilityChange;
}

int CvUnitCombatInfo::getNoCaptureChange() const
{
	return m_iNoCaptureChange;
}

int CvUnitCombatInfo::getAnimalIgnoresBordersChange() const
{
	return m_iAnimalIgnoresBordersChange;
}

int CvUnitCombatInfo::getNoDefensiveBonusChange() const
{
	return m_iNoDefensiveBonusChange;
}

int CvUnitCombatInfo::getGatherHerdChange() const
{
	return m_iGatherHerdChange;
}

int CvUnitCombatInfo::getReligiousCombatModifierChange() const
{
	return m_iReligiousCombatModifierChange;
}


//Booleans
bool CvUnitCombatInfo::isDefensiveVictoryMove() const
{
	return m_bDefensiveVictoryMove;
}

bool CvUnitCombatInfo::isFreeDrop() const
{
	return m_bFreeDrop;
}

bool CvUnitCombatInfo::isOffensiveVictoryMove() const
{
	return m_bOffensiveVictoryMove;
}

bool CvUnitCombatInfo::isOneUp() const
{
	return m_bOneUp;
}

bool CvUnitCombatInfo::isPillageEspionage() const
{
	return m_bPillageEspionage;
}

bool CvUnitCombatInfo::isPillageMarauder() const
{
	return m_bPillageMarauder;
}

bool CvUnitCombatInfo::isPillageOnMove() const
{
	return m_bPillageOnMove;
}

bool CvUnitCombatInfo::isPillageOnVictory() const
{
	return m_bPillageOnVictory;
}

bool CvUnitCombatInfo::isPillageResearch() const
{
	return m_bPillageResearch;
}

bool CvUnitCombatInfo::isBlitz() const
{
	return m_bBlitz;
}

bool CvUnitCombatInfo::isAmphib() const
{
	return m_bAmphib;
}

bool CvUnitCombatInfo::isRiver() const
{
	return m_bRiver;
}

bool CvUnitCombatInfo::isEnemyRoute() const
{
	return m_bEnemyRoute;
}

bool CvUnitCombatInfo::isAlwaysHeal() const
{
	return m_bAlwaysHeal;
}

bool CvUnitCombatInfo::isHillsDoubleMove() const
{
	return m_bHillsDoubleMove;
}

bool CvUnitCombatInfo::isImmuneToFirstStrikes() const
{
	return m_bImmuneToFirstStrikes;
}

bool CvUnitCombatInfo::isStampedeChange() const
{
	return m_bStampedeChange;
}

bool CvUnitCombatInfo::isRemoveStampede() const
{
	return m_bRemoveStampede;
}
bool CvUnitCombatInfo::isOnslaughtChange() const
{
	return m_bOnslaughtChange;
}


bool CvUnitCombatInfo::isAttackOnlyCitiesAdd() const
{
	return m_bAttackOnlyCitiesAdd;
}

bool CvUnitCombatInfo::isAttackOnlyCitiesSubtract() const
{
	return m_bAttackOnlyCitiesSubtract;
}

bool CvUnitCombatInfo::isIgnoreNoEntryLevelAdd() const
{
	return m_bIgnoreNoEntryLevelAdd;
}

bool CvUnitCombatInfo::isIgnoreNoEntryLevelSubtract() const
{
	return m_bIgnoreNoEntryLevelSubtract;
}

bool CvUnitCombatInfo::isIgnoreZoneofControlAdd() const
{
	return m_bIgnoreZoneofControlAdd;
}

bool CvUnitCombatInfo::isIgnoreZoneofControlSubtract() const
{
	return m_bIgnoreZoneofControlSubtract;
}

bool CvUnitCombatInfo::isFliesToMoveAdd() const
{
	return m_bFliesToMoveAdd;
}

bool CvUnitCombatInfo::isFliesToMoveSubtract() const
{
	return m_bFliesToMoveSubtract;
}

bool CvUnitCombatInfo::isCannotMergeSplit() const
{
	return m_bCannotMergeSplit;
}

bool CvUnitCombatInfo::isRBombardDirect() const
{
	return m_bRBombardDirect;
}

bool CvUnitCombatInfo::isRBombardForceAbility() const
{
	return m_bRBombardForceAbility;
}

bool CvUnitCombatInfo::changesMoveThroughPlots() const
{
	return (isAmphib() ||
			isCanMovePeaks() ||
			isCanLeadThroughPeaks() ||
			(int)m_aiTerrainDoubleMoveChangeTypes.size() > 0 ||
			(int)m_aiFeatureDoubleMoveChangeTypes.size() > 0 ||
			m_bHillsDoubleMove);
}

bool CvUnitCombatInfo::isCanMovePeaks() const
{
	return m_bCanMovePeaks;
}

bool CvUnitCombatInfo::isCanLeadThroughPeaks() const
{
	return m_bCanLeadThroughPeaks;
}

bool CvUnitCombatInfo::isZoneOfControl() const
{
	return m_bZoneOfControl;
}

bool CvUnitCombatInfo::isSpy() const
{
	return m_bSpy;
}

bool CvUnitCombatInfo::isAlwaysInvisible() const
{
	return m_bInvisible;
}

bool CvUnitCombatInfo::isForMilitary() const
{
	return m_bForMilitary;
}

bool CvUnitCombatInfo::isForNavalMilitary() const
{
	return m_bForNavalMilitary;
}

bool CvUnitCombatInfo::isHealsAs() const
{
	return m_bHealsAs;
}

bool CvUnitCombatInfo::isNoSelfHeal() const
{
	return m_bNoSelfHeal;
}
//Arrays
int CvUnitCombatInfo::getDomainModifierPercent(int i) const
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, i);
	return m_piDomainModifierPercent ? m_piDomainModifierPercent[i] : 0;
}

bool CvUnitCombatInfo::isAnyDomainModifierPercent() const
{
	return m_piDomainModifierPercent != NULL;
}


int CvUnitCombatInfo::getTerrainDoubleMoveChangeType(int i) const
{
	return m_aiTerrainDoubleMoveChangeTypes[i];
}

int CvUnitCombatInfo::getNumTerrainDoubleMoveChangeTypes() const
{
	return (int)m_aiTerrainDoubleMoveChangeTypes.size();
}

bool CvUnitCombatInfo::isTerrainDoubleMoveChangeType(int i) const
{
	return algo::any_of_equal(m_aiTerrainDoubleMoveChangeTypes, i);
}

int CvUnitCombatInfo::getFeatureDoubleMoveChangeType(int i) const
{
	return m_aiFeatureDoubleMoveChangeTypes[i];
}

int CvUnitCombatInfo::getNumFeatureDoubleMoveChangeTypes() const
{
	return (int)m_aiFeatureDoubleMoveChangeTypes.size();
}

bool CvUnitCombatInfo::isFeatureDoubleMoveChangeType(int i) const
{
	return algo::any_of_equal(m_aiFeatureDoubleMoveChangeTypes, i);
}
int CvUnitCombatInfo::getOnGameOption(int i) const
{
	return m_aiOnGameOptions[i];
}

int CvUnitCombatInfo::getNumOnGameOptions() const
{
	return (int)m_aiOnGameOptions.size();
}

bool CvUnitCombatInfo::isOnGameOption(int i) const
{
	return algo::any_of_equal(m_aiOnGameOptions, i);
}

int CvUnitCombatInfo::getNotOnGameOption(int i) const
{
	return m_aiNotOnGameOptions[i];
}

int CvUnitCombatInfo::getNumNotOnGameOptions() const
{
	return (int)m_aiNotOnGameOptions.size();
}

bool CvUnitCombatInfo::isNotOnGameOption(int i) const
{
	return algo::any_of_equal(m_aiNotOnGameOptions, i);
}

int CvUnitCombatInfo::getGGptsforUnitType(int i) const
{
	return m_aiGGptsforUnitTypes[i];
}

int CvUnitCombatInfo::getNumGGptsforUnitTypes() const
{
	return (int)m_aiGGptsforUnitTypes.size();
}

bool CvUnitCombatInfo::isGGptsforUnitType(int i) const
{
	return algo::any_of_equal(m_aiGGptsforUnitTypes, i);
}

int CvUnitCombatInfo::getDefaultStatusType(int i) const
{
	return m_aiDefaultStatusTypes[i];
}

int CvUnitCombatInfo::getNumDefaultStatusTypes() const
{
	return (int)m_aiDefaultStatusTypes.size();
}

bool CvUnitCombatInfo::isDefaultStatusType(int i) const
{
	return algo::any_of_equal(m_aiDefaultStatusTypes, i);
}

int CvUnitCombatInfo::getTrapImmunityUnitCombatType(int i) const
{
	return m_aiTrapImmunityUnitCombatTypes[i];
}

int CvUnitCombatInfo::getNumTrapImmunityUnitCombatTypes() const
{
	return (int)m_aiTrapImmunityUnitCombatTypes.size();
}

bool CvUnitCombatInfo::isTrapImmunityUnitCombatType(int i) const
{
	return algo::any_of_equal(m_aiTrapImmunityUnitCombatTypes, i);
}

int CvUnitCombatInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}

int CvUnitCombatInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}

bool CvUnitCombatInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}

// int vector utilizing pairing without delayed resolution

int CvUnitCombatInfo::getNumVisibilityIntensityChangeTypes() const
{
	return m_aVisibilityIntensityChangeTypes.size();
}

int CvUnitCombatInfo::getVisibilityIntensityChangeType(int iInvisibility) const
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

bool CvUnitCombatInfo::isVisibilityIntensityChangeType(int iInvisibility) const
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

int CvUnitCombatInfo::getNumInvisibilityIntensityChangeTypes() const
{
	return m_aInvisibilityIntensityChangeTypes.size();
}

int CvUnitCombatInfo::getInvisibilityIntensityChangeType(int iInvisibility) const
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

bool CvUnitCombatInfo::isInvisibilityIntensityChangeType(int iInvisibility) const
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

int CvUnitCombatInfo::getNumVisibilityIntensityRangeChangeTypes() const
{
	return m_aVisibilityIntensityRangeChangeTypes.size();
}

int CvUnitCombatInfo::getVisibilityIntensityRangeChangeType(int iInvisibility) const
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

bool CvUnitCombatInfo::isVisibilityIntensityRangeChangeType(int iInvisibility) const
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

int CvUnitCombatInfo::getNumVisibilityIntensitySameTileChangeTypes() const
{
	return m_aVisibilityIntensitySameTileChangeTypes.size();
}

int CvUnitCombatInfo::getVisibilityIntensitySameTileChangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensitySameTileChangeTypes.begin(); it != m_aVisibilityIntensitySameTileChangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvUnitCombatInfo::isVisibilityIntensitySameTileChangeType(int iInvisibility) const
{
	PROFILE_EXTRA_FUNC();
	for (InvisibilityArray::const_iterator it = m_aVisibilityIntensitySameTileChangeTypes.begin(); it != m_aVisibilityIntensitySameTileChangeTypes.end(); ++it)
	{
		if ((*it).first == (InvisibleTypes)iInvisibility)
		{
			return true;
		}
	}
	return false;
}


int CvUnitCombatInfo::getNumTerrainAttackChangeModifiers() const
{
	return (int)m_aTerrainAttackChangeModifiers.size();
}

const TerrainModifier& CvUnitCombatInfo::getTerrainAttackChangeModifier(int iTerrain) const
{
	FASSERT_BOUNDS(0, getNumTerrainAttackChangeModifiers(), iTerrain);
	return m_aTerrainAttackChangeModifiers[iTerrain];
}

int CvUnitCombatInfo::getNumTerrainDefenseChangeModifiers() const
{
	return (int)m_aTerrainDefenseChangeModifiers.size();
}

const TerrainModifier& CvUnitCombatInfo::getTerrainDefenseChangeModifier(int iTerrain) const
{
	FASSERT_BOUNDS(0, getNumTerrainDefenseChangeModifiers(), iTerrain);
	return m_aTerrainDefenseChangeModifiers[iTerrain];
}

int CvUnitCombatInfo::getNumTerrainWorkChangeModifiers() const
{
	return (int)m_aTerrainWorkChangeModifiers.size();
}

const TerrainModifier& CvUnitCombatInfo::getTerrainWorkChangeModifier(int iTerrain) const
{
	FASSERT_BOUNDS(0, getNumTerrainWorkChangeModifiers(), iTerrain);
	return m_aTerrainWorkChangeModifiers[iTerrain];
}

int CvUnitCombatInfo::getNumBuildWorkChangeModifiers() const
{
	return (int)m_aBuildWorkChangeModifiers.size();
}

const BuildModifier& CvUnitCombatInfo::getBuildWorkChangeModifier(int iBuild) const
{
	FASSERT_BOUNDS(0, getNumBuildWorkChangeModifiers(), iBuild);
	return m_aBuildWorkChangeModifiers[iBuild];
}

int CvUnitCombatInfo::getNumFeatureAttackChangeModifiers() const
{
	return (int)m_aFeatureAttackChangeModifiers.size();
}

const FeatureModifier& CvUnitCombatInfo::getFeatureAttackChangeModifier(int iFeature) const
{
	FASSERT_BOUNDS(0, getNumFeatureAttackChangeModifiers(), iFeature);
	return m_aFeatureAttackChangeModifiers[iFeature];
}

int CvUnitCombatInfo::getNumFeatureDefenseChangeModifiers() const
{
	return (int)m_aFeatureDefenseChangeModifiers.size();
}

const FeatureModifier& CvUnitCombatInfo::getFeatureDefenseChangeModifier(int iFeature) const
{
	FASSERT_BOUNDS(0, getNumFeatureDefenseChangeModifiers(), iFeature);
	return m_aFeatureDefenseChangeModifiers[iFeature];
}

int CvUnitCombatInfo::getNumFeatureWorkChangeModifiers() const
{
	return (int)m_aFeatureWorkChangeModifiers.size();
}

const FeatureModifier& CvUnitCombatInfo::getFeatureWorkChangeModifier(int iFeature) const
{
	FASSERT_BOUNDS(0, getNumFeatureWorkChangeModifiers(), iFeature);
	return m_aFeatureWorkChangeModifiers[iFeature];
}

int CvUnitCombatInfo::getNumUnitCombatChangeModifiers() const
{
	return (int)m_aUnitCombatChangeModifiers.size();
}

const UnitCombatModifier& CvUnitCombatInfo::getUnitCombatChangeModifier(int iUnitCombat) const
{
	FASSERT_BOUNDS(0, getNumUnitCombatChangeModifiers(), iUnitCombat);
	return m_aUnitCombatChangeModifiers[iUnitCombat];
}

int CvUnitCombatInfo::getNumFlankingStrengthbyUnitCombatTypesChange() const
{
	return (int)m_aFlankingStrengthbyUnitCombatTypeChange.size();
}

const UnitCombatModifier& CvUnitCombatInfo::getFlankingStrengthbyUnitCombatTypeChange(int iUnitCombat) const
{
	FASSERT_BOUNDS(0, getNumFlankingStrengthbyUnitCombatTypesChange(), iUnitCombat);
	return m_aFlankingStrengthbyUnitCombatTypeChange[iUnitCombat];
}

int CvUnitCombatInfo::getNumTrapAvoidanceUnitCombatTypes() const
{
	return (int)m_aTrapAvoidanceUnitCombatTypes.size();
}

const UnitCombatModifier& CvUnitCombatInfo::getTrapAvoidanceUnitCombatType(int iIndex) const
{
	return m_aTrapAvoidanceUnitCombatTypes[iIndex];
}


int CvUnitCombatInfo::getNumInvisibleTerrainChanges() const
{
	return (int)m_aInvisibleTerrainChanges.size();
}

const InvisibleTerrainChanges& CvUnitCombatInfo::getInvisibleTerrainChange(int iIndex) const
{
	return m_aInvisibleTerrainChanges[iIndex];
}

int CvUnitCombatInfo::getNumInvisibleFeatureChanges() const
{
	return (int)m_aInvisibleFeatureChanges.size();
}

const InvisibleFeatureChanges& CvUnitCombatInfo::getInvisibleFeatureChange(int iIndex) const
{
	return m_aInvisibleFeatureChanges[iIndex];
}

int CvUnitCombatInfo::getNumInvisibleImprovementChanges() const
{
	return (int)m_aInvisibleImprovementChanges.size();
}

const InvisibleImprovementChanges& CvUnitCombatInfo::getInvisibleImprovementChange(int iIndex) const
{
	return m_aInvisibleImprovementChanges[iIndex];
}

int CvUnitCombatInfo::getNumVisibleTerrainChanges() const
{
	return (int)m_aVisibleTerrainChanges.size();
}

const InvisibleTerrainChanges& CvUnitCombatInfo::getVisibleTerrainChange(int iIndex) const
{
	return m_aVisibleTerrainChanges[iIndex];
}

int CvUnitCombatInfo::getNumVisibleFeatureChanges() const
{
	return (int)m_aVisibleFeatureChanges.size();
}

const InvisibleFeatureChanges& CvUnitCombatInfo::getVisibleFeatureChange(int iIndex) const
{
	return m_aVisibleFeatureChanges[iIndex];
}

int CvUnitCombatInfo::getNumVisibleImprovementChanges() const
{
	return (int)m_aVisibleImprovementChanges.size();
}

const InvisibleImprovementChanges& CvUnitCombatInfo::getVisibleImprovementChange(int iIndex) const
{
	return m_aVisibleImprovementChanges[iIndex];
}

int CvUnitCombatInfo::getNumVisibleTerrainRangeChanges() const
{
	return (int)m_aVisibleTerrainRangeChanges.size();
}

const InvisibleTerrainChanges& CvUnitCombatInfo::getVisibleTerrainRangeChange(int iIndex) const
{
	return m_aVisibleTerrainRangeChanges[iIndex];
}

int CvUnitCombatInfo::getNumVisibleFeatureRangeChanges() const
{
	return (int)m_aVisibleFeatureRangeChanges.size();
}

const InvisibleFeatureChanges& CvUnitCombatInfo::getVisibleFeatureRangeChange(int iIndex) const
{
	return m_aVisibleFeatureRangeChanges[iIndex];
}

int CvUnitCombatInfo::getNumVisibleImprovementRangeChanges() const
{
	return (int)m_aVisibleImprovementRangeChanges.size();
}

const InvisibleImprovementChanges& CvUnitCombatInfo::getVisibleImprovementRangeChange(int iIndex) const
{
	return m_aVisibleImprovementRangeChanges[iIndex];
}

void CvUnitCombatInfo::getDataMembers(CvInfoUtil& util)
{
	// HYBRID migration (#196/#302): every wrapper-expressible field is declared here; the
	// remainder stays hand-written in read()/copyNonDefaults() (see the notes there).
	// getCheckSum is kept fully explicit/legacy (NOT delegated to CvInfoUtil): the legacy hash
	// leads with the hand-written outcome lists, deliberately omits m_bZoneOfControl, and
	// interleaves the hand-written arrays/pair-vectors/struct-vectors mid-order.
	// Declarations follow the legacy checksum order anyway, for readability.
	util
		// Textual references
		.addEnum(m_eReligion, L"ReligionType")
		.addEnum(m_eCulture, L"CultureType")
		.addEnum(m_eEra, L"EraType")
		// Integers
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
		.add(m_iAttackCombatModifierChange, L"iAttackCombatModifierChange")
		.add(m_iDefenseCombatModifierChange, L"iDefenseCombatModifierChange")
		.add(m_iVSBarbsChange, L"iVSBarbsChange")
		.add(m_iUnnerveChange, L"iUnnerveChange")
		.add(m_iEncloseChange, L"iEncloseChange")
		.add(m_iLungeChange, L"iLungeChange")
		.add(m_iDynamicDefenseChange, L"iDynamicDefenseChange")
		.add(m_iStrengthChange, L"iStrengthChange")
		.add(m_iEnduranceChange, L"iEnduranceChange")
		.add(m_iPoisonProbabilityModifierChange, L"iPoisonProbabilityModifierChange")
		.add(m_iCaptureProbabilityModifierChange, L"iCaptureProbabilityModifierChange")
		.add(m_iCaptureResistanceModifierChange, L"iCaptureResistanceModifierChange")
		.add(m_iBreakdownChanceChange, L"iBreakdownChanceChange")
		.add(m_iBreakdownDamageChange, L"iBreakdownDamageChange")
		.add(m_iTauntChange, L"iTauntChange")
		.add(m_iMaxHPChange, L"iMaxHPChange")
		.add(m_iStrengthModifier, L"iStrengthModifier")
		.add(m_iQualityBase, L"iQualityBase", -10)
		.add(m_iGroupBase, L"iGroupBase", -10)
		.add(m_iSizeBase, L"iSizeBase", -10)
		.add(m_iDamageModifierChange, L"iDamageModifierChange")
		.add(m_iUpkeepModifier, L"iUpkeepModifier")
		.add(m_iExtraUpkeep100, L"iExtraUpkeep100")
		.add(m_iRBombardDamageBase, L"iRBombardDamageBase")
		.add(m_iRBombardDamageLimitBase, L"iRBombardDamageLimitBase")
		.add(m_iRBombardDamageMaxUnitsBase, L"iRBombardDamageMaxUnitsBase")
		.add(m_iDCMBombRangeBase, L"iDCMBombRangeBase")
		.add(m_iDCMBombAccuracyBase, L"iDCMBombAccuracyBase")
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
		.add(m_iInsidiousnessChange, L"iInsidiousnessChange")
		.add(m_iInvestigationChange, L"iInvestigationChange")
		.add(m_iStealthStrikesChange, L"iStealthStrikesChange")
		.add(m_iStealthCombatModifierChange, L"iStealthCombatModifierChange")
		.add(m_iStealthDefenseChange, L"iStealthDefenseChange")
		.add(m_iDefenseOnlyChange, L"iDefenseOnlyChange")
		.add(m_iNoInvisibilityChange, L"iNoInvisibilityChange")
		.add(m_iNoCaptureChange, L"iNoCaptureChange")
		.add(m_iAnimalIgnoresBordersChange, L"iAnimalIgnoresBordersChange")
		.add(m_iNoDefensiveBonusChange, L"iNoDefensiveBonusChange")
		.add(m_iGatherHerdChange, L"iGatherHerdChange")
		.add(m_iReligiousCombatModifierChange, L"iReligiousCombatModifierChange")
		// Booleans
		.add(m_bDefensiveVictoryMove, L"bDefensiveVictoryMove")
		.add(m_bFreeDrop, L"bFreeDrop")
		.add(m_bOffensiveVictoryMove, L"bOffensiveVictoryMove")
		.add(m_bOneUp, L"bOneUp")
		.add(m_bPillageEspionage, L"bPillageEspionage")
		.add(m_bPillageMarauder, L"bPillageMarauder")
		.add(m_bPillageOnMove, L"bPillageOnMove")
		.add(m_bPillageOnVictory, L"bPillageOnVictory")
		.add(m_bPillageResearch, L"bPillageResearch")
		.add(m_bBlitz, L"bBlitz")
		.add(m_bAmphib, L"bAmphib")
		.add(m_bRiver, L"bRiver")
		.add(m_bEnemyRoute, L"bEnemyRoute")
		.add(m_bAlwaysHeal, L"bAlwaysHeal")
		.add(m_bHillsDoubleMove, L"bHillsDoubleMove")
		.add(m_bImmuneToFirstStrikes, L"bImmuneToFirstStrikes")
		.add(m_bStampedeChange, L"bStampedeChange")
		.add(m_bRemoveStampede, L"bRemoveStampede")
		.add(m_bOnslaughtChange, L"bOnslaughtChange")
		.add(m_bAttackOnlyCitiesAdd, L"bAttackOnlyCitiesAdd")
		.add(m_bAttackOnlyCitiesSubtract, L"bAttackOnlyCitiesSubtract")
		.add(m_bIgnoreNoEntryLevelAdd, L"bIgnoreNoEntryLevelAdd")
		.add(m_bIgnoreNoEntryLevelSubtract, L"bIgnoreNoEntryLevelSubtract")
		.add(m_bIgnoreZoneofControlAdd, L"bIgnoreZoneofControlAdd")
		.add(m_bIgnoreZoneofControlSubtract, L"bIgnoreZoneofControlSubtract")
		.add(m_bFliesToMoveAdd, L"bFliesToMoveAdd")
		.add(m_bFliesToMoveSubtract, L"bFliesToMoveSubtract")
		.add(m_bCanMovePeaks, L"bCanMovePeaks")
		.add(m_bCanLeadThroughPeaks, L"bCanLeadThroughPeaks")
		// Read and merged, but deliberately absent from the (hand-written) legacy checksum:
		.add(m_bZoneOfControl, L"bZoneOfControl")
		.add(m_bSpy, L"bSpy")
		.add(m_bCannotMergeSplit, L"bCannotMergeSplit")
		.add(m_bRBombardDirect, L"bRBombardDirect")
		.add(m_bRBombardForceAbility, L"bRBombardForceAbility")
		.add(m_bInvisible, L"bInvisible") // legacy read this tag twice; once is enough
		.add(m_bForMilitary, L"bForMilitary")
		.add(m_bForNavalMilitary, L"bForNavalMilitary")
		.add(m_bHealsAs, L"bHealsAs")
		.add(m_bNoSelfHeal, L"bNoSelfHeal")
		// Membership int-vectors (immediate resolution; wrapper merge = unique+sort).
		// NOTE: TrapImmunityUnitCombatTypes' legacy copyNonDefaults wrongly used the
		// delayed-resolution copy on an immediately-resolved vector (modular merge copied
		// zeros); the wrapper merge fixes that.
		.add(m_aiTerrainDoubleMoveChangeTypes, L"TerrainDoubleMoveChangeTypes")
		.add(m_aiFeatureDoubleMoveChangeTypes, L"FeatureDoubleMoveChangeTypes")
		.add(m_aiOnGameOptions, L"OnGameOptions")
		.add(m_aiNotOnGameOptions, L"NotOnGameOptions")
		.add(m_aiTrapImmunityUnitCombatTypes, L"TrapImmunityUnitCombatTypes")
		.add(m_aiCategories, L"Categories")
		// Self-contained sub-object
		.add(m_PropertyManipulators)
	;
}

bool CvUnitCombatInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	CvString szTextVal2;

	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	// Hand-written remainder, in legacy order:
	// - KillOutcomes / Actions: bespoke self-reading outcome objects (no wrapper).
	// - DomainMods: SetVariableListTagPair int array (no wrapper).
	// - GGptsforUnitTypes / DefaultStatusTypes: delayed-resolution int vectors (no wrapper yet).
	// - the 4 *IntensityChangeTypes lists: plain pair-vectors (no wrapper).
	// - the modifier/visibility struct-vectors: their structs (CvStructs.h) have no
	//   getDataMembers, and several columns need delayed resolution (BuildType and the
	//   self-referencing UnitCombatType FKs), which addStruct forces immediate.
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

	//Arrays
	pXML->SetVariableListTagPair(&m_piDomainModifierPercent, L"DomainMods", NUM_DOMAIN_TYPES);

	// int vectors with delayed resolution (no wrapper yet)
	pXML->SetOptionalVectorWithDelayedResolution(m_aiGGptsforUnitTypes, L"GGptsforUnitTypes");
	pXML->SetOptionalVectorWithDelayedResolution(m_aiDefaultStatusTypes, L"DefaultStatusTypes");

	// int vector utilizing pairing without delayed resolution
	m_aVisibilityIntensityChangeTypes.clear();
	if (pXML->TryMoveToXmlFirstChild(L"VisibilityIntensityChangeTypes"))
	{
		const int iNumSibs = pXML->GetXmlChildrenNumber();

		if (0 < iNumSibs)
		{
			if (pXML->TryMoveToXmlFirstChild())
			{
				for (int j = 0; j < iNumSibs; ++j)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						InvisibleTypes eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
						int iChange;
						pXML->GetNextXmlVal(&iChange);
						m_aVisibilityIntensityChangeTypes.push_back(std::make_pair(eInvisible, iChange));

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

	m_aInvisibilityIntensityChangeTypes.clear();
	if (pXML->TryMoveToXmlFirstChild(L"InvisibilityIntensityChangeTypes"))
	{
		const int iNumSibs = pXML->GetXmlChildrenNumber();

		if (0 < iNumSibs)
		{
			if (pXML->TryMoveToXmlFirstChild())
			{
				for (int j = 0; j < iNumSibs; ++j)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						InvisibleTypes eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
						int iChange;
						pXML->GetNextXmlVal(&iChange);
						m_aInvisibilityIntensityChangeTypes.push_back(std::make_pair(eInvisible, iChange));

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

	m_aVisibilityIntensityRangeChangeTypes.clear();
	if (pXML->TryMoveToXmlFirstChild(L"VisibilityIntensityRangeChangeTypes"))
	{
		const int iNumSibs = pXML->GetXmlChildrenNumber();

		if (0 < iNumSibs)
		{
			if (pXML->TryMoveToXmlFirstChild())
			{
				for (int j = 0; j < iNumSibs; ++j)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						InvisibleTypes eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
						int iChange;
						pXML->GetNextXmlVal(&iChange);
						m_aVisibilityIntensityRangeChangeTypes.push_back(std::make_pair(eInvisible, iChange));

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

	m_aVisibilityIntensitySameTileChangeTypes.clear();
	if (pXML->TryMoveToXmlFirstChild(L"VisibilityIntensitySameTileChangeTypes"))
	{
		const int iNumSibs = pXML->GetXmlChildrenNumber();

		if (0 < iNumSibs)
		{
			if (pXML->TryMoveToXmlFirstChild())
			{
				for (int j = 0; j < iNumSibs; ++j)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						InvisibleTypes eInvisible = (InvisibleTypes)pXML->GetInfoClass(szTextVal);
						int iChange;
						pXML->GetNextXmlVal(&iChange);
						m_aVisibilityIntensitySameTileChangeTypes.push_back(std::make_pair(eInvisible, iChange));

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

	if(pXML->TryMoveToXmlFirstChild(L"TerrainAttackChangeModifiers"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"TerrainAttackChangeModifier" );
		m_aTerrainAttackChangeModifiers.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"TerrainAttackChangeModifier"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"TerrainType");
					m_aTerrainAttackChangeModifiers[i].eTerrain = (TerrainTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aTerrainAttackChangeModifiers[i].iModifier), L"iModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"TerrainAttackChangeModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"TerrainDefenseChangeModifiers"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"TerrainDefenseChangeModifier" );
		m_aTerrainDefenseChangeModifiers.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"TerrainDefenseChangeModifier"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"TerrainType");
					m_aTerrainDefenseChangeModifiers[i].eTerrain = (TerrainTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aTerrainDefenseChangeModifiers[i].iModifier), L"iModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"TerrainDefenseChangeModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"TerrainWorkChangeModifiers"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"TerrainWorkChangeModifier" );
		m_aTerrainWorkChangeModifiers.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"TerrainWorkChangeModifier"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"TerrainType");
					m_aTerrainWorkChangeModifiers[i].eTerrain = (TerrainTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aTerrainWorkChangeModifiers[i].iModifier), L"iModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"TerrainWorkChangeModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"BuildWorkChangeModifiers"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"BuildWorkChangeModifier" );
		m_aBuildWorkChangeModifiers.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"BuildWorkChangeModifier"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"BuildType");
					pXML->GetChildXmlValByName(&(m_aBuildWorkChangeModifiers[i].iModifier), L"iModifier");
					GC.addDelayedResolution((int*)&(m_aBuildWorkChangeModifiers[i].eBuild), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"BuildWorkChangeModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"FeatureAttackChangeModifiers"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"FeatureAttackChangeModifier" );
		m_aFeatureAttackChangeModifiers.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"FeatureAttackChangeModifier"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"FeatureType");
					m_aFeatureAttackChangeModifiers[i].eFeature = (FeatureTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aFeatureAttackChangeModifiers[i].iModifier), L"iModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"FeatureAttackChangeModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"FeatureDefenseChangeModifiers"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"FeatureDefenseChangeModifier" );
		m_aFeatureDefenseChangeModifiers.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"FeatureDefenseChangeModifier"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"FeatureType");
					m_aFeatureDefenseChangeModifiers[i].eFeature = (FeatureTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aFeatureDefenseChangeModifiers[i].iModifier), L"iModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"FeatureDefenseChangeModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"FeatureWorkChangeModifiers"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"FeatureWorkChangeModifier" );
		m_aFeatureWorkChangeModifiers.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"FeatureWorkChangeModifier"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"FeatureType");
					m_aFeatureWorkChangeModifiers[i].eFeature = (FeatureTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aFeatureWorkChangeModifiers[i].iModifier), L"iModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"FeatureWorkChangeModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"UnitCombatChangeModifiers"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"UnitCombatChangeModifier" );
		m_aUnitCombatChangeModifiers.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"UnitCombatChangeModifier"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					pXML->GetChildXmlValByName(&(m_aUnitCombatChangeModifiers[i].iModifier), L"iModifier");
					GC.addDelayedResolution((int*)&(m_aUnitCombatChangeModifiers[i].eUnitCombat), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"UnitCombatChangeModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"FlankingStrengthbyUnitCombatTypesChanges"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"FlankingStrengthbyUnitCombatTypesChange" );
		m_aFlankingStrengthbyUnitCombatTypeChange.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"FlankingStrengthbyUnitCombatTypesChange"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					pXML->GetChildXmlValByName(&(m_aFlankingStrengthbyUnitCombatTypeChange[i].iModifier), L"iModifier");
					GC.addDelayedResolution((int*)&(m_aFlankingStrengthbyUnitCombatTypeChange[i].eUnitCombat), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"FlankingStrengthbyUnitCombatTypesChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"TrapAvoidanceUnitCombatTypes"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"TrapAvoidanceUnitCombatType" );
		m_aTrapAvoidanceUnitCombatTypes.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"TrapAvoidanceUnitCombatType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					pXML->GetChildXmlValByName(&(m_aTrapAvoidanceUnitCombatTypes[i].iModifier), L"iIntensity");
					GC.addDelayedResolution((int*)&(m_aTrapAvoidanceUnitCombatTypes[i].eUnitCombat), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"TrapAvoidanceUnitCombatType"));
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
		int iNum = pXML->GetXmlChildrenNumber(L"VisibleTerrainChange" );
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
		int iNum = pXML->GetXmlChildrenNumber(L"VisibleFeatureChange" );
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
		int iNum = pXML->GetXmlChildrenNumber(L"VisibleImprovementChange" );
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
		int iNum = pXML->GetXmlChildrenNumber(L"VisibleTerrainRangeChange" );
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
		int iNum = pXML->GetXmlChildrenNumber(L"VisibleFeatureRangeChange" );
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
		int iNum = pXML->GetXmlChildrenNumber(L"VisibleImprovementRangeChange" );
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

	return true;
}
void CvUnitCombatInfo::copyNonDefaults(CvUnitCombatInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const int iDefault = 0;

	CvInfoBase::copyNonDefaults(pClassInfo);

	m_KillOutcomeList.copyNonDefaults(&pClassInfo->m_KillOutcomeList);

	if (m_aOutcomeMissions.empty())
	{
		const int num = (int) pClassInfo->getNumActionOutcomes();
		for (int index = 0; index < num; index++)
		{
			m_aOutcomeMissions.push_back(pClassInfo->m_aOutcomeMissions[index]);
			pClassInfo->m_aOutcomeMissions[index] = NULL;
		}
	}

	// All declared fields (scalars, enums, membership vectors, property manipulators)
	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	// Hand-written remainder (see the note in getDataMembers)
	//Arrays
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


	if (getNumGGptsforUnitTypes() == 0)
	{
		const int iNum = pClassInfo->getNumGGptsforUnitTypes();
		m_aiGGptsforUnitTypes.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			GC.copyNonDefaultDelayedResolution((int*)&(m_aiGGptsforUnitTypes[i]), (int*)&(pClassInfo->m_aiGGptsforUnitTypes[i]));
		}
	}

	if (getNumDefaultStatusTypes() == 0)
	{
		const int iNum = pClassInfo->getNumDefaultStatusTypes();
		m_aiDefaultStatusTypes.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			GC.copyNonDefaultDelayedResolution((int*)&(m_aiDefaultStatusTypes[i]), (int*)&(pClassInfo->m_aiDefaultStatusTypes[i]));
		}
	}

	// int vectors utilizing pairing without delayed resolution

	if (getNumVisibilityIntensityChangeTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumVisibilityIntensityChangeTypes(); i++)
		{
			const InvisibleTypes eInvisible = ((InvisibleTypes)i);
			const int iChange = pClassInfo->getVisibilityIntensityChangeType(i);
			m_aVisibilityIntensityChangeTypes.push_back(std::make_pair(eInvisible, iChange));
		}
	}

	if (getNumInvisibilityIntensityChangeTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumInvisibilityIntensityChangeTypes(); i++)
		{
			const InvisibleTypes eInvisible = ((InvisibleTypes)i);
			const int iChange = pClassInfo->getInvisibilityIntensityChangeType(i);
			m_aInvisibilityIntensityChangeTypes.push_back(std::make_pair(eInvisible, iChange));
		}
	}

	if (getNumVisibilityIntensityRangeChangeTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumVisibilityIntensityRangeChangeTypes(); i++)
		{
			const InvisibleTypes eInvisible = ((InvisibleTypes)i);
			const int iChange = pClassInfo->getVisibilityIntensityRangeChangeType(i);
			m_aVisibilityIntensityRangeChangeTypes.push_back(std::make_pair(eInvisible, iChange));
		}
	}

	if (getNumVisibilityIntensitySameTileChangeTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumVisibilityIntensitySameTileChangeTypes(); i++)
		{
			const InvisibleTypes eInvisible = ((InvisibleTypes)i);
			const int iChange = pClassInfo->getVisibilityIntensitySameTileChangeType(i);
			m_aVisibilityIntensitySameTileChangeTypes.push_back(std::make_pair(eInvisible, iChange));
		}
	}

	if (getNumTerrainAttackChangeModifiers() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aTerrainAttackChangeModifiers, pClassInfo->m_aTerrainAttackChangeModifiers);
	}

	if (getNumTerrainDefenseChangeModifiers() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aTerrainDefenseChangeModifiers, pClassInfo->m_aTerrainDefenseChangeModifiers);
	}

	if (getNumTerrainWorkChangeModifiers() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aTerrainWorkChangeModifiers, pClassInfo->m_aTerrainWorkChangeModifiers);
	}

	if (getNumBuildWorkChangeModifiers() == 0)
	{
		const int iNum = pClassInfo->getNumBuildWorkChangeModifiers();
		m_aBuildWorkChangeModifiers.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aBuildWorkChangeModifiers[i].iModifier = pClassInfo->m_aBuildWorkChangeModifiers[i].iModifier;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aBuildWorkChangeModifiers[i].eBuild), (int*)&(pClassInfo->m_aBuildWorkChangeModifiers[i].eBuild));
		}
	}

	if (getNumFeatureAttackChangeModifiers() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aFeatureAttackChangeModifiers, pClassInfo->m_aFeatureAttackChangeModifiers);
	}

	if (getNumFeatureDefenseChangeModifiers() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aFeatureDefenseChangeModifiers, pClassInfo->m_aFeatureDefenseChangeModifiers);
	}

	if (getNumFeatureWorkChangeModifiers() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aFeatureWorkChangeModifiers, pClassInfo->m_aFeatureWorkChangeModifiers);
	}

	if (getNumUnitCombatChangeModifiers() == 0)
	{
		const int iNum = pClassInfo->getNumUnitCombatChangeModifiers();
		m_aUnitCombatChangeModifiers.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aUnitCombatChangeModifiers[i].iModifier = pClassInfo->m_aUnitCombatChangeModifiers[i].iModifier;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aUnitCombatChangeModifiers[i].eUnitCombat), (int*)&(pClassInfo->m_aUnitCombatChangeModifiers[i].eUnitCombat));
		}
	}

	if (getNumFlankingStrengthbyUnitCombatTypesChange() == 0)
	{
		const int iNum = pClassInfo->getNumFlankingStrengthbyUnitCombatTypesChange();
		m_aFlankingStrengthbyUnitCombatTypeChange.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aFlankingStrengthbyUnitCombatTypeChange[i].iModifier = pClassInfo->m_aFlankingStrengthbyUnitCombatTypeChange[i].iModifier;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aFlankingStrengthbyUnitCombatTypeChange[i].eUnitCombat), (int*)&(pClassInfo->m_aFlankingStrengthbyUnitCombatTypeChange[i].eUnitCombat));
		}
	}

	if (getNumTrapAvoidanceUnitCombatTypes() == 0)
	{
		const int iNum = pClassInfo->getNumTrapAvoidanceUnitCombatTypes();
		m_aTrapAvoidanceUnitCombatTypes.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aTrapAvoidanceUnitCombatTypes[i].iModifier = pClassInfo->m_aTrapAvoidanceUnitCombatTypes[i].iModifier;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aTrapAvoidanceUnitCombatTypes[i].eUnitCombat), (int*)&(pClassInfo->m_aTrapAvoidanceUnitCombatTypes[i].eUnitCombat));
		}
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
}

// Explicit (NOT delegated to CvInfoUtil) on purpose, byte-identical to the legacy hash:
// the hand-written outcome lists lead it, m_bZoneOfControl is read+merged but deliberately
// absent, and the hand-written arrays/pair-vectors/struct-vectors sit mid-order between
// declared fields. Keep this in the legacy order.
void CvUnitCombatInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	m_KillOutcomeList.getCheckSum(iSum);

	foreach_(const CvOutcomeMission* outcomeMission, m_aOutcomeMissions)
	{
		outcomeMission->getCheckSum(iSum);
	}

	// Textual References
	CheckSum(iSum, m_eReligion);
	CheckSum(iSum, m_eCulture);
	CheckSum(iSum, m_eEra);
	//Integers
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
	CheckSum(iSum, m_iRevoltProtection);
	CheckSum(iSum, m_iCollateralDamageProtection);
	CheckSum(iSum, m_iPillageChange);
	CheckSum(iSum, m_iUpgradeDiscount);
	CheckSum(iSum, m_iExperiencePercent);
	CheckSum(iSum, m_iKamikazePercent);
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
	CheckSum(iSum, m_iAttackCombatModifierChange);
	CheckSum(iSum, m_iDefenseCombatModifierChange);
	CheckSum(iSum, m_iVSBarbsChange);
	CheckSum(iSum, m_iUnnerveChange);
	CheckSum(iSum, m_iEncloseChange);
	CheckSum(iSum, m_iLungeChange);
	CheckSum(iSum, m_iDynamicDefenseChange);
	CheckSum(iSum, m_iStrengthChange);
	CheckSum(iSum, m_iEnduranceChange);
	CheckSum(iSum, m_iPoisonProbabilityModifierChange);
	CheckSum(iSum, m_iCaptureProbabilityModifierChange);
	CheckSum(iSum, m_iCaptureResistanceModifierChange);
	CheckSum(iSum, m_iBreakdownChanceChange);
	CheckSum(iSum, m_iBreakdownDamageChange);
	CheckSum(iSum, m_iTauntChange);
	CheckSum(iSum, m_iMaxHPChange);
	CheckSum(iSum, m_iStrengthModifier);
	CheckSum(iSum, m_iQualityBase);
	CheckSum(iSum, m_iGroupBase);
	CheckSum(iSum, m_iSizeBase);
	CheckSum(iSum, m_iDamageModifierChange);

	CheckSum(iSum, m_iUpkeepModifier);
	CheckSum(iSum, m_iExtraUpkeep100);

	CheckSum(iSum, m_iRBombardDamageBase);
	CheckSum(iSum, m_iRBombardDamageLimitBase);
	CheckSum(iSum, m_iRBombardDamageMaxUnitsBase);
	CheckSum(iSum, m_iDCMBombRangeBase);
	CheckSum(iSum, m_iDCMBombAccuracyBase);
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
	CheckSum(iSum, m_iInsidiousnessChange);
	CheckSum(iSum, m_iInvestigationChange);
	CheckSum(iSum, m_iStealthStrikesChange);
	CheckSum(iSum, m_iStealthCombatModifierChange);
	CheckSum(iSum, m_iStealthDefenseChange);
	CheckSum(iSum, m_iDefenseOnlyChange);
	CheckSum(iSum, m_iNoInvisibilityChange);
	CheckSum(iSum, m_iNoCaptureChange);
	CheckSum(iSum, m_iAnimalIgnoresBordersChange);
	CheckSum(iSum, m_iNoDefensiveBonusChange);
	CheckSum(iSum, m_iGatherHerdChange);
	CheckSum(iSum, m_iReligiousCombatModifierChange);
	//Boolean
	CheckSum(iSum, m_bDefensiveVictoryMove);
	CheckSum(iSum, m_bFreeDrop);
	CheckSum(iSum, m_bOffensiveVictoryMove);
	CheckSum(iSum, m_bOneUp);
	CheckSum(iSum, m_bPillageEspionage);
	CheckSum(iSum, m_bPillageMarauder);
	CheckSum(iSum, m_bPillageOnMove);
	CheckSum(iSum, m_bPillageOnVictory);
	CheckSum(iSum, m_bPillageResearch);
	CheckSum(iSum, m_bBlitz);
	CheckSum(iSum, m_bAmphib);
	CheckSum(iSum, m_bRiver);
	CheckSum(iSum, m_bEnemyRoute);
	CheckSum(iSum, m_bAlwaysHeal);
	CheckSum(iSum, m_bHillsDoubleMove);
	CheckSum(iSum, m_bImmuneToFirstStrikes);
	CheckSum(iSum, m_bStampedeChange);
	CheckSum(iSum, m_bRemoveStampede);
	CheckSum(iSum, m_bOnslaughtChange);
	CheckSum(iSum, m_bAttackOnlyCitiesAdd);
	CheckSum(iSum, m_bAttackOnlyCitiesSubtract);
	CheckSum(iSum, m_bIgnoreNoEntryLevelAdd);
	CheckSum(iSum, m_bIgnoreNoEntryLevelSubtract);
	CheckSum(iSum, m_bIgnoreZoneofControlAdd);
	CheckSum(iSum, m_bIgnoreZoneofControlSubtract);
	CheckSum(iSum, m_bFliesToMoveAdd);
	CheckSum(iSum, m_bFliesToMoveSubtract);

	CheckSum(iSum, m_bCanMovePeaks);
	CheckSum(iSum, m_bCanLeadThroughPeaks);
	CheckSum(iSum, m_bSpy);
	CheckSum(iSum, m_bCannotMergeSplit);
	CheckSum(iSum, m_bRBombardDirect);
	CheckSum(iSum, m_bRBombardForceAbility);
	CheckSum(iSum, m_bInvisible);
	CheckSum(iSum, m_bForMilitary);
	CheckSum(iSum, m_bForNavalMilitary);
	CheckSum(iSum, m_bHealsAs);
	CheckSum(iSum, m_bNoSelfHeal);

	// Arrays
	CheckSum(iSum, m_piDomainModifierPercent, NUM_DOMAIN_TYPES);

	// int vectors utilizing pairing without delayed resolution
	CheckSumC(iSum, m_aVisibilityIntensityChangeTypes);
	CheckSumC(iSum, m_aInvisibilityIntensityChangeTypes);
	CheckSumC(iSum, m_aVisibilityIntensityRangeChangeTypes);
	CheckSumC(iSum, m_aVisibilityIntensitySameTileChangeTypes);
	CheckSumC(iSum, m_aiTerrainDoubleMoveChangeTypes);
	CheckSumC(iSum, m_aiFeatureDoubleMoveChangeTypes);
	CheckSumC(iSum, m_aiOnGameOptions);
	CheckSumC(iSum, m_aiNotOnGameOptions);
	CheckSumC(iSum, m_aiGGptsforUnitTypes);
	CheckSumC(iSum, m_aiDefaultStatusTypes);
	CheckSumC(iSum, m_aiTrapImmunityUnitCombatTypes);
	CheckSumC(iSum, m_aiCategories);

	//int vectors utilizing struct with delayed resolution
	int iNumElements;
	iNumElements = m_aTerrainAttackChangeModifiers.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aTerrainAttackChangeModifiers[i].eTerrain);
		CheckSum(iSum, m_aTerrainAttackChangeModifiers[i].iModifier);
	}

	iNumElements = m_aTerrainDefenseChangeModifiers.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aTerrainDefenseChangeModifiers[i].eTerrain);
		CheckSum(iSum, m_aTerrainDefenseChangeModifiers[i].iModifier);
	}

	iNumElements = m_aTerrainWorkChangeModifiers.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aTerrainWorkChangeModifiers[i].eTerrain);
		CheckSum(iSum, m_aTerrainWorkChangeModifiers[i].iModifier);
	}

	iNumElements = m_aBuildWorkChangeModifiers.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aBuildWorkChangeModifiers[i].eBuild);
		CheckSum(iSum, m_aBuildWorkChangeModifiers[i].iModifier);
	}

	iNumElements = m_aFeatureAttackChangeModifiers.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aFeatureAttackChangeModifiers[i].eFeature);
		CheckSum(iSum, m_aFeatureAttackChangeModifiers[i].iModifier);
	}

	iNumElements = m_aFeatureDefenseChangeModifiers.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aFeatureDefenseChangeModifiers[i].eFeature);
		CheckSum(iSum, m_aFeatureDefenseChangeModifiers[i].iModifier);
	}

	iNumElements = m_aFeatureWorkChangeModifiers.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aFeatureWorkChangeModifiers[i].eFeature);
		CheckSum(iSum, m_aFeatureWorkChangeModifiers[i].iModifier);
	}

	iNumElements = m_aUnitCombatChangeModifiers.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aUnitCombatChangeModifiers[i].eUnitCombat);
		CheckSum(iSum, m_aUnitCombatChangeModifiers[i].iModifier);
	}

	iNumElements = m_aFlankingStrengthbyUnitCombatTypeChange.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aFlankingStrengthbyUnitCombatTypeChange[i].eUnitCombat);
		CheckSum(iSum, m_aFlankingStrengthbyUnitCombatTypeChange[i].iModifier);
	}

	iNumElements = m_aTrapAvoidanceUnitCombatTypes.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aTrapAvoidanceUnitCombatTypes[i].eUnitCombat);
		CheckSum(iSum, m_aTrapAvoidanceUnitCombatTypes[i].iModifier);
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

	m_PropertyManipulators.getCheckSum(iSum);

}

const CvOutcomeList* CvUnitCombatInfo::getKillOutcomeList() const
{
	return &m_KillOutcomeList;
}

int CvUnitCombatInfo::getNumActionOutcomes() const
{
	return m_aOutcomeMissions.size();
}

MissionTypes CvUnitCombatInfo::getActionOutcomeMission(int index) const
{
	return m_aOutcomeMissions[index]->getMission();
}

const CvOutcomeList* CvUnitCombatInfo::getActionOutcomeList(int index) const
{
	return m_aOutcomeMissions[index]->getOutcomeList();
}

const CvOutcomeList* CvUnitCombatInfo::getActionOutcomeListByMission(MissionTypes eMission) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const CvOutcomeMission * outcomeMission, m_aOutcomeMissions)
	{
		if (outcomeMission->getMission() == eMission)
		{
			return outcomeMission->getOutcomeList();
		}
	}
	return NULL;
}

const CvOutcomeMission* CvUnitCombatInfo::getOutcomeMission(int index) const
{
	return m_aOutcomeMissions[index];
}

const CvOutcomeMission* CvUnitCombatInfo::getOutcomeMissionByMission(MissionTypes eMission) const
{
	return algo::find_if(m_aOutcomeMissions, bind(CvOutcomeMission::getMission, _1) == eMission).get_value_or(NULL);
}