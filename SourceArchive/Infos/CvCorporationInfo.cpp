//------------------------------------------------------------------------------------------------
//  FILE:    CvCorporationInfo.cpp
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
#include "CvCorporationInfo.h"


//======================================================================================================
//					CvCorporationInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvCorporationInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvCorporationInfo::CvCorporationInfo() :
// m_iChar/m_iHeadquarterChar are non-XML, runtime-assigned GameFont indices (see setChar/
// setHeadquarterChar), m_iMissionType is runtime-assigned via setMissionType; m_iFreeUnit and the
// PrereqBuilding/CompetingCorporation pairs are readPass3 machinery; m_iTGAIndex (its ctor default
// -1 is the never-read TGA-filler sentinel - see RemoveTGAFiller) and m_piYieldChange (legacy
// allocates a zero array when the tag is absent) stay hand-written. Every other XML-backed field
// is declared in getDataMembers() and defaulted by initDataMembers() below.
 m_iChar(0)
,m_iTGAIndex(-1)
,m_iHeadquarterChar(0)
,m_iFreeUnit(NO_UNIT)
,m_iMissionType(NO_MISSION)
,m_paiPrereqBuilding(NULL)
,m_pabCompetingCorporation(NULL)
,m_piYieldChange(NULL)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvCorporationInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvCorporationInfo::~CvCorporationInfo()
{
	CvInfoUtil(this).uninitDataMembers(); // owns the declared commerce/yield arrays
	SAFE_DELETE_ARRAY(m_paiPrereqBuilding);
	SAFE_DELETE_ARRAY(m_pabCompetingCorporation);
	SAFE_DELETE_ARRAY(m_piYieldChange);
}


int CvCorporationInfo::getChar() const
{
	return m_iChar;
}



int CvCorporationInfo::getTGAIndex() const
{
	return m_iTGAIndex;
}


void CvCorporationInfo::setChar(int i)
{
	m_iChar = 8550 + (GC.getGAMEFONT_TGA_RELIGIONS() + m_iTGAIndex) * 2;
}


int CvCorporationInfo::getHeadquarterChar() const
{
	return m_iHeadquarterChar;
}


void CvCorporationInfo::setHeadquarterChar(int i)
{
	m_iHeadquarterChar = 8551 + (GC.getGAMEFONT_TGA_RELIGIONS() + m_iTGAIndex) * 2;
}


int CvCorporationInfo::getFreeUnit() const
{
	return m_iFreeUnit;
}


int CvCorporationInfo::getSpreadFactor() const
{
	return m_iSpreadFactor;
}


int CvCorporationInfo::getSpreadCost() const
{
	return m_iSpreadCost;
}


int CvCorporationInfo::getMaintenance() const
{
	return m_iMaintenance;
}


int CvCorporationInfo::getMissionType() const
{
	return m_iMissionType;
}


void CvCorporationInfo::setMissionType(int iNewType)
{
	m_iMissionType = iNewType;
}


int CvCorporationInfo::getBonusProduced() const
{
	return m_iBonusProduced;
}


const char* CvCorporationInfo::getMovieFile() const
{
	return m_szMovieFile;
}


const char* CvCorporationInfo::getMovieSound() const
{
	return m_szMovieSound;
}


const char* CvCorporationInfo::getSound() const
{
	return m_szSound;
}


TechTypes CvCorporationInfo::getObsoleteTech() const
{
	return m_iObsoleteTech;
}


int CvCorporationInfo::getSpread() const
{
	return m_iSpread;
}


int CvCorporationInfo::getHealth() const
{
	return m_iHealth;
}


int CvCorporationInfo::getHappiness() const
{
	return m_iHappiness;
}


int CvCorporationInfo::getMilitaryProductionModifier() const
{
	return m_iMilitaryProductionModifier;
}


int CvCorporationInfo::getFreeXP() const
{
	return m_iFreeXP;
}


int CvCorporationInfo::getPrereqGameOption() const
{
	return m_iPrereqGameOption;
}


int CvCorporationInfo::getPrereqBuilding(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), i);
	return m_paiPrereqBuilding ? m_paiPrereqBuilding[i] : false;
}


int CvCorporationInfo::getPrereqBuildingVectorSize() const					{return m_aszPrereqBuildingforPass3.size();}

CvString CvCorporationInfo::getPrereqBuildingNamesVectorElement(const int i) const	{return m_aszPrereqBuildingforPass3[i];}

int CvCorporationInfo::getPrereqBuildingValuesVectorElement(const int i) const		{return m_aiPrereqBuildingforPass3[i];}


bool CvCorporationInfo::isCompetingCorporation(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCorporationInfos(), i);
	return m_pabCompetingCorporation ? m_pabCompetingCorporation[i] : false;
}


int CvCorporationInfo::getCompetingCorporationVectorSize() const					{return m_aszCompetingCorporationforPass3.size();}

CvString CvCorporationInfo::getCompetingCorporationNamesVectorElement(const int i) const	{return m_aszCompetingCorporationforPass3[i];}

bool CvCorporationInfo::getCompetingCorporationValuesVectorElement(const int i) const		{return m_abCompetingCorporationforPass3[i];}


int CvCorporationInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldChange ? m_piYieldChange[i] : 0;
}


int* CvCorporationInfo::getYieldChangeArray() const
{
	return m_piYieldChange;
}


int CvCorporationInfo::getCommerceChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCommerceChange ? m_piCommerceChange[i] : 0;
}


int* CvCorporationInfo::getCommerceChangeArray() const
{
	return m_piCommerceChange;
}


const std::vector<BonusTypes>& CvCorporationInfo::getPrereqBonuses() const
{
	return m_vPrereqBonuses;
}


int CvCorporationInfo::getHeadquarterCommerce(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_paiHeadquarterCommerce ? m_paiHeadquarterCommerce[i] : 0;
}


int* CvCorporationInfo::getHeadquarterCommerceArray() const
{
	return m_paiHeadquarterCommerce;
}


int CvCorporationInfo::getCommerceProduced(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_paiCommerceProduced ? m_paiCommerceProduced[i] : 0;
}


int* CvCorporationInfo::getCommerceProducedArray() const
{
	return m_paiCommerceProduced;
}


int CvCorporationInfo::getYieldProduced(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_paiYieldProduced ? m_paiYieldProduced[i] : 0;
}


int* CvCorporationInfo::getYieldProducedArray() const
{
	return m_paiYieldProduced;
}


int CvCorporationInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvCorporationInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvCorporationInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


void CvCorporationInfo::getDataMembers(CvInfoUtil& util)
{
	// Kept hand-written: m_iFreeUnit + the PrereqBuildings/CompetingCorporations paired lists
	// (readPass3 machinery), m_iTGAIndex (no wrapper default reproduces the legacy triple of ctor -1
	// / read-default 0 / copy-compare 0 - and the ctor -1 is the load-bearing never-read TGA-filler
	// sentinel consumed by RemoveTGAFiller) and m_piYieldChange (legacy read allocates a zero array
	// when the tag is absent - the addYields wrapper would leave it NULL, changing the checksum and
	// the non-NULL guarantee of getYieldChangeArray()). m_iChar/m_iHeadquarterChar/m_iMissionType are
	// runtime fields, not XML-backed. getCheckSum stays explicit: legacy omits the read field
	// m_iTGAIndex and checksums the runtime m_iMissionType plus the pass3 arrays mid-order.
	util
		.addEnum(m_iTechPrereq, L"TechPrereq")
		.add(m_iSpreadFactor, L"iSpreadFactor")
		.add(m_iSpreadCost, L"iSpreadCost")
		.add(m_iMaintenance, L"iMaintenance")
		.addEnumAsInt(m_iBonusProduced, L"BonusProduced")
		.addEnum(m_iObsoleteTech, L"ObsoleteTech")
		.addEnumAsInt(m_iPrereqGameOption, L"PrereqGameOption")
		.add(m_iSpread, L"iSpread")
		.add(m_iHealth, L"iHealth")
		.add(m_iHappiness, L"iHappiness")
		.add(m_iMilitaryProductionModifier, L"iMilitaryProductionModifier")
		.add(m_iFreeXP, L"iFreeXP")
		.addCommerce(m_piCommerceChange, L"CommerceChanges")
		.add(m_vPrereqBonuses, L"PrereqBonuses")
		.addCommerce(m_paiHeadquarterCommerce, L"HeadquarterCommerces")
		.addCommerce(m_paiCommerceProduced, L"CommercesProduced")
		.addYields(m_paiYieldProduced, L"YieldsProduced")
		.add(m_aiCategories, L"Categories")
		.add(m_PropertyManipulators)
		.add(m_szMovieFile, L"MovieFile")
		.add(m_szMovieSound, L"MovieSound")
		.add(m_szSound, L"Sound")
	;
}


//
// read from xml
//
bool CvCorporationInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->GetOptionalChildXmlValByName(szTextVal, L"FreeUnit");
	m_aszExtraXMLforPass3.push_back(szTextVal);

	pXML->GetOptionalChildXmlValByName(&m_iTGAIndex, L"iTGAIndex");

	if (pXML->TryMoveToXmlFirstChild(L"PrereqBuildings"))
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
						m_aszPrereqBuildingforPass3.push_back(szTextVal);
						pXML->GetNextXmlVal(&iTemp);
						m_aiPrereqBuildingforPass3.push_back(iTemp);
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

	if (pXML->TryMoveToXmlFirstChild(L"CompetingCorporations"))
	{
		int iNumSibs = pXML->GetXmlChildrenNumber();
		bool bTemp = false;
		if (iNumSibs > 0)
		{
			if (pXML->TryMoveToXmlFirstChild())
			{
				for (int i=0;i<iNumSibs;i++)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						m_aszCompetingCorporationforPass3.push_back(szTextVal);
						pXML->GetNextXmlVal(&bTemp);
						m_abCompetingCorporationforPass3.push_back(bTemp);
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

	// if we can set the current xml node to it's next sibling
	if (pXML->TryMoveToXmlFirstChild(L"YieldChanges"))
	{
		// call the function that sets the yield change variable
		pXML->SetYields(&m_piYieldChange);
		pXML->MoveToXmlParent();
	}
	else
	{
		pXML->CvXMLLoadUtility::InitList(&m_piYieldChange, NUM_YIELD_TYPES);
	}

	return true;
}


void CvCorporationInfo::copyNonDefaults(const CvCorporationInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const int iDefault = 0;
	const int iTextDefault = -1;  //all integers which are TEXT_KEYS in the xml are -1 by default

	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	// NOTE: for the declared commerce/yield arrays the legacy copy unconditionally allocated
	// zero-filled arrays; the wrappers only allocate when a non-default source value is copied.
	// Loaded values are identical, but a modular-merged corporation with the tag absent in both
	// definitions now keeps a NULL array (getters already handle NULL).
	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if (m_iFreeUnit == iTextDefault) m_iFreeUnit = pClassInfo->getFreeUnit();

	if (getTGAIndex() == iDefault) m_iTGAIndex = pClassInfo->getTGAIndex();

	for ( int i = 0; i < pClassInfo->getPrereqBuildingVectorSize(); i++ )
	{
		m_aiPrereqBuildingforPass3.push_back(pClassInfo->getPrereqBuildingValuesVectorElement(i));
		m_aszPrereqBuildingforPass3.push_back(pClassInfo->getPrereqBuildingNamesVectorElement(i));
	}

	for ( int i = 0; i < pClassInfo->getCompetingCorporationVectorSize(); i++ )
	{
		m_abCompetingCorporationforPass3.push_back(pClassInfo->getCompetingCorporationValuesVectorElement(i));
		m_aszCompetingCorporationforPass3.push_back(pClassInfo->getCompetingCorporationNamesVectorElement(i));
	}

	if (!m_piYieldChange) CvXMLLoadUtility::InitList(&m_piYieldChange, NUM_YIELD_TYPES);
	for ( int j = 0; j < NUM_YIELD_TYPES; j++)
	{
		if ( getYieldChange(j) == iDefault && pClassInfo->getYieldChange(j) != iDefault)
		{
			m_piYieldChange[j] = pClassInfo->getYieldChange(j);
		}
	}
}


void CvCorporationInfo::getCheckSum(uint32_t& iSum) const
{
	// NOTE: kept explicit (not delegated to CvInfoUtil) to preserve the exact legacy checksum, which
	// omits the read field m_iTGAIndex and interleaves runtime/pass3/hand-written fields
	// (m_iMissionType, m_iFreeUnit, m_paiPrereqBuilding, m_pabCompetingCorporation, m_piYieldChange)
	// with the declared ones.
	CheckSum(iSum, m_iTechPrereq);
	CheckSum(iSum, m_iFreeUnit);
	CheckSum(iSum, m_iSpreadFactor);
	CheckSum(iSum, m_iSpreadCost);
	CheckSum(iSum, m_iMaintenance);
	CheckSum(iSum, m_iMissionType);
	CheckSum(iSum, m_iBonusProduced);

	CheckSum(iSum, m_iObsoleteTech);
	CheckSum(iSum, m_iSpread);
	CheckSum(iSum, m_iHealth);
	CheckSum(iSum, m_iHappiness);
	CheckSum(iSum, m_iFreeXP);
	CheckSum(iSum, m_iMilitaryProductionModifier);
	CheckSum(iSum, m_iPrereqGameOption);

	CheckSum(iSum, m_paiPrereqBuilding, GC.getNumBuildingInfos());

	CheckSum(iSum, m_pabCompetingCorporation, GC.getNumCorporationInfos());

	CheckSum(iSum, m_piYieldChange, NUM_YIELD_TYPES);
	CheckSum(iSum, m_piCommerceChange, NUM_COMMERCE_TYPES);

	// Arrays

	CheckSumC(iSum, m_vPrereqBonuses);
	CheckSum(iSum, m_paiHeadquarterCommerce, NUM_COMMERCE_TYPES);
	CheckSum(iSum, m_paiCommerceProduced, NUM_COMMERCE_TYPES);
	CheckSum(iSum, m_paiYieldProduced, NUM_YIELD_TYPES);

	CheckSumC(iSum, m_aiCategories);

	m_PropertyManipulators.getCheckSum(iSum);
}



bool CvCorporationInfo::readPass3()
{
	PROFILE_EXTRA_FUNC();
	m_paiPrereqBuilding = new int[GC.getNumBuildingInfos()];
	for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
	{
		m_paiPrereqBuilding[iI] = 0;
	}
	if (!m_aiPrereqBuildingforPass3.empty() && !m_aszPrereqBuildingforPass3.empty())
	{
		const int iNumLoad = m_aiPrereqBuildingforPass3.size();
		for (int iI = 0; iI < iNumLoad; iI++)
		{
			const int iTempIndex = GC.getInfoTypeForString(m_aszPrereqBuildingforPass3[iI]);
			if (iTempIndex >= 0 && iTempIndex < GC.getNumBuildingInfos())
				m_paiPrereqBuilding[iTempIndex] = m_aiPrereqBuildingforPass3[iI];
		}
		m_aszPrereqBuildingforPass3.clear();
		m_aiPrereqBuildingforPass3.clear();
	}

	m_pabCompetingCorporation = new bool[GC.getNumCorporationInfos()];
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		m_pabCompetingCorporation[iI] = 0;
	}
	if (!m_abCompetingCorporationforPass3.empty() && !m_aszCompetingCorporationforPass3.empty())
	{
		const int iNumLoad = m_abCompetingCorporationforPass3.size();
		for(int iI = 0; iI < iNumLoad; iI++)
		{
			const int iTempIndex = GC.getInfoTypeForString(m_aszCompetingCorporationforPass3[iI]);
			if (iTempIndex >= 0 && iTempIndex < GC.getNumCorporationInfos())
				m_pabCompetingCorporation[iTempIndex] = m_abCompetingCorporationforPass3[iI];
		}
		m_aszCompetingCorporationforPass3.clear();
		m_abCompetingCorporationforPass3.clear();
	}

	if (m_aszExtraXMLforPass3.size() < 1)
	{
		FErrorMsg("error");
		return false;
	}

	m_iFreeUnit = GC.getInfoTypeForString(m_aszExtraXMLforPass3[0]);

	m_aszExtraXMLforPass3.clear();

	return true;
}

