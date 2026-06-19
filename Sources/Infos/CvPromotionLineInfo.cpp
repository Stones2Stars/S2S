//------------------------------------------------------------------------------------------------
//  FILE:    CvPromotionLineInfo.cpp
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
#include "CvPromotionLineInfo.h"

//TB Promotion Line Info
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvPromotionLineInfo
//
//  DESC:   Contains info about Promotion Lines
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CvPromotionLineInfo::CvPromotionLineInfo()
{
	CvInfoUtil(this).initDataMembers();
}

CvPromotionLineInfo::~CvPromotionLineInfo()
{
	CvInfoUtil(this).uninitDataMembers();
}

void CvPromotionLineInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order; Categories is parked last because the legacy
	// checksum deliberately omits it. The class keeps an explicit getCheckSum: the two pair-vectors
	// (SetOptionalPairVector has no wrapper) close out the legacy checksum, and Categories must
	// stay out of it.
	util
		.addEnum(m_ePrereqTech, L"PrereqTech")
		.addEnum(m_eObsoleteTech, L"ObsoleteTech")
		.add(m_bBuildUp, L"bBuildUp")
		.add(m_bPoison, L"bPoison")
		.addEnum(m_ePropertyType, L"PropertyType")
		.add(m_aiUnitCombatPrereqTypes, L"UnitCombatPrereqTypes")
		.add(m_aiNotOnUnitCombatTypes, L"NotOnUnitCombatTypes")
		.add(m_aiNotOnDomainTypes, L"NotOnDomainTypes")
		.add(m_aiOnGameOptions, L"OnGameOptions")
		.add(m_aiNotOnGameOptions, L"NotOnGameOptions")
		// Not in the legacy checksum:
		.add(m_aiCategories, L"Categories")
	;
}

bool CvPromotionLineInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	// Hand-written: int vectors utilizing pairing (no CvInfoUtil wrapper for plain pair-vectors)
	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aUnitCombatContractChanceChanges, L"UnitCombatContractChanceChanges");

	pXML->SetOptionalPairVector<TechModifierArray, TechTypes, int>(&m_aTechContractChanceChanges, L"TechContractChanceChanges");

	return true;
}

void CvPromotionLineInfo::copyNonDefaults(const CvPromotionLineInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	// Hand-written: int vector utilizing pairing without delayed resolution
	if (getNumUnitCombatContractChanceChanges()==0)
	{
		for (int i=0; i < pClassInfo->getNumUnitCombatContractChanceChanges(); i++)
		{
			const UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			const int iChange = pClassInfo->getUnitCombatContractChanceChange(i);
			m_aUnitCombatContractChanceChanges.push_back(std::make_pair(eUnitCombat, iChange));
		}
	}

	if (getNumTechContractChanceChanges()==0)
	{
		for (int i=0; i < pClassInfo->getNumTechContractChanceChanges(); i++)
		{
			const TechTypes eTech = ((TechTypes)i);
			const int iChange = pClassInfo->getTechContractChanceChange(i);
			m_aTechContractChanceChanges.push_back(std::make_pair(eTech, iChange));
		}
	}
}

// Explicit (not delegated to CvInfoUtil) on purpose: the hand-written pair-vectors close out the
// legacy checksum, and the legacy checksum deliberately omits the (read) Categories vector.
// Keep this byte-identical to the legacy order.
void CvPromotionLineInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_ePrereqTech);
	CheckSum(iSum, m_eObsoleteTech);
	CheckSum(iSum, m_bBuildUp);
	CheckSum(iSum, m_bPoison);

	CheckSum(iSum, (int)m_ePropertyType);
	// bool vectors without delayed resolution
	CheckSumC(iSum, m_aiUnitCombatPrereqTypes);
	CheckSumC(iSum, m_aiNotOnUnitCombatTypes);
	CheckSumC(iSum, m_aiNotOnDomainTypes);
	CheckSumC(iSum, m_aiOnGameOptions);
	CheckSumC(iSum, m_aiNotOnGameOptions);
	// int vector utilizing pairing without delayed resolution
	CheckSumC(iSum, m_aUnitCombatContractChanceChanges);
	CheckSumC(iSum, m_aTechContractChanceChanges);
}

void CvPromotionLineInfo::doPostLoadCaching(uint32_t iThis)
{
	PROFILE_EXTRA_FUNC();
	//Establish speedy promotion & Building reference by line
	m_aiPromotions.clear();
	m_aiBuildings.clear();

	for ( int i = 0; i < GC.getNumPromotionInfos(); i++)
	{
		if (GC.getPromotionInfo((PromotionTypes)i).getPromotionLine() == iThis)
		{
			m_aiPromotions.push_back(i);
		}
	}
	for ( int i = 0; i < GC.getNumBuildingInfos(); i++)
	{
		if (GC.getBuildingInfo((BuildingTypes)i).getPromotionLineType() == iThis)
		{
			m_aiBuildings.push_back(i);
		}
	}
}

TechTypes CvPromotionLineInfo::getObsoleteTech() const
{
	return m_eObsoleteTech;
}

bool CvPromotionLineInfo::isBuildUp() const
{
	return m_bBuildUp;
}

bool CvPromotionLineInfo::isPoison() const
{
	return m_bPoison;
}

PropertyTypes CvPromotionLineInfo::getPropertyType() const
{
	return m_ePropertyType;
}

int CvPromotionLineInfo::getUnitCombatPrereqType(int i) const
{
	return m_aiUnitCombatPrereqTypes[i];
}

int CvPromotionLineInfo::getNumUnitCombatPrereqTypes() const
{
	return (int)m_aiUnitCombatPrereqTypes.size();
}

int CvPromotionLineInfo::getNotOnUnitCombatType(int i) const
{
	return m_aiNotOnUnitCombatTypes[i];
}

int CvPromotionLineInfo::getNumNotOnUnitCombatTypes() const
{
	return (int)m_aiNotOnUnitCombatTypes.size();
}

bool CvPromotionLineInfo::isNotOnUnitCombatType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiNotOnUnitCombatTypes, i);
}

int CvPromotionLineInfo::getNotOnDomainType(int i) const
{
	return m_aiNotOnDomainTypes[i];
}

int CvPromotionLineInfo::getNumNotOnDomainTypes() const
{
	return (int)m_aiNotOnDomainTypes.size();
}

bool CvPromotionLineInfo::isNotOnDomainType(int i) const
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, i);
	return algo::any_of_equal(m_aiNotOnDomainTypes, i);
}

int CvPromotionLineInfo::getOnGameOption(int i) const
{
	return m_aiOnGameOptions[i];
}

int CvPromotionLineInfo::getNumOnGameOptions() const
{
	return (int)m_aiOnGameOptions.size();
}

bool CvPromotionLineInfo::isOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiOnGameOptions, i);
}

int CvPromotionLineInfo::getNotOnGameOption(int i) const
{
	return m_aiNotOnGameOptions[i];
}

int CvPromotionLineInfo::getNumNotOnGameOptions() const
{
	return (int)m_aiNotOnGameOptions.size();
}

bool CvPromotionLineInfo::isNotOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiNotOnGameOptions, i);
}

int CvPromotionLineInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}

int CvPromotionLineInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}

bool CvPromotionLineInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}

// int vector utilizing pairing without delayed resolution
int CvPromotionLineInfo::getNumUnitCombatContractChanceChanges() const
{
	return m_aUnitCombatContractChanceChanges.size();
}

int CvPromotionLineInfo::getUnitCombatContractChanceChange(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aUnitCombatContractChanceChanges.begin(); it != m_aUnitCombatContractChanceChanges.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvPromotionLineInfo::isUnitCombatContractChanceChange(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aUnitCombatContractChanceChanges.begin(); it != m_aUnitCombatContractChanceChanges.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return true;
		}
	}
	return false;
}

int CvPromotionLineInfo::getNumTechContractChanceChanges() const
{
	return m_aTechContractChanceChanges.size();
}

int CvPromotionLineInfo::getTechContractChanceChange(int iTech) const
{
	PROFILE_EXTRA_FUNC();
	for (TechModifierArray::const_iterator it = m_aTechContractChanceChanges.begin(); it != m_aTechContractChanceChanges.end(); ++it)
	{
		if ((*it).first == (TechTypes)iTech)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvPromotionLineInfo::isTechContractChanceChange(int iTech) const
{
	PROFILE_EXTRA_FUNC();
	for (TechModifierArray::const_iterator it = m_aTechContractChanceChanges.begin(); it != m_aTechContractChanceChanges.end(); ++it)
	{
		if ((*it).first == (TechTypes)iTech)
		{
			return true;
		}
	}
	return false;
}

int CvPromotionLineInfo::getPromotion(int i) const
{
	return m_aiPromotions[i];
}

int CvPromotionLineInfo::getNumPromotions() const
{
	return (int)m_aiPromotions.size();
}

bool CvPromotionLineInfo::isPromotion(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), i);
	return algo::any_of_equal(m_aiPromotions, i);
}

int CvPromotionLineInfo::getBuilding(int i) const
{
	return m_aiBuildings[i];
}

int CvPromotionLineInfo::getNumBuildings() const
{
	return (int)m_aiBuildings.size();
}

bool CvPromotionLineInfo::isBuilding(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), i);
	return algo::any_of_equal(m_aiBuildings, i);
}

