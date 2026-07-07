//------------------------------------------------------------------------------------------------
//  FILE:    CvTechInfo.cpp
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
#include "CvTechInfo.h"


//======================================================================================================
//					CvTechInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvTechInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvTechInfo::CvTechInfo() :
// Hand-written (non-declarative) fields only - everything else is initialized by initDataMembers()
m_iFirstFreeUnit(NO_UNIT),
m_iFirstFreeProphet(NO_UNIT),
m_piDomainExtraMoves(NULL),
m_piFlavorValue(NULL),
m_piFreeSpecialistCount(NULL),
m_pbCommerceFlexible(NULL)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvTechInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvTechInfo::~CvTechInfo()
{
	CvInfoUtil(this).uninitDataMembers(); // frees the wrapper-owned m_piCommerceModifier array

	SAFE_DELETE_ARRAY(m_piDomainExtraMoves);
	SAFE_DELETE_ARRAY(m_piFlavorValue);
	SAFE_DELETE_ARRAY(m_pbCommerceFlexible);
	SAFE_DELETE_ARRAY(m_piFreeSpecialistCount); // was leaked by the legacy dtor
	GC.removeDelayedResolutionVector(m_piPrereqOrTechs);
	GC.removeDelayedResolutionVector(m_piPrereqAndTechs);
	GC.removeDelayedResolutionVector(m_aPrereqBuilding);
	GC.removeDelayedResolutionVector(m_aPrereqOrBuilding);
	GC.removeDelayedResolution((int*)&m_iFirstFreeUnit);
	GC.removeDelayedResolution((int*)&m_iFirstFreeProphet);
}



int CvTechInfo::getAdvisorType() const
{
	return m_iAdvisorType;
}


int CvTechInfo::getAIWeight() const
{
	return m_iAIWeight;
}


int CvTechInfo::getAITradeModifier() const
{
	return m_iAITradeModifier;
}


int CvTechInfo::getResearchCost() const
{
	return m_iResearchCost;
}


int CvTechInfo::getEra() const
{
	return m_iEra;
}


int CvTechInfo::getTradeRoutes() const
{
	return m_iTradeRoutes;
}


int CvTechInfo::getFeatureProductionModifier() const
{
	return m_iFeatureProductionModifier;
}


int CvTechInfo::getWorkerSpeedModifier() const
{
	return m_iWorkerSpeedModifier;
}

//DPII < Maintenance Modifier >
int CvTechInfo::getMaintenanceModifier() const
{
	return m_iMaintenanceModifier;
}


int CvTechInfo::getDistanceMaintenanceModifier() const
{
	return m_iDistanceMaintenanceModifier;
}


int CvTechInfo::getNumCitiesMaintenanceModifier() const
{
	return m_iNumCitiesMaintenanceModifier;
}


int CvTechInfo::getCoastalDistanceMaintenanceModifier() const
{
	return m_iCoastalDistanceMaintenanceModifier;
}

//DPII < Maintenance Modifier >
int CvTechInfo::getFirstFreeUnit() const
{
	return m_iFirstFreeUnit;
}


int CvTechInfo::getFirstFreeProphet() const
{
	return m_iFirstFreeProphet;
}


int CvTechInfo::getHealth() const
{
	return m_iHealth;
}


int CvTechInfo::getHappiness() const
{
	return m_iHappiness;
}


int CvTechInfo::getFirstFreeTechs() const
{
	return m_iFirstFreeTechs;
}


int CvTechInfo::getAssetValue() const
{
	return m_iAssetValue * 100;
}


int CvTechInfo::getPowerValue() const
{
	return m_iPowerValue * 100;
}


int CvTechInfo::getGridX() const
{
	return m_iGridX;
}


int CvTechInfo::getGridY() const
{
	return m_iGridY;
}


bool CvTechInfo::isRepeat() const
{
	return m_bRepeat;
}


bool CvTechInfo::isTrade() const
{
	return m_bTrade;
}


bool CvTechInfo::isDisable() const
{
	return m_bDisable;
}


bool CvTechInfo::isGoodyTech() const
{
	return m_bGoodyTech;
}


bool CvTechInfo::isExtraWaterSeeFrom() const
{
	return m_bExtraWaterSeeFrom;
}


bool CvTechInfo::isMapCentering() const
{
	return m_bMapCentering;
}


bool CvTechInfo::isMapVisible() const
{
	return m_bMapVisible;
}


bool CvTechInfo::isMapTrading() const
{
	return m_bMapTrading;
}


bool CvTechInfo::isTechTrading() const
{
	return m_bTechTrading;
}


bool CvTechInfo::isGoldTrading() const
{
	return m_bGoldTrading;
}


bool CvTechInfo::isOpenBordersTrading() const
{
	return m_bOpenBordersTrading;
}


bool CvTechInfo::isDefensivePactTrading() const
{
	return m_bDefensivePactTrading;
}


bool CvTechInfo::isPermanentAllianceTrading() const
{
	return m_bPermanentAllianceTrading;
}


bool CvTechInfo::isVassalStateTrading() const
{
	return m_bVassalStateTrading;
}


bool CvTechInfo::isBridgeBuilding() const
{
	return m_bBridgeBuilding;
}


bool CvTechInfo::isIrrigation() const
{
	return m_bIrrigation;
}


bool CvTechInfo::isIgnoreIrrigation() const
{
	return m_bIgnoreIrrigation;
}


bool CvTechInfo::isWaterWork() const
{
	return m_bWaterWork;
}


bool CvTechInfo::isRiverTrade() const
{
	return m_bRiverTrade;
}


bool CvTechInfo::getDCMAirBombTech1() const
{
	return m_bDCMAirBombTech1;
}


bool CvTechInfo::getDCMAirBombTech2() const
{
	return m_bDCMAirBombTech2;
}


std::wstring CvTechInfo::getQuote()	const
{
	CvWString text = gDLL->getText(m_szQuoteKey);
	FAssert(!text.empty());
	return text;
}


const char* CvTechInfo::getQuoteKey() const
{
	return m_szQuoteKey;
}


const char* CvTechInfo::getSound() const
{
	return m_szSound;
}


const char* CvTechInfo::getSoundMP() const
{
	return m_szSoundMP;
}


// Arrays

int CvTechInfo::getDomainExtraMoves(int i) const
{
	return m_piDomainExtraMoves ? m_piDomainExtraMoves[i] : 0;
}


int CvTechInfo::getFlavorValue(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFlavorTypes(), i);
	return m_piFlavorValue ? m_piFlavorValue[i] : 0;
}


const std::vector<TechTypes>& CvTechInfo::getPrereqOrTechs() const
{
	return m_piPrereqOrTechs;
}


const std::vector<TechTypes>& CvTechInfo::getPrereqAndTechs() const
{
	return m_piPrereqAndTechs;
}


bool CvTechInfo::isCommerceFlexible(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_pbCommerceFlexible ? m_pbCommerceFlexible[i] : false;
}


bool CvTechInfo::isTerrainTrade(int i) const
{
	return algo::any_of_equal(m_aeTerrainTrade, static_cast<TerrainTypes>(i));
}


//ls612: Tech Commerce Modifiers
int CvTechInfo::getCommerceModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCommerceModifier ? m_piCommerceModifier[i] : 0;
}


int* CvTechInfo::getCommerceModifierArray() const
{
	return m_piCommerceModifier;
}


bool CvTechInfo::isEmbassyTrading() const
{
	return m_bEmbassyTrading;
}


bool CvTechInfo::isEnablesDesertFarming() const
{
	return m_bEnablesDesertFarming;
}


bool CvTechInfo::isCanPassPeaks() const
{
	return m_bCanPassPeaks;
}


bool CvTechInfo::isMoveFastPeaks() const
{
	return m_bMoveFastPeaks;
}


bool CvTechInfo::isCanFoundOnPeaks() const
{
	return m_bCanFoundOnPeaks;
}

bool CvTechInfo::isEnableDarkAges() const
{
	return m_bEnableDarkAges;
}

bool CvTechInfo::isRebaseAnywhere() const
{
	return m_bRebaseAnywhere;
}

int CvTechInfo::getInflationModifier() const
{
	return m_iInflationModifier;
}

int CvTechInfo::getGlobalTradeModifier() const
{
	return m_iGlobalTradeModifier;
}

int CvTechInfo::getGlobalForeignTradeModifier() const
{
	return  m_iGlobalForeignTradeModifier;
}

int CvTechInfo::getTradeMissionModifier() const
{
	return m_iTradeMissionModifier;
}

int CvTechInfo::getCorporationRevenueModifier() const
{
	return m_iCorporationRevenueModifier;
}

int CvTechInfo::getCorporationMaintenanceModifier() const
{
	return m_iCorporationMaintenanceModifier;
}

int CvTechInfo::getPrereqGameOption() const
{
	return m_iPrereqGameOption;
}

int CvTechInfo::getFreeSpecialistCount(int i) const
{
	return m_piFreeSpecialistCount ? m_piFreeSpecialistCount[i] : 0;
}


int CvTechInfo::getNumPrereqBuildings() const
{
	return m_aPrereqBuilding.size();
}


const PrereqBuilding& CvTechInfo::getPrereqBuilding(int iIndex) const
{
	return m_aPrereqBuilding[iIndex];
}


int CvTechInfo::getPrereqBuildingType(int iIndex) const
{
	return (int)m_aPrereqBuilding[iIndex].eBuilding;
}


int CvTechInfo::getPrereqBuildingMinimumRequired(int iIndex) const
{
	return m_aPrereqBuilding[iIndex].iMinimumRequired;
}


int CvTechInfo::getNumPrereqOrBuildings() const
{
	return m_aPrereqOrBuilding.size();
}


const PrereqBuilding& CvTechInfo::getPrereqOrBuilding(int iIndex) const
{
	return m_aPrereqOrBuilding[iIndex];
}


int CvTechInfo::getPrereqOrBuildingType(int iIndex) const
{
	return (int)m_aPrereqOrBuilding[iIndex].eBuilding;
}


int CvTechInfo::getPrereqOrBuildingMinimumRequired(int iIndex) const
{
	return m_aPrereqOrBuilding[iIndex].iMinimumRequired;
}


int CvTechInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvTechInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvTechInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


bool CvTechInfo::isGlobal() const
{
	return m_bGlobal;
}


void CvTechInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order for the wrapped subset; non-checksummed fields
	// (iGridX/iGridY, the DCM air-bomb bools, the strings) are parked at the end. The class keeps an
	// explicit getCheckSum because hand-written fields (FirstFreeUnit/Prophet, DomainExtraMoves,
	// Flavors, the prereq tech/building vectors, CommerceFlexible, FreeSpecialistCounts) sit
	// mid-order in the legacy checksum, and the legacy checksum deliberately omits several read
	// fields (see above).
	util
		.addEnumAsInt(m_iAdvisorType, L"Advisor")
		.add(m_iAIWeight, L"iAIWeight")
		.add(m_iAITradeModifier, L"iAITradeModifier")
		.add(m_iResearchCost, L"iCost")
		.addEnumAsInt(m_iEra, L"Era")
		.add(m_iTradeRoutes, L"iTradeRoutes")
		.add(m_iFeatureProductionModifier, L"iFeatureProductionModifier")
		.add(m_iWorkerSpeedModifier, L"iWorkerSpeedModifier")
		.add(m_iMaintenanceModifier, L"iMaintenanceModifier")
		.add(m_iDistanceMaintenanceModifier, L"iDistanceMaintenanceModifier")
		.add(m_iNumCitiesMaintenanceModifier, L"iNumCitiesMaintenanceModifier")
		.add(m_iCoastalDistanceMaintenanceModifier, L"iCoastalDistanceMaintenanceModifier")
		.add(m_iHealth, L"iHealth")
		.add(m_iHappiness, L"iHappiness")
		.add(m_iFirstFreeTechs, L"iFirstFreeTechs")
		.add(m_iAssetValue, L"iAsset")
		.add(m_iPowerValue, L"iPower")
		.add(m_bRepeat, L"bRepeat")
		.add(m_bTrade, L"bTrade")
		.add(m_bDisable, L"bDisable")
		.add(m_bGoodyTech, L"bGoodyTech")
		.add(m_bExtraWaterSeeFrom, L"bExtraWaterSeeFrom")
		.add(m_bMapCentering, L"bMapCentering")
		.add(m_bMapVisible, L"bMapVisible")
		.add(m_bMapTrading, L"bMapTrading")
		.add(m_bTechTrading, L"bTechTrading")
		.add(m_bGoldTrading, L"bGoldTrading")
		.add(m_bOpenBordersTrading, L"bOpenBordersTrading")
		.add(m_bDefensivePactTrading, L"bDefensivePactTrading")
		.add(m_bPermanentAllianceTrading, L"bPermanentAllianceTrading")
		.add(m_bVassalStateTrading, L"bVassalTrading")
		.add(m_bBridgeBuilding, L"bBridgeBuilding")
		.add(m_bIrrigation, L"bIrrigation")
		.add(m_bIgnoreIrrigation, L"bIgnoreIrrigation")
		.add(m_bWaterWork, L"bWaterWork")
		.add(m_bRiverTrade, L"bRiverTrade")
		.add(m_bLanguage, L"bLanguage")
		.add(m_aeTerrainTrade, L"TerrainTrades")
		.addCommerce(m_piCommerceModifier, L"CommerceModifiers")
		.add(m_bCanPassPeaks, L"bCanPassPeaks")
		.add(m_bMoveFastPeaks, L"bMoveFastPeaks")
		.add(m_bCanFoundOnPeaks, L"bCanFoundOnPeaks")
		.add(m_bEmbassyTrading, L"bEmbassyTrading")
		.add(m_bEnableDarkAges, L"bEnableDarkAges")
		.add(m_bRebaseAnywhere, L"bRebaseAnywhere")
		.add(m_bEnablesDesertFarming, L"bAllowsDesertFarming")
		.add(m_iInflationModifier, L"iInflationModifier")
		.add(m_iGlobalTradeModifier, L"iGlobalTradeModifier")
		.add(m_iGlobalForeignTradeModifier, L"iGlobalForeignTradeModifier")
		.add(m_iTradeMissionModifier, L"iTradeMissionModifier")
		.add(m_iCorporationRevenueModifier, L"iCorporationRevenueModifier")
		.add(m_iCorporationMaintenanceModifier, L"iCorporationMaintenanceModifier")
		.addEnumAsInt(m_iPrereqGameOption, L"PrereqGameOption")
		.add(m_aiCategories, L"Categories")
		.add(m_bGlobal, L"bGlobal")
		// Not in the legacy checksum:
		.add(m_iGridX, L"iGridX")
		.add(m_iGridY, L"iGridY")
		.add(m_bDCMAirBombTech1, L"bDCMAirBombTech1")
		.add(m_bDCMAirBombTech2, L"bDCMAirBombTech2")
		.add(m_szQuoteKey, L"Quote")
		.add(m_szSound, L"Sound")
		.add(m_szSoundMP, L"SoundMP")
	;
}


bool CvTechInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	// Hand-written: int FKs with delayed resolution (addEnumAsInt is immediate-only)
	pXML->GetOptionalChildXmlValByName(szTextVal, L"FirstFreeUnit");
	GC.addDelayedResolution((int*)&m_iFirstFreeUnit, szTextVal);

	pXML->GetOptionalChildXmlValByName(szTextVal, L"FirstFreeProphet");
	GC.addDelayedResolution((int*)&m_iFirstFreeProphet, szTextVal);

	// Hand-written: bool array via SetCommerce (no bool-array wrapper)
	if (pXML->TryMoveToXmlFirstChild(L"CommerceFlexible"))
	{
		pXML->SetCommerce(&m_pbCommerceFlexible);
		pXML->MoveToXmlParent();
	}
	else
	{
		SAFE_DELETE_ARRAY(m_pbCommerceFlexible);
	}

	// Hand-written: SetVariableListTagPair dynamic arrays
	pXML->SetVariableListTagPair(&m_piDomainExtraMoves, L"DomainExtraMoves", NUM_DOMAIN_TYPES);
	pXML->SetVariableListTagPair(&m_piFlavorValue, L"Flavors", GC.getNumFlavorTypes());
	pXML->SetVariableListTagPair(&m_piFreeSpecialistCount, L"FreeSpecialistCounts", GC.getNumSpecialistInfos());

	if(pXML->TryMoveToXmlFirstChild(L"PrereqBuildings"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"PrereqBuilding" );
		m_aPrereqBuilding.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"PrereqBuilding"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"BuildingType");
					pXML->GetChildXmlValByName(&(m_aPrereqBuilding[i].iMinimumRequired), L"iNumBuildingNeeded");
					GC.addDelayedResolution((int*)&(m_aPrereqBuilding[i].eBuilding), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"PrereqBuilding"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"PrereqOrBuildings"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"PrereqOrBuilding" );
		m_aPrereqOrBuilding.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"PrereqOrBuilding"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"BuildingType");
					pXML->GetChildXmlValByName(&(m_aPrereqOrBuilding[i].iMinimumRequired), L"iNumBuildingNeeded");
					GC.addDelayedResolution((int*)&(m_aPrereqOrBuilding[i].eBuilding), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"PrereqOrBuilding"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	// Hand-written: self-FK tech vectors with delayed resolution
	pXML->SetOptionalVectorWithDelayedResolution(m_piPrereqOrTechs, L"OrPreReqs");
	pXML->SetOptionalVectorWithDelayedResolution(m_piPrereqAndTechs, L"AndPreReqs");

	return true;
}


void CvTechInfo::copyNonDefaults(const CvTechInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	const bool bDefault = false;
	const int iDefault = 0;

	GC.copyNonDefaultDelayedResolution((int*)&m_iFirstFreeUnit, (int*)&pClassInfo->m_iFirstFreeUnit);
	GC.copyNonDefaultDelayedResolution((int*)&m_iFirstFreeProphet, (int*)&pClassInfo->m_iFirstFreeProphet);

	for ( int j = 0; j < NUM_COMMERCE_TYPES; j++)
	{
		if (isCommerceFlexible(j) == bDefault && pClassInfo->isCommerceFlexible(j) != bDefault)
		{
			if ( NULL == m_pbCommerceFlexible )
			{
				CvXMLLoadUtility::InitList(&m_pbCommerceFlexible,NUM_COMMERCE_TYPES,bDefault);
			}
			m_pbCommerceFlexible[j] = pClassInfo->isCommerceFlexible(j);
		}
	}
	for ( int j = 0; j < NUM_DOMAIN_TYPES; j++)
	{
		if ((m_piDomainExtraMoves == NULL || m_piDomainExtraMoves[j] == iDefault) &&
			pClassInfo->getDomainExtraMoves(j) != iDefault)
		{
			if ( m_piDomainExtraMoves == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piDomainExtraMoves,NUM_DOMAIN_TYPES,iDefault);
			}
			m_piDomainExtraMoves[j] = pClassInfo->getDomainExtraMoves(j);
		}
	}
	for ( int j = 0; j < GC.getNumFlavorTypes(); j++)
	{
		if ((m_piFlavorValue == NULL || m_piFlavorValue[j] == iDefault) &&
			pClassInfo->getFlavorValue(j) != iDefault)
		{
			if ( m_piFlavorValue == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piFlavorValue,GC.getNumFlavorTypes(),iDefault);
			}
			m_piFlavorValue[j] = pClassInfo->getFlavorValue(j);
		}
	}

	for ( int j = 0; j < GC.getNumSpecialistInfos(); j++)
	{
		if ((m_piFreeSpecialistCount == NULL ||m_piFreeSpecialistCount[j] == 0) &&
			pClassInfo->getFreeSpecialistCount(j) != 0)
		{
			if ( m_piFreeSpecialistCount == NULL )
			{
				CvXMLLoadUtility::InitList(&m_piFreeSpecialistCount,GC.getNumSpecialistInfos(),0);
			}
			m_piFreeSpecialistCount[j] = pClassInfo->getFreeSpecialistCount(j);
		}
	}

	if (getNumPrereqBuildings() == 0)
	{
		const int iNum = pClassInfo->getNumPrereqBuildings();
		m_aPrereqBuilding.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aPrereqBuilding[i].iMinimumRequired = pClassInfo->m_aPrereqBuilding[i].iMinimumRequired;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aPrereqBuilding[i].eBuilding), (int*)&(pClassInfo->m_aPrereqBuilding[i].eBuilding));
		}
	}

	if (getNumPrereqOrBuildings() == 0)
	{
		const int iNum = pClassInfo->getNumPrereqOrBuildings();
		m_aPrereqOrBuilding.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aPrereqOrBuilding[i].iMinimumRequired = pClassInfo->m_aPrereqOrBuilding[i].iMinimumRequired;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aPrereqOrBuilding[i].eBuilding), (int*)&(pClassInfo->m_aPrereqOrBuilding[i].eBuilding));
		}
	}
	GC.copyNonDefaultDelayedResolutionVector(m_piPrereqOrTechs, pClassInfo->getPrereqOrTechs());
	GC.copyNonDefaultDelayedResolutionVector(m_piPrereqAndTechs, pClassInfo->getPrereqAndTechs());
}


// Explicit (not delegated to CvInfoUtil) on purpose: hand-written fields (FirstFreeUnit/Prophet,
// DomainExtraMoves, Flavors, prereq tech/building vectors, CommerceFlexible, FreeSpecialistCounts)
// sit mid-order, and the legacy checksum deliberately omits several read fields (iGridX/iGridY,
// bDCMAirBombTech1/2). Keep this byte-identical to the legacy order.
void CvTechInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	CheckSum(iSum, m_iAdvisorType);
	CheckSum(iSum, m_iAIWeight);
	CheckSum(iSum, m_iAITradeModifier);
	CheckSum(iSum, m_iResearchCost);
	CheckSum(iSum, m_iEra);
	CheckSum(iSum, m_iTradeRoutes);
	CheckSum(iSum, m_iFeatureProductionModifier);
	CheckSum(iSum, m_iWorkerSpeedModifier);
	CheckSum(iSum, m_iMaintenanceModifier);
	CheckSum(iSum, m_iDistanceMaintenanceModifier);
	CheckSum(iSum, m_iNumCitiesMaintenanceModifier);
	CheckSum(iSum, m_iCoastalDistanceMaintenanceModifier);
	CheckSum(iSum, m_iFirstFreeUnit);
	CheckSum(iSum, m_iFirstFreeProphet);
	CheckSum(iSum, m_iHealth);
	CheckSum(iSum, m_iHappiness);
	CheckSum(iSum, m_iFirstFreeTechs);
	CheckSum(iSum, m_iAssetValue);
	CheckSum(iSum, m_iPowerValue);

	CheckSum(iSum, m_bRepeat);
	CheckSum(iSum, m_bTrade);
	CheckSum(iSum, m_bDisable);
	CheckSum(iSum, m_bGoodyTech);
	CheckSum(iSum, m_bExtraWaterSeeFrom);
	CheckSum(iSum, m_bMapCentering);
	CheckSum(iSum, m_bMapVisible);
	CheckSum(iSum, m_bMapTrading);
	CheckSum(iSum, m_bTechTrading);
	CheckSum(iSum, m_bGoldTrading);
	CheckSum(iSum, m_bOpenBordersTrading);
	CheckSum(iSum, m_bDefensivePactTrading);
	CheckSum(iSum, m_bPermanentAllianceTrading);
	CheckSum(iSum, m_bVassalStateTrading);
	CheckSum(iSum, m_bBridgeBuilding);
	CheckSum(iSum, m_bIrrigation);
	CheckSum(iSum, m_bIgnoreIrrigation);
	CheckSum(iSum, m_bWaterWork);
	CheckSum(iSum, m_bRiverTrade);
	CheckSum(iSum, m_bLanguage);

	CheckSum(iSum, m_piDomainExtraMoves, NUM_DOMAIN_TYPES);
	CheckSum(iSum, m_piFlavorValue, GC.getNumFlavorTypes());
	CheckSumC(iSum, m_piPrereqOrTechs);
	CheckSumC(iSum, m_piPrereqAndTechs);
	CheckSum(iSum, m_pbCommerceFlexible, NUM_COMMERCE_TYPES);
	CheckSumC(iSum, m_aeTerrainTrade);
	//ls612: Tech Commerce Modifiers
	CheckSum(iSum, m_piCommerceModifier, NUM_COMMERCE_TYPES);

	CheckSum(iSum, m_bCanPassPeaks);
	CheckSum(iSum, m_bMoveFastPeaks);
	CheckSum(iSum, m_bCanFoundOnPeaks);
	CheckSum(iSum, m_bEmbassyTrading);
	CheckSum(iSum, m_bEnableDarkAges);
	CheckSum(iSum, m_bRebaseAnywhere);
	CheckSum(iSum, m_bEnablesDesertFarming);

	CheckSum(iSum, m_iInflationModifier);
	CheckSum(iSum, m_iGlobalTradeModifier);
	CheckSum(iSum, m_iGlobalForeignTradeModifier);
	CheckSum(iSum, m_iTradeMissionModifier);
	CheckSum(iSum, m_iCorporationRevenueModifier);
	CheckSum(iSum, m_iCorporationMaintenanceModifier);
	CheckSum(iSum, m_iPrereqGameOption);

	CheckSum(iSum, m_piFreeSpecialistCount, GC.getNumSpecialistInfos());
	CheckSumC(iSum, m_aiCategories);

	const int iNumElements = m_aPrereqBuilding.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aPrereqBuilding[i].eBuilding);
		CheckSum(iSum, m_aPrereqBuilding[i].iMinimumRequired);
	}
	//TB Tech Tags
	CheckSum(iSum, m_bGlobal);
	//TB Tech Tags end
}


// Toffer - Derived tech cache
void CvTechInfo::setLeadsTo(const TechTypes eTech)
{
	m_leadsTo.insert(eTech);
}


void CvTechInfo::doPostLoadCaching(uint32_t iThis)
{
	PROFILE_EXTRA_FUNC();
	foreach_(const TechTypes ePrereq, getPrereqOrTechs())
	{
		GC.getTechInfo(ePrereq).setLeadsTo((TechTypes)iThis);
	}
	foreach_(const TechTypes ePrereq, getPrereqAndTechs())
	{
		GC.getTechInfo(ePrereq).setLeadsTo((TechTypes)iThis);
	}
}

