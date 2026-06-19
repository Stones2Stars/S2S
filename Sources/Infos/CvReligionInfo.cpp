//------------------------------------------------------------------------------------------------
//  FILE:    CvReligionInfo.cpp
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
#include "CvReligionInfo.h"


//======================================================================================================
//					CvReligionInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvReligionInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvReligionInfo::CvReligionInfo() :
// m_iChar/m_iHolyCityChar are non-XML, runtime-assigned GameFont indices (see setChar/setHolyCityChar),
// m_iMissionType is runtime-assigned via setMissionType; m_iFreeUnit (delayed-resolution int FK),
// m_iTGAIndex (its ctor default -1 is the never-read TGA-filler sentinel - see RemoveTGAFiller) and
// m_piFlavorValue (SetVariableListTagPair) stay hand-written. Every other XML-backed field is declared
// in getDataMembers() and defaulted by initDataMembers() below.
m_iChar(0),
m_iTGAIndex(-1),
m_iHolyCityChar(0),
m_iMissionType(NO_MISSION),
m_iFreeUnit(NO_UNIT),
m_piFlavorValue(NULL)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvReligionInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvReligionInfo::~CvReligionInfo()
{
	CvInfoUtil(this).uninitDataMembers(); // owns the three declared commerce arrays
	SAFE_DELETE_ARRAY(m_piFlavorValue);

	GC.removeDelayedResolution((int*)&m_iFreeUnit);
}


int CvReligionInfo::getChar() const
{
	return m_iChar;
}


int CvReligionInfo::getTGAIndex() const
{
	return m_iTGAIndex;
}


void CvReligionInfo::setChar(int i)
{
	m_iChar = 8550 + m_iTGAIndex * 2;
}


int CvReligionInfo::getHolyCityChar() const
{
	return m_iHolyCityChar;
}


void CvReligionInfo::setHolyCityChar(int i)
{
	m_iHolyCityChar = 8551 + m_iTGAIndex * 2;
}


int CvReligionInfo::getFreeUnit() const
{
	return m_iFreeUnit;
}


int CvReligionInfo::getNumFreeUnits() const
{
	return m_iNumFreeUnits;
}


int CvReligionInfo::getSpreadFactor() const
{
	return m_iSpreadFactor;
}


int CvReligionInfo::getMissionType() const
{
	return m_iMissionType;
}


void CvReligionInfo::setMissionType(int iNewType)
{
	m_iMissionType = iNewType;
}


const char* CvReligionInfo::getTechButton() const
{
	return m_szTechButton;
}


const char* CvReligionInfo::getGenericTechButton() const
{
	return m_szGenericTechButton;
}


const char* CvReligionInfo::getMovieFile() const
{
	return m_szMovieFile;
}


const char* CvReligionInfo::getMovieSound() const
{
	return m_szMovieSound;
}


const char* CvReligionInfo::getButtonDisabled() const
{
	static char szDisabled[512];

	szDisabled[0] = '\0';

	if ( getButton() && strlen(getButton()) > 4 )
	{
		strncpy( szDisabled, getButton(), strlen(getButton()) - 4 );
		szDisabled[strlen(getButton()) - 4] = '\0';
		strcat( szDisabled, "_D.dds" );
	}

	return szDisabled;
}


const char* CvReligionInfo::getSound() const
{
	return m_szSound;
}


void CvReligionInfo::setAdjectiveKey(const char* szVal)
{
	m_szAdjectiveKey = szVal;
}


const wchar_t* CvReligionInfo::getAdjectiveKey() const
{
	return m_szAdjectiveKey;
}


// Arrays

int CvReligionInfo::getGlobalReligionCommerce(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_paiGlobalReligionCommerce ? m_paiGlobalReligionCommerce[i] : 0;
}


int* CvReligionInfo::getGlobalReligionCommerceArray() const
{
	return m_paiGlobalReligionCommerce;
}


int CvReligionInfo::getHolyCityCommerce(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_paiHolyCityCommerce ? m_paiHolyCityCommerce[i] : 0;
}


int* CvReligionInfo::getHolyCityCommerceArray() const
{
	return m_paiHolyCityCommerce;
}


int CvReligionInfo::getStateReligionCommerce(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_paiStateReligionCommerce ? m_paiStateReligionCommerce[i] : 0;
}


int* CvReligionInfo::getStateReligionCommerceArray() const
{
	return m_paiStateReligionCommerce;
}


int CvReligionInfo::getFlavorValue(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFlavorTypes(), i);
	return m_piFlavorValue ? m_piFlavorValue[i] : 0;
}


int CvReligionInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvReligionInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvReligionInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


void CvReligionInfo::getDataMembers(CvInfoUtil& util)
{
	// Kept hand-written: m_iFreeUnit (delayed-resolution int FK - no wrapper yet), m_iTGAIndex (no
	// wrapper default reproduces the legacy triple of ctor -1 / read-default 0 / copy-compare 0 - and
	// the ctor -1 is the load-bearing never-read TGA-filler sentinel consumed by RemoveTGAFiller),
	// m_piFlavorValue (SetVariableListTagPair dynamic array), m_szTechButton (required read - the
	// wrapper only does optional reads, so the load-time missing-tag diagnostic would be lost) and
	// m_szAdjectiveKey (CvWString - no wrapper). m_iChar/m_iHolyCityChar/m_iMissionType/
	// m_shrineBuildings are runtime fields, not XML-backed. getCheckSum stays explicit: legacy omits
	// m_iTGAIndex and checksums the runtime m_iMissionType mid-order.
	util
		.addEnum(m_iTechPrereq, L"TechPrereq")
		.add(m_iNumFreeUnits, L"iFreeUnits")
		.add(m_iSpreadFactor, L"iSpreadFactor")
		.addCommerce(m_paiGlobalReligionCommerce, L"GlobalReligionCommerces")
		.addCommerce(m_paiHolyCityCommerce, L"HolyCityCommerces")
		.addCommerce(m_paiStateReligionCommerce, L"StateReligionCommerces")
		.add(m_aiCategories, L"Categories")
		.add(m_PropertyManipulators)
		.add(m_szGenericTechButton, L"GenericTechButton")
		.add(m_szMovieFile, L"MovieFile")
		.add(m_szMovieSound, L"MovieSound")
		.add(m_szSound, L"Sound")
	;
}


//
// read from xml
//
bool CvReligionInfo::read(CvXMLLoadUtility* pXML)
{
	CvString szTextVal;
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->GetOptionalChildXmlValByName(szTextVal, L"FreeUnit");
	GC.addDelayedResolution((int*)&m_iFreeUnit, szTextVal);

	pXML->GetOptionalChildXmlValByName(&m_iTGAIndex, L"iTGAIndex");

	pXML->GetChildXmlValByName(m_szTechButton, L"TechButton");

	pXML->GetOptionalChildXmlValByName(szTextVal, L"Adjective");
	setAdjectiveKey(szTextVal);

	pXML->SetVariableListTagPair(&m_piFlavorValue, L"Flavors", GC.getNumFlavorTypes());

	return true;
}


void CvReligionInfo::copyNonDefaults(const CvReligionInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const int iDefault = 0;
	const CvString cDefault = CvString::format("").GetCString();
	const CvWString wDefault = CvWString::format(L"").GetCString();

	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	GC.copyNonDefaultDelayedResolution((int*)&m_iFreeUnit, (int*)&pClassInfo->m_iFreeUnit);

	if (getTGAIndex() == iDefault) m_iTGAIndex = pClassInfo->getTGAIndex();

	if (getTechButton() == cDefault) m_szTechButton = pClassInfo->getTechButton();
	if (getAdjectiveKey() == wDefault) setAdjectiveKey(CvString::format("%s",pClassInfo->getAdjectiveKey()).GetCString());

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
}


void CvReligionInfo::getCheckSum(uint32_t& iSum) const
{
	// NOTE: kept explicit (not delegated to CvInfoUtil) to preserve the exact legacy checksum, which
	// omits the read field m_iTGAIndex, includes the runtime field m_iMissionType mid-order, and has
	// the hand-written m_iFreeUnit/m_piFlavorValue interleaved with declared fields.
	CheckSum(iSum, m_iTechPrereq);
	CheckSum(iSum, m_iFreeUnit);
	CheckSum(iSum, m_iNumFreeUnits);
	CheckSum(iSum, m_iSpreadFactor);
	CheckSum(iSum, m_iMissionType);

	// Arrays

	CheckSum(iSum, m_paiGlobalReligionCommerce, NUM_COMMERCE_TYPES);
	CheckSum(iSum, m_paiHolyCityCommerce, NUM_COMMERCE_TYPES);
	CheckSum(iSum, m_paiStateReligionCommerce, NUM_COMMERCE_TYPES);
	CheckSum(iSum, m_piFlavorValue, GC.getNumFlavorTypes());

	CheckSumC(iSum, m_aiCategories);

	m_PropertyManipulators.getCheckSum(iSum);
}

