//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:	CvBuildingInfos.cpp
//
//  PURPOSE: Info class for buildings
//
//------------------------------------------------------------------------------------------------
//  Copyright (c) 2003 Firaxis Games, Inc. All rights reserved.
//------------------------------------------------------------------------------------------------

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvBuildingInfo.h"
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvImprovementInfo.h"
#include "CvInfoUtil.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Tools/CheckSum.h"

//======================================================================================================
//					CvBuildingInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvBuildingInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvBuildingInfo::CvBuildingInfo() :
// Every XML-backed field declared in getDataMembers is initialized by initDataMembers()
// below; this list holds only the runtime/derived fields, the readPass3 targets and the
// hand-written loader remainder (see the comment in getDataMembers).
m_iMissionType(NO_MISSION),
m_iGreatPeopleUnitType(NO_UNIT),
m_ePropertySpawnUnit(NO_UNIT),
m_bNotShowInCity(false),
m_bEnablesOtherBuildings(false),
m_bEnablesUnits(false),
m_bDamageAttackerCapable(false),
m_pbCommerceFlexible(NULL),
m_pabHurry(NULL),
m_piSpecialistCount(NULL),
m_piFreeSpecialistCount(NULL),
m_piBonusProductionModifier(NULL),
m_piDomainFreeExperience(NULL),
m_piDomainProductionModifier(NULL),
m_piFlavorValue(NULL),
m_piVictoryThreshold(NULL),
m_piBonusDefenseChanges(NULL),
m_ppaiSpecialistYieldChange(NULL),
m_ppaiSpecialistCommerceChange(NULL),
m_ppaiLocalSpecialistYieldChange(NULL),
m_ppaiLocalSpecialistCommerceChange(NULL),
m_ppaiBonusYieldModifier(NULL),
m_ppaiTechSpecialistChange(NULL),
m_ppaiBonusCommerceModifier(NULL),
m_ppaiBonusYieldChanges(NULL),
m_ppaiBonusCommercePercentChanges(NULL),
m_ppaiVicinityBonusYieldChanges(NULL),
m_ppiImprovementYieldChanges(NULL)
{
	CvInfoUtil(this).initDataMembers();
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvBuildingInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvBuildingInfo::~CvBuildingInfo()
{
	// Frees the wrapper-owned arrays/exprs and removes delayed resolution for the declared
	// FK fields (the six building FK enums and the building/unit-keyed IDValueMaps).
	CvInfoUtil(this).uninitDataMembers();

	SAFE_DELETE_ARRAY(m_piVictoryThreshold);
	SAFE_DELETE_ARRAY(m_piSpecialistCount);
	SAFE_DELETE_ARRAY(m_piFreeSpecialistCount);
	SAFE_DELETE_ARRAY(m_piBonusProductionModifier);
	SAFE_DELETE_ARRAY(m_piDomainFreeExperience);
	SAFE_DELETE_ARRAY(m_piDomainProductionModifier);
	SAFE_DELETE_ARRAY(m_piFlavorValue);
	SAFE_DELETE_ARRAY(m_pbCommerceFlexible);
	SAFE_DELETE_ARRAY2(m_ppaiSpecialistYieldChange, GC.getNumSpecialistInfos());
	SAFE_DELETE_ARRAY2(m_ppaiSpecialistCommerceChange, GC.getNumSpecialistInfos());
	SAFE_DELETE_ARRAY2(m_ppaiLocalSpecialistYieldChange, GC.getNumSpecialistInfos());
	SAFE_DELETE_ARRAY2(m_ppaiLocalSpecialistCommerceChange, GC.getNumSpecialistInfos());
	SAFE_DELETE_ARRAY2(m_ppaiBonusYieldModifier, GC.getNumSpecialistInfos());
	GC.removeDelayedResolutionVector(m_aePrereqOrCivics);
	GC.removeDelayedResolutionVector(m_aePrereqAndCivics);
	SAFE_DELETE_ARRAY(m_piBonusDefenseChanges);
	SAFE_DELETE_ARRAY2(m_ppaiBonusCommerceModifier, GC.getNumBonusInfos());
	SAFE_DELETE_ARRAY2(m_ppaiBonusYieldChanges, GC.getNumBonusInfos());
	SAFE_DELETE_ARRAY2(m_ppaiBonusCommercePercentChanges, GC.getNumBonusInfos());
	SAFE_DELETE_ARRAY2(m_ppaiVicinityBonusYieldChanges, GC.getNumBonusInfos());
	SAFE_DELETE_ARRAY2(m_ppaiTechSpecialistChange, GC.getNumTechInfos());
	SAFE_DELETE_ARRAY2(m_ppiImprovementYieldChanges, GC.getNumImprovementInfos());

	//TB Building Tags
	SAFE_DELETE_ARRAY(m_pabHurry);
	//SAFE_DELETE(m_pExprFreePromotionCondition);

	GC.removeDelayedResolutionVector(m_aEnabledCivilizationTypes);
	GC.removeDelayedResolutionVector(m_aiFreeTraitTypes);
	GC.removeDelayedResolutionVector(m_aiPrereqInCityBuildings);
	GC.removeDelayedResolutionVector(m_vPrereqNotInCityBuildings);
	GC.removeDelayedResolutionVector(m_vPrereqOrBuilding);
	GC.removeDelayedResolutionVector(m_vReplacementBuilding);
	GC.removeDelayedResolutionVector(m_aiCategories);

	m_aUnitProductionModifier.removeDelayedResolution();
}

int CvBuildingInfo::getVictoryThreshold(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumVictoryInfos(), i);
	return m_piVictoryThreshold ? m_piVictoryThreshold[i] : 0;
}

int CvBuildingInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldChange ? m_piYieldChange[i] : 0;
}

int* CvBuildingInfo::getYieldChangeArray() const
{
	return m_piYieldChange;
}

int CvBuildingInfo::getYieldPerPopChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldPerPopChange ? m_piYieldPerPopChange[i] : 0;
}

int* CvBuildingInfo::getYieldPerPopChangeArray() const
{
	return m_piYieldPerPopChange;
}

int CvBuildingInfo::getYieldModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldModifier ? m_piYieldModifier[i] : 0;
}

int* CvBuildingInfo::getYieldModifierArray() const
{
	return m_piYieldModifier;
}

int CvBuildingInfo::getPowerYieldModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piPowerYieldModifier ? m_piPowerYieldModifier[i] : 0;
}

int* CvBuildingInfo::getPowerYieldModifierArray() const
{
	return m_piPowerYieldModifier;
}

int CvBuildingInfo::getAreaYieldModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piAreaYieldModifier ? m_piAreaYieldModifier[i] : 0;
}

int* CvBuildingInfo::getAreaYieldModifierArray() const
{
	return m_piAreaYieldModifier;
}

int CvBuildingInfo::getGlobalYieldModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piGlobalYieldModifier ? m_piGlobalYieldModifier[i] : 0;
}

int* CvBuildingInfo::getGlobalYieldModifierArray() const
{
	return m_piGlobalYieldModifier;
}

int CvBuildingInfo::getRiverPlotYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piRiverPlotYieldChange ? m_piRiverPlotYieldChange[i] : 0;
}

int* CvBuildingInfo::getRiverPlotYieldChangeArray() const
{
	return m_piRiverPlotYieldChange;
}

int CvBuildingInfo::getGlobalSeaPlotYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piGlobalSeaPlotYieldChange ? m_piGlobalSeaPlotYieldChange[i] : 0;
}

int* CvBuildingInfo::getGlobalSeaPlotYieldChangeArray() const
{
	return m_piGlobalSeaPlotYieldChange;
}

int CvBuildingInfo::getCommerceChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCommerceChange ? m_piCommerceChange[i] : 0;
}

int* CvBuildingInfo::getCommerceChangeArray() const
{
	return m_piCommerceChange;
}

int CvBuildingInfo::getCommercePerPopChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCommercePerPopChange ? m_piCommercePerPopChange[i] : 0;
}

int* CvBuildingInfo::getCommercePerPopChangeArray() const
{
	return m_piCommercePerPopChange;
}

int CvBuildingInfo::getCommerceChangeDoubleTime(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	if (i == COMMERCE_CULTURE && GC.getGame().isOption(GAMEOPTION_CULTURE_EQUILIBRIUM))
		return m_piCommerceChangeDoubleTime ? m_piCommerceChangeDoubleTime[i] : 1000;
	return m_piCommerceChangeDoubleTime ? m_piCommerceChangeDoubleTime[i] : 0;
}

int CvBuildingInfo::getCommerceModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCommerceModifier ? m_piCommerceModifier[i] : 0;
}

int* CvBuildingInfo::getCommerceModifierArray() const
{
	return m_piCommerceModifier;
}

int CvBuildingInfo::getGlobalCommerceModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piGlobalCommerceModifier ? m_piGlobalCommerceModifier[i] : 0;
}

int* CvBuildingInfo::getGlobalCommerceModifierArray() const
{
	return m_piGlobalCommerceModifier;
}

int CvBuildingInfo::getSpecialistExtraCommerce(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piSpecialistExtraCommerce ? m_piSpecialistExtraCommerce[i] : 0;
}

int* CvBuildingInfo::getSpecialistExtraCommerceArray() const
{
	return m_piSpecialistExtraCommerce;
}

int CvBuildingInfo::getStateReligionCommerce(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piStateReligionCommerce ? m_piStateReligionCommerce[i] : 0;
}

int* CvBuildingInfo::getStateReligionCommerceArray() const
{
	return m_piStateReligionCommerce;
}

int CvBuildingInfo::getCommerceHappiness(int i) const
{
	FASSERT_BOUNDS(NO_COMMERCE, NUM_COMMERCE_TYPES, i);

	if (i == NO_COMMERCE)
	{
		return m_piCommerceHappiness ? 1 : 0;
	}
	else
	{
		return m_piCommerceHappiness ? m_piCommerceHappiness[i] : 0;
	}
}

int CvBuildingInfo::getSpecialistCount(int i) const
{
	FASSERT_BOUNDS(NO_SPECIALIST, GC.getNumSpecialistInfos(), i);

	if (i == NO_SPECIALIST)
	{
		return m_piSpecialistCount ? 1 : 0;
	}
	return m_piSpecialistCount ? m_piSpecialistCount[i] : 0;
}

int CvBuildingInfo::getFreeSpecialistCount(int i) const
{
	FASSERT_BOUNDS(NO_SPECIALIST, GC.getNumSpecialistInfos(), i);

	if (i == NO_SPECIALIST)
	{
		return m_piFreeSpecialistCount ? 1 : 0;
	}
	return m_piFreeSpecialistCount ? m_piFreeSpecialistCount[i] : 0;
}

int CvBuildingInfo::getBonusProductionModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_piBonusProductionModifier ? m_piBonusProductionModifier[i] : 0;
}

int CvBuildingInfo::getDomainFreeExperience(int i) const
{
	FASSERT_BOUNDS(NO_DOMAIN, NUM_DOMAIN_TYPES, i);

	if (i == NO_DOMAIN)
	{
		return m_piDomainFreeExperience ? 1 : 0;
	}
	return m_piDomainFreeExperience ? m_piDomainFreeExperience[i] : 0;
}

int CvBuildingInfo::getDomainProductionModifier(int i) const
{
	FASSERT_BOUNDS(NO_DOMAIN, NUM_DOMAIN_TYPES, i);

	if (i == NO_DOMAIN)
	{
		return m_piDomainProductionModifier ? 1 : 0;
	}
	return m_piDomainProductionModifier ? m_piDomainProductionModifier[i] : 0;
}

const std::vector<TechTypes>& CvBuildingInfo::getPrereqAndTechs() const
{
	return m_piPrereqAndTechs;
}

int CvBuildingInfo::getFlavorValue(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFlavorTypes(), i);
	return m_piFlavorValue ? m_piFlavorValue[i] : 0;
}

bool CvBuildingInfo::isCommerceFlexible(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_pbCommerceFlexible ? m_pbCommerceFlexible[i] : false;
}

int CvBuildingInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}

int CvBuildingInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}

bool CvBuildingInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}

int CvBuildingInfo::getPrereqInCityBuilding(const int i) const
{
	return m_aiPrereqInCityBuildings[i];
}

short CvBuildingInfo::getNumPrereqInCityBuildings() const
{
	return m_aiPrereqInCityBuildings.size();
}

bool CvBuildingInfo::isPrereqInCityBuilding(const int i) const
{
	return algo::any_of_equal(m_aiPrereqInCityBuildings, i);
}


int CvBuildingInfo::getPrereqNotInCityBuilding(const int i) const
{
	return m_vPrereqNotInCityBuildings[i];
}

short CvBuildingInfo::getNumPrereqNotInCityBuildings() const
{
	return m_vPrereqNotInCityBuildings.size();
}


int CvBuildingInfo::getSpecialistYieldChange(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j)

	return (m_ppaiSpecialistYieldChange && m_ppaiSpecialistYieldChange[i]) ? m_ppaiSpecialistYieldChange[i][j] : 0;
}

int* CvBuildingInfo::getSpecialistYieldChangeArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	return m_ppaiSpecialistYieldChange[i];
}

int CvBuildingInfo::getSpecialistCommerceChange(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, j);

	return (m_ppaiSpecialistCommerceChange && m_ppaiSpecialistCommerceChange[i]) ? m_ppaiSpecialistCommerceChange[i][j] : 0;
}

int* CvBuildingInfo::getSpecialistCommerceChangeArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	return m_ppaiSpecialistCommerceChange[i];
}

//Team Project (1)
int CvBuildingInfo::getLocalSpecialistYieldChange(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);

	return (m_ppaiLocalSpecialistYieldChange && m_ppaiLocalSpecialistYieldChange[i]) ? m_ppaiLocalSpecialistYieldChange[i][j] : 0;
}

int* CvBuildingInfo::getLocalSpecialistYieldChangeArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	return m_ppaiLocalSpecialistYieldChange[i];
}

int CvBuildingInfo::getLocalSpecialistCommerceChange(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, j);

	return (m_ppaiLocalSpecialistCommerceChange && m_ppaiLocalSpecialistCommerceChange[i]) ? m_ppaiLocalSpecialistCommerceChange[i][j] : 0;
}

int* CvBuildingInfo::getLocalSpecialistCommerceChangeArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), i);
	return m_ppaiLocalSpecialistCommerceChange[i];
}

int CvBuildingInfo::getBonusYieldModifier(int i, int j) const
{
	FASSERT_BOUNDS(NO_BONUS, GC.getNumBonusInfos(), i);

	if (i == NO_BONUS)
	{
		return m_ppaiBonusYieldModifier ? 1 : 0;
	}
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return (m_ppaiBonusYieldModifier && m_ppaiBonusYieldModifier[i]) ? m_ppaiBonusYieldModifier[i][j] : 0;
}

int* CvBuildingInfo::getBonusYieldModifierArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_ppaiBonusYieldModifier == NULL ? NULL : m_ppaiBonusYieldModifier[i];
}

int CvBuildingInfo::getGlobalBuildingCommerceChange(BuildingTypes eBuilding, CommerceTypes eCommerce) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const BuildingCommerce& pair, m_aGlobalBuildingCommerceChanges)
	{
		if (pair.first == eBuilding)
		{
			return pair.second[eCommerce];
		}
	}
	return 0;
}

const python::list CvBuildingInfo::cyGetGlobalBuildingCommerceChanges() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();

	foreach_(const BuildingCommerce& pair, m_aGlobalBuildingCommerceChanges)
	{
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
		{
			const int iValue = pair.second[i];
			if (iValue != 0)
				pyList.append(BuildingCommerceChange(pair.first, (CommerceTypes)i, iValue));
		}
	}
	return pyList;
}

const python::list CvBuildingInfo::cyGetTechYieldChanges100() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();

	foreach_(const TechArray& pChange, m_techYieldChanges)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			const int iValue = pChange.second[i];
			if (iValue != 0)
				pyList.append(TechYieldChange(pChange.first, (YieldTypes)i, iValue));
		}
	}
	return pyList;
}

const python::list CvBuildingInfo::cyGetTechYieldModifiers() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();

	foreach_(const TechArray& pair, m_techYieldModifiers)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			const int iValue = pair.second[i];
			if (iValue != 0)
				pyList.append(TechYieldChange(pair.first, (YieldTypes)i, iValue));
		}
	}
	return pyList;
}

const python::list CvBuildingInfo::cyGetTechCommerceChanges100() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();

	foreach_(const TechCommerceArray& pChange, m_techCommerceChanges)
	{
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
		{
			const int iValue = pChange.second[i];
			if (iValue != 0)
				pyList.append(TechCommerceChange(pChange.first, (CommerceTypes)i, iValue));
		}
	}
	return pyList;
}

const python::list CvBuildingInfo::cyGetTechCommerceModifiers() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();

	foreach_(const TechCommerceArray& pChange, m_techCommerceModifiers)
	{
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
		{
			const int iValue = pChange.second[i];
			if (iValue != 0)
				pyList.append(TechCommerceChange(pChange.first, (CommerceTypes)i, iValue));
		}
	}
	return pyList;
}


const python::list CvBuildingInfo::cyGetTerrainYieldChanges() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();

	foreach_(const TerrainArray& pChange, m_aTerrainYieldChanges)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			const int iValue = pChange.second[i];
			if (iValue != 0)
				pyList.append(TerrainYieldChange(pChange.first, (YieldTypes)i, iValue));
		}
	}
	return pyList;
}


const python::list CvBuildingInfo::cyGetPlotYieldChanges() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();

	foreach_(const PlotArray& pChange, m_aPlotYieldChanges)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			const int iValue = pChange.second[i];
			if (iValue != 0)
				pyList.append(GenericTrippleInt((int)pChange.first, i, iValue));
		}
	}
	return pyList;
}

const python::list CvBuildingInfo::cyGetImprovementYieldChanges() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();

	foreach_(const ImprovementArray& pChange, m_aImprovementYieldChanges)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			const int iValue = pChange.second[i];
			if (iValue != 0)
				pyList.append(GenericTrippleInt((int)pChange.first, i, iValue));
		}
	}
	return pyList;
}

const python::list CvBuildingInfo::cyGetGlobalImprovementYieldChanges() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();

	foreach_(const ImprovementArray& pChange, m_aGlobalImprovementYieldChanges)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
		{
			const int iValue = pChange.second[i];
			if (iValue != 0)
				pyList.append(GenericTrippleInt((int)pChange.first, i, iValue));
		}
	}
	return pyList;
}

const python::list CvBuildingInfo::cyGetFreePromoTypes() const
{
	PROFILE_EXTRA_FUNC();
	python::list pyList = python::list();
	foreach_(const FreePromoTypes& pChange, m_aFreePromoTypes)
		pyList.append(pChange);
	return pyList;
}

const char* CvBuildingInfo::getButton() const
{
	const CvArtInfoBuilding* pBuildingArtInfo = getArtInfo();
	return pBuildingArtInfo ? pBuildingArtInfo->getButton() : NULL;
}

const CvArtInfoBuilding* CvBuildingInfo::getArtInfo() const
{
	return ARTFILEMGR.getBuildingArtInfo(getArtDefineTag());
}

const CvArtInfoMovie* CvBuildingInfo::getMovieInfo() const
{
	const char* pcTag = getMovieDefineTag();
	if (NULL != pcTag && 0 != _tcscmp(pcTag, "") && 0 != _tcscmp(pcTag, "NONE"))
	{
		return ARTFILEMGR.getMovieArtInfo(pcTag);
	}
	else
	{
		return NULL;
	}
}

const char* CvBuildingInfo::getMovie() const
{
	const CvArtInfoMovie* pArt = getMovieInfo();
	return pArt ? pArt->getPath() : NULL;
}

int CvBuildingInfo::getNoEntryDefenseLevel() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_REALISTIC_SIEGE))
	{
		return 0;
	}
	return m_iNoEntryDefenseLevel;
}


bool CvBuildingInfo::isPrereqOrCivics(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCivicInfos(), i);
	return algo::any_of_equal(m_aePrereqOrCivics, static_cast<CivicTypes>(i));
}

bool CvBuildingInfo::isPrereqAndCivics(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCivicInfos(), i);
	return algo::any_of_equal(m_aePrereqAndCivics, static_cast<CivicTypes>(i));
}

bool CvBuildingInfo::isPrereqOrTerrain(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return algo::any_of_equal(m_aePrereqOrTerrain, static_cast<TerrainTypes>(i));
}

bool CvBuildingInfo::isPrereqAndTerrain(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return algo::any_of_equal(m_aePrereqAndTerrain, static_cast<TerrainTypes>(i));
}

bool CvBuildingInfo::isPrereqOrFeature(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return algo::any_of_equal(m_aePrereqOrFeature, static_cast<FeatureTypes>(i));
}

int CvBuildingInfo::getBonusDefenseChanges(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_piBonusDefenseChanges ? m_piBonusDefenseChanges[i] : 0;
}

int CvBuildingInfo::getBonusCommerceModifier(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, j);
	return (m_ppaiBonusCommerceModifier && m_ppaiBonusCommerceModifier[i]) ? m_ppaiBonusCommerceModifier[i][j] : 0;
}

int* CvBuildingInfo::getBonusCommerceModifierArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_ppaiBonusCommerceModifier[i];
}

int CvBuildingInfo::getBonusYieldChanges(int i, int j) const
{
	FASSERT_BOUNDS(NO_BONUS, GC.getNumBonusInfos(), i);

	if (i == NO_BONUS)
	{
		return (m_ppaiBonusYieldChanges ? 1 : 0);
	}
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return (m_ppaiBonusYieldChanges && m_ppaiBonusYieldChanges[i]) ? m_ppaiBonusYieldChanges[i][j] : 0;
}

int* CvBuildingInfo::getBonusYieldChangesArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_ppaiBonusYieldChanges[i];
}

int CvBuildingInfo::getBonusCommercePercentChanges(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, j);
	return (m_ppaiBonusCommercePercentChanges && m_ppaiBonusCommercePercentChanges[i]) ? m_ppaiBonusCommercePercentChanges[i][j] : 0;
}

int* CvBuildingInfo::getBonusCommercePercentChangesArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_ppaiBonusCommercePercentChanges[i];
}

int CvBuildingInfo::getVicinityBonusYieldChanges(int i, int j) const
{
	FASSERT_BOUNDS(NO_BONUS, GC.getNumBonusInfos(), i);

	if (i == NO_BONUS)
	{
		return m_ppaiVicinityBonusYieldChanges ? 1 : 0;
	}
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return (m_ppaiVicinityBonusYieldChanges && m_ppaiVicinityBonusYieldChanges[i]) ? m_ppaiVicinityBonusYieldChanges[i][j] : 0;
}

int* CvBuildingInfo::getVicinityBonusYieldChangesArray(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_ppaiVicinityBonusYieldChanges[i];
}

int CvBuildingInfo::getTechSpecialistChange(int i, int j) const
{
	FASSERT_BOUNDS(NO_TECH, GC.getNumTechInfos(), i);

	if (i == NO_TECH)
	{
		return (m_ppaiTechSpecialistChange ? 1 : 0);
	}
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), j);
	return (m_ppaiTechSpecialistChange && m_ppaiTechSpecialistChange[i]) ? m_ppaiTechSpecialistChange[i][j] : 0;
}

int* CvBuildingInfo::getTechSpecialistChangeArray(int i) const
{
	return m_ppaiTechSpecialistChange[i];
}


int CvBuildingInfo::getImprovementYieldChanges(int i, int j) const
{
	FASSERT_BOUNDS(0, GC.getNumImprovementInfos(), i);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, j);
	return (m_ppiImprovementYieldChanges && m_ppiImprovementYieldChanges[i]) ? m_ppiImprovementYieldChanges[i][j] : 0;
}

//TB Combat Mods (Buildings) begin

UnitTypes CvBuildingInfo::getPropertySpawnUnit() const
{
	return m_ePropertySpawnUnit;
}

PropertyTypes CvBuildingInfo::getPropertySpawnProperty() const
{
	return m_ePropertySpawnProperty;
}

PromotionLineTypes CvBuildingInfo::getPromotionLineType() const
{
	return m_ePromotionLineType;
}
//integers
int CvBuildingInfo::getLinePriority() const
{
	return m_iLinePriority;
}

int CvBuildingInfo::getNationalCaptureProbabilityModifier() const
{
	return m_iNationalCaptureProbabilityModifier;
}

int CvBuildingInfo::getNationalCaptureResistanceModifier() const
{
	return m_iNationalCaptureResistanceModifier;
}

int CvBuildingInfo::getLocalCaptureProbabilityModifier() const
{
	return m_iLocalCaptureProbabilityModifier;
}

int CvBuildingInfo::getLocalCaptureResistanceModifier() const
{
	return m_iLocalCaptureResistanceModifier;
}

int CvBuildingInfo::getLocalDynamicDefense() const
{
	return m_iLocalDynamicDefense;
}

int CvBuildingInfo::getRiverDefensePenalty() const
{
	return m_iRiverDefensePenalty;
}

int CvBuildingInfo::getMinDefense() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_REALISTIC_SIEGE))
	{
		return 0;
	}
	return m_iMinDefense;
}

int CvBuildingInfo::getBuildingDefenseRecoverySpeedModifier() const
{
	return m_iBuildingDefenseRecoverySpeedModifier;
}

int CvBuildingInfo::getCityDefenseRecoverySpeedModifier() const
{
	return m_iCityDefenseRecoverySpeedModifier;
}

int CvBuildingInfo::getDamageAttackerChance() const
{
	return m_iDamageAttackerChance;
}

int CvBuildingInfo::getDamageToAttacker() const
{
	return m_iDamageToAttacker;
}

int CvBuildingInfo::getMaxPopulationAllowed() const
{
	if (!GC.getGame().isOption(GAMEOPTION_EXP_MAXIMUM_POPULATION))
	{
		return -1;
	}
	return m_iMaxPopulationAllowed;
}

int CvBuildingInfo::getMaxPopulationChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_EXP_MAXIMUM_POPULATION))
	{
		return 0;
	}
	return m_iMaxPopulationChange;
}

int CvBuildingInfo::getInsidiousness() const
{
	return m_iInsidiousness;
}

int CvBuildingInfo::getInvestigation() const
{
	return m_iInvestigation;
}

int CvBuildingInfo::getPopulationChange() const
{
	return m_iPopulationChange;
}

//Booleans
bool CvBuildingInfo::isDamageAllAttackers() const
{
	return m_bDamageAllAttackers;
}

bool CvBuildingInfo::isDamageAttackerCapable() const
{
	return m_bDamageAttackerCapable;
}

bool CvBuildingInfo::isQuarantine() const
{
	return m_bQuarantine;
}

int CvBuildingInfo::getNumHealUnitCombatTypes() const
{
	return (int)m_aHealUnitCombatTypes.size();
}

const HealUnitCombat& CvBuildingInfo::getHealUnitCombatType(int iUnitCombat) const
{
	FASSERT_BOUNDS(0, getNumHealUnitCombatTypes(), iUnitCombat);
	return m_aHealUnitCombatTypes[iUnitCombat];
}

int CvBuildingInfo::getNumBonusAidModifiers() const
{
	return (int)m_aBonusAidModifiers.size();
}

const BonusAidModifiers& CvBuildingInfo::getBonusAidModifier(int iIndex) const
{
	return m_aBonusAidModifiers[iIndex];
}

int CvBuildingInfo::getNumAidRateChanges() const
{
	return (int)m_aAidRateChanges.size();
}

const AidRateChanges& CvBuildingInfo::getAidRateChange(int iIndex) const
{
	return m_aAidRateChanges[iIndex];
}

int CvBuildingInfo::getNumEnabledCivilizationTypes() const
{
	return (int)m_aEnabledCivilizationTypes.size();
}

const EnabledCivilizations& CvBuildingInfo::getEnabledCivilizationType(int iIndex) const
{
	return m_aEnabledCivilizationTypes[iIndex];
}


int CvBuildingInfo::getUnitCombatRetrainType(int i) const
{
	return m_aiUnitCombatRetrainTypes[i];
}

int CvBuildingInfo::getNumUnitCombatRetrainTypes() const
{
	return (int)m_aiUnitCombatRetrainTypes.size();
}

bool CvBuildingInfo::isUnitCombatRetrainType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiUnitCombatRetrainTypes, i);
}

int CvBuildingInfo::getMayDamageAttackingUnitCombatType(int i) const
{
	return m_aiMayDamageAttackingUnitCombatTypes[i];
}

int CvBuildingInfo::getNumMayDamageAttackingUnitCombatTypes() const
{
	return (int)m_aiMayDamageAttackingUnitCombatTypes.size();
}

bool CvBuildingInfo::isMayDamageAttackingUnitCombatType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiMayDamageAttackingUnitCombatTypes, i);
}

int CvBuildingInfo::getNumUnitCombatDefenseAgainstModifiers() const
{
	return m_aUnitCombatDefenseAgainstModifiers.size();
}

int CvBuildingInfo::getUnitCombatDefenseAgainstModifier(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aUnitCombatDefenseAgainstModifiers.begin(); it != m_aUnitCombatDefenseAgainstModifiers.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}

	return 0;
}

int CvBuildingInfo::getNumUnitCombatProdModifiers() const
{
	return m_aUnitCombatProdModifiers.size();
}

int CvBuildingInfo::getUnitCombatProdModifier(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aUnitCombatProdModifiers.begin(); it != m_aUnitCombatProdModifiers.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}

	return 0;
}

int CvBuildingInfo::getTechHappiness(TechTypes eTech) const
{
	return m_aTechHappinessChanges.getValue(eTech);
}

int CvBuildingInfo::getTechHealth(TechTypes eTech) const
{
	return m_aTechHealthChanges.getValue(eTech);
}

bool CvBuildingInfo::isHurry(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumHurryInfos(), i);
	return m_pabHurry ? m_pabHurry[i] : false;
}
//TB Combat Mods (Buildings) end


int CvBuildingInfo::getMaxPopAllowed() const
{
	return m_iMaxPopAllowed;
}


bool CvBuildingInfo::EnablesOtherBuildings() const
{
	return m_bEnablesOtherBuildings;
}


namespace CvBuildingInternal
{
	bool calculateEnablesOtherBuildings(const CvBuildingInfo& kBuilding, BuildingTypes eBuilding)
	{
		PROFILE_EXTRA_FUNC();
		// add the building and its bonuses to the query to see if they influence the construct condition of a building
		std::vector<GOMQuery> queries;
		GOMQuery query;
		query.GOM = GOM_BUILDING;
		query.id = eBuilding;
		queries.push_back(query);
		query.GOM = GOM_BONUS;
		foreach_(const BonusModifier& kFreeBonus, kBuilding.getFreeBonuses())
		{
			query.id = kFreeBonus.first;
			queries.push_back(query);
		}

		foreach_(const CvBuildingInfo* loopBuilding, GC.getBuildingInfos())
		{
			if (loopBuilding->isPrereqInCityBuilding(eBuilding)
			||  loopBuilding->isPrereqOrBuilding(eBuilding))
			{
				return true;
			}
		}

		foreach_(const BonusTypes eFreeBonus, kBuilding.getFreeBonuses() | map_keys)
		{
			foreach_(const CvBuildingInfo* loopBuilding, GC.getBuildingInfos())
			{
				if (loopBuilding->getPrereqAndBonus() == eFreeBonus
				|| algo::any_of_equal(loopBuilding->getPrereqOrBonuses(), eFreeBonus))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool calculateEnablesUnits(const CvBuildingInfo& kBuilding, BuildingTypes eBuilding)
	{
		PROFILE_EXTRA_FUNC();
		// add the building and its bonuses to the query to see if they influence the construct condition of a building
		std::vector<GOMQuery> queries;
		GOMQuery query;
		query.GOM = GOM_BUILDING;
		query.id = eBuilding;
		queries.push_back(query);
		query.GOM = GOM_BONUS;
		foreach_(const BonusModifier& kFreeBonus, kBuilding.getFreeBonuses())
		{
			query.id = kFreeBonus.first;
			queries.push_back(query);
		}

		for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
		{
			const CvUnitInfo& kUnit = GC.getUnitInfo((UnitTypes)iI);

			const BoolExpr* condition = kUnit.getTrainCondition();
			if (condition != NULL && condition->getInvolvesGOM(queries))
			{
				return true;
			}

			if (kUnit.isPrereqAndBuilding(eBuilding))
			{
				return true;
			}

			for (int iK = 0; iK < kUnit.getPrereqOrBuildingsNum(); iK++)
			{
				if (kUnit.getPrereqOrBuilding((BuildingTypes)iK) == eBuilding)
				{
					return true;
				}
			}

			if (kBuilding.getFreeBonuses().hasValue((BonusTypes)kUnit.getPrereqAndBonus()))
			{
				return true;
			}

			foreach_(const BonusTypes eBonus, kUnit.getPrereqOrBonuses())
			{
				if (kBuilding.getFreeBonuses().hasValue(eBonus))
				{
					return true;
				}
			}
		}
		return false;
	}
}


namespace
{
	// Transitive set of an improvement plus everything it upgrades into (ImprovementUpgrade +
	// AlternativeImprovementUpgradeTypes), guarded against cycles. Walks downstream only.
	void collectImprovementUpgradeClosure(ImprovementTypes eImprovement, std::vector<ImprovementTypes>& aChain)
	{
		if (eImprovement == NO_IMPROVEMENT
		|| std::find(aChain.begin(), aChain.end(), eImprovement) != aChain.end())
		{
			return;
		}
		aChain.push_back(eImprovement);

		const CvImprovementInfo& kInfo = GC.getImprovementInfo(eImprovement);
		collectImprovementUpgradeClosure(kInfo.getImprovementUpgrade(), aChain);
		for (int i = 0; i < kInfo.getNumAlternativeImprovementUpgradeTypes(); ++i)
		{
			collectImprovementUpgradeClosure((ImprovementTypes)kInfo.getAlternativeImprovementUpgradeType(i), aChain);
		}
	}

	// A building bonus listed for a base improvement should follow that tile as it upgrades.
	// Fold each base entry's yields onto every downstream improvement in its upgrade chain, so
	// the city/player accumulation, AI valuation and help text all see the full set unchanged.
	void expandImprovementYieldsAlongUpgradeChain(IDValueMap<ImprovementTypes, YieldArray>& kMap)
	{
		const std::vector<std::pair<ImprovementTypes, YieldArray> > aBase(kMap.begin(), kMap.end());
		for (std::vector<std::pair<ImprovementTypes, YieldArray> >::const_iterator it = aBase.begin(), itEnd = aBase.end(); it != itEnd; ++it)
		{
			std::vector<ImprovementTypes> aChain;
			collectImprovementUpgradeClosure(it->first, aChain);
			for (std::vector<ImprovementTypes>::const_iterator itc = aChain.begin(), itcEnd = aChain.end(); itc != itcEnd; ++itc)
			{
				if (*itc != it->first)
				{
					kMap.addArrayValue(*itc, it->second);
				}
			}
		}
	}
}

// #195 Phase 2: aggregate the scattered GOM-expressible typed Prereq* fields into one
// introspectable requirement list. Read-only description; canConstruct still evaluates.
// Non-GOM prereqs (population, culture level, properties, war/power) and the vicinity-
// bonus / state-religion variants keep their bespoke handling and are intentionally not
// modelled here yet -- see the plan doc for the staged coverage.
void CvBuildingInfo::buildConstructRequirements()
{
	PROFILE_EXTRA_FUNC();
	m_constructRequirements.clear();

	// --- GOM_BUILDING ---
	{
		ConstructRequirement req(GOM_BUILDING, REQOP_REQUIRE_ALL);
		for (int i = 0, n = getNumPrereqInCityBuildings(); i < n; i++)
		{
			req.aiIds.push_back(getPrereqInCityBuilding(i));
		}
		if (!req.aiIds.empty()) m_constructRequirements.push_back(req);
	}
	{
		ConstructRequirement req(GOM_BUILDING, REQOP_REQUIRE_ANY);
		for (int i = 0, n = getNumPrereqOrBuilding(); i < n; i++)
		{
			req.aiIds.push_back(getPrereqOrBuilding(i));
		}
		if (!req.aiIds.empty()) m_constructRequirements.push_back(req);
	}
	{
		ConstructRequirement req(GOM_BUILDING, REQOP_FORBID);
		for (int i = 0, n = getNumPrereqNotInCityBuildings(); i < n; i++)
		{
			req.aiIds.push_back(getPrereqNotInCityBuilding(i));
		}
		if (!req.aiIds.empty()) m_constructRequirements.push_back(req);
	}
	foreach_(const BuildingModifier2& kPair, getPrereqNumOfBuildings())
	{
		m_constructRequirements.push_back(ConstructRequirement(GOM_BUILDING, REQOP_REQUIRE_COUNT, kPair.first, kPair.second));
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
		if (!req.aiIds.empty()) m_constructRequirements.push_back(req);
	}

	// --- GOM_BONUS ---
	if (getPrereqAndBonus() != NO_BONUS)
	{
		m_constructRequirements.push_back(ConstructRequirement(GOM_BONUS, REQOP_REQUIRE_ALL, getPrereqAndBonus()));
	}
	{
		ConstructRequirement req(GOM_BONUS, REQOP_REQUIRE_ANY);
		foreach_(const BonusTypes eBonus, getPrereqOrBonuses())
		{
			req.aiIds.push_back(eBonus);
		}
		if (!req.aiIds.empty()) m_constructRequirements.push_back(req);
	}

	// --- GOM_RELIGION (city must have this religion) ---
	if (getPrereqReligion() != NO_RELIGION)
	{
		m_constructRequirements.push_back(ConstructRequirement(GOM_RELIGION, REQOP_REQUIRE_ALL, getPrereqReligion()));
	}

	// --- GOM_CORPORATION ---
	if (getPrereqCorporation() != NO_CORPORATION)
	{
		m_constructRequirements.push_back(ConstructRequirement(GOM_CORPORATION, REQOP_REQUIRE_ALL, getPrereqCorporation()));
	}

	// --- GOM_CIVIC (per-civic AND / OR membership) ---
	{
		ConstructRequirement reqAnd(GOM_CIVIC, REQOP_REQUIRE_ALL);
		ConstructRequirement reqOr(GOM_CIVIC, REQOP_REQUIRE_ANY);
		for (int i = 0, n = GC.getNumCivicInfos(); i < n; i++)
		{
			if (isPrereqAndCivics(i)) reqAnd.aiIds.push_back(i);
			if (isPrereqOrCivics(i)) reqOr.aiIds.push_back(i);
		}
		if (!reqAnd.aiIds.empty()) m_constructRequirements.push_back(reqAnd);
		if (!reqOr.aiIds.empty()) m_constructRequirements.push_back(reqOr);
	}

	// --- GOM_OPTION (game option must be enabled) ---
	if (getPrereqGameOption() != -1)
	{
		m_constructRequirements.push_back(ConstructRequirement(GOM_OPTION, REQOP_REQUIRE_ALL, getPrereqGameOption()));
	}

	// --- GOM_TERRAIN (per-terrain AND / OR membership) ---
	{
		ConstructRequirement reqAnd(GOM_TERRAIN, REQOP_REQUIRE_ALL);
		ConstructRequirement reqOr(GOM_TERRAIN, REQOP_REQUIRE_ANY);
		for (int i = 0, n = GC.getNumTerrainInfos(); i < n; i++)
		{
			if (isPrereqAndTerrain(i)) reqAnd.aiIds.push_back(i);
			if (isPrereqOrTerrain(i)) reqOr.aiIds.push_back(i);
		}
		if (!reqAnd.aiIds.empty()) m_constructRequirements.push_back(reqAnd);
		if (!reqOr.aiIds.empty()) m_constructRequirements.push_back(reqOr);
	}

	// --- GOM_FEATURE (per-feature OR membership) ---
	{
		ConstructRequirement reqOr(GOM_FEATURE, REQOP_REQUIRE_ANY);
		for (int i = 0, n = GC.getNumFeatureInfos(); i < n; i++)
		{
			if (isPrereqOrFeature(i)) reqOr.aiIds.push_back(i);
		}
		if (!reqOr.aiIds.empty()) m_constructRequirements.push_back(reqOr);
	}

	// --- GOM_IMPROVEMENT (any listed improvement in the vicinity) ---
	{
		ConstructRequirement req(GOM_IMPROVEMENT, REQOP_REQUIRE_ANY);
		foreach_(const ImprovementTypes eImp, getPrereqOrImprovements())
		{
			req.aiIds.push_back(eImp);
		}
		if (!req.aiIds.empty()) m_constructRequirements.push_back(req);
	}

	// --- GOM_HERITAGE (any listed heritage) ---
	{
		ConstructRequirement req(GOM_HERITAGE, REQOP_REQUIRE_ANY);
		foreach_(const HeritageTypes eHeritage, getPrereqOrHeritage())
		{
			req.aiIds.push_back(eHeritage);
		}
		if (!req.aiIds.empty()) m_constructRequirements.push_back(req);
	}

	// Deliberately NOT modelled yet (bespoke semantics, no consumer needs them):
	// vicinity / raw-vicinity bonus prereqs (in-vicinity, not in-city), state-religion
	// (player-level), and the non-GOM prereqs (population, culture level, properties,
	// war/power). These keep their existing typed handling in canConstruct.
}

//	Derive the list of bonuses this building's effect tables reference, by probing every
//	bonus type against the same getters the consumption math reads (happiness/health/
//	defense changes, per-yield changes+modifiers, per-commerce modifiers). Brute force is
//	fine here: it runs once at load; afterwards per-city consumption visits only these few
//	bonuses instead of scanning all bonus types per built building every turn.
void CvBuildingInfo::buildConsumptionRelevantBonuses()
{
	PROFILE_EXTRA_FUNC();
	m_consumptionRelevantBonuses.clear();

	for (int iBonus = 0; iBonus < GC.getNumBonusInfos(); iBonus++)
	{
		const BonusTypes eBonus = static_cast<BonusTypes>(iBonus);

		bool bTouches =
			   getBonusHappinessChanges().getValue(eBonus) != 0
			|| getBonusHealthChanges().getValue(eBonus) != 0
			|| getBonusDefenseChanges(eBonus) != 0;

		for (int iJ = 0; !bTouches && iJ < NUM_YIELD_TYPES; iJ++)
		{
			bTouches = getBonusYieldChanges(eBonus, iJ) != 0 || getBonusYieldModifier(eBonus, iJ) != 0;
		}
		for (int iJ = 0; !bTouches && iJ < NUM_COMMERCE_TYPES; iJ++)
		{
			bTouches = getBonusCommerceModifier(eBonus, iJ) != 0;
		}

		if (bTouches)
		{
			m_consumptionRelevantBonuses.push_back(eBonus);
		}
	}
}

void CvBuildingInfo::doPostLoadCaching(uint32_t iThis)
{
	PROFILE_EXTRA_FUNC();

	// Make improvement-yield bonuses follow their target tile through its upgrade chain.
	expandImprovementYieldsAlongUpgradeChain(m_aImprovementYieldChanges);
	expandImprovementYieldsAlongUpgradeChain(m_aGlobalImprovementYieldChanges);

	int iCount = getNumReplacementBuilding();
	if (iCount > 0)
	{
		// Toffer - Prune self reference, to make the code XML idiot proof.
		//	A building was once set to replace itself, it caused an infinite loop in the canBuild logic used for MODDEROPTION_HIDE_REPLACED_BUILDINGS.
		//	Instead of doing a self reference check in all loops for this vector I thought it more clean to just prune it here.
		const int iId = GC.getInfoTypeForString(getType());

		std::vector<int>::iterator itr = find(m_vReplacementBuilding.begin(), m_vReplacementBuilding.end(), iId);
		while (itr != m_vReplacementBuilding.end())
		{
			FErrorMsg(CvString::format("%s is set to replace itself!!", getType()).c_str())
			m_vReplacementBuilding.erase(itr);
			iCount--;
			itr = find(m_vReplacementBuilding.begin(), m_vReplacementBuilding.end(), iId);
		}

		for (int i = 0; i < iCount; i++)
		{
			GC.getBuildingInfo((BuildingTypes)getReplacementBuilding(i)).setReplacedBuilding(iId);
		}
	}
	m_bEnablesOtherBuildings = CvBuildingInternal::calculateEnablesOtherBuildings(*this, (BuildingTypes)iThis);
	m_bEnablesUnits = CvBuildingInternal::calculateEnablesUnits(*this, (BuildingTypes)iThis);

	buildConstructRequirements();
	buildConsumptionRelevantBonuses();

	if (getHolyCity() != NO_RELIGION)
	{
		GC.getReligionInfo((ReligionTypes)getReligionType()).addShrineBuilding((BuildingTypes)iThis);
	}
}

void CvBuildingInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	// Declared fields fold in here, in getDataMembers declaration order. The explicit
	// remainder below covers the hand-written loader fields plus the runtime/derived
	// fields the legacy checksum always included (m_iMissionType, m_bDamageAttackerCapable,
	// m_vReplacedBuilding, the readPass3 targets). Same field set as before the
	// declarative migration; fold order changed (one-time recalc prompt on old saves).
	CvInfoUtil(this).checkSum(iSum);

	CheckSum(iSum, m_iMissionType); // runtime, set via setMissionType

	CheckSumI(iSum, GC.getNumSpecialistInfos(), m_piSpecialistCount);
	CheckSumI(iSum, GC.getNumSpecialistInfos(), m_piFreeSpecialistCount);
	CheckSumI(iSum, GC.getNumBonusInfos(), m_piBonusProductionModifier);
	CheckSumI(iSum, NUM_DOMAIN_TYPES, m_piDomainFreeExperience);
	CheckSumI(iSum, NUM_DOMAIN_TYPES, m_piDomainProductionModifier);
	CheckSumI(iSum, GC.getNumFlavorTypes(), m_piFlavorValue);

	CheckSumI(iSum, NUM_COMMERCE_TYPES, m_pbCommerceFlexible);

	if (m_ppaiSpecialistYieldChange)
	{
		for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppaiSpecialistYieldChange[i]);
		}
	}
	if (m_ppaiSpecialistCommerceChange)
	{
		for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
		{
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppaiSpecialistCommerceChange[i]);
		}
	}

	if (m_ppaiLocalSpecialistYieldChange)
	{
		for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppaiLocalSpecialistYieldChange[i]);
		}
	}

	if (m_ppaiLocalSpecialistCommerceChange)
	{
		for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
		{
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppaiLocalSpecialistCommerceChange[i]);
		}
	}

	if (m_ppaiBonusYieldModifier)
	{
		for(int i = 0; i < GC.getNumBonusInfos(); i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppaiBonusYieldModifier[i]);
		}
	}

	CheckSumC(iSum, m_aePrereqOrCivics);
	CheckSumC(iSum, m_aePrereqAndCivics);

	CheckSumC(iSum, m_aUnitProductionModifier);
	CheckSumI(iSum, GC.getNumBonusInfos(), m_piBonusDefenseChanges);

	for(int i = 0; i < GC.getNumTechInfos(); i++)
	{
		if (m_ppaiTechSpecialistChange)
			CheckSumI(iSum, GC.getNumSpecialistInfos(), m_ppaiTechSpecialistChange[i]);
	}

	for(int i = 0; i < GC.getNumBonusInfos(); i++)
	{
		if (m_ppaiBonusCommerceModifier)
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppaiBonusCommerceModifier[i]);
		if (m_ppaiBonusCommercePercentChanges)
			CheckSumI(iSum, NUM_COMMERCE_TYPES, m_ppaiBonusCommercePercentChanges[i]);
		if (m_ppaiBonusYieldChanges)
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppaiBonusYieldChanges[i]);
		if (m_ppaiVicinityBonusYieldChanges)
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppaiVicinityBonusYieldChanges[i]);
	}

	if (m_ppiImprovementYieldChanges)
	{
		for (int i = 0; i < GC.getNumImprovementInfos(); i++)
		{
			CheckSumI(iSum, NUM_YIELD_TYPES, m_ppiImprovementYieldChanges[i]);
		}
	}

	m_Properties.getCheckSum(iSum);
	m_PropertiesAllCities.getCheckSum(iSum);
	m_PrereqMinProperties.getCheckSum(iSum);
	m_PrereqMaxProperties.getCheckSum(iSum);
	m_PrereqPlayerMinProperties.getCheckSum(iSum);
	m_PrereqPlayerMaxProperties.getCheckSum(iSum);

	//TB Combat Mods (Buildings) begin
	CheckSum(iSum, m_ePropertySpawnUnit); // readPass3
	CheckSum(iSum, m_bDamageAttackerCapable); // derived from bDamageAllAttackers / may-damage list

	//Structs
	foreach_(const FreePromoTypes& freePromotion, m_aFreePromoTypes)
	{
		CheckSum(iSum, freePromotion.ePromotion);
		if (freePromotion.m_pExprFreePromotionCondition)
			freePromotion.m_pExprFreePromotionCondition->getCheckSum(iSum);
	}

	int iNumElements = m_aHealUnitCombatTypes.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aHealUnitCombatTypes[i].eUnitCombat);
		CheckSum(iSum, m_aHealUnitCombatTypes[i].iHeal);
	}

	iNumElements = m_aBonusAidModifiers.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aBonusAidModifiers[i].eBonusType);
		CheckSum(iSum, m_aBonusAidModifiers[i].ePropertyType);
		CheckSum(iSum, m_aBonusAidModifiers[i].iModifier);
	}

	iNumElements = m_aAidRateChanges.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aAidRateChanges[i].ePropertyType);
		CheckSum(iSum, m_aAidRateChanges[i].iChange);
	}

	iNumElements = m_aEnabledCivilizationTypes.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aEnabledCivilizationTypes[i].eCivilization);
	}

	CheckSumC(iSum, m_aiMayDamageAttackingUnitCombatTypes);
	CheckSumC(iSum, m_aUnitCombatDefenseAgainstModifiers);
	CheckSumC(iSum, m_aUnitCombatProdModifiers);
	CheckSumC(iSum, m_aiFreeTraitTypes);
	CheckSumC(iSum, m_aiCategories);
	CheckSumC(iSum, m_aiPrereqInCityBuildings);
	CheckSumC(iSum, m_vPrereqNotInCityBuildings);
	CheckSumC(iSum, m_vPrereqOrBuilding);
	CheckSumC(iSum, m_vReplacementBuilding);
	CheckSumC(iSum, m_vReplacedBuilding); // derived in doPostLoadCaching

	// Arrays
	CheckSumI(iSum, GC.getNumHurryInfos(), m_pabHurry);

	//if (m_pExprFreePromotionCondition)
	//	m_pExprFreePromotionCondition->getCheckSum(iSum);
	//TB Combat Mods (Buildings) end

	CheckSum(iSum, m_iGreatPeopleUnitType); // readPass3
	CheckSum(iSum, m_piVictoryThreshold, GC.getNumVictoryInfos());

	CheckSumC(iSum, m_aPlotYieldChanges);
}

void CvBuildingInfo::getDataMembers(CvInfoUtil& util)
{
	// Kept hand-written (each at its read()/copyNonDefaults/getCheckSum site):
	// - readPass3 machinery: m_iGreatPeopleUnitType ("GreatPeopleUnitType"), m_ePropertySpawnUnit
	//   ("PropertySpawnUnit") - resolved via m_aszExtraXMLforPass3.
	// - m_szArtDefineTag - setNotShowInCity() side effect must run after read AND after merge-copy.
	// - SetVariableListTagPair dynamic arrays (no wrapper): m_piSpecialistCount,
	//   m_piFreeSpecialistCount, m_piBonusProductionModifier, m_piDomainFreeExperience,
	//   m_piDomainProductionModifier, m_piFlavorValue, m_piBonusDefenseChanges, m_pabHurry,
	//   m_piVictoryThreshold; m_pbCommerceFlexible is a bool* read via SetCommerce (no bool-array
	//   wrapper).
	// - 2D int** tables (no wrapper yet, #196 deferral): Specialist/LocalSpecialist Yield+Commerce,
	//   BonusYieldModifier, BonusCommerceModifier, BonusCommercePercentChanges, BonusYieldChanges,
	//   VicinityBonusYieldChanges, TechSpecialistChange, ImprovementYieldChanges (m_ppi).
	// - Delayed-resolution vectors (no wrapper): m_aePrereqOrCivics, m_aePrereqAndCivics,
	//   m_aiFreeTraitTypes, m_aiCategories, m_aiPrereqInCityBuildings, m_vPrereqNotInCityBuildings,
	//   m_vPrereqOrBuilding, m_vReplacementBuilding.
	// - m_aUnitProductionModifier: UnitTypes loads AFTER buildings, but isDelayedResolutionRequired
	//   reads m_infoClassXmlLoadOrder, which has no entry yet for classes that have not started
	//   loading - the auto decision would pick immediate resolution and lose every entry. Must stay
	//   on readWithDelayedResolution until the order table covers unloaded classes.
	// - m_aPlotYieldChanges: PlotTypes is not an info class (no InfoClassTraits specialization), so
	//   the IDValueMap wrapper's delayed-resolution check would index the load-order table at -1.
	// - Struct-vectors (structs live in CvStructs.h without getDataMembers; EnabledCivilizations
	//   additionally needs delayed resolution, which addStruct forces off): m_aFreePromoTypes,
	//   m_aHealUnitCombatTypes, m_aBonusAidModifiers, m_aAidRateChanges, m_aEnabledCivilizationTypes.
	// - Pair-vectors via SetOptionalPairVector (no wrapper): m_aUnitCombatDefenseAgainstModifiers,
	//   m_aUnitCombatProdModifiers.
	// - m_aiMayDamageAttackingUnitCombatTypes: legacy merge-copy sets m_bDamageAttackerCapable from
	//   the copied entries - a side effect the vector wrapper cannot reproduce.
	// - CvProperties sub-objects (self-reading, no wrapper): m_Properties, m_PropertiesAllCities,
	//   m_PrereqMin/MaxProperties, m_PrereqPlayerMin/MaxProperties.
	// - Runtime/derived (not XML-backed): m_iMissionType, m_bNotShowInCity, m_bEnablesOtherBuildings,
	//   m_bEnablesUnits, m_bDamageAttackerCapable, m_vReplacedBuilding, m_constructRequirements,
	//   m_consumptionRelevantBonuses.
	util
		.addEnum(m_iObsoleteTech, L"ObsoleteTech")
		.add(m_piBonusHealthChanges, L"BonusHealthChanges")
		.add(m_aTechHappinessChanges, L"TechHappinessChanges")
		.add(m_aTechHealthChanges, L"TechHealthChanges")
		.add(m_religionChange, L"ReligionChanges")
		.add(m_prereqOrImprovement, L"PrereqOrImprovement")
		.add(m_improvementFreeSpecialists, L"ImprovementFreeSpecialists")
		.add(m_freeBonuses, L"ExtraFreeBonuses")
		.add(m_aGlobalBuildingCommerceChanges, L"GlobalBuildingExtraCommerces", L"BuildingType", L"CommerceChanges")
		.add(m_aePrereqOrTerrain, L"PrereqOrTerrain")
		.add(m_aePrereqAndTerrain, L"PrereqAndTerrain")
		.add(m_aePrereqOrFeature, L"PrereqOrFeature")
		.add(m_fVisibilityPriority, L"fVisibilityPriority")
		.add(m_PropertyManipulators)
		.addBoolExpr(m_pExprNewCityFree, L"NewCityFree")
		.addBoolExpr(m_pExprConstructCondition, L"ConstructCondition")
		.addYields(m_piYieldChange, L"YieldChanges")

		// Enum FKs (delayed/immediate resolution picked automatically; the six building
		// self-references resolve via the delayed-resolution map, as the legacy reads did)
		.addEnum(m_eSpecialBuilding, L"SpecialBuildingType")
		.addEnum(m_iFreeStartEra, L"FreeStartEra")
		.addEnum(m_eFreeSpecialTech, L"FreeSpecialTech")
		.addEnum(m_ePropertySpawnProperty, L"PropertySpawnProperty")
		.addEnum(m_ePromotionLineType, L"PromotionLineType")
		.addEnum(m_iFreeBuilding, L"FreeBuilding")
		.addEnum(m_iFreeAreaBuilding, L"FreeAreaBuilding")
		.addEnum(m_iProductionContinueBuilding, L"ProductionContinueBuilding")
		.addEnum(m_iPrereqAnyoneBuilding, L"PrereqAnyoneBuilding")
		.addEnum(m_iExtendsBuilding, L"ExtendsBuilding")
		.addEnum(m_iObsoletesToBuilding, L"ObsoletesToBuilding")

		// Legacy int members holding a type index (immediate resolution, default -1)
		.addEnumAsInt(m_iAdvisorType, L"Advisor")
		.addEnumAsInt(m_iPrereqGameOption, L"PrereqGameOption")
		.addEnumAsInt(m_iNotGameOption, L"NotGameOption")
		.addEnumAsInt(m_iHolyCity, L"HolyCity")
		.addEnumAsInt(m_iReligionType, L"ReligionType")
		.addEnumAsInt(m_iStateReligion, L"StateReligion")
		.addEnumAsInt(m_iPrereqReligion, L"PrereqReligion")
		.addEnumAsInt(m_iPrereqCorporation, L"PrereqCorporation")
		.addEnumAsInt(m_iFoundsCorporation, L"FoundsCorporation")
		.addEnumAsInt(m_iGlobalReligionCommerce, L"GlobalReligionCommerce")
		.addEnumAsInt(m_iGlobalCorporationCommerce, L"GlobalCorporationCommerce")
		.addEnumAsInt(m_iVictoryPrereq, L"VictoryPrereq")
		.addEnumAsInt(m_iMaxStartEra, L"MaxStartEra")
		.addEnumAsInt(m_iPrereqAndTech, L"PrereqTech")
		.addEnumAsInt(m_iPrereqAndBonus, L"Bonus")
		.addEnumAsInt(m_iPowerBonus, L"PowerBonus")
		.addEnumAsInt(m_iCivicOption, L"CivicOption")
		.addEnumAsInt(m_iVoteSourceType, L"DiploVoteType")
		.addEnumAsInt(m_iPrereqCultureLevel, L"PrereqCultureLevel")
		.addEnumAsInt(m_iPrereqVicinityBonus, L"VicinityBonus")
		.addEnumAsInt(m_iPrereqRawVicinityBonus, L"RawVicinityBonus")

		// Booleans
		.add(m_bNoLimit, L"bNoLimit")
		.add(m_bTeamShare, L"bTeamShare")
		.add(m_bAutoBuild, L"bAutoBuild")
		.add(m_bWater, L"bWater")
		.add(m_bRiver, L"bRiver")
		.add(m_bFreshWater, L"bFreshWater")
		.add(m_bPower, L"bPower")
		.add(m_bOrbital, L"bOrbital")
		.add(m_bOrbitalInfrastructure, L"bOrbitalInfrastructure")
		.add(m_bNoHolyCity, L"bNoHolyCity")
		.add(m_bAreaBorderObstacle, L"bBorderObstacle")
		.add(m_bForceTeamVoteEligible, L"bForceTeamVoteEligible")
		.add(m_bCapital, L"bCapital")
		.add(m_bGovernmentCenter, L"bGovernmentCenter")
		.add(m_bGoldenAge, L"bGoldenAge")
		.add(m_bAllowsNukes, L"bAllowsNukes")
		.add(m_bMapCentering, L"bMapCentering")
		.add(m_bNoUnhappiness, L"bNoUnhappiness")
		.add(m_bNoUnhealthyPopulation, L"bNoUnhealthyPopulation")
		.add(m_bBuildingOnlyHealthy, L"bBuildingOnlyHealthy")
		.add(m_bNeverCapture, L"bNeverCapture")
		.add(m_bNukeImmune, L"bNukeImmune")
		.add(m_bCenterInCity, L"bCenterInCity")
		.add(m_bStateReligionInCity, L"bNeedStateReligionInCity")
		.add(m_bApplyFreePromotionOnMove, L"bApplyFreePromotionOnMove")
		.add(m_bNoEnemyPillagingIncome, L"bNoEnemyPillagingIncome")
		.add(m_bProvidesFreshWater, L"bProvidesFreshWater")
		.add(m_bForceAllTradeRoutes, L"bForceAllTradeRoutes")
		.add(m_bPrereqPower, L"bPrereqPower")
		.add(m_bForceNoPrereqScaling, L"bForceNoPrereqScaling")
		.add(m_bPrereqWar, L"bPrereqWar")
		.add(m_bRequiresActiveCivics, L"bRequiresActiveCivics")
		.add(m_bZoneOfControl, L"bZoneOfControl")
		.add(m_bProtectedCulture, L"bProtectedCulture")
		.add(m_bDamageAllAttackers, L"bDamageAllAttackers") // read() derives m_bDamageAttackerCapable from it
		.add(m_bQuarantine, L"bQuarantine")

		// Integers
		.add(m_iGreatPeopleRateChange, L"iGreatPeopleRateChange")
		.add(m_iDCMAirbombMission, L"iDCMAirbombMission")
		.add(m_iAIWeight, L"iAIWeight")
		.add(m_iProductionCost, L"iCost", -1)
		.add(m_iProductionCostSize, L"iCostSizeModifier", -1)
		.add(m_iProductionCostCount, L"iCostCountModifier", -1)
		.add(m_iProductionCostMaterials, L"iCostMaterialsModifier", -1)
		.add(m_iProductionCostComplexity, L"iCostComplexityModifier", -1)
		.add(m_iHurryCostModifier, L"iHurryCostModifier")
		.add(m_iHurryAngerModifier, L"iHurryAngerModifier")
		.add(m_iMinAreaSize, L"iMinAreaSize")
		.add(m_iConquestProbability, L"iConquestProb", 50)
		.add(m_iNumCitiesPrereq, L"iCitiesPrereq")
		.add(m_iNumTeamsPrereq, L"iTeamsPrereq")
		.add(m_iUnitLevelPrereq, L"iLevelPrereq")
		.add(m_iMinLatitude, L"iMinLatitude")
		.add(m_iMaxLatitude, L"iMaxLatitude", 90)
		.add(m_iGreatPeopleRateModifier, L"iGreatPeopleRateModifier")
		.add(m_iGreatGeneralRateModifier, L"iGreatGeneralRateModifier")
		.add(m_iDomesticGreatGeneralRateModifier, L"iDomesticGreatGeneralRateModifier")
		.add(m_iGlobalGreatPeopleRateModifier, L"iGlobalGreatPeopleRateModifier")
		.add(m_iAnarchyModifier, L"iAnarchyModifier")
		.add(m_iGoldenAgeModifier, L"iGoldenAgeModifier")
		.add(m_iGlobalHurryModifier, L"iGlobalHurryModifier")
		.add(m_iFreeExperience, L"iExperience")
		.add(m_iGlobalFreeExperience, L"iGlobalExperience")
		.add(m_iFoodKept, L"iFoodKept")
		.add(m_iAirlift, L"iAirlift")
		.add(m_iAirModifier, L"iAirModifier")
		.add(m_iAirUnitCapacity, L"iAirUnitCapacity")
		.add(m_iNukeModifier, L"iNukeModifier")
		.add(m_iNukeExplosionRand, L"iNukeExplosionRand")
		.add(m_iFreeSpecialist, L"iFreeSpecialist")
		.add(m_iAreaFreeSpecialist, L"iAreaFreeSpecialist")
		.add(m_iGlobalFreeSpecialist, L"iGlobalFreeSpecialist")
		.add(m_iMaintenanceModifier, L"iMaintenanceModifier")
		.add(m_iGlobalMaintenanceModifier, L"iGlobalMaintenanceModifier")
		.add(m_iAreaMaintenanceModifier, L"iAreaMaintenanceModifier")
		.add(m_iOtherAreaMaintenanceModifier, L"iOtherAreaMaintenanceModifier")
		.add(m_iDistanceMaintenanceModifier, L"iDistanceMaintenanceModifier")
		.add(m_iNumCitiesMaintenanceModifier, L"iNumCitiesMaintenanceModifier")
		.add(m_iCoastalDistanceMaintenanceModifier, L"iCoastalDistanceMaintenanceModifier")
		.add(m_iConnectedCityMaintenanceModifier, L"iConnectedCityMaintenanceModifier")
		.add(m_iWarWearinessModifier, L"iWarWearinessModifier")
		.add(m_iGlobalWarWearinessModifier, L"iGlobalWarWearinessModifier")
		.add(m_iEnemyWarWearinessModifier, L"iEnemyWarWearinessModifier")
		.add(m_iHealRateChange, L"iHealRateChange")
		.add(m_iHealth, L"iHealth")
		.add(m_iAreaHealth, L"iAreaHealth")
		.add(m_iGlobalHealth, L"iGlobalHealth")
		.add(m_iHappiness, L"iHappiness")
		.add(m_iAreaHappiness, L"iAreaHappiness")
		.add(m_iGlobalHappiness, L"iGlobalHappiness")
		.add(m_iStateReligionHappiness, L"iStateReligionHappiness")
		.add(m_iWorkerSpeedModifier, L"iWorkerSpeedModifier")
		.add(m_iMilitaryProductionModifier, L"iMilitaryProductionModifier")
		.add(m_iSpaceProductionModifier, L"iSpaceProductionModifier")
		.add(m_iGlobalSpaceProductionModifier, L"iGlobalSpaceProductionModifier")
		.add(m_iTradeRoutes, L"iTradeRoutes")
		.add(m_iCoastalTradeRoutes, L"iCoastalTradeRoutes")
		.add(m_iGlobalTradeRoutes, L"iGlobalTradeRoutes")
		.add(m_iTradeRouteModifier, L"iTradeRouteModifier")
		.add(m_iForeignTradeRouteModifier, L"iForeignTradeRouteModifier")
		.add(m_iGlobalPopulationChange, L"iGlobalPopulationChange")
		.add(m_iFreeTechs, L"iFreeTechs")
		.add(m_iDefenseModifier, L"iDefense")
		.add(m_iBombardDefenseModifier, L"iBombardDefense")
		.add(m_iAllCityDefenseModifier, L"iAllCityDefense")
		.add(m_iEspionageDefenseModifier, L"iEspionageDefense")
		.add(m_iUnitUpgradePriceModifier, L"iUnitUpgradePriceModifier")
		.add(m_iRevIdxLocal, L"iRevIdxLocal")
		.add(m_iRevIdxNational, L"iRevIdxNational")
		.add(m_iRevIdxDistanceModifier, L"iRevIdxDistanceModifier")
		.add(m_iMaxPopAllowed, L"iObsoletePopulation", -1)
		.add(m_iAssetValue, L"iAsset")
		.add(m_iPowerValue, L"iPower")
		.add(m_iPillageGoldModifier, L"iPillageGoldModifier")
		.add(m_iWorldTradeRoutes, L"iWorldTradeRoutes")
		.add(m_iGlobalPopulationgrowthratepercentage, L"iGlobalPopulationgrowthratepercentage")
		.add(m_iPopulationgrowthratepercentage, L"iPopulationgrowthratepercentage")
		.add(m_iLineOfSight, L"iLineOfSight")
		.add(m_iInflationModifier, L"iInflationModifier")
		.add(m_iAdjacentDamagePercent, L"iAdjacentDamagePercent")
		.add(m_iPrereqPopulation, L"iPrereqPopulation")
		.add(m_iWorkableRadius, L"iWorkableRadius")
		.add(m_iOccupationTimeModifier, L"iOccupationTimeModifier")
		.add(m_iNoEntryDefenseLevel, L"iNoEntryDefenseLevel")
		.add(m_iNumUnitFullHeal, L"iNumUnitFullHeal")
		.add(m_iNumPopulationEmployed, L"iNumPopulationEmployed")
		.add(m_iHealthPercentPerPopulation, L"iHealthPercentPerPopulation")
		.add(m_iHappinessPercentPerPopulation, L"iHappinessPercentPerPopulation")
		.add(m_iLinePriority, L"iLinePriority")
		.add(m_iNationalCaptureProbabilityModifier, L"iNationalCaptureProbabilityModifier")
		.add(m_iNationalCaptureResistanceModifier, L"iNationalCaptureResistanceModifier")
		.add(m_iLocalCaptureProbabilityModifier, L"iLocalCaptureProbabilityModifier")
		.add(m_iLocalCaptureResistanceModifier, L"iLocalCaptureResistanceModifier")
		.add(m_iLocalDynamicDefense, L"iLocalDynamicDefense")
		.add(m_iRiverDefensePenalty, L"iRiverDefensePenalty")
		.add(m_iMinDefense, L"iMinDefense")
		.add(m_iBuildingDefenseRecoverySpeedModifier, L"iBuildingDefenseRecoverySpeedModifier")
		.add(m_iCityDefenseRecoverySpeedModifier, L"iCityDefenseRecoverySpeedModifier")
		.add(m_iDamageAttackerChance, L"iDamageAttackerChance")
		.add(m_iDamageToAttacker, L"iDamageToAttacker")
		.add(m_iMaxPopulationAllowed, L"iMaxPopulationAllowed", -1)
		.add(m_iMaxPopulationChange, L"iMaxPopulationChange")
		.add(m_iInsidiousness, L"iInsidiousness")
		.add(m_iInvestigation, L"iInvestigation")
		.add(m_iPopulationChange, L"iPopulationChange")
		.add(m_iMaxGlobalInstances, L"iMaxGlobalInstances", -1)
		.add(m_iMaxTeamInstances, L"iMaxTeamInstances", -1)
		.add(m_iMaxPlayerInstances, L"iMaxPlayerInstances", -1)
		.add(m_iExtraPlayerInstances, L"iExtraPlayerInstances")

		// Membership vectors (immediate resolution, like the legacy SetOptionalVector reads)
		.add(m_piPrereqAndTechs, L"TechTypes")
		.add(m_aePrereqOrBonuses, L"PrereqBonuses")
		.add(m_prereqOrHeritage, L"PrereqOrHeritage")
		.add(m_piPrereqOrVicinityBonuses, L"PrereqVicinityBonuses")
		.add(m_aePrereqOrRawVicinityBonuses, L"PrereqRawVicinityBonuses")
		.add(m_aiUnitCombatRetrainTypes, L"UnitCombatRetrainTypes")
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")

		// IDValueMaps (the building-keyed ones pick delayed resolution automatically,
		// matching the legacy readWithDelayedResolution calls)
		.add(m_piBonusHappinessChanges, L"BonusHappinessChanges")
		.add(m_aUnitCombatFreeExperience, L"UnitCombatFreeExperiences")
		.add(m_aUnitCombatExtraStrength, L"UnitCombatExtraStrengths")
		.add(m_aBuildingHappinessChanges, L"BuildingHappinessChanges")
		.add(m_aBuildingProductionModifier, L"BuildingProductionModifiers")
		.add(m_aGlobalBuildingProductionModifier, L"GlobalBuildingProductionModifiers")
		.add(m_aPrereqNumOfBuilding, L"PrereqAmountBuildings")
		.add(m_aGlobalBuildingCostModifier, L"GlobalBuildingCostModifiers")

		// IDValueMaps of paired arrays
		.add(m_techYieldChanges, L"TechYieldChanges", L"PrereqTech", L"TechYield")
		.add(m_techYieldModifiers, L"TechYieldModifiers", L"PrereqTech", L"TechYield")
		.add(m_techCommerceChanges, L"TechCommerceChanges", L"TechType", L"CommercePercents")
		.add(m_techCommerceModifiers, L"TechCommerceModifiers", L"PrereqTech", L"TechCommerce")
		.add(m_aTerrainYieldChanges, L"TerrainYieldChanges", L"TerrainType", L"YieldChanges")
		.add(m_aImprovementYieldChanges, L"ImprovementYieldChanges", L"ImprovementType", L"YieldChanges")
		.add(m_aGlobalImprovementYieldChanges, L"GlobalImprovementYieldChanges", L"ImprovementType", L"YieldChanges")

		// Yield / commerce arrays (wrapper-owned)
		.addYields(m_piRiverPlotYieldChange, L"RiverPlotYieldChanges")
		.addYields(m_piGlobalSeaPlotYieldChange, L"GlobalSeaPlotYieldChanges")
		.addYields(m_piYieldPerPopChange, L"YieldPerPopChanges")
		.addYields(m_piYieldModifier, L"YieldModifiers")
		.addYields(m_piPowerYieldModifier, L"PowerYieldModifiers")
		.addYields(m_piAreaYieldModifier, L"AreaYieldModifiers")
		.addYields(m_piGlobalYieldModifier, L"GlobalYieldModifiers")
		.addCommerce(m_piCommerceChange, L"CommerceChanges")
		.addCommerce(m_piCommercePerPopChange, L"CommercePerPopChanges")
		.addCommerce(m_piCommerceChangeDoubleTime, L"CommerceChangeDoubleTimes")
		.addCommerce(m_piCommerceModifier, L"CommerceModifiers")
		.addCommerce(m_piGlobalCommerceModifier, L"GlobalCommerceModifiers")
		.addCommerce(m_piSpecialistExtraCommerce, L"SpecialistExtraCommerces")
		.addCommerce(m_piStateReligionCommerce, L"StateReligionCommerces")
		.addCommerce(m_piCommerceHappiness, L"CommerceHappinesses")

		// Strings (not checksummed, same as before)
		.add(m_szMovieDefineTag, L"MovieDefineTag")
		.add(m_szConstructSound, L"ConstructSound")
	;
}

//
// read from XML
//
bool CvBuildingInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;

	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->GetOptionalChildXmlValByName(m_szArtDefineTag, L"ArtDefineTag");

	setNotShowInCity();

	pXML->GetOptionalChildXmlValByName(szTextVal, L"GreatPeopleUnitType");
	m_aszExtraXMLforPass3.push_back(szTextVal);

	pXML->SetVariableListTagPair(&m_piSpecialistCount, L"SpecialistCounts", GC.getNumSpecialistInfos());
	pXML->SetVariableListTagPair(&m_piFreeSpecialistCount, L"FreeSpecialistCounts", GC.getNumSpecialistInfos());

	if (pXML->TryMoveToXmlFirstChild(L"CommerceFlexibles"))
	{
		pXML->SetCommerce(&m_pbCommerceFlexible);
		pXML->MoveToXmlParent();
	}
	else
		SAFE_DELETE_ARRAY(m_pbCommerceFlexible);

	pXML->SetVariableListTagPair(&m_piBonusProductionModifier, L"BonusProductionModifiers", GC.getNumBonusInfos());
	pXML->SetVariableListTagPair(&m_piDomainFreeExperience, L"DomainFreeExperiences", NUM_DOMAIN_TYPES);
	pXML->SetVariableListTagPair(&m_piDomainProductionModifier, L"DomainProductionModifiers", NUM_DOMAIN_TYPES);

	if (pXML->TryMoveToXmlFirstChild(L"SpecialistYieldChanges"))
	{
		const int iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"SpecialistYieldChange"))
		{
			for (int j = 0; j < iNumChildren; ++j)
			{
				pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
				const int k = pXML->GetInfoClass(szTextVal);
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

			// set the current xml node to it's parent node
			pXML->MoveToXmlParent();
		}

		// set the current xml node to it's parent node
		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"SpecialistCommerceChanges"))
	{
		const int iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"SpecialistCommerceChange"))
		{
			for (int j = 0; j < iNumChildren; ++j)
			{
				pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
				const int k = pXML->GetInfoClass(szTextVal);
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

			// set the current xml node to it's parent node
			pXML->MoveToXmlParent();
		}

		// set the current xml node to it's parent node
		pXML->MoveToXmlParent();
	}

//Team Project (1)
	if (pXML->TryMoveToXmlFirstChild(L"LocalSpecialistYieldChanges"))
	{
		const int iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"LocalSpecialistYieldChange"))
		{
			for (int j = 0; j < iNumChildren; ++j)
			{
				pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
				const int k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiLocalSpecialistYieldChange == NULL )
					{
						m_ppaiLocalSpecialistYieldChange = new int*[GC.getNumSpecialistInfos()];

						for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
						{
							m_ppaiLocalSpecialistYieldChange[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"YieldChanges"))
					{
						// call the function that sets the yield change variable
						pXML->SetYields(&m_ppaiLocalSpecialistYieldChange[k]);
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

	if (pXML->TryMoveToXmlFirstChild(L"LocalSpecialistCommerceChanges"))
	{
		const int iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"LocalSpecialistCommerceChange"))
		{
			for (int j = 0; j < iNumChildren; ++j)
			{
				pXML->GetChildXmlValByName(szTextVal, L"SpecialistType");
				const int k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiLocalSpecialistCommerceChange == NULL )
					{
						m_ppaiLocalSpecialistCommerceChange = new int*[GC.getNumSpecialistInfos()];

						for(int i = 0; i < GC.getNumSpecialistInfos(); i++)
						{
							m_ppaiLocalSpecialistCommerceChange[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"CommerceChanges"))
					{
						// call the function that sets the yield change variable
						pXML->SetCommerce(&m_ppaiLocalSpecialistCommerceChange[k]);
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

	if (pXML->TryMoveToXmlFirstChild(L"BonusYieldModifiers"))
	{
		const int iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"BonusYieldModifier"))
		{
			for (int j = 0; j < iNumChildren; ++j)
			{
				pXML->GetChildXmlValByName(szTextVal, L"BonusType");
				const int k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiBonusYieldModifier == NULL )
					{
						m_ppaiBonusYieldModifier = new int*[GC.getNumBonusInfos()];

						for(int i = 0; i < GC.getNumBonusInfos(); i++)
						{
							m_ppaiBonusYieldModifier[i] = 0;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"YieldModifiers"))
					{
						// call the function that sets the yield change variable
						pXML->SetYields(&m_ppaiBonusYieldModifier[k]);
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

	pXML->SetVariableListTagPair(&m_piFlavorValue, L"Flavors", GC.getNumFlavorTypes());

	pXML->SetVariableListTagPair(&m_piBonusDefenseChanges, L"BonusDefenseChanges", GC.getNumBonusInfos());

	if (pXML->TryMoveToXmlFirstChild(L"BonusCommerceModifiers"))
	{
		const int iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"BonusCommerceModifier"))
		{
			for (int j = 0; j < iNumChildren; ++j)
			{
				pXML->GetChildXmlValByName(szTextVal, L"BonusType");
				const int k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiBonusCommerceModifier == NULL )
					{
						m_ppaiBonusCommerceModifier = new int*[GC.getNumBonusInfos()];

						for(int i = 0; i < GC.getNumBonusInfos(); i++)
						{
							m_ppaiBonusCommerceModifier[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"CommerceModifiers"))
					{
						// call the function that sets the commerce change variable
						pXML->SetCommerce(&m_ppaiBonusCommerceModifier[k]);
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

	if (pXML->TryMoveToXmlFirstChild(L"BonusCommercePercentChanges"))
	{
		const int iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"BonusCommercePercentChange"))
		{
			for (int j = 0; j < iNumChildren; ++j)
			{
				pXML->GetChildXmlValByName(szTextVal, L"BonusType");
				const int k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiBonusCommercePercentChanges == NULL )
					{
						m_ppaiBonusCommercePercentChanges = new int*[GC.getNumBonusInfos()];

						for(int i = 0; i < GC.getNumBonusInfos(); i++)
						{
							m_ppaiBonusCommercePercentChanges[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"CommercePercents"))
					{
						// call the function that sets the commerce change variable
						pXML->SetCommerce(&m_ppaiBonusCommercePercentChanges[k]);
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

	if (pXML->TryMoveToXmlFirstChild(L"BonusYieldChanges"))
	{
		const int iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"BonusYieldChange"))
		{
			for (int j = 0; j < iNumChildren; ++j)
			{
				pXML->GetChildXmlValByName(szTextVal, L"BonusType");
				const int k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiBonusYieldChanges == NULL )
					{
						m_ppaiBonusYieldChanges = new int*[GC.getNumBonusInfos()];

						for(int i = 0; i < GC.getNumBonusInfos(); i++)
						{
							m_ppaiBonusYieldChanges[i] = NULL;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"YieldChanges"))
					{
						// call the function that sets the commerce change variable
						pXML->SetYields(&m_ppaiBonusYieldChanges[k]);
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

	if (pXML->TryMoveToXmlFirstChild(L"VicinityBonusYieldChanges"))
	{
		const int iNumChildren = pXML->GetXmlChildrenNumber();

		if (pXML->TryMoveToXmlFirstChild(L"BonusYieldChange"))
		{
			for (int j = 0; j < iNumChildren; ++j)
			{
				pXML->GetChildXmlValByName(szTextVal, L"BonusType");
				const int k = pXML->GetInfoClass(szTextVal);
				if (k > -1)
				{
					if ( m_ppaiVicinityBonusYieldChanges == NULL )
					{
						m_ppaiVicinityBonusYieldChanges = new int*[GC.getNumBonusInfos()];

						for(int i = 0; i < GC.getNumBonusInfos(); i++)
						{
							m_ppaiVicinityBonusYieldChanges[i] = 0;
						}
					}
					if (pXML->TryMoveToXmlFirstChild(L"YieldChanges"))
					{
						// call the function that sets the commerce change variable
						pXML->SetYields(&m_ppaiVicinityBonusYieldChanges[k]);
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

	pXML->SetOptionalVectorWithDelayedResolution(m_aePrereqOrCivics, L"PrereqOrCivics");
	pXML->SetOptionalVectorWithDelayedResolution(m_aePrereqAndCivics, L"PrereqAndCivics");

	if (pXML->TryMoveToXmlFirstChild(L"TechSpecialistChanges"))
	{
		const int iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				for (int j = 0; j < iNumSibs; j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"PrereqTech");
					const int k = pXML->GetInfoClass(szTextVal);

					if (k > -1)
					{
						if ( m_ppaiTechSpecialistChange == NULL )
						{
							m_ppaiTechSpecialistChange = new int*[GC.getNumTechInfos()];

							for(int i = 0; i < GC.getNumTechInfos(); i++)
							{
								m_ppaiTechSpecialistChange[i] = NULL;
							}
						}
						pXML->SetVariableListTagPair(&m_ppaiTechSpecialistChange[k], L"SpecialistCounts", GC.getNumSpecialistInfos());
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

	if (pXML->TryMoveToXmlFirstChild(L"ImprovementYieldChanges"))
	{
		const int iNumSibs = pXML->GetXmlChildrenNumber();
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (0 < iNumSibs)
			{
				for (int j = 0; j < iNumSibs; j++)
				{
					pXML->GetChildXmlValByName(szTextVal, L"ImprovementType");
					const int iIndex = pXML->GetInfoClass(szTextVal);

					if (iIndex > -1)
					{
						if ( m_ppiImprovementYieldChanges == NULL )
						{
							m_ppiImprovementYieldChanges = new int*[GC.getNumImprovementInfos()];

							for(int i = 0; i < GC.getNumImprovementInfos(); i++ )
							{
								m_ppiImprovementYieldChanges[i] = NULL;
							}
						}
						// if we can set the current xml node to it's next sibling
						if (pXML->TryMoveToXmlFirstChild(L"ImprovementYields"))
						{
							// call the function that sets the yield change variable
							pXML->SetYields(&m_ppiImprovementYieldChanges[iIndex]);
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

	m_Properties.read(pXML);
	m_PropertiesAllCities.read(pXML, L"PropertiesAllCities");
	m_PrereqMinProperties.read(pXML, L"PrereqMinProperties");
	m_PrereqMaxProperties.read(pXML, L"PrereqMaxProperties");
	m_PrereqPlayerMinProperties.read(pXML, L"PrereqPlayerMinProperties");
	m_PrereqPlayerMaxProperties.read(pXML, L"PrereqPlayerMaxProperties");

	//TB Combat Mods (Buildings) begin
	pXML->GetOptionalChildXmlValByName(szTextVal, L"PropertySpawnUnit");
	m_aszExtraXMLforPass3.push_back(szTextVal);

	// m_bDamageAllAttackers itself is read declaratively above; this derived flag is not XML-backed.
	if (m_bDamageAllAttackers)
	{
		m_bDamageAttackerCapable = true;
	}

	//Structs

	if(pXML->TryMoveToXmlFirstChild(L"FreePromoTypes"))
	{
		const int iNum = pXML->GetXmlChildrenNumber(L"FreePromoType" );
		m_aFreePromoTypes.resize(iNum);

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"FreePromoType"))
			{
				int i = 0;
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"PromotionType");
					m_aFreePromoTypes[i].ePromotion = (PromotionTypes)pXML->GetInfoClass(szTextVal);
					FAssert(m_aFreePromoTypes[i].ePromotion > -1);
					if (pXML->TryMoveToXmlFirstChild(L"FreePromotionCondition"))
					{
						m_aFreePromoTypes[i].m_pExprFreePromotionCondition = BoolExpr::read(pXML);
						pXML->MoveToXmlParent();
					}
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"FreePromoType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	pXML->SetOptionalVectorWithDelayedResolution(m_aiFreeTraitTypes, L"FreeTraitTypes");
	pXML->SetOptionalVectorWithDelayedResolution(m_aiCategories, L"Categories");
	pXML->SetOptionalVectorWithDelayedResolution(m_aiPrereqInCityBuildings, L"PrereqInCityBuildings");
	pXML->SetOptionalVectorWithDelayedResolution(m_vPrereqNotInCityBuildings, L"PrereqNotInCityBuildings");
	pXML->SetOptionalVectorWithDelayedResolution(m_vPrereqOrBuilding, L"PrereqOrBuildings");
	pXML->SetOptionalVectorWithDelayedResolution(m_vReplacementBuilding, L"ReplacementBuildings");

	if(pXML->TryMoveToXmlFirstChild(L"HealUnitCombatTypes"))
	{
		const int iNum = pXML->GetXmlChildrenNumber(L"HealUnitCombatType" );
		m_aHealUnitCombatTypes.resize(iNum);

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"HealUnitCombatType"))
			{
				int i = 0;
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					m_aHealUnitCombatTypes[i].eUnitCombat = (UnitCombatTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aHealUnitCombatTypes[i].iHeal), L"iHeal");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"HealUnitCombatType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"BonusAidModifiers"))
	{
		const int iNum = pXML->GetXmlChildrenNumber(L"BonusAidModifier" );
		m_aBonusAidModifiers.resize(iNum);

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"BonusAidModifier"))
			{
				int i = 0;
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"BonusType");
					m_aBonusAidModifiers[i].eBonusType = (BonusTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(szTextVal, L"PropertyType");
					m_aBonusAidModifiers[i].ePropertyType = (PropertyTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aBonusAidModifiers[i].iModifier), L"iModifier");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"BonusAidModifier"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"AidRateChanges"))
	{
		const int iNum = pXML->GetXmlChildrenNumber(L"AidRateChange" );
		m_aAidRateChanges.resize(iNum);

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"AidRateChange"))
			{
				int i = 0;
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"PropertyType");
					m_aAidRateChanges[i].ePropertyType = (PropertyTypes)pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&(m_aAidRateChanges[i].iChange), L"iChange");
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"AidRateChange"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"EnabledCivilizationTypes"))
	{
		const int iNum = pXML->GetXmlChildrenNumber(L"EnabledCivilizationType" );
		m_aEnabledCivilizationTypes.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"EnabledCivilizationType"))
			{
				int i = 0;
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

	pXML->SetOptionalVector(&m_aiMayDamageAttackingUnitCombatTypes, L"MayDamageAttackingUnitCombatTypes");

	// int vector utilizing pairing without delayed resolution
	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aUnitCombatDefenseAgainstModifiers, L"UnitCombatDefenseAgainstModifiers");

	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aUnitCombatProdModifiers, L"UnitCombatProdModifiers");


	//Arrays
	pXML->SetVariableListTagPair(&m_pabHurry, L"Hurrys", GC.getNumHurryInfos());
	//TB Combat Mods (Buildings) end

	pXML->SetVariableListTagPair(&m_piVictoryThreshold, L"VictoryThresholds", GC.getNumVictoryInfos());

	// UnitTypes loads after buildings; must stay on explicit delayed resolution (see getDataMembers).
	m_aUnitProductionModifier.readWithDelayedResolution(pXML, L"UnitProductionModifiers");

	// PlotTypes is not an info class, so the paired-array wrapper cannot take this one (see getDataMembers).
	m_aPlotYieldChanges.readPairedArrays(pXML, L"PlotYieldChanges", L"PlotType", L"Yields");

	return true;
}

bool CvBuildingInfo::readPass3()
{
	PROFILE_EXTRA_FUNC();
	// PrereqOrCivics / PrereqAndCivics now resolve via SetOptionalVectorWithDelayedResolution (no pass3).

	if (m_aszExtraXMLforPass3.empty())
	{
		FErrorMsg("error");
		return false;
	}
	m_iGreatPeopleUnitType = GC.getInfoTypeForString(m_aszExtraXMLforPass3[0]);

	m_ePropertySpawnUnit = (UnitTypes) GC.getInfoTypeForString(m_aszExtraXMLforPass3[1]);

	m_aszExtraXMLforPass3.clear();

	return true;
}

void CvBuildingInfo::copyNonDefaults(CvBuildingInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const bool bDefault = false;
	const int iDefault = 0;
	const int iTextDefault = -1;
	const CvString cDefault = CvString::format("").GetCString();

	if ( getArtDefineTag() == cDefault ) // "ArtDefineTag"
	{
		m_szArtDefineTag = pClassInfo->getArtDefineTag();
		setNotShowInCity();
	}

	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if (getGreatPeopleUnitType() == iTextDefault) m_iGreatPeopleUnitType = pClassInfo->getGreatPeopleUnitType();


	for ( int j = 0; j < NUM_COMMERCE_TYPES; j++)
	{
		if ( isCommerceFlexible(j) == bDefault && pClassInfo->isCommerceFlexible(j) != bDefault)
		{
			if ( NULL == m_pbCommerceFlexible )
			{
				CvXMLLoadUtility::InitList(&m_pbCommerceFlexible,NUM_COMMERCE_TYPES,bDefault);
			}
			m_pbCommerceFlexible[j] = pClassInfo->isCommerceFlexible(j);
		}
	}

	for ( int j = 0; j < GC.getNumSpecialistInfos(); j++)
	{
		if ( getSpecialistCount(j) == iDefault && pClassInfo->getSpecialistCount(j) != iDefault)
		{
			if ( NULL == m_piSpecialistCount )
			{
				CvXMLLoadUtility::InitList(&m_piSpecialistCount,GC.getNumSpecialistInfos(),iDefault);
			}
			m_piSpecialistCount[j] = pClassInfo->getSpecialistCount(j);
		}
		if ( getFreeSpecialistCount(j) == iDefault && pClassInfo->getFreeSpecialistCount(j) != iDefault)
		{
			if ( NULL == m_piFreeSpecialistCount )
			{
				CvXMLLoadUtility::InitList(&m_piFreeSpecialistCount,GC.getNumSpecialistInfos(),iDefault);
			}
			m_piFreeSpecialistCount[j] = pClassInfo->getFreeSpecialistCount(j);
		}
	}

	for ( int j = 0; j < GC.getNumBonusInfos(); j++)
	{
		if ( getBonusProductionModifier(j) == iDefault && pClassInfo->getBonusProductionModifier(j) != iDefault )
		{
			if ( NULL == m_piBonusProductionModifier )
			{
				CvXMLLoadUtility::InitList(&m_piBonusProductionModifier,GC.getNumBonusInfos(),iDefault);
			}
			m_piBonusProductionModifier[j] = pClassInfo->getBonusProductionModifier(j);
		}
	}

	for ( int j = 0; j < NUM_DOMAIN_TYPES; j++)
	{
		if ( getDomainFreeExperience(j) == iDefault && pClassInfo->getDomainFreeExperience(j) != iDefault)
		{
			if ( NULL == m_piDomainFreeExperience )
			{
				CvXMLLoadUtility::InitList(&m_piDomainFreeExperience,NUM_DOMAIN_TYPES,iDefault);
			}
			m_piDomainFreeExperience[j] = pClassInfo->getDomainFreeExperience(j);
		}
		if ( getDomainProductionModifier(j) == iDefault && pClassInfo->getDomainProductionModifier(j) != iDefault)
		{
			if ( NULL == m_piDomainProductionModifier )
			{
				CvXMLLoadUtility::InitList(&m_piDomainProductionModifier,NUM_DOMAIN_TYPES,iDefault);
			}
			m_piDomainProductionModifier[j] = pClassInfo->getDomainProductionModifier(j);
		}
	}

	for ( int i = 0; i < GC.getNumSpecialistInfos(); i++)
	{
		for ( int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if ( getSpecialistYieldChange(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getSpecialistYieldChange(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiSpecialistYieldChange == NULL )
					{
						CvXMLLoadUtility::InitList(&m_ppaiSpecialistYieldChange, GC.getNumSpecialistInfos());
					}
					if ( m_ppaiSpecialistYieldChange[i] == NULL )
					{
						CvXMLLoadUtility::InitList(&m_ppaiSpecialistYieldChange[i], NUM_YIELD_TYPES);
					}

					m_ppaiSpecialistYieldChange[i][j] = iChange;
				}
			}

			if ( getLocalSpecialistYieldChange(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getLocalSpecialistYieldChange(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiLocalSpecialistYieldChange == NULL )
					{
						CvXMLLoadUtility::InitList(&m_ppaiLocalSpecialistYieldChange, GC.getNumSpecialistInfos());
					}
					if (m_ppaiLocalSpecialistYieldChange[i] == NULL)
					{
						CvXMLLoadUtility::InitList(&m_ppaiLocalSpecialistYieldChange[i], NUM_YIELD_TYPES);
					}

					m_ppaiLocalSpecialistYieldChange[i][j] = iChange;
				}
			}
		}
		for ( int j = 0; j < NUM_COMMERCE_TYPES; j++)
		{
			if ( getSpecialistCommerceChange(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getSpecialistCommerceChange(i, j);

				if ( iChange != iDefault )
				{
					if (m_ppaiSpecialistCommerceChange == NULL)
					{
						CvXMLLoadUtility::InitList(&m_ppaiSpecialistCommerceChange, GC.getNumSpecialistInfos());
					}
					if (m_ppaiSpecialistCommerceChange[i] == NULL)
					{
						CvXMLLoadUtility::InitList(&m_ppaiSpecialistCommerceChange[i], NUM_COMMERCE_TYPES);
					}

					m_ppaiSpecialistCommerceChange[i][j] = iChange;
				}
			}

			if ( getLocalSpecialistCommerceChange(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getLocalSpecialistCommerceChange(i, j);

				if ( iChange != iDefault )
				{
					if (m_ppaiLocalSpecialistCommerceChange == NULL)
					{
						CvXMLLoadUtility::InitList(&m_ppaiLocalSpecialistCommerceChange, GC.getNumSpecialistInfos());
					}
					if (m_ppaiLocalSpecialistCommerceChange[i] == NULL)
					{
						CvXMLLoadUtility::InitList(&m_ppaiLocalSpecialistCommerceChange[i], NUM_COMMERCE_TYPES);
					}

					m_ppaiLocalSpecialistCommerceChange[i][j] = iChange;
				}
			}
		}
	}
	for ( int i = 0; i < GC.getNumBonusInfos(); i++)
	{
		for ( int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if ( getBonusYieldModifier(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getBonusYieldModifier(i, j);

				if ( iChange != iDefault )
				{
					if (m_ppaiBonusYieldModifier == NULL)
					{
						CvXMLLoadUtility::InitList(&m_ppaiBonusYieldModifier, GC.getNumBonusInfos());
					}
					if (m_ppaiBonusYieldModifier[i] == NULL)
					{
						CvXMLLoadUtility::InitList(&m_ppaiBonusYieldModifier[i], NUM_YIELD_TYPES);
					}

					m_ppaiBonusYieldModifier[i][j] = iChange;
				}
			}
		}
	}
	for ( int j = 0; j < GC.getNumFlavorTypes(); j++)
	{
		if ( getFlavorValue(j) == iDefault && pClassInfo->getFlavorValue(j) != iDefault)
		{
			if ( NULL == m_piFlavorValue )
			{
				CvXMLLoadUtility::InitList(&m_piFlavorValue,GC.getNumFlavorTypes(),iDefault);
			}
			m_piFlavorValue[j] = pClassInfo->getFlavorValue(j);
		}
	}

	for ( int j = 0; j < GC.getNumBonusInfos(); j++)
	{
		if ( getBonusDefenseChanges(j) == iDefault && pClassInfo->getBonusDefenseChanges(j) != iDefault)
		{
			if ( NULL == m_piBonusDefenseChanges )
			{
				CvXMLLoadUtility::InitList(&m_piBonusDefenseChanges,GC.getNumBonusInfos(),iDefault);
			}
			m_piBonusDefenseChanges[j] = pClassInfo->getBonusDefenseChanges(j);
		}
	}
	for ( int i = 0; i < GC.getNumBonusInfos(); i++)
	{
		for ( int j = 0; j < NUM_COMMERCE_TYPES; j++)
		{
			if ( getBonusCommerceModifier(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getBonusCommerceModifier(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiBonusCommerceModifier == NULL )
					{
						m_ppaiBonusCommerceModifier = new int*[GC.getNumBonusInfos()];
						for(int k = 0; k < GC.getNumBonusInfos(); k++)
						{
							m_ppaiBonusCommerceModifier[k] = NULL;
						}
					}
					if ( m_ppaiBonusCommerceModifier[i] == NULL )
					{
						m_ppaiBonusCommerceModifier[i] = new int[NUM_COMMERCE_TYPES];
						for(int k = 0; k < NUM_COMMERCE_TYPES; k++)
						{
							m_ppaiBonusCommerceModifier[i][k] = NULL;
						}
					}

					m_ppaiBonusCommerceModifier[i][j] = iChange;
				}
			}
		}
	}

	for ( int i = 0; i < GC.getNumBonusInfos(); i++)
	{
		for ( int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if ( getBonusYieldChanges(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getBonusYieldChanges(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiBonusYieldChanges == NULL )
					{
						m_ppaiBonusYieldChanges = new int*[GC.getNumBonusInfos()];
						for(int k = 0; k < GC.getNumBonusInfos(); k++)
						{
							m_ppaiBonusYieldChanges[k] = NULL;
						}
					}
					if ( m_ppaiBonusYieldChanges[i] == NULL )
					{
						m_ppaiBonusYieldChanges[i] = new int[NUM_YIELD_TYPES];
						for(int k = 0; k < NUM_YIELD_TYPES; k++)
						{
							m_ppaiBonusYieldChanges[i][k] = NULL;
						}
					}

					m_ppaiBonusYieldChanges[i][j] = iChange;
				}
			}
		}
	}

	for ( int i = 0; i < GC.getNumBonusInfos(); i++)
	{
		for ( int j = 0; j < NUM_COMMERCE_TYPES; j++)
		{
			if ( getBonusCommercePercentChanges(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getBonusCommercePercentChanges(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiBonusCommercePercentChanges == NULL )
					{
						m_ppaiBonusCommercePercentChanges = new int*[GC.getNumBonusInfos()];
						for(int k = 0; k < GC.getNumBonusInfos(); k++)
						{
							m_ppaiBonusCommercePercentChanges[k] = NULL;
						}
					}
					if ( m_ppaiBonusCommercePercentChanges[i] == NULL )
					{
						m_ppaiBonusCommercePercentChanges[i] = new int[NUM_COMMERCE_TYPES];
						for(int k = 0; k < NUM_COMMERCE_TYPES; k++)
						{
							m_ppaiBonusCommercePercentChanges[i][k] = NULL;
						}
					}

					m_ppaiBonusCommercePercentChanges[i][j] = iChange;
				}
			}
		}
	}

	for ( int i = 0; i < GC.getNumBonusInfos(); i++)
	{
		for ( int j = 0; j < NUM_YIELD_TYPES; j++)
		{
			if ( getVicinityBonusYieldChanges(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getVicinityBonusYieldChanges(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiVicinityBonusYieldChanges == NULL )
					{
						m_ppaiVicinityBonusYieldChanges = new int*[GC.getNumBonusInfos()];
						for(int k = 0; k < GC.getNumBonusInfos(); k++)
						{
							m_ppaiVicinityBonusYieldChanges[k] = NULL;
						}
					}
					if ( m_ppaiVicinityBonusYieldChanges[i] == NULL )
					{
						m_ppaiVicinityBonusYieldChanges[i] = new int[NUM_YIELD_TYPES];
						for(int k = 0; k < NUM_YIELD_TYPES; k++)
						{
							m_ppaiVicinityBonusYieldChanges[i][k] = NULL;
						}
					}

					m_ppaiVicinityBonusYieldChanges[i][j] = iChange;
				}
			}
		}
	}

	for ( int i = 0; i < GC.getNumImprovementInfos(); i++ )
	{
		for ( int j = 0; j < NUM_YIELD_TYPES; j++ )
		{
			if ( getImprovementYieldChanges(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getImprovementYieldChanges(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppiImprovementYieldChanges == NULL )
					{
						m_ppiImprovementYieldChanges = new int*[GC.getNumImprovementInfos()];
						for(int k = 0; k < GC.getNumImprovementInfos(); k++)
						{
							m_ppiImprovementYieldChanges[k] = NULL;
						}
					}
					if ( m_ppiImprovementYieldChanges[i] == NULL )
					{
						m_ppiImprovementYieldChanges[i] = new int[NUM_YIELD_TYPES];
						for(int k = 0; k < NUM_YIELD_TYPES; k++)
						{
							m_ppiImprovementYieldChanges[i][k] = NULL;
						}
					}

					m_ppiImprovementYieldChanges[i][j] = iChange;
				}
			}
		}
	}

	for ( int i = 0; i < GC.getNumTechInfos(); i++)
	{
		for ( int j = 0; j < GC.getNumSpecialistInfos(); j++)
		{
			if ( getTechSpecialistChange(i,j) == iDefault )
			{
				const int iChange = pClassInfo->getTechSpecialistChange(i, j);

				if ( iChange != iDefault )
				{
					if ( m_ppaiTechSpecialistChange == NULL )
					{
						m_ppaiTechSpecialistChange = new int*[GC.getNumTechInfos()];
						for(int k = 0; k < GC.getNumTechInfos(); k++)
						{
							m_ppaiTechSpecialistChange[k] = NULL;
						}
					}
					if ( m_ppaiTechSpecialistChange[i] == NULL )
					{
						m_ppaiTechSpecialistChange[i] = new int[GC.getNumSpecialistInfos()];
						for(int k = 0; k < GC.getNumSpecialistInfos(); k++)
						{
							m_ppaiTechSpecialistChange[i][k] = NULL;
						}
					}

					m_ppaiTechSpecialistChange[i][j] = iChange;
				}
			}
		}
	}

	GC.copyNonDefaultDelayedResolutionVector(m_aePrereqOrCivics, pClassInfo->m_aePrereqOrCivics);
	GC.copyNonDefaultDelayedResolutionVector(m_aePrereqAndCivics, pClassInfo->m_aePrereqAndCivics);

	m_Properties.copyNonDefaults(pClassInfo->getProperties());
	m_PropertiesAllCities.copyNonDefaults(pClassInfo->getPropertiesAllCities());
	m_PrereqMinProperties.copyNonDefaults(pClassInfo->getPrereqMinProperties());
	m_PrereqMaxProperties.copyNonDefaults(pClassInfo->getPrereqMaxProperties());
	m_PrereqPlayerMinProperties.copyNonDefaults(pClassInfo->getPrereqPlayerMinProperties());
	m_PrereqPlayerMaxProperties.copyNonDefaults(pClassInfo->getPrereqPlayerMaxProperties());


	//TB Combat Mods (Buildings) begin
	if (m_ePropertySpawnUnit == NO_UNIT) m_ePropertySpawnUnit = pClassInfo->getPropertySpawnUnit();

	// m_bDamageAllAttackers itself merges declaratively above; this derived flag does not.
	if (isDamageAllAttackers() || pClassInfo->isDamageAllAttackers())
	{
		m_bDamageAttackerCapable = true;
	}
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aFreePromoTypes, pClassInfo->getFreePromoTypes());
	GC.copyNonDefaultDelayedResolutionVector(m_aiFreeTraitTypes, pClassInfo->m_aiFreeTraitTypes);
	GC.copyNonDefaultDelayedResolutionVector(m_aiCategories, pClassInfo->m_aiCategories);
	GC.copyNonDefaultDelayedResolutionVector(m_aiPrereqInCityBuildings, pClassInfo->m_aiPrereqInCityBuildings);
	GC.copyNonDefaultDelayedResolutionVector(m_vPrereqNotInCityBuildings, pClassInfo->m_vPrereqNotInCityBuildings);
	GC.copyNonDefaultDelayedResolutionVector(m_vPrereqOrBuilding, pClassInfo->m_vPrereqOrBuilding);
	GC.copyNonDefaultDelayedResolutionVector(m_vReplacementBuilding, pClassInfo->m_vReplacementBuilding);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aHealUnitCombatTypes, pClassInfo->m_aHealUnitCombatTypes);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aBonusAidModifiers, pClassInfo->m_aBonusAidModifiers);
	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aAidRateChanges, pClassInfo->m_aAidRateChanges);

	if (getNumEnabledCivilizationTypes() == 0)
	{
		const int iNum = pClassInfo->getNumEnabledCivilizationTypes();
		m_aEnabledCivilizationTypes.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			GC.copyNonDefaultDelayedResolution((int*)&(m_aEnabledCivilizationTypes[i].eCivilization), (int*)&(pClassInfo->m_aEnabledCivilizationTypes[i].eCivilization));
		}
	}

	if (getNumMayDamageAttackingUnitCombatTypes() == 0)
	{
		m_aiMayDamageAttackingUnitCombatTypes.clear();
		for ( int i = 0; i < pClassInfo->getNumMayDamageAttackingUnitCombatTypes(); i++)
		{
			m_aiMayDamageAttackingUnitCombatTypes.push_back(pClassInfo->getMayDamageAttackingUnitCombatType(i));
			if (getMayDamageAttackingUnitCombatType(i) > 0)
			{
				m_bDamageAttackerCapable = true;
			}
		}
	}

	// int vector utilizing pairing without delayed resolution

	if (getNumUnitCombatDefenseAgainstModifiers()==0)
	{
		for (int i=0; i < pClassInfo->getNumUnitCombatDefenseAgainstModifiers(); i++)
		{
			const UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			const int iChange = pClassInfo->getUnitCombatDefenseAgainstModifier(i);
			m_aUnitCombatDefenseAgainstModifiers.push_back(std::make_pair(eUnitCombat, iChange));
		}
	}

	if (getNumUnitCombatProdModifiers()==0)
	{
		for (int i=0; i < pClassInfo->getNumUnitCombatProdModifiers(); i++)
		{
			const UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			const int iChange = pClassInfo->getUnitCombatProdModifier(i);
			m_aUnitCombatProdModifiers.push_back(std::make_pair(eUnitCombat, iChange));
		}
	}


	//Arrays
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
	//TB Combat Mods (Buildings) end

	for ( int i = 0; i < GC.getNumVictoryInfos(); i++ )
	{
		if (getVictoryThreshold(i) == 0 && pClassInfo->getVictoryThreshold(i) != 0)
		{
			if ( NULL == m_piVictoryThreshold )
			{
				CvXMLLoadUtility::InitList(&m_piVictoryThreshold,GC.getNumVictoryInfos(), 0);
			}
			m_piVictoryThreshold[i] = pClassInfo->getVictoryThreshold(i);
		}
	}

	// UnitTypes loads after buildings; stays on explicit delayed resolution (see getDataMembers).
	m_aUnitProductionModifier.copyNonDefaultDelayedResolution(pClassInfo->getUnitProductionModifiers());

	// PlotTypes is not an info class; the paired-array wrapper cannot take this one (see getDataMembers).
	m_aPlotYieldChanges.copyNonDefaults(pClassInfo->getPlotYieldChanges());
}

bool CvBuildingInfo::isNewCityFree(const CvGameObject* pObject) const
{
	return m_pExprNewCityFree && m_pExprNewCityFree->evaluate(pObject);
}

const BoolExpr* CvBuildingInfo::getConstructCondition() const
{
	return m_pExprConstructCondition;
}

bool CvBuildingInfo::getNotShowInCity() const
{
	return m_bNotShowInCity;
}

void CvBuildingInfo::setNotShowInCity()
{
	m_bNotShowInCity = (m_szArtDefineTag == "" || getArtInfo()->getScale() == 0.0 || stricmp(getArtInfo()->getNIF(), "Art/empty.nif") == 0);
}

bool CvBuildingInfo::isPrereqOrBuilding(const int i) const
{
	return algo::any_of_equal(m_vPrereqOrBuilding, i);
}

int CvBuildingInfo::getPrereqOrBuilding(const int i) const
{
	return m_vPrereqOrBuilding[i];
}
short CvBuildingInfo::getNumPrereqOrBuilding() const
{
	return m_vPrereqOrBuilding.size();
}

int CvBuildingInfo::getReplacementBuilding(const int i) const
{
	return m_vReplacementBuilding[i];
}
short CvBuildingInfo::getNumReplacementBuilding() const
{
	return m_vReplacementBuilding.size();
}

void CvBuildingInfo::setReplacedBuilding(const int i)
{
	if (algo::none_of_equal(m_vReplacedBuilding, i))
	{
		m_vReplacedBuilding.push_back(i);
	}
}
int CvBuildingInfo::getReplacedBuilding(const int i) const
{
	return m_vReplacedBuilding[i];
}
short CvBuildingInfo::getNumReplacedBuilding() const
{
	return m_vReplacedBuilding.size();
}
