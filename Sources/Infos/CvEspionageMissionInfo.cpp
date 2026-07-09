//------------------------------------------------------------------------------------------------
//  FILE:    CvEspionageMissionInfo.cpp
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
#include "CvEspionageMissionInfo.h"



//======================================================================================================
//					CvEspionageMissionInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvEspionageMissionInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvEspionageMissionInfo::CvEspionageMissionInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvEspionageMissionInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvEspionageMissionInfo::~CvEspionageMissionInfo()
{
	CvInfoUtil(this).uninitDataMembers();
}


void CvEspionageMissionInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. The last 8 fields (m_bNuke onward) are read from XML
	// but were never part of the legacy checksum, so they are parked at the end and getCheckSum below
	// stays explicit, reproducing the legacy field set byte-for-byte.
	util
		.add(m_iCost, L"iCost")
		.add(m_bIsPassive, L"bIsPassive")
		.add(m_bIsTwoPhases, L"bIsTwoPhases")
		.add(m_bTargetsCity, L"bTargetsCity")
		.add(m_bSelectPlot, L"bSelectPlot")
		.addEnum(m_iTechPrereq, L"TechPrereq")
		.add(m_iVisibilityLevel, L"iVisibilityLevel")
		.add(m_bInvestigateCity, L"bInvestigateCity")
		.add(m_bSeeDemographics, L"bSeeDemographics")
		.add(m_bNoActiveMissions, L"bNoActiveMissions")
		.add(m_bSeeResearch, L"bSeeResearch")
		.add(m_bDestroyImprovement, L"bDestroyImprovement")
		.add(m_iDestroyBuildingCostFactor, L"iDestroyBuildingCostFactor")
		.add(m_iDestroyUnitCostFactor, L"iDestroyUnitCostFactor")
		.add(m_iDestroyProjectCostFactor, L"iDestroyProjectCostFactor")
		.add(m_iDestroyProductionCostFactor, L"iDestroyProductionCostFactor")
		.add(m_iBuyUnitCostFactor, L"iBuyUnitCostFactor")
		.add(m_iBuyCityCostFactor, L"iBuyCityCostFactor")
		.add(m_iStealTreasuryTypes, L"iStealTreasuryTypes")
		.add(m_iCityInsertCultureAmountFactor, L"iCityInsertCultureAmountFactor")
		.add(m_iCityInsertCultureCostFactor, L"iCityInsertCultureCostFactor")
		.add(m_iCityPoisonWaterCounter, L"iCityPoisonWaterCounter")
		.add(m_iCityUnhappinessCounter, L"iCityUnhappinessCounter")
		.add(m_iCityRevoltCounter, L"iCityRevoltCounter")
		.add(m_iBuyTechCostFactor, L"iBuyTechCostFactor")
		.add(m_iSwitchCivicCostFactor, L"iSwitchCivicCostFactor")
		.add(m_iSwitchReligionCostFactor, L"iSwitchReligionCostFactor")
		.add(m_iPlayerAnarchyCounter, L"iPlayerAnarchyCounter")
		.add(m_iCounterespionageNumTurns, L"iCounterespionageNumTurns")
		.add(m_iCounterespionageMod, L"iCounterespionageMod")
		.add(m_iDifficultyMod, L"iDifficultyMod")
		// Read but never checksummed (legacy omission, preserved):
		.add(m_bNuke, L"bNuke")
		.add(m_bRevolt, L"bRevolt")
		.add(m_bDisablePower, L"bDisablePower")
		.addEnumAsInt(m_iPrereqGameOption, L"PrereqGameOption")
		.add(m_iWarWearinessCounter, L"iWarWearinessCounter")
		.add(m_iSabatogeResearchCostFactor, L"iSabatogeResearchCostFactor")
		.add(m_iRemoveReligionsCostFactor, L"iRemoveReligionsCostFactor")
		.add(m_iRemoveCorporationsCostFactor, L"iRemoveCorporationsCostFactor")
	;
}


int CvEspionageMissionInfo::getCost() const
{
	return m_iCost;
}


bool CvEspionageMissionInfo::isPassive() const
{
	return m_bIsPassive;
}


bool CvEspionageMissionInfo::isTwoPhases() const
{
	return m_bIsTwoPhases;
}


bool CvEspionageMissionInfo::isTargetsCity() const
{
	return m_bTargetsCity;
}


bool CvEspionageMissionInfo::isSelectPlot() const
{
	return m_bSelectPlot;
}


int CvEspionageMissionInfo::getVisibilityLevel() const
{
	return m_iVisibilityLevel;
}


bool CvEspionageMissionInfo::isInvestigateCity() const
{
	return m_bInvestigateCity;
}


bool CvEspionageMissionInfo::isSeeDemographics() const
{
	return m_bSeeDemographics;
}


bool CvEspionageMissionInfo::isNoActiveMissions() const
{
	return m_bNoActiveMissions;
}


bool CvEspionageMissionInfo::isSeeResearch() const
{
	return m_bSeeResearch;
}


bool CvEspionageMissionInfo::isDestroyImprovement() const
{
	return m_bDestroyImprovement;
}


int CvEspionageMissionInfo::getDestroyBuildingCostFactor() const
{
	return m_iDestroyBuildingCostFactor;
}


int CvEspionageMissionInfo::getDestroyUnitCostFactor() const
{
	return m_iDestroyUnitCostFactor;
}


int CvEspionageMissionInfo::getDestroyProjectCostFactor() const
{
	return m_iDestroyProjectCostFactor;
}


int CvEspionageMissionInfo::getDestroyProductionCostFactor() const
{
	return m_iDestroyProductionCostFactor;
}


int CvEspionageMissionInfo::getBuyUnitCostFactor() const
{
	return m_iBuyUnitCostFactor;
}


int CvEspionageMissionInfo::getBuyCityCostFactor() const
{
	return m_iBuyCityCostFactor;
}


int CvEspionageMissionInfo::getStealTreasuryTypes() const
{
	return m_iStealTreasuryTypes;
}


int CvEspionageMissionInfo::getCityInsertCultureAmountFactor() const
{
	return m_iCityInsertCultureAmountFactor;
}


int CvEspionageMissionInfo::getCityInsertCultureCostFactor() const
{
	return m_iCityInsertCultureCostFactor;
}


int CvEspionageMissionInfo::getCityPoisonWaterCounter() const
{
	return m_iCityPoisonWaterCounter;
}


int CvEspionageMissionInfo::getCityUnhappinessCounter() const
{
	return m_iCityUnhappinessCounter;
}


int CvEspionageMissionInfo::getCityRevoltCounter() const
{
	return m_iCityRevoltCounter;
}


int CvEspionageMissionInfo::getBuyTechCostFactor() const
{
	return m_iBuyTechCostFactor;
}


int CvEspionageMissionInfo::getSwitchCivicCostFactor() const
{
	return m_iSwitchCivicCostFactor;
}


int CvEspionageMissionInfo::getSwitchReligionCostFactor() const
{
	return m_iSwitchReligionCostFactor;
}


int CvEspionageMissionInfo::getPlayerAnarchyCounter() const
{
	return m_iPlayerAnarchyCounter;
}


int CvEspionageMissionInfo::getCounterespionageNumTurns() const
{
	return m_iCounterespionageNumTurns;
}


int CvEspionageMissionInfo::getCounterespionageMod() const
{
	return m_iCounterespionageMod;
}


int CvEspionageMissionInfo::getDifficultyMod() const
{
	return m_iDifficultyMod;
}


bool CvEspionageMissionInfo::isNuke() const
{
	return m_bNuke;
}


bool CvEspionageMissionInfo::isRevolt() const
{
	return m_bRevolt;
}


bool CvEspionageMissionInfo::isDisablePower() const
{
	return m_bDisablePower;
}


int CvEspionageMissionInfo::getPrereqGameOption() const
{
	return m_iPrereqGameOption;
}


int CvEspionageMissionInfo::getWarWearinessCounter() const
{
	return m_iWarWearinessCounter;
}


int CvEspionageMissionInfo::getSabatogeResearchCostFactor() const
{
	return m_iSabatogeResearchCostFactor;
}


int CvEspionageMissionInfo::getRemoveReligionsCostFactor() const
{
	return m_iRemoveReligionsCostFactor;
}


int CvEspionageMissionInfo::getRemoveCorporationsCostFactor() const
{
	return m_iRemoveCorporationsCostFactor;
}


bool CvEspionageMissionInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvEspionageMissionInfo::copyNonDefaults(const CvEspionageMissionInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvEspionageMissionInfo::getCheckSum(uint32_t& iSum) const
{
	// Explicit (not delegated): the legacy checksum omits the last 8 read fields (m_bNuke through
	// m_iRemoveCorporationsCostFactor); delegating would fold them in and change the value.
	CheckSum(iSum, m_iCost);
	CheckSum(iSum, m_bIsPassive);
	CheckSum(iSum, m_bIsTwoPhases);
	CheckSum(iSum, m_bTargetsCity);
	CheckSum(iSum, m_bSelectPlot);

	CheckSum(iSum, m_iTechPrereq);
	CheckSum(iSum, m_iVisibilityLevel);
	CheckSum(iSum, m_bInvestigateCity);
	CheckSum(iSum, m_bSeeDemographics);
	CheckSum(iSum, m_bNoActiveMissions);
	CheckSum(iSum, m_bSeeResearch);

	CheckSum(iSum, m_bDestroyImprovement);
	CheckSum(iSum, m_iDestroyBuildingCostFactor);
	CheckSum(iSum, m_iDestroyUnitCostFactor);
	CheckSum(iSum, m_iDestroyProjectCostFactor);
	CheckSum(iSum, m_iDestroyProductionCostFactor);
	CheckSum(iSum, m_iBuyUnitCostFactor);
	CheckSum(iSum, m_iBuyCityCostFactor);
	CheckSum(iSum, m_iStealTreasuryTypes);
	CheckSum(iSum, m_iCityInsertCultureAmountFactor);
	CheckSum(iSum, m_iCityInsertCultureCostFactor);
	CheckSum(iSum, m_iCityPoisonWaterCounter);
	CheckSum(iSum, m_iCityUnhappinessCounter);
	CheckSum(iSum, m_iCityRevoltCounter);
	CheckSum(iSum, m_iBuyTechCostFactor);
	CheckSum(iSum, m_iSwitchCivicCostFactor);
	CheckSum(iSum, m_iSwitchReligionCostFactor);
	CheckSum(iSum, m_iPlayerAnarchyCounter);
	CheckSum(iSum, m_iCounterespionageNumTurns);
	CheckSum(iSum, m_iCounterespionageMod);
	CheckSum(iSum, m_iDifficultyMod);
}

