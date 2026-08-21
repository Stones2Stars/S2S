//------------------------------------------------------------------------------------------------
//  FILE:    CvSpecialistInfo.cpp
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
#include "CvSpecialistInfo.h"


//======================================================================================================
//					CvSpecialistInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvSpecialistInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvSpecialistInfo::CvSpecialistInfo() :
// m_iMissionType is a non-XML, runtime-assigned field (see setMissionType); m_iGreatPeopleUnitType
// (delayed-resolution int FK), m_piFlavorValue (SetVariableListTagPair) and the
// UnitCombatExperienceTypes pair stay hand-written. Every other XML-backed field is declared in
// getDataMembers() and defaulted by initDataMembers() below.
 m_iGreatPeopleUnitType(NO_UNIT)
,m_iMissionType(NO_MISSION)
,m_piFlavorValue(NULL)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvSpecialistInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvSpecialistInfo::~CvSpecialistInfo()
{
	PROFILE_EXTRA_FUNC();
	// Owns the declared yield/commerce arrays and the tech maps' delayed-resolution registrations.
	CvInfoUtil(this).uninitDataMembers();
	SAFE_DELETE_ARRAY(m_piFlavorValue);

	for (int i=0; i<(int)m_aUnitCombatExperienceTypes.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aUnitCombatExperienceTypes[i]));
	}

	for (int i=0; i<(int)m_aUnitCombatExperienceTypesNull.size(); i++)
	{
		GC.removeDelayedResolution((int*)&(m_aUnitCombatExperienceTypesNull[i]));
	}

	GC.removeDelayedResolution((int*)&m_iGreatPeopleUnitType);
}


int CvSpecialistInfo::getGreatPeopleUnitType() const
{
	return m_iGreatPeopleUnitType;
}


int CvSpecialistInfo::getGreatPeopleRateChange() const
{
	return m_iGreatPeopleRateChange;
}


int CvSpecialistInfo::getMissionType() const
{
	return m_iMissionType;
}


void CvSpecialistInfo::setMissionType(int iNewType)
{
	m_iMissionType = iNewType;
}


bool CvSpecialistInfo::isVisible() const
{
	return m_bVisible;
}


int CvSpecialistInfo::getExperience() const
{
	return m_iExperience;
}


// Arrays

int CvSpecialistInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldChange ? m_piYieldChange[i] : 0;
}


const int* CvSpecialistInfo::getYieldChangeArray() const
{
	return m_piYieldChange;
}


int CvSpecialistInfo::getCommerceChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCommerceChange ? m_piCommerceChange[i] : 0;
}


int CvSpecialistInfo::getFlavorValue(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFlavorTypes(), i);
	return m_piFlavorValue ? m_piFlavorValue[i] : 0;
}


const char* CvSpecialistInfo::getTexture() const
{
	return m_szTexture;
}


int CvSpecialistInfo::getHealthPercent() const
{
	return m_iHealthPercent;
}

int CvSpecialistInfo::getHappinessPercent() const
{
	return m_iHappinessPercent;
}


bool CvSpecialistInfo::isSlave() const
{
	return m_bSlave;
}


//TB Specialist Tags
int CvSpecialistInfo::getInsidiousness() const
{
	return m_iInsidiousness;
}


int CvSpecialistInfo::getInvestigation() const
{
	return m_iInvestigation;
}


//int CvSpecialistInfo::getNumTechHappinessTypes() const
//{
//	return (int)m_aTechHappinessTypes.size();
//}

int CvSpecialistInfo::getTechHappiness(TechTypes eTech) const
{
	FASSERT_BOUNDS(0, GC.getNumTechInfos(), eTech);
	return m_aTechHappinessTypes.getValue(eTech);
}


//int CvSpecialistInfo::getNumTechHealthTypes() const
//{
//	return (int)m_aTechHealthTypes.size();
//}

int CvSpecialistInfo::getTechHealth(TechTypes eTech) const
{
	FASSERT_BOUNDS(0, GC.getNumTechInfos(), eTech);
	return m_aTechHealthTypes.getValue(eTech);
}


int CvSpecialistInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvSpecialistInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvSpecialistInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


int CvSpecialistInfo::getNumUnitCombatExperienceTypes() const
{
	return (int)m_aUnitCombatExperienceTypes.size();
}


const UnitCombatModifier& CvSpecialistInfo::getUnitCombatExperienceType(int iUnitCombat) const
{
	FASSERT_BOUNDS(0, (int)m_aUnitCombatExperienceTypes.size(), iUnitCombat);
	FASSERT_BOUNDS(0, (int)m_aUnitCombatExperienceTypesNull.size(), iUnitCombat);

	if (!GC.getGame().isOption(GAMEOPTION_UNIT_XP_FROM_SPECIALISTS) && isVisible())
	{
		return m_aUnitCombatExperienceTypesNull[iUnitCombat];
	}
	return m_aUnitCombatExperienceTypes[iUnitCombat];
}



void CvSpecialistInfo::getDataMembers(CvInfoUtil& util)
{
	// Kept hand-written: m_iGreatPeopleUnitType (delayed-resolution int FK - no wrapper yet),
	// m_piFlavorValue (SetVariableListTagPair dynamic array) and the UnitCombatExperienceTypes block
	// (one XML walk fills two parallel vectors - the real one and the zero-modifier Null twin - with
	// delayed-resolution pointers into both). m_iMissionType is runtime-assigned, not XML-backed.
	// getCheckSum stays explicit: the hand-written fields and the runtime m_iMissionType sit
	// mid-order in the legacy checksum.
	util
		.add(m_bVisible, L"bVisible")
		.add(m_iGreatPeopleRateChange, L"iGreatPeopleRateChange")
		.add(m_aiCategories, L"Categories")
		.addYields(m_piYieldChange, L"Yields")
		.addCommerce(m_piCommerceChange, L"Commerces")
		.add(m_iExperience, L"iExperience")
		.add(m_PropertyManipulators)
		.add(m_iHealthPercent, L"iHealthPercent")
		.add(m_iHappinessPercent, L"iHappinessPercent")
		.add(m_bSlave, L"bSlave")
		.add(m_iInsidiousness, L"iInsidiousness")
		.add(m_iInvestigation, L"iInvestigation")
		.add(m_aTechHappinessTypes, L"TechHappinessTypes")
		.add(m_aTechHealthTypes, L"TechHealthTypes")
		.add(m_szTexture, L"Texture")
	;
}


// read from xml
//
bool CvSpecialistInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->GetOptionalChildXmlValByName(szTextVal, L"GreatPeopleUnitType");
	GC.addDelayedResolution((int*)&m_iGreatPeopleUnitType, szTextVal);

	pXML->SetVariableListTagPair(&m_piFlavorValue, L"Flavors", GC.getNumFlavorTypes());

	if(pXML->TryMoveToXmlFirstChild(L"UnitCombatExperienceTypes"))
	{
		int i = 0;
		int iNum = pXML->GetXmlChildrenNumber(L"UnitCombatExperienceType" );
		m_aUnitCombatExperienceTypes.resize(iNum); // Important to keep the delayed resolution pointers correct
		m_aUnitCombatExperienceTypesNull.resize(iNum);

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"UnitCombatExperienceType"))
			{
				do
				{
					pXML->GetChildXmlValByName(szTextVal, L"UnitCombatType");
					pXML->GetChildXmlValByName(&(m_aUnitCombatExperienceTypes[i].iModifier), L"iModifier");
					GC.addDelayedResolution((int*)&(m_aUnitCombatExperienceTypes[i].eUnitCombat), szTextVal);
					GC.addDelayedResolution((int*)&(m_aUnitCombatExperienceTypesNull[i].eUnitCombat), szTextVal);
					m_aUnitCombatExperienceTypesNull[i].iModifier = 0;
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"UnitCombatExperienceType"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}
	return true;
}


void CvSpecialistInfo::copyNonDefaults(const CvSpecialistInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	// NOTE: the legacy copy dereferenced m_piCommerceChange without a NULL check (a latent crash for
	// modular merges when this definition has no Commerces tag); the wrapper handles NULL correctly.
	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	GC.copyNonDefaultDelayedResolution((int*)&m_iGreatPeopleUnitType, (int*)&pClassInfo->m_iGreatPeopleUnitType);

	const int iDefault = 0;
	for ( int i = 0; i < GC.getNumFlavorTypes(); i++ )
	{
		if ( getFlavorValue(i) == iDefault && pClassInfo->getFlavorValue(i) != iDefault )
		{
			if ( NULL == m_piFlavorValue )
			{
				CvXMLLoadUtility::InitList(&m_piFlavorValue,GC.getNumFlavorTypes(),iDefault);
			}
			m_piFlavorValue[i] = pClassInfo->getFlavorValue(i);
		}
	}

	if (getNumUnitCombatExperienceTypes() == 0)
	{
		const int iNum = pClassInfo->getNumUnitCombatExperienceTypes();
		m_aUnitCombatExperienceTypes.resize(iNum);
		m_aUnitCombatExperienceTypesNull.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aUnitCombatExperienceTypes[i].iModifier = pClassInfo->m_aUnitCombatExperienceTypes[i].iModifier;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aUnitCombatExperienceTypes[i].eUnitCombat), (int*)&(pClassInfo->m_aUnitCombatExperienceTypes[i].eUnitCombat));
			m_aUnitCombatExperienceTypesNull[i].iModifier = 0;
			GC.copyNonDefaultDelayedResolution((int*)&(m_aUnitCombatExperienceTypesNull[i].eUnitCombat), (int*)&(pClassInfo->m_aUnitCombatExperienceTypesNull[i].eUnitCombat));
		}
	}
}


void CvSpecialistInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	// NOTE: kept explicit (not delegated to CvInfoUtil) to preserve the exact legacy checksum: the
	// hand-written m_iGreatPeopleUnitType/m_piFlavorValue/UnitCombatExperienceTypes and the runtime
	// m_iMissionType sit mid-order between declared fields.
	CheckSum(iSum, m_iGreatPeopleUnitType);
	CheckSum(iSum, m_iGreatPeopleRateChange);
	CheckSum(iSum, m_iMissionType);
	CheckSum(iSum, m_iHealthPercent);
	CheckSum(iSum, m_iHappinessPercent);
	CheckSum(iSum, m_bSlave);
	CheckSum(iSum, m_iExperience);
	m_PropertyManipulators.getCheckSum(iSum);
	CheckSum(iSum, m_bVisible);
	CheckSum(iSum, m_piYieldChange, NUM_YIELD_TYPES);
	CheckSum(iSum, m_piCommerceChange, NUM_COMMERCE_TYPES);
	CheckSum(iSum, m_piFlavorValue, GC.getNumFlavorTypes());
	//Team Project (1)
	//TB Specialist Tags
	// int
	CheckSum(iSum, m_iInsidiousness);
	CheckSum(iSum, m_iInvestigation);
	CheckSumC(iSum, m_aTechHappinessTypes);
	CheckSumC(iSum, m_aTechHealthTypes);
	CheckSumC(iSum, m_aiCategories);

	int iNumElements = m_aUnitCombatExperienceTypes.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aUnitCombatExperienceTypes[i].eUnitCombat);
		CheckSum(iSum, m_aUnitCombatExperienceTypes[i].iModifier);
	}

	iNumElements = m_aUnitCombatExperienceTypesNull.size();
	for (int i = 0; i < iNumElements; ++i)
	{
		CheckSum(iSum, m_aUnitCombatExperienceTypesNull[i].eUnitCombat);
		CheckSum(iSum, m_aUnitCombatExperienceTypesNull[i].iModifier);
	}

}

