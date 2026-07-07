//------------------------------------------------------------------------------------------------
//  FILE:    CvProjectInfo.cpp
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
#include "CvProjectInfo.h"


//======================================================================================================
//					CvProjectInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvProjectInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvProjectInfo::CvProjectInfo() :
	// m_iAnyoneProjectPrereq and the ProjectsNeeded pair are readPass3 machinery; the
	// SetVariableListTagPair arrays stay hand-written. Every other XML-backed field is declared in
	// getDataMembers() and defaulted by initDataMembers() below.
	m_iAnyoneProjectPrereq(NO_PROJECT),
	m_piBonusProductionModifier(NULL),
	m_piVictoryThreshold(NULL),
	m_piVictoryMinThreshold(NULL),
	m_piProjectsNeeded(NULL)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvProjectInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvProjectInfo::~CvProjectInfo()
{
	CvInfoUtil(this).uninitDataMembers(); // owns the declared CommerceModifiers array
	SAFE_DELETE_ARRAY(m_piBonusProductionModifier);
	SAFE_DELETE_ARRAY(m_piVictoryThreshold);
	SAFE_DELETE_ARRAY(m_piVictoryMinThreshold);
	SAFE_DELETE_ARRAY(m_piProjectsNeeded);
}


int CvProjectInfo::getVictoryPrereq() const
{
	return m_iVictoryPrereq;
}


int CvProjectInfo::getAnyoneProjectPrereq() const
{
	return m_iAnyoneProjectPrereq;
}


void CvProjectInfo::setAnyoneProjectPrereq(int i)
{
	m_iAnyoneProjectPrereq = i;
}


int CvProjectInfo::getMaxGlobalInstances() const
{
	return m_iMaxGlobalInstances;
}


int CvProjectInfo::getMaxTeamInstances() const
{
	return m_iMaxTeamInstances;
}


int CvProjectInfo::getProductionCost() const
{
	return m_iProductionCost;
}


int CvProjectInfo::getNukeInterception() const
{
	return m_iNukeInterception;
}


int CvProjectInfo::getTechShare() const
{
	return m_iTechShare;
}


//DPII < Maintenance Modifiers >
int CvProjectInfo::getGlobalMaintenanceModifier() const
{
	return m_iGlobalMaintenanceModifier;
}


int CvProjectInfo::getDistanceMaintenanceModifier() const
{
	return m_iDistanceMaintenanceModifier;
}


int CvProjectInfo::getNumCitiesMaintenanceModifier() const
{
	return m_iNumCitiesMaintenanceModifier;

}


int CvProjectInfo::getConnectedCityMaintenanceModifier() const
{
	return m_iConnectedCityMaintenanceModifier;
}

//DPII < Maintenance Modifiers >

int CvProjectInfo::getEveryoneSpecialUnit() const
{
	return m_iEveryoneSpecialUnit;
}


int CvProjectInfo::getEveryoneSpecialBuilding() const
{
	return m_iEveryoneSpecialBuilding;
}


int CvProjectInfo::getVictoryDelayPercent() const
{
	return m_iVictoryDelayPercent;
}


int CvProjectInfo::getSuccessRate() const
{
	return m_iSuccessRate;
}


bool CvProjectInfo::isSpaceship() const
{
	return m_bSpaceship;
}


bool CvProjectInfo::isAllowsNukes() const
{
	return m_bAllowsNukes;
}


const char* CvProjectInfo::getMovieArtDef() const
{
	return m_szMovieArtDef;
}


const char* CvProjectInfo::getCreateSound() const
{
	return m_szCreateSound;
}


void CvProjectInfo::setCreateSound(const char* szVal)
{
	m_szCreateSound = szVal;
}


// Arrays

int CvProjectInfo::getBonusProductionModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), i);
	return m_piBonusProductionModifier ? m_piBonusProductionModifier[i] : 0;
}


int CvProjectInfo::getVictoryThreshold(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumVictoryInfos(), i);
	return m_piVictoryThreshold ? m_piVictoryThreshold[i] : 0;
}


int CvProjectInfo::getVictoryMinThreshold(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumVictoryInfos(), i);

	if (m_piVictoryMinThreshold && m_piVictoryMinThreshold[i] != 0)
	{
		return m_piVictoryMinThreshold[i];
	}
	return getVictoryThreshold(i);
}


int CvProjectInfo::getProjectsNeeded(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumProjectInfos(), i);
	return m_piProjectsNeeded ? m_piProjectsNeeded[i] : false;
}


int CvProjectInfo::getWorldHappiness() const
{
	return m_iWorldHappiness;
}


int CvProjectInfo::getGlobalHappiness() const
{
	return m_iGlobalHappiness;
}


int CvProjectInfo::getWorldHealth() const
{
	return m_iWorldHealth;
}


int CvProjectInfo::getGlobalHealth() const
{
	return m_iGlobalHealth;
}


int CvProjectInfo::getWorldTradeRoutes() const
{
	return m_iWorldTradeRoutes;
}


int CvProjectInfo::getInflationModifier() const
{
	return m_iInflationModifier;
}


bool CvProjectInfo::isTechShareWithHalfCivs() const
{
	return m_bTechShareWithHalfCivs;
}


int CvProjectInfo::getCommerceModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCommerceModifier ? m_piCommerceModifier[i] : 0;
}


int* CvProjectInfo::getCommerceModifierArray() const
{
	return m_piCommerceModifier;
}


int CvProjectInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvProjectInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvProjectInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


int CvProjectInfo::getProjectsNeededVectorSize() const						{ return m_aszProjectsNeededforPass3.size(); }

CvString CvProjectInfo::getProjectsNeededNamesVectorElement(int i) const	{ return m_aszProjectsNeededforPass3[i]; }

int CvProjectInfo::getProjectsNeededValuesVectorElement(int i) const		{ return m_aiProjectsNeededforPass3[i]; }



void CvProjectInfo::getDataMembers(CvInfoUtil& util)
{
	// Kept hand-written: the SetVariableListTagPair dynamic arrays (m_piBonusProductionModifier and
	// the victory-keyed m_piVictoryThreshold/m_piVictoryMinThreshold - no wrapper) plus the
	// PrereqProjects/AnyonePrereqProject readPass3 machinery (m_piProjectsNeeded,
	// m_iAnyoneProjectPrereq). getCheckSum stays explicit: the hand-written and pass3 fields sit
	// mid-order in the legacy checksum.
	util
		.addEnumAsInt(m_iVictoryPrereq, L"VictoryPrereq")
		.addEnum(m_iTechPrereq, L"TechPrereq")
		.add(m_iMaxGlobalInstances, L"iMaxGlobalInstances", -1)
		.add(m_iMaxTeamInstances, L"iMaxTeamInstances", -1)
		.add(m_iProductionCost, L"iCost")
		.add(m_iNukeInterception, L"iNukeInterception")
		.add(m_iTechShare, L"iTechShare")
		.add(m_iGlobalMaintenanceModifier, L"iGlobalMaintenanceModifier")
		.add(m_iDistanceMaintenanceModifier, L"iDistanceMaintenanceModifier")
		.add(m_iNumCitiesMaintenanceModifier, L"iNumCitiesMaintenanceModifier")
		.add(m_iConnectedCityMaintenanceModifier, L"iConnectedCityMaintenanceModifier")
		.addEnumAsInt(m_iEveryoneSpecialUnit, L"EveryoneSpecialUnit")
		.addEnumAsInt(m_iEveryoneSpecialBuilding, L"EveryoneSpecialBuilding")
		.add(m_iVictoryDelayPercent, L"iVictoryDelayPercent")
		.add(m_iSuccessRate, L"iSuccessRate")
		.add(m_bSpaceship, L"bSpaceship")
		.add(m_bAllowsNukes, L"bAllowsNukes")
		.add(m_iWorldHappiness, L"iWorldHappiness")
		.add(m_iGlobalHappiness, L"iGlobalHappiness")
		.add(m_iWorldHealth, L"iWorldHealth")
		.add(m_iGlobalHealth, L"iGlobalHealth")
		.add(m_iWorldTradeRoutes, L"iWorldTradeRoutes")
		.add(m_bTechShareWithHalfCivs, L"bTechShareWithHalfCivs")
		.add(m_iInflationModifier, L"iInflationModifier")
		.addCommerce(m_piCommerceModifier, L"CommerceModifiers")
		.add(m_aiCategories, L"Categories")
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")
		.add(m_szMovieArtDef, L"MovieDefineTag")
		.add(m_szCreateSound, L"CreateSound")
	;
}


bool CvProjectInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->SetVariableListTagPair(&m_piBonusProductionModifier, L"BonusProductionModifiers", GC.getNumBonusInfos());
	pXML->SetVariableListTagPair(&m_piVictoryThreshold, L"VictoryThresholds",  GC.getNumVictoryInfos());
	pXML->SetVariableListTagPair(&m_piVictoryMinThreshold, L"VictoryMinThresholds",  GC.getNumVictoryInfos());

	if (pXML->TryMoveToXmlFirstChild(L"PrereqProjects"))
	{
		int iNumSibs = pXML->GetXmlChildrenNumber();
		int iTemp = 0;
		if (iNumSibs > 0)
		{
			if (pXML->TryMoveToXmlFirstChild())
			{
				for (int i=0;i<iNumSibs;i++)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						m_aszProjectsNeededforPass3.push_back(szTextVal);
						pXML->GetNextXmlVal(&iTemp);
						m_aiProjectsNeededforPass3.push_back(iTemp);
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

	pXML->GetOptionalChildXmlValByName(szTextVal, L"AnyonePrereqProject");
	m_aszExtraXMLforPass3.push_back(szTextVal);

	return true;
}



void CvProjectInfo::copyNonDefaults(const CvProjectInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const int iDefault = 0;
	const int iTextDefault = -1;  //all integers which are TEXT_KEYS in the xml are -1 by default

	CvInfoBase::copyNonDefaults(pClassInfo);

	// NOTE: the legacy copy dereferenced m_piCommerceModifier without a NULL check (a latent crash
	// for modular merges when this definition has no CommerceModifiers tag); the wrapper handles
	// NULL correctly.
	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	for ( int i = 0; i < GC.getNumBonusInfos(); i++)
	{
		if (getBonusProductionModifier(i) == iDefault && pClassInfo->getBonusProductionModifier(i) != iDefault)
		{
			if ( NULL == m_piBonusProductionModifier )
			{
				CvXMLLoadUtility::InitList(&m_piBonusProductionModifier,GC.getNumBonusInfos(),iDefault);
			}
			m_piBonusProductionModifier[i] = pClassInfo->getBonusProductionModifier(i);
		}
	}
	for ( int i = 0; i < GC.getNumVictoryInfos(); i++)
	{
		if (getVictoryThreshold(i) == iDefault && pClassInfo->getVictoryThreshold(i) != iDefault)
		{
			if ( NULL == m_piVictoryThreshold )
			{
				CvXMLLoadUtility::InitList(&m_piVictoryThreshold,GC.getNumVictoryInfos(),iDefault);
			}
			m_piVictoryThreshold[i] = pClassInfo->getVictoryThreshold(i);
		}
		if (getVictoryMinThreshold(i) == iDefault && pClassInfo->getVictoryMinThreshold(i) != iDefault)
		{
			if ( NULL == m_piVictoryMinThreshold )
			{
				CvXMLLoadUtility::InitList(&m_piVictoryMinThreshold,GC.getNumVictoryInfos(),iDefault);
			}
			m_piVictoryMinThreshold[i] = pClassInfo->getVictoryMinThreshold(i);
		}
	}

	if (m_iAnyoneProjectPrereq == iTextDefault) m_iAnyoneProjectPrereq = pClassInfo->getAnyoneProjectPrereq();

	for ( int i = 0; i < pClassInfo->getProjectsNeededVectorSize(); i++ )
	{
		m_aiProjectsNeededforPass3.push_back(pClassInfo->getProjectsNeededValuesVectorElement(i));
		m_aszProjectsNeededforPass3.push_back(pClassInfo->getProjectsNeededNamesVectorElement(i));
	}
}


bool CvProjectInfo::readPass3()
{
	PROFILE_EXTRA_FUNC();
	m_piProjectsNeeded = new int[GC.getNumProjectInfos()];
	for (int iI = 0; iI < GC.getNumProjectInfos(); iI++)
	{
		m_piProjectsNeeded[iI] = 0;
	}
	if (!m_aiProjectsNeededforPass3.empty() && !m_aszProjectsNeededforPass3.empty())
	{
		int iNumLoad = m_aiProjectsNeededforPass3.size();
		for(int iI = 0; iI < iNumLoad; iI++)
		{
			int iTempIndex = GC.getInfoTypeForString(m_aszProjectsNeededforPass3[iI]);
			if (iTempIndex >= 0 && iTempIndex < GC.getNumEventInfos())
				m_piProjectsNeeded[iTempIndex] = m_aiProjectsNeededforPass3[iI];
		}
		m_aszProjectsNeededforPass3.clear();
		m_aiProjectsNeededforPass3.clear();
	}

	if (m_aszExtraXMLforPass3.size() < 1)
	{
		FErrorMsg("error");
		return false;
	}
	m_iAnyoneProjectPrereq = GC.getInfoTypeForString(m_aszExtraXMLforPass3[0]);

	return true;
}



void CvProjectInfo::getCheckSum(uint32_t &iSum) const
{
	// NOTE: kept explicit (not delegated to CvInfoUtil) to preserve the exact legacy checksum: the
	// hand-written SetVariableListTagPair arrays and the pass3 fields (m_iAnyoneProjectPrereq,
	// m_piProjectsNeeded) sit mid-order between declared fields.
	CheckSum(iSum, m_iWorldHappiness);
	CheckSum(iSum, m_iGlobalHappiness);
	CheckSum(iSum, m_iWorldHealth);
	CheckSum(iSum, m_iGlobalHealth);
	CheckSum(iSum, m_iWorldTradeRoutes);
	CheckSum(iSum, m_iInflationModifier);
	CheckSum(iSum, m_bTechShareWithHalfCivs);

	CheckSum(iSum, m_piCommerceModifier, NUM_COMMERCE_TYPES);

	CheckSum(iSum, m_iVictoryPrereq);
	CheckSum(iSum, m_iTechPrereq);
	CheckSum(iSum, m_iAnyoneProjectPrereq);
	CheckSum(iSum, m_iMaxGlobalInstances);
	CheckSum(iSum, m_iMaxTeamInstances);
	CheckSum(iSum, m_iProductionCost);
	CheckSum(iSum, m_iNukeInterception);
	CheckSum(iSum, m_iTechShare);

	CheckSum(iSum, m_iGlobalMaintenanceModifier);
	CheckSum(iSum, m_iDistanceMaintenanceModifier);
	CheckSum(iSum, m_iNumCitiesMaintenanceModifier);
	CheckSum(iSum, m_iConnectedCityMaintenanceModifier);

	CheckSum(iSum, m_iEveryoneSpecialUnit);
	CheckSum(iSum, m_iEveryoneSpecialBuilding);
	CheckSum(iSum, m_iVictoryDelayPercent);
	CheckSum(iSum, m_iSuccessRate);

	CheckSum(iSum, m_bSpaceship);
	CheckSum(iSum, m_bAllowsNukes);

	// Arrays

	CheckSum(iSum, m_piBonusProductionModifier, GC.getNumBonusInfos());
	CheckSum(iSum, m_piVictoryThreshold, GC.getNumVictoryInfos());
	CheckSum(iSum, m_piVictoryMinThreshold, GC.getNumVictoryInfos());
	CheckSum(iSum, m_piProjectsNeeded, GC.getNumProjectInfos());

	// Vectors

	CheckSumC(iSum, m_aiCategories);
	CheckSumC(iSum, m_aeMapCategoryTypes);
}

