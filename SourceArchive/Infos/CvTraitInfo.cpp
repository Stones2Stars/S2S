#include "CvGameCoreDLL.h"
#include "CvTraitInfo.h"
#include "Defines/CvGlobals.h"
#include "CvInfoUtil.h"
#include "AI/CvGameAI.h"

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvTraitInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvTraitInfo::CvTraitInfo()
	// Only the fields NOT declared in getDataMembers are initialized here;
	// everything declared there is initialized by initDataMembers() below.
	: m_ppbFreePromotionUnitCombats(NULL)
	, m_ppaiSpecialistYieldChange(NULL)
	, m_ppaiSpecialistCommerceChange(NULL)
	, m_piFlavorValue(NULL)
	, m_ppaiImprovementYieldChange(NULL)
	, m_piBonusHappinessChangesFiltered(NULL)
	, m_ppaiSpecialistYieldChangeFiltered(NULL)
	, m_ppaiSpecialistCommerceChangeFiltered(NULL)
	, m_ppaiImprovementYieldChangeFiltered(NULL)
{
	CvInfoUtil(this).initDataMembers();
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvTraitInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvTraitInfo::~CvTraitInfo()
{
	PROFILE_EXTRA_FUNC();
	// The 1D yield/commerce arrays and the trait-prereq delayed resolutions are owned by their
	// CvInfoUtil wrappers and released by uninitDataMembers() below.
	SAFE_DELETE_ARRAY2(m_ppbFreePromotionUnitCombats, GC.getNumPromotionInfos());
	SAFE_DELETE_ARRAY2(m_ppaiSpecialistYieldChange, GC.getNumSpecialistInfos());
	SAFE_DELETE_ARRAY2(m_ppaiSpecialistCommerceChange, GC.getNumSpecialistInfos());
	SAFE_DELETE_ARRAY(m_piFlavorValue);
	SAFE_DELETE_ARRAY2(m_ppaiImprovementYieldChange, GC.getNumImprovementInfos());

//	 int vector utilizing struct with delayed resolution
	for (int i=0; i<(int)m_aDisallowedTraitTypes.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aDisallowedTraitTypes[i]));
	}

	for (int i=0; i<(int)m_aBuildingProductionModifiers.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aBuildingProductionModifiers[i]));
	}

	for (int i=0; i<(int)m_aUnitProductionModifiers.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aUnitProductionModifiers[i]));
	}

	for (int i=0; i<(int)m_aCivicOptionNoUpkeepTypes.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aCivicOptionNoUpkeepTypes[i]));
	}


	SAFE_DELETE_ARRAY2(m_ppaiSpecialistYieldChangeFiltered, GC.getNumSpecialistInfos());
	SAFE_DELETE_ARRAY2(m_ppaiSpecialistCommerceChangeFiltered, GC.getNumSpecialistInfos());
	SAFE_DELETE_ARRAY2(m_ppaiImprovementYieldChangeFiltered, GC.getNumImprovementInfos());

	CvInfoUtil(this).uninitDataMembers();
}

int CvTraitInfo::getHealth() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iHealth > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iHealth < 0)
		{
			return 0;
		}
	}
	return m_iHealth;
}

int CvTraitInfo::getHappiness() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iHappiness > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iHappiness < 0)
		{
			return 0;
		}
	}
	return m_iHappiness;
}

int CvTraitInfo::getMaxAnarchy() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMaxAnarchy > -1)
		{
			return -1;
		}
	}
	return m_iMaxAnarchy;
}

int CvTraitInfo::getUpkeepModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iUpkeepModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iUpkeepModifier > 0)
		{
			return 0;
		}
	}
	return m_iUpkeepModifier;
}

int CvTraitInfo::getLevelExperienceModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iLevelExperienceModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iLevelExperienceModifier > 0)
		{
			return 0;
		}
	}
	return m_iLevelExperienceModifier;
}

int CvTraitInfo::getGreatPeopleRateModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iGreatPeopleRateModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iGreatPeopleRateModifier < 0)
		{
			return 0;
		}
	}
	return m_iGreatPeopleRateModifier;
}

int CvTraitInfo::getGreatGeneralRateModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iGreatGeneralRateModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iGreatGeneralRateModifier < 0)
		{
			return 0;
		}
	}
	return m_iGreatGeneralRateModifier;
}

int CvTraitInfo::getDomesticGreatGeneralRateModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iDomesticGreatGeneralRateModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iDomesticGreatGeneralRateModifier < 0)
		{
			return 0;
		}
	}
	return m_iDomesticGreatGeneralRateModifier;
}

int CvTraitInfo::getMaxGlobalBuildingProductionModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMaxGlobalBuildingProductionModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iMaxGlobalBuildingProductionModifier < 0)
		{
			return 0;
		}
	}
	return m_iMaxGlobalBuildingProductionModifier;
}

int CvTraitInfo::getMaxTeamBuildingProductionModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMaxTeamBuildingProductionModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iMaxTeamBuildingProductionModifier < 0)
		{
			return 0;
		}
	}
	return m_iMaxTeamBuildingProductionModifier;
}

int CvTraitInfo::getMaxPlayerBuildingProductionModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMaxPlayerBuildingProductionModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iMaxPlayerBuildingProductionModifier < 0)
		{
			return 0;
		}
	}
	return m_iMaxPlayerBuildingProductionModifier;
}

/********************************************************************************/
/**		REVDCM									2/16/10				phungus420	*/
/**																				*/
/**		RevTrait Effects														*/
/********************************************************************************/

int CvTraitInfo::getRevIdxLocal() const
//Note: Positive is penalty
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iRevIdxLocal < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iRevIdxLocal > 0)
		{
			return 0;
		}
	}
	return m_iRevIdxLocal;
}

int CvTraitInfo::getRevIdxNational() const
{
//Note: Positive is Penalty
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iRevIdxNational < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iRevIdxNational > 0)
		{
			return 0;
		}
	}
	return m_iRevIdxNational;
}

int CvTraitInfo::getRevIdxDistanceModifier() const
{
//Note: Positive is penalty
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iRevIdxDistanceModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iRevIdxDistanceModifier > 0)
		{
			return 0;
		}
	}
	return m_iRevIdxDistanceModifier;
}

int CvTraitInfo::getRevIdxHolyCityGood() const
{
//Note: Positive is bonus
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iRevIdxHolyCityGood > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iRevIdxHolyCityGood < 0)
		{
			return 0;
		}
	}
	return m_iRevIdxHolyCityGood;
}

int CvTraitInfo::getRevIdxHolyCityBad() const
{
//Note: Positive is Penalty
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iRevIdxHolyCityBad < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iRevIdxHolyCityBad > 0)
		{
			return 0;
		}
	}
	return m_iRevIdxHolyCityBad;
}

float CvTraitInfo::getRevIdxNationalityMod() const
{
//Note: Positive is Penalty
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_fRevIdxNationalityMod < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_fRevIdxNationalityMod > 0)
		{
			return 0;
		}
	}
	return m_fRevIdxNationalityMod;
}

float CvTraitInfo::getRevIdxBadReligionMod() const
{
//Note: Positive is Penalty
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_fRevIdxBadReligionMod < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_fRevIdxBadReligionMod > 0)
		{
			return 0;
		}
	}
	return m_fRevIdxBadReligionMod;
}

float CvTraitInfo::getRevIdxGoodReligionMod() const
{
//Note: Positive is Bonus
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_fRevIdxGoodReligionMod > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_fRevIdxGoodReligionMod < 0)
		{
			return 0;
		}
	}
	return m_fRevIdxGoodReligionMod;
}

bool CvTraitInfo::isNonStateReligionCommerce() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_bNonStateReligionCommerce)
		{
				return false;
		}
	}
	return m_bNonStateReligionCommerce;
}

bool CvTraitInfo::isUpgradeAnywhere() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_bUpgradeAnywhere)
		{
				return false;
		}
	}
	return m_bUpgradeAnywhere;
}
/********************************************************************************/
/**		REVDCM									END								*/
/********************************************************************************/

const char* CvTraitInfo::getShortDescription() const
{
	return m_szShortDescription;
}

void CvTraitInfo::setShortDescription(const char* szVal)
{
	m_szShortDescription = szVal;
}

// Arrays

int CvTraitInfo::getExtraYieldThreshold(int i) const
{
	if (!m_paiExtraYieldThreshold)
		return -1;

	if (m_paiExtraYieldThresholdFiltered && GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_paiExtraYieldThreshold[i] > -1)
		{
			m_paiExtraYieldThresholdFiltered[i] = -1;
		}
		else
		{
			m_paiExtraYieldThresholdFiltered[i] = m_paiExtraYieldThreshold[i];
		}
		return m_paiExtraYieldThresholdFiltered[i];
	}
	else
	{
		return m_paiExtraYieldThreshold[i];
	}
}

int CvTraitInfo::getTradeYieldModifier(int i) const
{
	if (m_paiTradeYieldModifier)
	{
		if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) && m_paiTradeYieldModifierFiltered)
		{
			if (isNegativeTrait() && m_paiTradeYieldModifier[i] > 0)
			{
				m_paiTradeYieldModifierFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_paiTradeYieldModifier[i] < 0)
			{
				m_paiTradeYieldModifierFiltered[i] = 0;
			}
			else
			{
				m_paiTradeYieldModifierFiltered[i] = m_paiTradeYieldModifier[i];
			}
			return m_paiTradeYieldModifierFiltered ? m_paiTradeYieldModifierFiltered[i] : 0;
		}
	}
	return m_paiTradeYieldModifier ? m_paiTradeYieldModifier[i] : 0;
}

int CvTraitInfo::getCommerceChange(int i) const
{
	if (m_paiCommerceChange)
	{
		if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) && m_paiCommerceChangeFiltered)
		{
			if (isNegativeTrait() && m_paiCommerceChange[i] > 0)
			{
				m_paiCommerceChangeFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_paiCommerceChange[i] < 0)
			{
				m_paiCommerceChangeFiltered[i] = 0;
			}
			else
			{
				m_paiCommerceChangeFiltered[i] = m_paiCommerceChange[i];
			}
			return m_paiCommerceChangeFiltered ? m_paiCommerceChangeFiltered[i] : 0;
		}
	}
	return m_paiCommerceChange ? m_paiCommerceChange[i] : 0;
}

int CvTraitInfo::getCommerceModifier(int i) const
{
	if (m_paiCommerceModifier)
	{
		if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) && m_paiCommerceModifierFiltered)
		{
			if (isNegativeTrait() && m_paiCommerceModifier[i] > 0)
			{
				m_paiCommerceModifierFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_paiCommerceModifier[i] < 0)
			{
				m_paiCommerceModifierFiltered[i] = 0;
			}
			else
			{
				m_paiCommerceModifierFiltered[i] = m_paiCommerceModifier[i];
			}
			return m_paiCommerceModifierFiltered ? m_paiCommerceModifierFiltered[i] : 0;
		}
	}
	return m_paiCommerceModifier ? m_paiCommerceModifier[i] : 0;
}

bool CvTraitInfo::isFreePromotionUnitCombats(int i, int j) const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) && isImpurePromotions())
	{
		return false;
	}
	return (m_ppbFreePromotionUnitCombats && m_ppbFreePromotionUnitCombats[i]) ? m_ppbFreePromotionUnitCombats[i][j] : false;
}

//TB Traits Mods begin

PromotionLineTypes CvTraitInfo::getPromotionLine() const
{
	return m_ePromotionLine;
}

int CvTraitInfo::getGreatPeopleUnitType() const
{
	return m_iGreatPeopleUnitType;
}

SpecialistTypes CvTraitInfo::getEraAdvanceFreeSpecialistType() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_eEraAdvanceFreeSpecialistType != NO_SPECIALIST)
		{
				return NO_SPECIALIST;
		}
	}
	return m_eEraAdvanceFreeSpecialistType;
}

int CvTraitInfo::getGoldenAgeonBirthofGreatPeopleType() const
{
	if (isNegativeTrait() && GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		return NO_UNIT;
	}
	return m_iGoldenAgeonBirthofGreatPeopleType;
}

int CvTraitInfo::getWarWearinessAccumulationModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iWarWearinessAccumulationModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iWarWearinessAccumulationModifier > 0)
		{
			return 0;
		}
	}
	return m_iWarWearinessAccumulationModifier;
}

int CvTraitInfo::getCivicAnarchyTimeModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iCivicAnarchyTimeModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iCivicAnarchyTimeModifier > 0)
		{
			return 0;
		}
	}
	return m_iCivicAnarchyTimeModifier;
}

int CvTraitInfo::getReligiousAnarchyTimeModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iReligiousAnarchyTimeModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iReligiousAnarchyTimeModifier > 0)
		{
			return 0;
		}
	}
	return m_iReligiousAnarchyTimeModifier;
}

int CvTraitInfo::getImprovementUpgradeRateModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iImprovementUpgradeRateModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iImprovementUpgradeRateModifier < 0)
		{
			return 0;
		}
	}
	return m_iImprovementUpgradeRateModifier;
}

int CvTraitInfo::getWorkerSpeedModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iWorkerSpeedModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iWorkerSpeedModifier < 0)
		{
			return 0;
		}
	}
	return m_iWorkerSpeedModifier;
}

int CvTraitInfo::getMaxConscript() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMaxConscript > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iMaxConscript < 0)
		{
			return 0;
		}
	}
	return m_iMaxConscript;
}

int CvTraitInfo::getDistanceMaintenanceModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iDistanceMaintenanceModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iDistanceMaintenanceModifier > 0)
		{
			return 0;
		}
	}
	return m_iDistanceMaintenanceModifier;
}

int CvTraitInfo::getNumCitiesMaintenanceModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iNumCitiesMaintenanceModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iNumCitiesMaintenanceModifier > 0)
		{
			return 0;
		}
	}
	return m_iNumCitiesMaintenanceModifier;
}

int CvTraitInfo::getCorporationMaintenanceModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iCorporationMaintenanceModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iCorporationMaintenanceModifier > 0)
		{
			return 0;
		}
	}
	return m_iCorporationMaintenanceModifier;
}

int CvTraitInfo::getStateReligionGreatPeopleRateModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iStateReligionGreatPeopleRateModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iStateReligionGreatPeopleRateModifier < 0)
		{
			return 0;
		}
	}
	return m_iStateReligionGreatPeopleRateModifier;
}

int CvTraitInfo::getFreeExperience() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iFreeExperience > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iFreeExperience < 0)
		{
			return 0;
		}
	}
	return m_iFreeExperience;
}

int CvTraitInfo::getFreeUnitUpkeepCivilian() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait())
		{
			if (m_iFreeUnitUpkeepCivilian > 0)
			{
				return 0;
			}
		}
		else if (m_iFreeUnitUpkeepCivilian < 0)
		{
			return 0;
		}
	}
	return m_iFreeUnitUpkeepCivilian;
}

int CvTraitInfo::getFreeUnitUpkeepMilitary() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait())
		{
			if (m_iFreeUnitUpkeepMilitary > 0)
			{
				return 0;
			}
		}
		else if (m_iFreeUnitUpkeepMilitary < 0)
		{
			return 0;
		}
	}
	return m_iFreeUnitUpkeepMilitary;
}

int CvTraitInfo::getFreeUnitUpkeepCivilianPopPercent() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait())
		{
			if (m_iFreeUnitUpkeepCivilianPopPercent > 0)
			{
				return 0;
			}
		}
		else if (m_iFreeUnitUpkeepCivilianPopPercent < 0)
		{
			return 0;
		}
	}
	return m_iFreeUnitUpkeepCivilianPopPercent;
}

int CvTraitInfo::getFreeUnitUpkeepMilitaryPopPercent() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait())
		{
			if (m_iFreeUnitUpkeepMilitaryPopPercent > 0)
			{
				return 0;
			}
		}
		else if (m_iFreeUnitUpkeepMilitaryPopPercent < 0)
		{
			return 0;
		}
	}
	return m_iFreeUnitUpkeepMilitaryPopPercent;
}

int CvTraitInfo::getCivilianUnitUpkeepMod() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait())
		{
			if (m_iCivilianUnitUpkeepMod < 0)
			{
				return 0;
			}
		}
		else if (m_iCivilianUnitUpkeepMod > 0)
		{
			return 0;
		}
	}
	return m_iCivilianUnitUpkeepMod;
}

int CvTraitInfo::getMilitaryUnitUpkeepMod() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait())
		{
			if (m_iMilitaryUnitUpkeepMod < 0)
			{
				return 0;
			}
		}
		else if (m_iMilitaryUnitUpkeepMod > 0)
		{
			return 0;
		}
	}
	return m_iMilitaryUnitUpkeepMod;
}

int CvTraitInfo::getHappyPerMilitaryUnit() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iHappyPerMilitaryUnit > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iHappyPerMilitaryUnit < 0)
		{
			return 0;
		}
	}
	return m_iHappyPerMilitaryUnit;
}

int CvTraitInfo::getLargestCityHappiness() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iLargestCityHappiness > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iLargestCityHappiness < 0)
		{
			return 0;
		}
	}
	return m_iLargestCityHappiness;
}

int CvTraitInfo::getFreeSpecialist() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait())
		{
			if (m_iFreeSpecialist > 0)
			{
				return 0;
			}
		}
		else if (m_iFreeSpecialist < 0)
		{
			return 0;
		}
	}
	return m_iFreeSpecialist;
}

int CvTraitInfo::getTradeRoutes() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait())
		{
			if (m_iTradeRoutes > 0)
			{
				return 0;
			}
		}
		else if (m_iTradeRoutes < 0)
		{
			return 0;
		}
	}
	return m_iTradeRoutes;
}

int CvTraitInfo::getStateReligionHappiness() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait())
		{
			if (m_iStateReligionHappiness > 0)
			{
				return 0;
			}
		}
		else if (m_iStateReligionHappiness < 0)
		{
			return 0;
		}
	}
	return m_iStateReligionHappiness;
}

int CvTraitInfo::getNonStateReligionHappiness() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iNonStateReligionHappiness > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iNonStateReligionHappiness < 0)
		{
			return 0;
		}
	}
	return m_iNonStateReligionHappiness;
}

int CvTraitInfo::getStateReligionUnitProductionModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iStateReligionUnitProductionModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iStateReligionUnitProductionModifier < 0)
		{
			return 0;
		}
	}
	return m_iStateReligionUnitProductionModifier;
}

int CvTraitInfo::getStateReligionBuildingProductionModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iStateReligionBuildingProductionModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iStateReligionBuildingProductionModifier < 0)
		{
			return 0;
		}
	}
	return m_iStateReligionBuildingProductionModifier;
}

int CvTraitInfo::getStateReligionFreeExperience() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iStateReligionFreeExperience > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iStateReligionFreeExperience < 0)
		{
			return 0;
		}
	}
	return m_iStateReligionFreeExperience;
}

int CvTraitInfo::getExpInBorderModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iExpInBorderModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iExpInBorderModifier < 0)
		{
			return 0;
		}
	}
	return m_iExpInBorderModifier;
}

int CvTraitInfo::getCityDefenseBonus() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iCityDefenseBonus > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iCityDefenseBonus < 0)
		{
			return 0;
		}
	}
	return m_iCityDefenseBonus;
}

int CvTraitInfo::getMilitaryProductionModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMilitaryProductionModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iMilitaryProductionModifier < 0)
		{
			return 0;
		}
	}
	return m_iMilitaryProductionModifier;
}

int CvTraitInfo::getAttitudeModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iAttitudeModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iAttitudeModifier < 0)
		{
			return 0;
		}
	}
	return m_iAttitudeModifier;
}

int CvTraitInfo::getLinePriority() const
{
	return m_iLinePriority;
}

int CvTraitInfo::getEspionageDefense() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iEspionageDefense > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iEspionageDefense < 0)
		{
			return 0;
		}
	}
	return m_iEspionageDefense;
}

int CvTraitInfo::getMinAnarchy() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMinAnarchy < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iMinAnarchy > 0)
		{
			return 0;
		}
	}
	return m_iMinAnarchy;
}

int CvTraitInfo::getMaxTradeRoutesChange() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMaxTradeRoutesChange > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iMaxTradeRoutesChange < 0)
		{
			return 0;
		}
	}
	return m_iMaxTradeRoutesChange;
}

int CvTraitInfo::getGoldenAgeDurationModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iGoldenAgeDurationModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iGoldenAgeDurationModifier < 0)
		{
			return 0;
		}
	}
	return m_iGoldenAgeDurationModifier;
}

int CvTraitInfo::getGreatPeopleRateChange() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iGreatPeopleRateChange > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iGreatPeopleRateChange < 0)
		{
			return 0;
		}
	}
	return m_iGreatPeopleRateChange;
}

int CvTraitInfo::getHurryAngerModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iHurryAngerModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iHurryAngerModifier > 0)
		{
			return 0;
		}
	}
	return m_iHurryAngerModifier;
}

int CvTraitInfo::getHurryCostModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iHurryCostModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iHurryCostModifier > 0)
		{
			return 0;
		}
	}
	return m_iHurryCostModifier;
}

int CvTraitInfo::getEnemyWarWearinessModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iEnemyWarWearinessModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iEnemyWarWearinessModifier < 0)
		{
			return 0;
		}
	}
	return m_iEnemyWarWearinessModifier;
}

int CvTraitInfo::getForeignTradeRouteModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iForeignTradeRouteModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iForeignTradeRouteModifier < 0)
		{
			return 0;
		}
	}
	return m_iForeignTradeRouteModifier;
}

int CvTraitInfo::getBombardDefense() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iBombardDefense > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iBombardDefense < 0)
		{
			return 0;
		}
	}
	return m_iBombardDefense;
}

int CvTraitInfo::getUnitUpgradePriceModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iUnitUpgradePriceModifier < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iUnitUpgradePriceModifier > 0)
		{
			return 0;
		}
	}
	return m_iUnitUpgradePriceModifier;
}

int CvTraitInfo::getCoastalTradeRoutes() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iCoastalTradeRoutes > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iCoastalTradeRoutes < 0)
		{
			return 0;
		}
	}
	return m_iCoastalTradeRoutes;
}

int CvTraitInfo::getGlobalPopulationgrowthratepercentage() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iGlobalPopulationgrowthratepercentage < 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iGlobalPopulationgrowthratepercentage > 0)
		{
			return 0;
		}
	}
	return m_iGlobalPopulationgrowthratepercentage;
}

int CvTraitInfo::getCityStartCulture(bool bForLoad) const
{
	if (!bForLoad && GC.getGame().isOption(GAMEOPTION_CULTURE_1_CITY_TILE_FOUNDING))
	{
		return 0;
	}
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iCityStartCulture > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iCityStartCulture < 0)
		{
			return 0;
		}
	}
	return m_iCityStartCulture;
}

int CvTraitInfo::getGlobalAirUnitCapacity() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iGlobalAirUnitCapacity > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iGlobalAirUnitCapacity < 0)
		{
			return 0;
		}
	}
	return m_iGlobalAirUnitCapacity;
}

int CvTraitInfo::getCapitalXPModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iCapitalXPModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iCapitalXPModifier < 0)
		{
			return 0;
		}
	}
	return m_iCapitalXPModifier;
}

int CvTraitInfo::getHolyCityofStateReligionXPModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iHolyCityofStateReligionXPModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iHolyCityofStateReligionXPModifier < 0)
		{
			return 0;
		}
	}
	return m_iHolyCityofStateReligionXPModifier;
}

int CvTraitInfo::getHolyCityofNonStateReligionXPModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iHolyCityofNonStateReligionXPModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iHolyCityofNonStateReligionXPModifier < 0)
		{
			return 0;
		}
	}
	return m_iHolyCityofNonStateReligionXPModifier;
}

int CvTraitInfo::getBonusPopulationinNewCities() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iBonusPopulationinNewCities > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iBonusPopulationinNewCities < 0)
		{
			return 0;
		}
	}
	return m_iBonusPopulationinNewCities;
}

int CvTraitInfo::getMissileRange() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMissileRange > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iMissileRange < 0)
		{
			return 0;
		}
	}
	return m_iMissileRange;
}

int CvTraitInfo::getFlightOperationRange() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iFlightOperationRange > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iFlightOperationRange < 0)
		{
			return 0;
		}
	}
	return m_iFlightOperationRange;
}

int CvTraitInfo::getNavalCargoSpace() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iNavalCargoSpace > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iNavalCargoSpace < 0)
		{
			return 0;
		}
	}
	return m_iNavalCargoSpace;
}

int CvTraitInfo::getMissileCargoSpace() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iMissileCargoSpace > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iMissileCargoSpace < 0)
		{
			return 0;
		}
	}
	return m_iMissileCargoSpace;
}

int CvTraitInfo::getNationalCaptureProbabilityModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iNationalCaptureProbabilityModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iNationalCaptureProbabilityModifier < 0)
		{
			return 0;
		}
	}
	return m_iNationalCaptureProbabilityModifier;
}

int CvTraitInfo::getNationalCaptureResistanceModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iNationalCaptureResistanceModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iNationalCaptureResistanceModifier < 0)
		{
			return 0;
		}
	}
	return m_iNationalCaptureResistanceModifier;
}

int CvTraitInfo::getStateReligionSpreadProbabilityModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iStateReligionSpreadProbabilityModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iStateReligionSpreadProbabilityModifier < 0)
		{
			return 0;
		}
	}
	return m_iStateReligionSpreadProbabilityModifier;
}

int CvTraitInfo::getNonStateReligionSpreadProbabilityModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iNonStateReligionSpreadProbabilityModifier > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iNonStateReligionSpreadProbabilityModifier < 0)
		{
			return 0;
		}
	}
	return m_iNonStateReligionSpreadProbabilityModifier;
}

int CvTraitInfo::getFreedomFighterChange() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_iFreedomFighterChange > 0)
		{
			return 0;
		}
		else if (!isNegativeTrait() && m_iFreedomFighterChange < 0)
		{
			return 0;
		}
	}
	return m_iFreedomFighterChange;
}

bool CvTraitInfo::isMilitaryFoodProduction() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (isNegativeTrait() && m_bMilitaryFoodProduction)
		{
				return false;
		}
	}
	return m_bMilitaryFoodProduction;
}

bool CvTraitInfo::isImpurePropertyManipulators() const
{
	return m_bImpurePropertyManipulators;
}

bool CvTraitInfo::isImpurePromotions() const
{
	return m_bImpurePromotions;
}

bool CvTraitInfo::isCivilizationTrait() const
{
	return m_bCivilizationTrait;
}

bool CvTraitInfo::isAllowsInquisitions() const
{
	return m_bAllowsInquisitions && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || !isNegativeTrait());
}

bool CvTraitInfo::isCoastalAIInfluence() const
{
	return m_bCoastalAIInfluence;
}

bool CvTraitInfo::isBarbarianSelectionOnly() const
{
	return m_bBarbarianSelectionOnly;
}

bool CvTraitInfo::isCitiesStartwithStateReligion() const
{
	return m_bCitiesStartwithStateReligion && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || !isNegativeTrait());
}

bool CvTraitInfo::isDraftsOnCityCapture() const
{
	return m_bDraftsOnCityCapture && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || !isNegativeTrait());
}

bool CvTraitInfo::isFreeSpecialistperWorldWonder() const
{
	return m_bFreeSpecialistperWorldWonder && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || !isNegativeTrait());
}

bool CvTraitInfo::isFreeSpecialistperNationalWonder() const
{
	return m_bFreeSpecialistperNationalWonder && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || !isNegativeTrait());
}

bool CvTraitInfo::isFreeSpecialistperTeamProject() const
{
	return m_bFreeSpecialistperTeamProject && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || !isNegativeTrait());
}

bool CvTraitInfo::isExtraGoody() const
{
	return m_bExtraGoody && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || !isNegativeTrait());
}

bool CvTraitInfo::isAllReligionsActive() const
{
	return m_bAllReligionsActive && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || !isNegativeTrait());
}

bool CvTraitInfo::isBansNonStateReligions() const
{
	return m_bBansNonStateReligions && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || isNegativeTrait());
}

bool CvTraitInfo::isFreedomFighter() const
{
	return m_bFreedomFighter && (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) || !isNegativeTrait());
}

// bool vector without delayed resolution
int CvTraitInfo::getNotOnGameOption(int i) const
{
	return m_aiNotOnGameOptions[i];
}

int CvTraitInfo::getNumNotOnGameOptions() const
{
	return (int)m_aiNotOnGameOptions.size();
}

bool CvTraitInfo::isNotOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiNotOnGameOptions, i);
}

int CvTraitInfo::getOnGameOption(int i) const
{
	return m_aiOnGameOptions[i];
}

int CvTraitInfo::getNumOnGameOptions() const
{
	return (int)m_aiOnGameOptions.size();
}

bool CvTraitInfo::isOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiOnGameOptions, i);
}

int CvTraitInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}

int CvTraitInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}

bool CvTraitInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}

bool CvTraitInfo::isValidTrait(bool bGameStart) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < GC.getNumGameOptionInfos(); iI++)
	{
		if (GC.getGame().isOption((GameOptionTypes)iI))
		{
			if (isNotOnGameOption(iI)) return false;
		}
		else if (isOnGameOption(iI)) return false;
	}

	if (bGameStart && isBarbarianSelectionOnly())
	{
		return true;
	}

	if (isNegativeTrait())
	{
		if (GC.getGame().isOption(GAMEOPTION_LEADER_NO_NEGATIVE_TRAITS)
		|| bGameStart && GC.getGame().isOption(GAMEOPTION_LEADER_START_NO_POSITIVE_TRAITS) && GC.getGame().isOption(GAMEOPTION_LEADER_DEVELOPING))
		{
			return false;
		}
	}
	else if (bGameStart && GC.getGame().isOption(GAMEOPTION_LEADER_START_NO_POSITIVE_TRAITS))
	{
		return false;
	}

	if (isCivilizationTrait()) return true;

	if (GC.getGame().isOption(GAMEOPTION_LEADER_DEVELOPING))
	{
		if (getLinePriority() == 0)
		{
			return false;
		}
	}
	else if (getLinePriority() != 0)
	{
		return false;
	}
	return true;
}

//Arrays
int CvTraitInfo::getSpecialistYieldChange(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_ppaiSpecialistYieldChange && m_ppaiSpecialistYieldChange[i] && m_ppaiSpecialistYieldChangeFiltered && m_ppaiSpecialistYieldChangeFiltered[i])
		{
			if (isNegativeTrait() && m_ppaiSpecialistYieldChange[i][j] > 0)
			{
				m_ppaiSpecialistYieldChangeFiltered[i][j] = 0;
			}
			else if (!isNegativeTrait() && m_ppaiSpecialistYieldChange[i][j] < 0)
			{
				m_ppaiSpecialistYieldChangeFiltered[i][j] = 0;
			}
			else
			{
				m_ppaiSpecialistYieldChangeFiltered[i][j] = (m_ppaiSpecialistYieldChange && m_ppaiSpecialistYieldChange[i]) ? m_ppaiSpecialistYieldChange[i][j] : 0;
			}
			return (m_ppaiSpecialistYieldChangeFiltered && m_ppaiSpecialistYieldChangeFiltered[i]) ? m_ppaiSpecialistYieldChangeFiltered[i][j] : 0;
		}
	}
	return (m_ppaiSpecialistYieldChange && m_ppaiSpecialistYieldChange[i]) ? m_ppaiSpecialistYieldChange[i][j] : 0;
}

int* CvTraitInfo::getSpecialistYieldChangeArray(int i) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if (m_ppaiSpecialistYieldChange && m_ppaiSpecialistYieldChange[i] && m_ppaiSpecialistYieldChangeFiltered)
			{
				if (isNegativeTrait() && m_ppaiSpecialistYieldChange[i][j] > 0)
				{
					m_ppaiSpecialistYieldChangeFiltered[i][j] = 0;
				}
				else if (!isNegativeTrait() && m_ppaiSpecialistYieldChange[i][j] < 0)
				{
					m_ppaiSpecialistYieldChangeFiltered[i][j] = 0;
				}
				else
				{
					m_ppaiSpecialistYieldChangeFiltered[i][j] = (m_ppaiSpecialistYieldChange && m_ppaiSpecialistYieldChange[i]) ? m_ppaiSpecialistYieldChange[i][j] : 0;
				}
			}
		}
		return (m_ppaiSpecialistYieldChangeFiltered) ? m_ppaiSpecialistYieldChangeFiltered[i] : 0;
	}
	return (m_ppaiSpecialistYieldChange) ? m_ppaiSpecialistYieldChange[i] : 0;
}

int CvTraitInfo::getYieldModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_piYieldModifier && m_piYieldModifierFiltered)
		{
			if (isNegativeTrait() && m_piYieldModifier[i] > 0)
			{
				m_piYieldModifierFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_piYieldModifier[i] < 0)
			{
				m_piYieldModifierFiltered[i] = 0;
			}
			else
			{
				m_piYieldModifierFiltered[i] = m_piYieldModifier ? m_piYieldModifier[i] : 0;
			}
			return m_piYieldModifierFiltered ? m_piYieldModifierFiltered[i] : 0;
		}
	}
	return m_piYieldModifier ? m_piYieldModifier[i] : 0;
}

int* CvTraitInfo::getYieldModifierArray() const
{
	PROFILE_EXTRA_FUNC();
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			if (m_piYieldModifier && m_piYieldModifier[i])
			{
				if (isNegativeTrait() && m_piYieldModifier[i] > 0)
				{
					m_piYieldModifierFiltered[i] = 0;
				}
				else if (!isNegativeTrait() && m_piYieldModifier[i] < 0)
				{
					m_piYieldModifierFiltered[i] = 0;
				}
				else
				{
					m_piYieldModifierFiltered[i] = m_piYieldModifier ? m_piYieldModifier[i] : 0;
				}
			}
		}
		return m_piYieldModifierFiltered;
	}
	return m_piYieldModifier;
}

int CvTraitInfo::getCapitalYieldModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_piCapitalYieldModifier && m_piCapitalYieldModifierFiltered)
		{
			if (isNegativeTrait() && m_piCapitalYieldModifier[i] > 0)
			{
				m_piCapitalYieldModifierFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_piCapitalYieldModifier[i] < 0)
			{
				m_piCapitalYieldModifierFiltered[i] = 0;
			}
			else
			{
				m_piCapitalYieldModifierFiltered[i] = m_piCapitalYieldModifier ? m_piCapitalYieldModifier[i] : 0;
			}
			return m_piCapitalYieldModifierFiltered ? m_piCapitalYieldModifierFiltered[i] : 0;
		}
	}
	return m_piCapitalYieldModifier ? m_piCapitalYieldModifier[i] : 0;
}

int* CvTraitInfo::getCapitalYieldModifierArray() const
{
	PROFILE_EXTRA_FUNC();
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			if (m_piCapitalYieldModifier && m_piCapitalYieldModifier[i])
			{
				if (isNegativeTrait() && m_piCapitalYieldModifier[i] > 0)
				{
					m_piCapitalYieldModifierFiltered[i] = 0;
				}
				else if (!isNegativeTrait() && m_piCapitalYieldModifier[i] < 0)
				{
					m_piCapitalYieldModifierFiltered[i] = 0;
				}
				else
				{
					m_piCapitalYieldModifierFiltered[i] = m_piCapitalYieldModifier ? m_piCapitalYieldModifier[i] : 0;
				}
			}
		}
		return m_piCapitalYieldModifierFiltered;
	}
	return m_piCapitalYieldModifier;
}

int CvTraitInfo::getCapitalCommerceModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_piCapitalCommerceModifier && m_piCapitalCommerceModifierFiltered)
		{
			if (isNegativeTrait() && m_piCapitalCommerceModifier[i] > 0)
			{
				m_piCapitalCommerceModifierFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_piCapitalCommerceModifier[i] < 0)
			{
				m_piCapitalCommerceModifierFiltered[i] = 0;
			}
			else
			{
				m_piCapitalCommerceModifierFiltered[i] = m_piCapitalCommerceModifier ? m_piCapitalCommerceModifier[i] : 0;
			}
			return m_piCapitalCommerceModifierFiltered ? m_piCapitalCommerceModifierFiltered[i] : 0;
		}
	}
	return m_piCapitalCommerceModifier ? m_piCapitalCommerceModifier[i] : 0;
}

int* CvTraitInfo::getCapitalCommerceModifierArray() const
{
	PROFILE_EXTRA_FUNC();
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
		{
			if (m_piCapitalCommerceModifier && m_piCapitalCommerceModifier[i])
			{
				if (isNegativeTrait() && m_piCapitalCommerceModifier[i] > 0)
				{
					m_piCapitalCommerceModifierFiltered[i] = 0;
				}
				else if (!isNegativeTrait() && m_piCapitalCommerceModifier[i] < 0)
				{
					m_piCapitalCommerceModifierFiltered[i] = 0;
				}
				else
				{
					m_piCapitalCommerceModifierFiltered[i] = m_piCapitalCommerceModifier ? m_piCapitalCommerceModifier[i] : 0;
				}
			}
		}
		return m_piCapitalCommerceModifierFiltered;
	}
	return m_piCapitalCommerceModifier;
}

int CvTraitInfo::getSpecialistExtraCommerce(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_piSpecialistExtraCommerce && m_piSpecialistExtraCommerceFiltered)
		{
			if (isNegativeTrait() && m_piSpecialistExtraCommerce[i] > 0)
			{
				m_piSpecialistExtraCommerceFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_piSpecialistExtraCommerce[i] < 0)
			{
				m_piSpecialistExtraCommerceFiltered[i] = 0;
			}
			else
			{
				m_piSpecialistExtraCommerceFiltered[i] = m_piSpecialistExtraCommerce ? m_piSpecialistExtraCommerce[i] : 0;
			}
			return m_piSpecialistExtraCommerceFiltered ? m_piSpecialistExtraCommerceFiltered[i] : 0;
		}
	}
	return m_piSpecialistExtraCommerce ? m_piSpecialistExtraCommerce[i] : 0;
}

int* CvTraitInfo::getSpecialistExtraCommerceArray() const
{
	PROFILE_EXTRA_FUNC();
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
		{
			if (m_piSpecialistExtraCommerce && m_piSpecialistExtraCommerce[i])
			{
				if (isNegativeTrait() && m_piSpecialistExtraCommerce[i] > 0)
				{
					m_piSpecialistExtraCommerceFiltered[i] = 0;
				}
				else if (!isNegativeTrait() && m_piSpecialistExtraCommerce[i] < 0)
				{
					m_piSpecialistExtraCommerceFiltered[i] = 0;
				}
				else
				{
					m_piSpecialistExtraCommerceFiltered[i] = m_piSpecialistExtraCommerce ? m_piSpecialistExtraCommerce[i] : 0;
				}
			}
		}
		return m_piSpecialistExtraCommerceFiltered;
	}
	return m_piSpecialistExtraCommerce;
}

int CvTraitInfo::getSpecialistExtraYield(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_piSpecialistExtraYield && m_piSpecialistExtraYieldFiltered)
		{
			if (isNegativeTrait() && m_piSpecialistExtraYield[i] > 0)
			{
				m_piSpecialistExtraYieldFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_piSpecialistExtraYield[i] < 0)
			{
				m_piSpecialistExtraYieldFiltered[i] = 0;
			}
			else
			{
				m_piSpecialistExtraYieldFiltered[i] = m_piSpecialistExtraYield ? m_piSpecialistExtraYield[i] : 0;
			}
			return m_piSpecialistExtraYieldFiltered ? m_piSpecialistExtraYieldFiltered[i] : 0;
		}
	}
	return m_piSpecialistExtraYield ? m_piSpecialistExtraYield[i] : 0;
}

int* CvTraitInfo::getSpecialistExtraYieldArray() const
{
	PROFILE_EXTRA_FUNC();
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			if (m_piSpecialistExtraYield && m_piSpecialistExtraYield[i])
			{
				if (isNegativeTrait() && m_piSpecialistExtraYield[i] > 0)
				{
					m_piSpecialistExtraYieldFiltered[i] = 0;
				}
				else if (!isNegativeTrait() && m_piSpecialistExtraYield[i] < 0)
				{
					m_piSpecialistExtraYieldFiltered[i] = 0;
				}
				else
				{
					m_piSpecialistExtraYieldFiltered[i] = m_piSpecialistExtraYield ? m_piSpecialistExtraYield[i] : 0;
				}
			}
		}
		return m_piSpecialistExtraYieldFiltered;
	}
	return m_piSpecialistExtraYield;
}

int CvTraitInfo::getFlavorValue(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFlavorTypes(), i);

	return m_piFlavorValue ? m_piFlavorValue[i] : 0;
}

int CvTraitInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_piYieldChange && m_piYieldChangeFiltered)
		{
			if (isNegativeTrait() && m_piYieldChange[i] > 0)
			{
				m_piYieldChangeFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_piYieldChange[i] < 0)
			{
				m_piYieldChangeFiltered[i] = 0;
			}
			else
			{
				m_piYieldChangeFiltered[i] = m_piYieldChange ? m_piYieldChange[i] : 0;
			}
			return m_piYieldChangeFiltered ? m_piYieldChangeFiltered[i] : 0;
		}
	}
	return m_piYieldChange ? m_piYieldChange[i] : 0;
}

int CvTraitInfo::getSpecialistCommerceChange(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, j);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_ppaiSpecialistCommerceChange && m_ppaiSpecialistCommerceChange[i] && m_ppaiSpecialistCommerceChangeFiltered && m_ppaiSpecialistCommerceChangeFiltered[i])
		{
			if (isNegativeTrait() && m_ppaiSpecialistCommerceChange[i][j] > 0)
			{
				m_ppaiSpecialistCommerceChangeFiltered[i][j] = 0;
			}
			else if (!isNegativeTrait() && m_ppaiSpecialistCommerceChange[i][j] < 0)
			{
				m_ppaiSpecialistCommerceChangeFiltered[i][j] = 0;
			}
			else
			{
				m_ppaiSpecialistCommerceChangeFiltered[i][j] = (m_ppaiSpecialistCommerceChange && m_ppaiSpecialistCommerceChange[i]) ? m_ppaiSpecialistCommerceChange[i][j] : 0;
			}
			return (m_ppaiSpecialistCommerceChangeFiltered && m_ppaiSpecialistCommerceChangeFiltered[i]) ? m_ppaiSpecialistCommerceChangeFiltered[i][j] : 0;
		}
	}
	return (m_ppaiSpecialistCommerceChange && m_ppaiSpecialistCommerceChange[i]) ? m_ppaiSpecialistCommerceChange[i][j] : 0;
}

int* CvTraitInfo::getSpecialistCommerceChangeArray(int i) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int j = 0; j < NUM_COMMERCE_TYPES; j++)
		{
			if (m_ppaiSpecialistCommerceChange && m_ppaiSpecialistCommerceChange[i])
			{
				if (isNegativeTrait() && m_ppaiSpecialistCommerceChange[i][j] > 0)
				{
					m_ppaiSpecialistCommerceChangeFiltered[i][j] = 0;
				}
				else if (!isNegativeTrait() && m_ppaiSpecialistCommerceChange[i][j] < 0)
				{
					m_ppaiSpecialistCommerceChangeFiltered[i][j] = 0;
				}
				else
				{
					m_ppaiSpecialistCommerceChangeFiltered[i][j] = (m_ppaiSpecialistCommerceChange && m_ppaiSpecialistCommerceChange[i]) ? m_ppaiSpecialistCommerceChange[i][j] : 0;
				}
			}
		}
		return m_ppaiSpecialistCommerceChangeFiltered[i];
	}
	return m_ppaiSpecialistCommerceChange[i];
}

int CvTraitInfo::getLessYieldThreshold(int i) const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_paiLessYieldThreshold && m_paiLessYieldThresholdFiltered)
		{
			// AIAndy: There is something wrong with this logic, both branches are identical
			// TB: Thanks for pointing that out there!
			if (!isNegativeTrait() && m_paiLessYieldThreshold[i] > 0)
			{
				m_paiLessYieldThresholdFiltered[i] = -1;
			}
			else
			{
				m_paiLessYieldThresholdFiltered[i] = m_paiLessYieldThreshold ? m_paiLessYieldThreshold[i] : 0;
			}
			return m_paiLessYieldThresholdFiltered ? m_paiLessYieldThresholdFiltered[i] : 0;
		}
	}
	return m_paiLessYieldThreshold ? m_paiLessYieldThreshold[i] : 0;
}

int CvTraitInfo::getSeaPlotYieldChanges(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_piSeaPlotYieldChanges && m_piSeaPlotYieldChangesFiltered)
		{
			if (isNegativeTrait() && m_piSeaPlotYieldChanges[i] > 0)
			{
				m_piSeaPlotYieldChangesFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_piSeaPlotYieldChanges[i] < 0)
			{
				m_piSeaPlotYieldChangesFiltered[i] = 0;
			}
			else
			{
				m_piSeaPlotYieldChangesFiltered[i] = m_piSeaPlotYieldChanges ? m_piSeaPlotYieldChanges[i] : 0;
			}
			return m_piSeaPlotYieldChangesFiltered ? m_piSeaPlotYieldChangesFiltered[i] : 0;
		}
	}
	return m_piSeaPlotYieldChanges ? m_piSeaPlotYieldChanges[i] : 0;
}

int* CvTraitInfo::getSeaPlotYieldChangesArray() const
{
	PROFILE_EXTRA_FUNC();
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			if (m_piSeaPlotYieldChanges && m_piSeaPlotYieldChanges[i])
			{
				if (isNegativeTrait() && m_piSeaPlotYieldChanges[i] > 0)
				{
					m_piSeaPlotYieldChangesFiltered[i] = 0;
				}
				else if (!isNegativeTrait() && m_piSeaPlotYieldChanges[i] < 0)
				{
					m_piSeaPlotYieldChangesFiltered[i] = 0;
				}
				else
				{
					m_piSeaPlotYieldChangesFiltered[i] = m_piSeaPlotYieldChanges ? m_piSeaPlotYieldChanges[i] : 0;
				}
			}
		}
		return m_piSeaPlotYieldChangesFiltered;
	}
	return m_piSeaPlotYieldChanges;
}

int CvTraitInfo::getImprovementYieldChange(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumImprovementInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_ppaiImprovementYieldChange && m_ppaiImprovementYieldChange[i] && m_ppaiImprovementYieldChangeFiltered && m_ppaiImprovementYieldChangeFiltered[i])
		{
			if (isNegativeTrait() && m_ppaiImprovementYieldChange[i][j] > 0)
			{
				m_ppaiImprovementYieldChangeFiltered[i][j] = 0;
			}
			else if (!isNegativeTrait() && m_ppaiImprovementYieldChange[i][j] < 0)
			{
				m_ppaiImprovementYieldChangeFiltered[i][j] = 0;
			}
			else
			{
				m_ppaiImprovementYieldChangeFiltered[i][j] = (m_ppaiImprovementYieldChange && m_ppaiImprovementYieldChange[i]) ? m_ppaiImprovementYieldChange[i][j] : 0;
			}
			return (m_ppaiImprovementYieldChangeFiltered && m_ppaiImprovementYieldChangeFiltered[i]) ? m_ppaiImprovementYieldChangeFiltered[i][j] : 0;
		}
	}
	return (m_ppaiImprovementYieldChange && m_ppaiImprovementYieldChange[i]) ? m_ppaiImprovementYieldChange[i][j] : 0;
}

int* CvTraitInfo::getImprovementYieldChangeArray(int i) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumImprovementInfos(), i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if (m_ppaiImprovementYieldChange && m_ppaiImprovementYieldChange[i] && m_ppaiImprovementYieldChangeFiltered)
			{
				if (isNegativeTrait() && m_ppaiImprovementYieldChange[i][j] > 0)
				{
					m_ppaiImprovementYieldChangeFiltered[i][j] = 0;
				}
				else if (!isNegativeTrait() && m_ppaiImprovementYieldChange[i][j] < 0)
				{
					m_ppaiImprovementYieldChangeFiltered[i][j] = 0;
				}
				else
				{
					m_ppaiImprovementYieldChangeFiltered[i][j] = (m_ppaiImprovementYieldChange && m_ppaiImprovementYieldChange[i]) ? m_ppaiImprovementYieldChange[i][j] : 0;
				}
			}
		}
		return (m_ppaiImprovementYieldChangeFiltered) ? m_ppaiImprovementYieldChangeFiltered[i] : 0;
	}
	return (m_ppaiImprovementYieldChange) ? m_ppaiImprovementYieldChange[i] : 0;
}

int CvTraitInfo::getGoldenAgeYieldChanges(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_piGoldenAgeYieldChanges && m_piGoldenAgeYieldChangesFiltered)
		{
			if (isNegativeTrait() && m_piGoldenAgeYieldChanges[i] > 0)
			{
				m_piGoldenAgeYieldChangesFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_piGoldenAgeYieldChanges[i] < 0)
			{
				m_piGoldenAgeYieldChangesFiltered[i] = 0;
			}
			else
			{
				m_piGoldenAgeYieldChangesFiltered[i] = m_piGoldenAgeYieldChanges ? m_piGoldenAgeYieldChanges[i] : 0;
			}
			return m_piGoldenAgeYieldChangesFiltered ? m_piGoldenAgeYieldChangesFiltered[i] : 0;
		}
	}
	return m_piGoldenAgeYieldChanges ? m_piGoldenAgeYieldChanges[i] : 0;
}

int* CvTraitInfo::getGoldenAgeYieldChangesArray() const
{
	PROFILE_EXTRA_FUNC();
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			if (m_piGoldenAgeYieldChanges && m_piGoldenAgeYieldChanges[i])
			{
				if (isNegativeTrait() && m_piGoldenAgeYieldChanges[i] > 0)
				{
					m_piGoldenAgeYieldChangesFiltered[i] = 0;
				}
				else if (!isNegativeTrait() && m_piGoldenAgeYieldChanges[i] < 0)
				{
					m_piGoldenAgeYieldChangesFiltered[i] = 0;
				}
				else
				{
					m_piGoldenAgeYieldChangesFiltered[i] = m_piGoldenAgeYieldChanges ? m_piGoldenAgeYieldChanges[i] : 0;
				}
			}
		}
		return m_piGoldenAgeYieldChangesFiltered;
	}
	return m_piGoldenAgeYieldChanges;
}

int CvTraitInfo::getGoldenAgeCommerceChanges(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		if (m_piGoldenAgeCommerceChanges && m_piGoldenAgeCommerceChangesFiltered)
		{
			if (isNegativeTrait() && m_piGoldenAgeCommerceChanges[i] > 0)
			{
				m_piGoldenAgeCommerceChangesFiltered[i] = 0;
			}
			else if (!isNegativeTrait() && m_piGoldenAgeCommerceChanges[i] < 0)
			{
				m_piGoldenAgeCommerceChangesFiltered[i] = 0;
			}
			else
			{
				m_piGoldenAgeCommerceChangesFiltered[i] = m_piGoldenAgeCommerceChanges ? m_piGoldenAgeCommerceChanges[i] : 0;
			}
			return m_piGoldenAgeCommerceChangesFiltered ? m_piGoldenAgeCommerceChangesFiltered[i] : 0;
		}
	}
	return m_piGoldenAgeCommerceChanges ? m_piGoldenAgeCommerceChanges[i] : 0;
}

int* CvTraitInfo::getGoldenAgeCommerceChangesArray() const
{
	PROFILE_EXTRA_FUNC();
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
		{
			if (m_piGoldenAgeCommerceChanges && m_piGoldenAgeCommerceChanges[i])
			{
				if (isNegativeTrait() && m_piGoldenAgeCommerceChanges[i] > 0)
				{
					m_piGoldenAgeCommerceChangesFiltered[i] = 0;
				}
				else if (!isNegativeTrait() && m_piGoldenAgeCommerceChanges[i] < 0)
				{
					m_piGoldenAgeCommerceChangesFiltered[i] = 0;
				}
				else
				{
					m_piGoldenAgeCommerceChangesFiltered[i] = m_piGoldenAgeCommerceChanges ? m_piGoldenAgeCommerceChanges[i] : 0;
				}
			}
		}
		return m_piGoldenAgeCommerceChangesFiltered;
	}
	return m_piGoldenAgeCommerceChanges;
}

namespace PureTraits
{
	namespace
	{
		template <typename T1, typename T2>
		bool anyValue(const std::pair<T1, T2>&)
		{
			return true;
		}
		template <typename T1, typename T2>
		bool isPositiveValue(const std::pair<T1, T2>& pair)
		{
			return pair.second > 0;
		}
		template <typename T1, typename T2>
		bool isNegativeValue(const std::pair<T1, T2>& pair)
		{
			return pair.second < 0;
		}
	}

	template <typename T1, typename T2>
	bst::function<bool(const std::pair<T1, T2>&)> getPredicate(bool bNegativeTrait)
	{
		if (!GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
			return bind(anyValue<T1, T2>, _1);

		if (bNegativeTrait)
			return bind(isNegativeValue<T1, T2>, _1);
		else
			return bind(isPositiveValue<T1, T2>, _1);
	}
};

const IDValueMap<ImprovementTypes, int>::filtered CvTraitInfo::getImprovementUpgradeModifiers() const
{
	return filter(m_aImprovementUpgradeModifierTypes, PureTraits::getPredicate<ImprovementTypes, int>(m_bNegativeTrait));
}

const IDValueMap<BuildTypes, int>::filtered CvTraitInfo::getBuildWorkerSpeedModifiers() const
{
	return filter(m_aBuildWorkerSpeedModifierTypes, PureTraits::getPredicate<BuildTypes, int>(m_bNegativeTrait));
}

int CvTraitInfo::getNumDisallowedTraitTypes() const
{
	return (int)m_aDisallowedTraitTypes.size();
}

DisallowedTraitType CvTraitInfo::isDisallowedTraitType(int iTrait) const
{
	FASSERT_BOUNDS(0, getNumDisallowedTraitTypes(), iTrait);

	return m_aDisallowedTraitTypes[iTrait];
}

const IDValueMap<DomainTypes, int>::filtered CvTraitInfo::getDomainFreeExperience() const
{
	return filter(m_aDomainFreeExperiences, PureTraits::getPredicate<DomainTypes, int>(m_bNegativeTrait));
}

const IDValueMap<DomainTypes, int>::filtered CvTraitInfo::getDomainProductionModifiers() const
{
	return filter(m_aDomainProductionModifiers, PureTraits::getPredicate<DomainTypes, int>(m_bNegativeTrait));
}

int CvTraitInfo::getNumBuildingProductionModifiers() const
{
	return (int)m_aBuildingProductionModifiers.size();
}

BuildingModifier CvTraitInfo::getBuildingProductionModifier(int iBuilding) const
{
	FASSERT_BOUNDS(0, getNumBuildingProductionModifiers(), iBuilding);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		BuildingModifier kMod = m_aBuildingProductionModifiers[iBuilding];
		if (isNegativeTrait() && kMod.iModifier > 0)
		{
			kMod.iModifier = 0;
		}
		else if (!isNegativeTrait() && kMod.iModifier < 0)
		{
			kMod.iModifier = 0;
		}
		return kMod;
	}
	return m_aBuildingProductionModifiers[iBuilding];
}

const IDValueMap<TechTypes, int>::filtered CvTraitInfo::getTechResearchModifiers() const
{
	return filter(m_aTechResearchModifiers, PureTraits::getPredicate<TechTypes, int>(m_bNegativeTrait));
}

const IDValueMap<SpecialBuildingTypes, int>::filtered CvTraitInfo::getSpecialBuildingProductionModifiers() const
{
	return filter(m_aSpecialBuildingProductionModifiers, PureTraits::getPredicate<SpecialBuildingTypes, int>(m_bNegativeTrait));
}

const IDValueMap<BuildingTypes, int>::filtered CvTraitInfo::getBuildingHappinessModifiersFiltered() const
{
	return filter(m_aBuildingHappinessModifiers, PureTraits::getPredicate<BuildingTypes, int>(m_bNegativeTrait));
}

int CvTraitInfo::getNumUnitProductionModifiers() const
{
	return (int)m_aUnitProductionModifiers.size();
}

UnitModifier CvTraitInfo::getUnitProductionModifier(int iUnit) const
{
	FASSERT_BOUNDS(0, getNumUnitProductionModifiers(), iUnit);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		UnitModifier kMod = m_aUnitProductionModifiers[iUnit];
		if (isNegativeTrait() && kMod.iModifier > 0)
		{
			kMod.iModifier = 0;
		}
		else if (!isNegativeTrait() && kMod.iModifier < 0)
		{
			kMod.iModifier = 0;
		}
		return kMod;
	}
	return m_aUnitProductionModifiers[iUnit];
}

int CvTraitInfo::getNumSpecialUnitProductionModifiers() const
{
	return (int)m_aSpecialUnitProductionModifiers.size();
}

SpecialUnitModifier CvTraitInfo::getSpecialUnitProductionModifier(int iSpecialUnit) const
{
	FASSERT_BOUNDS(0, getNumSpecialUnitProductionModifiers(), iSpecialUnit);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		SpecialUnitModifier kMod = m_aSpecialUnitProductionModifiers[iSpecialUnit];
		if (isNegativeTrait() && kMod.iModifier > 0)
		{
			kMod.iModifier = 0;
		}
		else if (!isNegativeTrait() && kMod.iModifier < 0)
		{
			kMod.iModifier = 0;
		}
		return kMod;
	}
	return m_aSpecialUnitProductionModifiers[iSpecialUnit];
}

int CvTraitInfo::getNumCivicOptionNoUpkeepTypes() const
{
	return (int)m_aCivicOptionNoUpkeepTypes.size();
}

CivicOptionTypeBool CvTraitInfo::isCivicOptionNoUpkeepType(int iCivicOption) const
{
	FASSERT_BOUNDS(0, getNumCivicOptionNoUpkeepTypes(), iCivicOption);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		CivicOptionTypeBool kMod = m_aCivicOptionNoUpkeepTypes[iCivicOption];
		if (isNegativeTrait() && kMod.bBool)
		{
			kMod.bBool = 0;
		}
		else if (!isNegativeTrait() && !kMod.bBool)
		{
			kMod.bBool = 0;
		}
		return kMod;
	}
	return m_aCivicOptionNoUpkeepTypes[iCivicOption];
}

//Team Project (8)
int CvTraitInfo::getNumUnitCombatFreeExperiences() const
{
	return (int)m_aUnitCombatFreeExperiences.size();
}

UnitCombatModifier CvTraitInfo::getUnitCombatFreeExperience(int iUnitCombat) const
{
	FASSERT_BOUNDS(0, getNumUnitCombatFreeExperiences(), iUnitCombat);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		UnitCombatModifier kMod = m_aUnitCombatFreeExperiences[iUnitCombat];
		if (isNegativeTrait() && kMod.iModifier > 0)
		{
			kMod.iModifier = 0;
		}
		else if (!isNegativeTrait() && kMod.iModifier < 0)
		{
			kMod.iModifier = 0;
		}
		return kMod;
	}
	return m_aUnitCombatFreeExperiences[iUnitCombat];
}

int CvTraitInfo::getNumUnitCombatProductionModifiers() const
{
	return (int)m_aUnitCombatProductionModifiers.size();
}

UnitCombatModifier CvTraitInfo::getUnitCombatProductionModifier(int iUnitCombat) const
{
	FASSERT_BOUNDS(0, getNumUnitCombatProductionModifiers(), iUnitCombat);

	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS))
	{
		UnitCombatModifier kMod = m_aUnitCombatProductionModifiers[iUnitCombat];
		if (isNegativeTrait() && kMod.iModifier > 0)
		{
			kMod.iModifier = 0;
		}
		else if (!isNegativeTrait() && kMod.iModifier < 0)
		{
			kMod.iModifier = 0;
		}
		return kMod;
	}
	return m_aUnitCombatProductionModifiers[iUnitCombat];
}

const IDValueMap<BonusTypes, int>::filtered CvTraitInfo::getBonusHappinessChanges() const
{
	return filter(m_aBonusHappinessChanges, PureTraits::getPredicate<BonusTypes, int>(m_bNegativeTrait));
}

void CvTraitInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_aSpecialBuildingProductionModifiers, L"SpecialBuildingProductionModifierTypes")
		.add(m_aBuildWorkerSpeedModifierTypes, L"BuildWorkerSpeedModifierTypes")
		.add(m_aBuildingHappinessModifiers, L"BuildingHappinessModifierTypes")
		.add(m_aTechResearchModifiers, L"TechResearchModifiers")
		.add(m_aBonusHappinessChanges, L"BonusHappinessChanges")
		.add(m_aImprovementUpgradeModifierTypes, L"ImprovementUpgradeModifierTypes")
		.add(m_aDomainFreeExperiences, L"DomainFreeExperiences")
		.add(m_aDomainProductionModifiers, L"DomainProductionModifiers")

		// Integer scalars
		.add(m_iHealth, L"iHealth")
		.add(m_iHappiness, L"iHappiness")
		.add(m_iMaxAnarchy, L"iMaxAnarchy", -1)
		.add(m_iUpkeepModifier, L"iUpkeepModifier")
		.add(m_iLevelExperienceModifier, L"iLevelExperienceModifier")
		.add(m_iGreatPeopleRateModifier, L"iGreatPeopleRateModifier")
		.add(m_iGreatGeneralRateModifier, L"iGreatGeneralRateModifier")
		.add(m_iDomesticGreatGeneralRateModifier, L"iDomesticGreatGeneralRateModifier")
		.add(m_iMaxGlobalBuildingProductionModifier, L"iMaxGlobalBuildingProductionModifier")
		.add(m_iMaxTeamBuildingProductionModifier, L"iMaxTeamBuildingProductionModifier")
		.add(m_iMaxPlayerBuildingProductionModifier, L"iMaxPlayerBuildingProductionModifier")
		.add(m_iRevIdxLocal, L"iRevIdxLocal")
		.add(m_iRevIdxNational, L"iRevIdxNational")
		.add(m_iRevIdxDistanceModifier, L"iRevIdxDistanceModifier")
		.add(m_iRevIdxHolyCityGood, L"iRevIdxHolyCityGood")
		.add(m_iRevIdxHolyCityBad, L"iRevIdxHolyCityBad")
		.add(m_iWarWearinessAccumulationModifier, L"iWarWearinessAccumulationModifier")
		.add(m_iCivicAnarchyTimeModifier, L"iCivicAnarchyTimeModifier")
		.add(m_iReligiousAnarchyTimeModifier, L"iReligiousAnarchyTimeModifier")
		.add(m_iImprovementUpgradeRateModifier, L"iImprovementUpgradeRateModifier")
		.add(m_iWorkerSpeedModifier, L"iWorkerSpeedModifier")
		.add(m_iMaxConscript, L"iMaxConscript")
		.add(m_iDistanceMaintenanceModifier, L"iDistanceMaintenanceModifier")
		.add(m_iNumCitiesMaintenanceModifier, L"iNumCitiesMaintenanceModifier")
		.add(m_iCorporationMaintenanceModifier, L"iCorporationMaintenanceModifier")
		.add(m_iStateReligionGreatPeopleRateModifier, L"iStateReligionGreatPeopleRateModifier")
		.add(m_iFreeExperience, L"iFreeExperience")
		.add(m_iFreeUnitUpkeepCivilian, L"iFreeUnitUpkeepCivilian")
		.add(m_iFreeUnitUpkeepMilitary, L"iFreeUnitUpkeepMilitary")
		.add(m_iFreeUnitUpkeepCivilianPopPercent, L"iFreeUnitUpkeepCivilianPopPercent")
		.add(m_iFreeUnitUpkeepMilitaryPopPercent, L"iFreeUnitUpkeepMilitaryPopPercent")
		.add(m_iCivilianUnitUpkeepMod, L"iCivilianUnitUpkeepMod")
		.add(m_iMilitaryUnitUpkeepMod, L"iMilitaryUnitUpkeepMod")
		.add(m_iHappyPerMilitaryUnit, L"iHappyPerMilitaryUnit")
		.add(m_iLargestCityHappiness, L"iLargestCityHappiness")
		.add(m_iFreeSpecialist, L"iFreeSpecialist")
		.add(m_iTradeRoutes, L"iTradeRoutes")
		.add(m_iStateReligionHappiness, L"iStateReligionHappiness")
		.add(m_iNonStateReligionHappiness, L"iNonStateReligionHappiness")
		.add(m_iStateReligionUnitProductionModifier, L"iStateReligionUnitProductionModifier")
		.add(m_iStateReligionBuildingProductionModifier, L"iStateReligionBuildingProductionModifier")
		.add(m_iStateReligionFreeExperience, L"iStateReligionFreeExperience")
		.add(m_iExpInBorderModifier, L"iExpInBorderModifier")
		.add(m_iCityDefenseBonus, L"iCityDefenseBonus")
		.add(m_iMilitaryProductionModifier, L"iMilitaryProductionModifier")
		.add(m_iAttitudeModifier, L"iAttitudeModifier")
		.add(m_iLinePriority, L"iLinePriority")
		.add(m_iEspionageDefense, L"iEspionageDefense")
		.add(m_iMinAnarchy, L"iMinAnarchy")
		.add(m_iMaxTradeRoutesChange, L"iMaxTradeRoutesChange")
		.add(m_iGoldenAgeDurationModifier, L"iGoldenAgeDurationModifier")
		.add(m_iGreatPeopleRateChange, L"iGreatPeopleRateChange")
		.add(m_iHurryAngerModifier, L"iHurryAngerModifier")
		.add(m_iHurryCostModifier, L"iHurryCostModifier")
		.add(m_iEnemyWarWearinessModifier, L"iEnemyWarWearinessModifier")
		.add(m_iForeignTradeRouteModifier, L"iForeignTradeRouteModifier")
		.add(m_iBombardDefense, L"iBombardDefense")
		.add(m_iUnitUpgradePriceModifier, L"iUnitUpgradePriceModifier")
		.add(m_iCoastalTradeRoutes, L"iCoastalTradeRoutes")
		.add(m_iGlobalPopulationgrowthratepercentage, L"iGlobalPopulationgrowthratepercentage")
		.add(m_iCityStartCulture, L"iCityStartCulture")
		.add(m_iGlobalAirUnitCapacity, L"iGlobalAirUnitCapacity")
		.add(m_iCapitalXPModifier, L"iCapitalXPModifier")
		.add(m_iHolyCityofStateReligionXPModifier, L"iHolyCityofStateReligionXPModifier")
		.add(m_iHolyCityofNonStateReligionXPModifier, L"iHolyCityofNonStateReligionXPModifier")
		.add(m_iBonusPopulationinNewCities, L"iBonusPopulationinNewCities")
		.add(m_iMissileRange, L"iMissileRange")
		.add(m_iFlightOperationRange, L"iFlightOperationRange")
		.add(m_iNavalCargoSpace, L"iNavalCargoSpace")
		.add(m_iMissileCargoSpace, L"iMissileCargoSpace")
		.add(m_iNationalCaptureProbabilityModifier, L"iNationalCaptureProbabilityModifier")
		.add(m_iNationalCaptureResistanceModifier, L"iNationalCaptureResistanceModifier")
		.add(m_iStateReligionSpreadProbabilityModifier, L"iStateReligionSpreadProbabilityModifier")
		.add(m_iNonStateReligionSpreadProbabilityModifier, L"iNonStateReligionSpreadProbabilityModifier")
		.add(m_iFreedomFighterChange, L"iFreedomFighterChange")

		// Float scalars
		.add(m_fRevIdxNationalityMod, L"fRevIdxNationalityMod")
		.add(m_fRevIdxBadReligionMod, L"fRevIdxBadReligionMod")
		.add(m_fRevIdxGoodReligionMod, L"fRevIdxGoodReligionMod")

		// Booleans
		.add(m_bNonStateReligionCommerce, L"bNonStateReligionCommerce")
		.add(m_bUpgradeAnywhere, L"bUpgradeAnywhere")
		.add(m_bMilitaryFoodProduction, L"bMilitaryFoodProduction")
		.add(m_bNegativeTrait, L"bNegativeTrait")
		.add(m_bImpurePropertyManipulators, L"bImpurePropertyManipulators")
		.add(m_bImpurePromotions, L"bImpurePromotions")
		.add(m_bCivilizationTrait, L"bCivilizationTrait")
		.add(m_bAllowsInquisitions, L"bAllowsInquisitions")
		.add(m_bCoastalAIInfluence, L"bCoastalAIInfluence")
		.add(m_bBarbarianSelectionOnly, L"bBarbarianSelectionOnly")
		.add(m_bCitiesStartwithStateReligion, L"bCitiesStartwithStateReligion")
		.add(m_bDraftsOnCityCapture, L"bDraftsOnCityCapture")
		.add(m_bFreeSpecialistperWorldWonder, L"bFreeSpecialistperWorldWonder")
		.add(m_bFreeSpecialistperNationalWonder, L"bFreeSpecialistperNationalWonder")
		.add(m_bFreeSpecialistperTeamProject, L"bFreeSpecialistperTeamProject")
		.add(m_bExtraGoody, L"bExtraGoody")
		.add(m_bAllReligionsActive, L"bAllReligionsActive")
		.add(m_bBansNonStateReligions, L"bBansNonStateReligions")
		.add(m_bFreedomFighter, L"bFreedomFighter")

		// Enum FKs. Tech/PromotionLine/Specialist load before Trait, so these resolve immediately
		// (same as the legacy GetInfoClass reads); the trait self-FKs auto-pick delayed resolution
		// (same as the legacy GetOptionalTypeEnumWithDelayedResolution reads).
		.addEnum(m_ePrereqTech, L"PrereqTech")
		.addEnum(m_ePromotionLine, L"PromotionLine")
		.addEnum(m_eEraAdvanceFreeSpecialistType, L"EraAdvanceFreeSpecialistType")
		.addEnum(m_iPrereqTrait, L"TraitPrereq")
		.addEnum(m_iPrereqOrTrait1, L"TraitPrereqOr1")
		.addEnum(m_iPrereqOrTrait2, L"TraitPrereqOr2")

		// Legacy int members holding a UnitTypes index (immediate resolution, like the legacy reads)
		.addEnumAsInt(m_iGreatPeopleUnitType, L"GreatPeopleUnitType")
		.addEnumAsInt(m_iGoldenAgeonBirthofGreatPeopleType, L"GoldenAgeonBirthofGreatPersonType")

		// Strings (not checksummed)
		.add(m_szShortDescription, L"ShortDescription")

		// Membership lists (merge = unique+sort)
		.add(m_aiOnGameOptions, L"OnGameOptions")
		.add(m_aiNotOnGameOptions, L"NotOnGameOptions")
		.add(m_aiCategories, L"Categories")

		// Yield/commerce arrays (wrapper owns the heap array — dtor relies on uninitDataMembers).
		// The *Filtered twins deliberately read the same tags as their primaries: they start as a
		// copy of the raw values and are filtered lazily at runtime by the PURE_TRAITS getters.
		.addYields(m_paiExtraYieldThreshold, L"ExtraYieldThresholds")
		.addYields(m_paiExtraYieldThresholdFiltered, L"ExtraYieldThresholds")
		.addYields(m_paiTradeYieldModifier, L"TradeYieldModifiers")
		.addYields(m_paiTradeYieldModifierFiltered, L"TradeYieldModifiers")
		.addCommerce(m_paiCommerceChange, L"CommerceChanges")
		.addCommerce(m_paiCommerceChangeFiltered, L"CommerceChanges")
		.addCommerce(m_paiCommerceModifier, L"CommerceModifiers")
		.addCommerce(m_paiCommerceModifierFiltered, L"CommerceModifiers")
		.addYields(m_piYieldModifier, L"YieldModifiers")
		.addYields(m_piYieldModifierFiltered, L"YieldModifiers")
		.addYields(m_piCapitalYieldModifier, L"CapitalYieldModifiers")
		.addYields(m_piCapitalYieldModifierFiltered, L"CapitalYieldModifiers")
		.addCommerce(m_piCapitalCommerceModifier, L"CapitalCommerceModifiers")
		.addCommerce(m_piCapitalCommerceModifierFiltered, L"CapitalCommerceModifiers")
		.addCommerce(m_piSpecialistExtraCommerce, L"SpecialistExtraCommerces")
		.addCommerce(m_piSpecialistExtraCommerceFiltered, L"SpecialistExtraCommerces")
		.addYields(m_piSpecialistExtraYield, L"SpecialistExtraYields")
		.addYields(m_piSpecialistExtraYieldFiltered, L"SpecialistExtraYields")
		.addYields(m_piYieldChange, L"YieldChanges")
		.addYields(m_piYieldChangeFiltered, L"YieldChanges")
		.addYields(m_paiLessYieldThreshold, L"LessYieldThresholds")
		.addYields(m_paiLessYieldThresholdFiltered, L"LessYieldThresholds")
		.addYields(m_piSeaPlotYieldChanges, L"SeaPlotYieldChanges")
		.addYields(m_piSeaPlotYieldChangesFiltered, L"SeaPlotYieldChanges")
		.addYields(m_piGoldenAgeYieldChanges, L"GoldenAgeYieldChanges")
		.addYields(m_piGoldenAgeYieldChangesFiltered, L"GoldenAgeYieldChanges")
		.addCommerce(m_piGoldenAgeCommerceChanges, L"GoldenAgeCommerceChanges")
		.addCommerce(m_piGoldenAgeCommerceChangesFiltered, L"GoldenAgeCommerceChanges")

		// Self-contained sub-object
		.add(m_PropertyManipulators)
	;

	// Still hand-written in read()/copyNonDefaults()/getCheckSum() (see #298):
	// - 2D int**/bool** arrays (m_ppbFreePromotionUnitCombats, m_ppaiSpecialistYieldChange,
	//   m_ppaiSpecialistCommerceChange, m_ppaiImprovementYieldChange + Filtered twins) — the
	//   documented m_ppi wrapper blocker.
	// - m_piFlavorValue — SetVariableListTagPair dynamic array (no wrapper).
	// - m_aDisallowedTraitTypes, m_aBuildingProductionModifiers, m_aUnitProductionModifiers,
	//   m_aCivicOptionNoUpkeepTypes — struct-vectors with delayed-resolution FKs (addStruct
	//   resolves nested FKs immediately, so it cannot express these).
	// - m_aSpecialUnitProductionModifiers, m_aUnitCombatFreeExperiences,
	//   m_aUnitCombatProductionModifiers — immediate FKs, but the shared CvStructs.h structs
	//   do not declare getDataMembers, which addStruct requires.
	// - m_PropertyManipulatorsNull, m_piBonusHappinessChangesFiltered — runtime-only, never read.
}

bool CvTraitInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	// Scalars, strings, enum FKs, membership lists, yield/commerce arrays and the property
	// manipulators are declared in getDataMembers and read by CvInfoUtil(this).readXml below.

	FAssertMsg((GC.getNumPromotionInfos() > 0) && (GC.getNumUnitCombatInfos()) > 0,"either the number of promotion infos is zero or less or the number of unit combat types is zero or less");
	if (pXML->TryMoveToXmlFirstChild(L"FreePromotionUnitCombatTypes"))
	{
		int iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				CvXMLLoadUtility::InitPointerList(&m_ppbFreePromotionUnitCombats, GC.getNumPromotionInfos());
				for (int j=0;j<iNumSibs;j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"PromotionType");
					int iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						// delete the array since it will be reallocated
						SAFE_DELETE_ARRAY(m_ppbFreePromotionUnitCombats[iIndex]);
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"UnitCombatTypes"))
						{
							// get the total number of children the current xml node has
							int iNumChildren = pXML->GetXmlChildrenNumber();

							if (0 < iNumChildren)
							{
								CvXMLLoadUtility::InitList(&m_ppbFreePromotionUnitCombats[iIndex], GC.getNumUnitCombatInfos());

								// if the call to the function that sets the current xml node to it's first non-comment
								// child and sets the parameter with the new node's value succeeds
								CvString szTag;
								if (pXML->GetChildXmlVal(szTag))
								{
									int iIndex2 = pXML->GetInfoClass(szTag);
									if (iIndex2 > -1)
									{
										m_ppbFreePromotionUnitCombats[iIndex][iIndex2] = true;
									}
									if(!(iNumChildren <= GC.getNumUnitCombatInfos()))
									{
										char	szMessage[1024];
										sprintf( szMessage, "For loop iterator is greater than array size \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
										gDLL->MessageBox(szMessage, "XML Error");
									}
									// loop through all the siblings, we start at 1 since we already have the first value
									for (int i=1;i<iNumChildren;i++)
									{
										// if the call to the function that sets the current xml node to it's first non-comment
										// sibling and sets the parameter with the new node's value does not succeed
										// we will break out of this for loop

										if (!pXML->GetNextXmlVal(szTag))
										{
											break;
										}
										iIndex2 = pXML->GetInfoClass(szTag);
										if (iIndex2 > -1)
										{
											m_ppbFreePromotionUnitCombats[iIndex][iIndex2] = true;
										}

									}
									// set the current xml node to it's parent node
									pXML->MoveToXmlParent();
								}
							}
							else
							{
								SAFE_DELETE_ARRAY(m_ppbFreePromotionUnitCombats[iIndex]);
							}

							pXML->MoveToXmlParent();
						}
						else
						{
							SAFE_DELETE_ARRAY(m_ppbFreePromotionUnitCombats[iIndex]);
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

	//arrays
	int j;
	int k;
	int iNumChildren;
	if (pXML->TryMoveToXmlFirstChild(L"SpecialistYieldChanges"))
	{
		iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"SpecialistYieldChange"))
		{
			for(j=0;j<iNumChildren;j++)
			{
				pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
				k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiSpecialistYieldChange == NULL )
					{
						m_ppaiSpecialistYieldChange = new int*[GC.getNumSpecialistInfos()];

						for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
						{
							m_ppaiSpecialistYieldChange[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"YieldChanges"))
					{
						// call the function that sets the yield change variable
						pXML->SetYields(&m_ppaiSpecialistYieldChange[k]);
						pXML->MoveToXmlParent();
					}
				}

				if (!pXML->TryMoveToXmlNextSibling())
				{
					break;
				}
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"SpecialistCommerceChanges"))
	{
		iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"SpecialistCommerceChange"))
		{
			for(j=0;j<iNumChildren;j++)
			{
				pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
				k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiSpecialistCommerceChange == NULL )
					{
						m_ppaiSpecialistCommerceChange = new int*[GC.getNumSpecialistInfos()];

						for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
						{
							m_ppaiSpecialistCommerceChange[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"CommerceChanges"))
					{
						// call the function that sets the yield change variable
						pXML->SetCommerce(&m_ppaiSpecialistCommerceChange[k]);
						pXML->MoveToXmlParent();
					}
				}

				if (!pXML->TryMoveToXmlNextSibling())
				{
					break;
				}
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}
	pXML->SetVariableListTagPair(&m_piFlavorValue, L"Flavors", GC.getNumFlavorTypes());

	if (pXML->TryMoveToXmlFirstChild(L"ImprovementYieldChanges"))
	{
		iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"ImprovementYieldChange"))
		{
			for(j=0;j<iNumChildren;j++)
			{
				pXML->GetChildXmlValByName(szTextVal, L"ImprovementType");
				k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiImprovementYieldChange == NULL )
					{
						m_ppaiImprovementYieldChange = new int*[GC.getNumImprovementInfos()];

						for(int i = 0; i < GC.getNumImprovementInfos(); i++)
						{
							m_ppaiImprovementYieldChange[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"YieldChanges"))
					{
						// call the function that sets the yield change variable
						pXML->SetYields(&m_ppaiImprovementYieldChange[k]);
						pXML->MoveToXmlParent();
					}
				}

				if (!pXML->TryMoveToXmlNextSibling())
				{
					break;
				}
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"DisallowedTraitTypes"))
	{
		int i = 0;
		int iNum = pXML->GetXmlChildrenNumber(L"DisallowedTraitType" );
		m_aDisallowedTraitTypes.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"DisallowedTraitType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"TraitType");
					GC.addDelayedResolution((int*)&(m_aDisallowedTraitTypes[i].eTrait), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"DisallowedTraitType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"BuildingProductionModifierTypes"))
	{
		int i = 0;
		int iNum = pXML->GetXmlChildrenNumber(L"BuildingProductionModifierType" );
		m_aBuildingProductionModifiers.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"BuildingProductionModifierType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"BuildingType");
					pXML->GetChildXmlValByName(&(m_aBuildingProductionModifiers[i].iModifier), L"iBuildingProductionModifier");
					GC.addDelayedResolution((int*)&(m_aBuildingProductionModifiers[i].eBuilding), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"BuildingProductionModifierType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"UnitProductionModifierTypes"))
	{
		int i = 0;
		int iNum = pXML->GetXmlChildrenNumber(L"UnitProductionModifierType" );
		m_aUnitProductionModifiers.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"UnitProductionModifierType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitType");
					pXML->GetChildXmlValByName(&(m_aUnitProductionModifiers[i].iModifier), L"iUnitProductionModifier");
					GC.addDelayedResolution((int*)&(m_aUnitProductionModifiers[i].eUnit), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"UnitProductionModifierType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"SpecialUnitProductionModifierTypes"))
	{
		int i = 0;
		int iNum = pXML->GetXmlChildrenNumber(L"SpecialUnitProductionModifierType" );
		m_aSpecialUnitProductionModifiers.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"SpecialUnitProductionModifierType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"SpecialUnitType");
					m_aSpecialUnitProductionModifiers[i].eSpecialUnit = (SpecialUnitTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aSpecialUnitProductionModifiers[i].iModifier), L"iSpecialUnitProductionModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"SpecialUnitProductionModifierType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"CivicOptionNoUpkeepTypes"))
	{
		int i = 0;
		int iNum = pXML->GetXmlChildrenNumber(L"CivicOptionNoUpkeepType" );
		m_aCivicOptionNoUpkeepTypes.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"CivicOptionNoUpkeepType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"CivicOptionType");
					pXML->GetChildXmlValByName(&(m_aCivicOptionNoUpkeepTypes[i].bBool), L"bCivicOptionNoUpkeep");
					GC.addDelayedResolution((int*)&(m_aCivicOptionNoUpkeepTypes[i].eCivicOption), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"CivicOptionNoUpkeepType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"UnitCombatFreeExperiences"))
	{
		int i = 0;
		int iNum = pXML->GetXmlChildrenNumber(L"UnitCombatFreeExperience" );
		m_aUnitCombatFreeExperiences.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"UnitCombatFreeExperience"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					m_aUnitCombatFreeExperiences[i].eUnitCombat = (UnitCombatTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aUnitCombatFreeExperiences[i].iModifier), L"iModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"UnitCombatFreeExperience"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"UnitCombatProductionModifiers"))
	{
		int i = 0;
		int iNum = pXML->GetXmlChildrenNumber(L"UnitCombatProductionModifier" );
		m_aUnitCombatProductionModifiers.resize(iNum);
		if(pXML->TryMoveToXmlFirstChild())
		{

			if (pXML->TryMoveToXmlFirstOfSiblings(L"UnitCombatProductionModifier"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					m_aUnitCombatProductionModifiers[i].eUnitCombat = (UnitCombatTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aUnitCombatProductionModifiers[i].iModifier), L"iModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"UnitCombatProductionModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	//For Pure Traits (the Filtered 1D arrays are declared in getDataMembers; only the 2D
	// Filtered arrays remain hand-written here)

	if (pXML->TryMoveToXmlFirstChild(L"SpecialistYieldChanges"))
	{
		iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"SpecialistYieldChange"))
		{
			for(j=0;j<iNumChildren;j++)
			{
				pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
				k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiSpecialistYieldChangeFiltered == NULL )
					{
						m_ppaiSpecialistYieldChangeFiltered = new int*[GC.getNumSpecialistInfos()];

						for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
						{
							m_ppaiSpecialistYieldChangeFiltered[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"YieldChanges"))
					{
						// call the function that sets the yield change variable
						pXML->SetYields(&m_ppaiSpecialistYieldChangeFiltered[k]);
						pXML->MoveToXmlParent();
					}
				}

				if (!pXML->TryMoveToXmlNextSibling())
				{
					break;
				}
			}

			// set the current xml node to it's parent node
			pXML->MoveToXmlParent();
		}

		// set the current xml node to it's parent node
		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"SpecialistCommerceChanges"))
	{
		iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"SpecialistCommerceChange"))
		{
			for(j=0;j<iNumChildren;j++)
			{
				pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
				k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiSpecialistCommerceChangeFiltered == NULL )
					{
						m_ppaiSpecialistCommerceChangeFiltered = new int*[GC.getNumSpecialistInfos()];

						for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
						{
							m_ppaiSpecialistCommerceChangeFiltered[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"CommerceChanges"))
					{
						// call the function that sets the yield change variable
						pXML->SetCommerce(&m_ppaiSpecialistCommerceChangeFiltered[k]);
						pXML->MoveToXmlParent();
					}
				}

				if (!pXML->TryMoveToXmlNextSibling())
				{
					break;
				}
			}

			// set the current xml node to it's parent node
			pXML->MoveToXmlParent();
		}

		// set the current xml node to it's parent node
		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"ImprovementYieldChanges"))
	{
		iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"ImprovementYieldChange"))
		{
			for(j=0;j<iNumChildren;j++)
			{
				pXML->GetChildXmlValByName(szTextVal, L"ImprovementType");
				k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiImprovementYieldChangeFiltered == NULL )
					{
						m_ppaiImprovementYieldChangeFiltered = new int*[GC.getNumImprovementInfos()];

						for(int i = 0; i < GC.getNumImprovementInfos(); i++)
						{
							m_ppaiImprovementYieldChangeFiltered[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"YieldChanges"))
					{
						// call the function that sets the yield change variable
						pXML->SetYields(&m_ppaiImprovementYieldChangeFiltered[k]);
						pXML->MoveToXmlParent();
					}
				}

				if (!pXML->TryMoveToXmlNextSibling())
				{
					break;
				}
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}

void CvTraitInfo::copyNonDefaults(CvTraitInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	int iDefault = 0;

	CvInfoBase::copyNonDefaults(pClassInfo);

	// Scalars, strings, enum FKs, membership lists, yield/commerce arrays and the property
	// manipulators are declared in getDataMembers and merged by CvInfoUtil(this).copyNonDefaults
	// below. NOTE: the wrapper merge compares against the type-correct defaults and merges every
	// declared field, which fixes several legacy merge bugs — see getCheckSum's parity comment.

	for ( int j = 0; j < GC.getNumPromotionInfos(); j++ )
	{
		for ( int i = 0; i < GC.getNumUnitCombatInfos(); i++ )
		{
			if (!isFreePromotionUnitCombats(j, i) && pClassInfo->isFreePromotionUnitCombats(j, i))
			{
				if ( NULL == m_ppbFreePromotionUnitCombats )
				{
					CvXMLLoadUtility::Init2DList(&m_ppbFreePromotionUnitCombats, GC.getNumPromotionInfos(), GC.getNumUnitCombatInfos());
				}
				else if ( NULL == m_ppbFreePromotionUnitCombats[i] )
				{
					CvXMLLoadUtility::InitList(&m_ppbFreePromotionUnitCombats[i],GC.getNumUnitCombatInfos());
				}
				m_ppbFreePromotionUnitCombats[j][i] = pClassInfo->isFreePromotionUnitCombats(j, i);
			}
		}
	}

	//Arrays
	for ( int i = 0; i < GC.getNumSpecialistInfos(); i++)
	{
		for ( int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if ( getSpecialistYieldChange(i,j) == iDefault )
			{
				int iChange = pClassInfo->getSpecialistYieldChange(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiSpecialistYieldChange == NULL )
					{
						m_ppaiSpecialistYieldChange = new int*[GC.getNumSpecialistInfos()];
						for(int k = 0; k < GC.getNumSpecialistInfos(); k++)
						{
							m_ppaiSpecialistYieldChange[k] = NULL;
						}
					}
					if ( m_ppaiSpecialistYieldChange[i] == NULL )
					{
						CvXMLLoadUtility::InitList(&m_ppaiSpecialistYieldChange[i], NUM_YIELD_TYPES);
					}

					if ( m_ppaiSpecialistYieldChangeFiltered == NULL )
					{
						m_ppaiSpecialistYieldChangeFiltered = new int*[GC.getNumSpecialistInfos()];
						for(int k = 0; k < GC.getNumSpecialistInfos(); k++)
						{
							m_ppaiSpecialistYieldChangeFiltered[k] = NULL;
						}
					}
					if ( m_ppaiSpecialistYieldChangeFiltered[i] == NULL )
					{
						CvXMLLoadUtility::InitList(&m_ppaiSpecialistYieldChangeFiltered[i], NUM_YIELD_TYPES);
					}

					m_ppaiSpecialistYieldChange[i][j] = iChange;
					m_ppaiSpecialistYieldChangeFiltered[i][j] = iChange;
				}
			}
		}
	}

	for ( int i = 0; i < GC.getNumImprovementInfos(); i++)
	{
		for ( int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if ( getImprovementYieldChange(i,j) == iDefault )
			{
				int iChange = pClassInfo->getImprovementYieldChange(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiImprovementYieldChange == NULL )
					{
						m_ppaiImprovementYieldChange = new int*[GC.getNumImprovementInfos()];
						for(int k = 0; k < GC.getNumImprovementInfos(); k++)
						{
							m_ppaiImprovementYieldChange[k] = NULL;
						}
					}
					if ( m_ppaiImprovementYieldChange[i] == NULL )
					{
						CvXMLLoadUtility::InitList(&m_ppaiImprovementYieldChange[i], NUM_YIELD_TYPES);
					}

					if ( m_ppaiImprovementYieldChangeFiltered == NULL )
					{
						m_ppaiImprovementYieldChangeFiltered = new int*[GC.getNumImprovementInfos()];
						for(int k = 0; k < GC.getNumImprovementInfos(); k++)
						{
							m_ppaiImprovementYieldChangeFiltered[k] = NULL;
						}
					}
					if ( m_ppaiImprovementYieldChangeFiltered[i] == NULL )
					{
						CvXMLLoadUtility::InitList(&m_ppaiImprovementYieldChangeFiltered[i], NUM_YIELD_TYPES);
					}

					m_ppaiImprovementYieldChange[i][j] = iChange;
					m_ppaiImprovementYieldChangeFiltered[i][j] = iChange;
				}
			}
		}
	}

	for ( int i = 0; i < GC.getNumSpecialistInfos(); i++)
	{
		for ( int j = 0; j < NUM_COMMERCE_TYPES; j++)
		{
			if ( getSpecialistCommerceChange(i,j) == iDefault )
			{
				int iChange = pClassInfo->getSpecialistCommerceChange(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiSpecialistCommerceChange == NULL )
					{
						m_ppaiSpecialistCommerceChange = new int*[GC.getNumSpecialistInfos()];
						for(int k = 0; k < GC.getNumSpecialistInfos(); k++)
						{
							m_ppaiSpecialistCommerceChange[k] = NULL;
						}
					}
					if ( m_ppaiSpecialistCommerceChange[i] == NULL )
					{
						CvXMLLoadUtility::InitList(&m_ppaiSpecialistCommerceChange[i], NUM_COMMERCE_TYPES);
					}

					if ( m_ppaiSpecialistCommerceChangeFiltered == NULL )
					{
						m_ppaiSpecialistCommerceChangeFiltered = new int*[GC.getNumSpecialistInfos()];
						for(int k = 0; k < GC.getNumSpecialistInfos(); k++)
						{
							m_ppaiSpecialistCommerceChangeFiltered[k] = NULL;
						}
					}
					if ( m_ppaiSpecialistCommerceChangeFiltered[i] == NULL )
					{
						CvXMLLoadUtility::InitList(&m_ppaiSpecialistCommerceChangeFiltered[i], NUM_COMMERCE_TYPES);
					}

					m_ppaiSpecialistCommerceChange[i][j] = iChange;
					m_ppaiSpecialistCommerceChangeFiltered[i][j] = iChange;
				}
			}
		}
	}

	if (getNumDisallowedTraitTypes() == 0)
	{
		int iNum = pClassInfo->getNumDisallowedTraitTypes();
		m_aDisallowedTraitTypes.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			GC.copyNonDefaultDelayedResolution((int*)&(m_aDisallowedTraitTypes[i].eTrait), (int*)&(pClassInfo->m_aDisallowedTraitTypes[i].eTrait));
		}
	}

	if (getNumCivicOptionNoUpkeepTypes() == 0)
	{
		int iNum = pClassInfo->getNumCivicOptionNoUpkeepTypes();
		m_aCivicOptionNoUpkeepTypes.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aCivicOptionNoUpkeepTypes[i].bBool = pClassInfo->m_aCivicOptionNoUpkeepTypes[i].bBool;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aCivicOptionNoUpkeepTypes[i].eCivicOption), (int*)&(pClassInfo->m_aCivicOptionNoUpkeepTypes[i].eCivicOption));
		}
	}

	for ( int i = 0; i < GC.getNumFlavorTypes(); i++ )
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

	if (getNumBuildingProductionModifiers() == 0)
	{
		int iNum = pClassInfo->getNumBuildingProductionModifiers();
		m_aBuildingProductionModifiers.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aBuildingProductionModifiers[i].iModifier = pClassInfo->m_aBuildingProductionModifiers[i].iModifier;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aBuildingProductionModifiers[i].eBuilding), (int*)&(pClassInfo->m_aBuildingProductionModifiers[i].eBuilding));
		}
	}

	if (getNumUnitProductionModifiers() == 0)
	{
		int iNum = pClassInfo->getNumUnitProductionModifiers();
		m_aUnitProductionModifiers.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aUnitProductionModifiers[i].iModifier = pClassInfo->m_aUnitProductionModifiers[i].iModifier;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aUnitProductionModifiers[i].eUnit), (int*)&(pClassInfo->m_aUnitProductionModifiers[i].eUnit));
		}
	}

	if (getNumSpecialUnitProductionModifiers() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aSpecialUnitProductionModifiers, pClassInfo->m_aSpecialUnitProductionModifiers);
	}

//Team Project (8)
	if (getNumUnitCombatFreeExperiences() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aUnitCombatFreeExperiences, pClassInfo->m_aUnitCombatFreeExperiences);
	}

	if (getNumUnitCombatProductionModifiers() == 0)
	{
		CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aUnitCombatProductionModifiers, pClassInfo->m_aUnitCombatProductionModifiers);
	}

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

void CvTraitInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	for ( int j = 0; j < GC.getNumPromotionInfos(); j++ )
	{
		for ( int i = 0; i < GC.getNumUnitCombatInfos(); i++ )
		{
			CheckSum(iSum, isFreePromotionUnitCombats(j,i));
		}
	}

	CheckSum(iSum, m_iHealth);
	CheckSum(iSum, m_iHappiness);
	CheckSum(iSum, m_iMaxAnarchy);
	CheckSum(iSum, m_iUpkeepModifier);
	CheckSum(iSum, m_iLevelExperienceModifier);
	CheckSum(iSum, m_iGreatPeopleRateModifier);
	CheckSum(iSum, m_iGreatGeneralRateModifier);
	CheckSum(iSum, m_iDomesticGreatGeneralRateModifier);
	CheckSum(iSum, m_iMaxGlobalBuildingProductionModifier);
	CheckSum(iSum, m_iMaxTeamBuildingProductionModifier);
	CheckSum(iSum, m_iMaxPlayerBuildingProductionModifier);

	CheckSum(iSum, m_iRevIdxLocal);
	CheckSum(iSum, m_iRevIdxNational);
	CheckSum(iSum, m_iRevIdxDistanceModifier);
	CheckSum(iSum, m_iRevIdxHolyCityGood);
	CheckSum(iSum, m_iRevIdxHolyCityBad);
	CheckSum(iSum, m_fRevIdxNationalityMod);
	CheckSum(iSum, m_fRevIdxGoodReligionMod);
	CheckSum(iSum, m_fRevIdxBadReligionMod);
	CheckSum(iSum, m_bNonStateReligionCommerce);
	CheckSum(iSum, m_bUpgradeAnywhere);

	// Arrays

	CheckSum(iSum, m_paiExtraYieldThreshold, NUM_YIELD_TYPES);
	CheckSum(iSum, m_paiTradeYieldModifier, NUM_YIELD_TYPES);
	CheckSum(iSum, m_paiCommerceChange, NUM_COMMERCE_TYPES);
	CheckSum(iSum, m_paiCommerceModifier, NUM_COMMERCE_TYPES);

	m_PropertyManipulators.getCheckSum(iSum);

	//TB Traits Mods begin
	//Textual References
	CheckSum(iSum, m_iPrereqTrait);
	CheckSum(iSum, m_iPrereqOrTrait1);
	CheckSum(iSum, m_iPrereqOrTrait2);
	CheckSum(iSum, m_ePromotionLine);
	CheckSum(iSum, m_iGreatPeopleUnitType);
	CheckSum(iSum, m_iGoldenAgeonBirthofGreatPeopleType);
	CheckSum(iSum, m_ePrereqTech);
	CheckSum(iSum, m_eEraAdvanceFreeSpecialistType);
	//integers
	CheckSum(iSum, m_iWarWearinessAccumulationModifier);
	CheckSum(iSum, m_iCivicAnarchyTimeModifier);
	CheckSum(iSum, m_iReligiousAnarchyTimeModifier);
	CheckSum(iSum, m_iImprovementUpgradeRateModifier);
	CheckSum(iSum, m_iWorkerSpeedModifier);
	CheckSum(iSum, m_iMaxConscript);
	CheckSum(iSum, m_iDistanceMaintenanceModifier);
	CheckSum(iSum, m_iNumCitiesMaintenanceModifier);
	CheckSum(iSum, m_iCorporationMaintenanceModifier);
	CheckSum(iSum, m_iStateReligionGreatPeopleRateModifier);
	CheckSum(iSum, m_iFreeExperience);
	CheckSum(iSum, m_iFreeUnitUpkeepCivilian);
	CheckSum(iSum, m_iFreeUnitUpkeepMilitary);
	CheckSum(iSum, m_iFreeUnitUpkeepCivilianPopPercent);
	CheckSum(iSum, m_iFreeUnitUpkeepMilitaryPopPercent);
	CheckSum(iSum, m_iCivilianUnitUpkeepMod);
	CheckSum(iSum, m_iMilitaryUnitUpkeepMod);
	CheckSum(iSum, m_iHappyPerMilitaryUnit);
	CheckSum(iSum, m_iLargestCityHappiness);
	CheckSum(iSum, m_iFreeSpecialist);
	CheckSum(iSum, m_iTradeRoutes);
	CheckSum(iSum, m_iStateReligionHappiness);
	CheckSum(iSum, m_iNonStateReligionHappiness);
	CheckSum(iSum, m_iStateReligionUnitProductionModifier);
	CheckSum(iSum, m_iStateReligionBuildingProductionModifier);
	CheckSum(iSum, m_iStateReligionFreeExperience);
	CheckSum(iSum, m_iExpInBorderModifier);
	CheckSum(iSum, m_iCityDefenseBonus);
	CheckSum(iSum, m_iMilitaryProductionModifier);
	CheckSum(iSum, m_iAttitudeModifier);
	CheckSum(iSum, m_iLinePriority);
	CheckSum(iSum, m_iEspionageDefense);
	CheckSum(iSum, m_iMinAnarchy);
	CheckSum(iSum, m_iMaxTradeRoutesChange);
	CheckSum(iSum, m_iGoldenAgeDurationModifier);
	CheckSum(iSum, m_iGreatPeopleRateChange);
	CheckSum(iSum, m_iHurryAngerModifier);
	CheckSum(iSum, m_iHurryCostModifier);
	CheckSum(iSum, m_iEnemyWarWearinessModifier);
	CheckSum(iSum, m_iForeignTradeRouteModifier);
	CheckSum(iSum, m_iBombardDefense);
	CheckSum(iSum, m_iUnitUpgradePriceModifier);
	CheckSum(iSum, m_iCoastalTradeRoutes);
	CheckSum(iSum, m_iGlobalPopulationgrowthratepercentage);
	CheckSum(iSum, m_iCityStartCulture);
	CheckSum(iSum, m_iGlobalAirUnitCapacity);
	CheckSum(iSum, m_iCapitalXPModifier);
	CheckSum(iSum, m_iHolyCityofStateReligionXPModifier);
	CheckSum(iSum, m_iHolyCityofNonStateReligionXPModifier);
	CheckSum(iSum, m_iBonusPopulationinNewCities);
	CheckSum(iSum, m_iMissileRange);
	CheckSum(iSum, m_iFlightOperationRange);
	CheckSum(iSum, m_iNavalCargoSpace);
	CheckSum(iSum, m_iMissileCargoSpace);
	CheckSum(iSum, m_iNationalCaptureProbabilityModifier);
	CheckSum(iSum, m_iNationalCaptureResistanceModifier);
	CheckSum(iSum, m_iStateReligionSpreadProbabilityModifier);
	CheckSum(iSum, m_iNonStateReligionSpreadProbabilityModifier);
	CheckSum(iSum, m_iFreedomFighterChange);
	//booleans
	CheckSum(iSum, m_bMilitaryFoodProduction);
	CheckSum(iSum, m_bNegativeTrait);
	CheckSum(iSum, m_bImpurePropertyManipulators);
	CheckSum(iSum, m_bImpurePromotions);
	CheckSum(iSum, m_bCivilizationTrait);
	CheckSum(iSum, m_bAllowsInquisitions);
	CheckSum(iSum, m_bCoastalAIInfluence);
	CheckSum(iSum, m_bBarbarianSelectionOnly);
	CheckSum(iSum, m_bCitiesStartwithStateReligion);
	CheckSum(iSum, m_bDraftsOnCityCapture);
	CheckSum(iSum, m_bFreeSpecialistperWorldWonder);
	CheckSum(iSum, m_bFreeSpecialistperNationalWonder);
	CheckSum(iSum, m_bFreeSpecialistperTeamProject);
	CheckSum(iSum, m_bExtraGoody);
	CheckSum(iSum, m_bAllReligionsActive);
	CheckSum(iSum, m_bBansNonStateReligions);
	CheckSum(iSum, m_bFreedomFighter);
	// bool vector without delayed resolution
	CheckSumC(iSum, m_aiNotOnGameOptions);
	CheckSumC(iSum, m_aiOnGameOptions);
	CheckSumC(iSum, m_aiCategories);
	//Arrays
	if (m_ppaiSpecialistYieldChange)
	{
		for(int i=0;i<GC.getNumSpecialistInfos();i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppaiSpecialistYieldChange[i]);
		}
	}

	CheckSumI(iSum, NUM_YIELD_TYPES, m_piYieldModifier);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piCapitalYieldModifier);
	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_piCapitalCommerceModifier);
	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_piSpecialistExtraCommerce);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piSpecialistExtraYield);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piYieldChange);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piSeaPlotYieldChanges);
//Team Project (7)
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piGoldenAgeYieldChanges);
	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_piGoldenAgeCommerceChanges);

	if (m_ppaiSpecialistCommerceChange)
	{
		for(int i=0;i<GC.getNumSpecialistInfos();i++)
		{
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppaiSpecialistCommerceChange[i]);
		}
	}

	CheckSum(iSum, m_paiLessYieldThreshold, NUM_YIELD_TYPES);
	if (m_ppaiImprovementYieldChange)
	{
		for(int i=0;i<GC.getNumImprovementInfos();i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppaiImprovementYieldChange[i]);
		}
	}
	// int vectors utilizing struct with delayed resolution
	int iNumElements;
	int i;

	iNumElements = m_aDisallowedTraitTypes.size();
	for (i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aDisallowedTraitTypes[i].eTrait);
	}

	CheckSum(iSum, m_piFlavorValue, GC.getNumFlavorTypes());

	iNumElements = m_aBuildingProductionModifiers.size();
	for (i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aBuildingProductionModifiers[i].eBuilding);
		CheckSum(iSum, m_aBuildingProductionModifiers[i].iModifier);
	}

	iNumElements = m_aUnitProductionModifiers.size();
	for (i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aUnitProductionModifiers[i].eUnit);
		CheckSum(iSum, m_aUnitProductionModifiers[i].iModifier);
	}

	iNumElements = m_aSpecialUnitProductionModifiers.size();
	for (i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aSpecialUnitProductionModifiers[i].eSpecialUnit);
		CheckSum(iSum, m_aSpecialUnitProductionModifiers[i].iModifier);
	}

	iNumElements = m_aCivicOptionNoUpkeepTypes.size();
	for (i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aCivicOptionNoUpkeepTypes[i].eCivicOption);
		CheckSum(iSum, m_aCivicOptionNoUpkeepTypes[i].bBool);
	}

//Team Project (8)
	iNumElements = m_aUnitCombatFreeExperiences.size();
	for (i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aUnitCombatFreeExperiences[i].eUnitCombat);
		CheckSum(iSum, m_aUnitCombatFreeExperiences[i].iModifier);
	}

	iNumElements = m_aUnitCombatProductionModifiers.size();
	for (i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aUnitCombatProductionModifiers[i].eUnitCombat);
		CheckSum(iSum, m_aUnitCombatProductionModifiers[i].iModifier);
	}

	//For Pure Traits
	CheckSum(iSum, m_paiExtraYieldThresholdFiltered, NUM_YIELD_TYPES);
	CheckSum(iSum, m_paiTradeYieldModifierFiltered, NUM_YIELD_TYPES);
	CheckSum(iSum, m_paiCommerceChangeFiltered, NUM_COMMERCE_TYPES);
	CheckSum(iSum, m_paiCommerceModifierFiltered, NUM_COMMERCE_TYPES);
	if (m_ppaiSpecialistYieldChangeFiltered)
	{
		for(i=0;i<GC.getNumSpecialistInfos();i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppaiSpecialistYieldChangeFiltered[i]);
		}
	}

	CheckSumI(iSum, NUM_YIELD_TYPES, m_piYieldModifierFiltered);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piCapitalYieldModifierFiltered);
	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_piCapitalCommerceModifierFiltered);
	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_piSpecialistExtraCommerceFiltered);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piSpecialistExtraYieldFiltered);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piYieldChangeFiltered);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piSeaPlotYieldChangesFiltered);
//Team Project (7)
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piGoldenAgeYieldChangesFiltered);
	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_piGoldenAgeCommerceChangesFiltered);
	CheckSum(iSum, m_paiLessYieldThresholdFiltered, NUM_YIELD_TYPES);

	if (m_ppaiSpecialistCommerceChangeFiltered)
	{
		for(i=0;i<GC.getNumSpecialistInfos();i++)
		{
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppaiSpecialistCommerceChangeFiltered[i]);
		}
	}

	if (m_ppaiImprovementYieldChangeFiltered)
	{
		for(i=0;i<GC.getNumImprovementInfos();i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppaiImprovementYieldChangeFiltered[i]);
		}
	}

	//TB Traits Mods end

	// Checksum parity (savegame asset checksum): the fields migrated to getDataMembers are still
	// checksummed by hand ABOVE, in the legacy fold order — CheckSum is an order-sensitive rotating
	// fold, so delegating to CvInfoUtil::checkSum (declaration order) would shift every trait's sum.
	// The IDValueMaps below were already delegated (folded last); these explicit CheckSumC calls
	// reproduce IDValueMapWrapper::checkSum byte-for-byte in the same (declaration) order.
	// If a field is added to getDataMembers, add a matching CheckSum line here.
	CheckSumC(iSum, m_aSpecialBuildingProductionModifiers);
	CheckSumC(iSum, m_aBuildWorkerSpeedModifierTypes);
	CheckSumC(iSum, m_aBuildingHappinessModifiers);
	CheckSumC(iSum, m_aTechResearchModifiers);
	CheckSumC(iSum, m_aBonusHappinessChanges);
	CheckSumC(iSum, m_aImprovementUpgradeModifierTypes);
	CheckSumC(iSum, m_aDomainFreeExperiences);
	CheckSumC(iSum, m_aDomainProductionModifiers);
}


const CvPropertyManipulators* CvTraitInfo::getPropertyManipulators() const
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS) && isImpurePropertyManipulators())
	{
		return &m_PropertyManipulatorsNull;
	}

	return &m_PropertyManipulators;
}