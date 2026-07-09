//------------------------------------------------------------------------------------------------
//  FILE:    CvVoteSourceInfo.cpp
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
#include "CvVoteSourceInfo.h"


CvVoteSourceInfo::CvVoteSourceInfo() :
	// Only the hand-written delayed-resolution FK needs explicit init here;
	// every declared field is defaulted by initDataMembers() below.
	m_iCivic(NO_CIVIC)
{
	CvInfoUtil(this).initDataMembers();
}


CvVoteSourceInfo::~CvVoteSourceInfo()
{
	// m_aiReligionYields/m_aiReligionCommerces are owned by their addYields/addCommerce
	// wrappers and freed by uninitDataMembers.
	CvInfoUtil(this).uninitDataMembers();

	GC.removeDelayedResolution((int*)&m_iCivic);
}


void CvVoteSourceInfo::getDataMembers(CvInfoUtil& util)
{
	// m_iCivic is an int FK read with DELAYED resolution (addEnumAsInt is immediate-only — see the
	// "not yet supported" list in declarative-info-loading.md), so it stays hand-written in
	// read/copyNonDefaults/dtor. It sits mid-order in the legacy checksum, which also includes the
	// two CvStrings (StringWrapper checksum is a no-op), so getCheckSum below stays explicit.
	util
		.add(m_iVoteInterval, L"iVoteInterval")
		.addEnumAsInt(m_iFreeSpecialist, L"FreeSpecialist")
		.addYields(m_aiReligionYields, L"ReligionYields")
		.addCommerce(m_aiReligionCommerces, L"ReligionCommerces")
		.add(m_szPopupText, L"PopupText")
		.add(m_szSecretaryGeneralText, L"SecretaryGeneralText")
	;
}


int CvVoteSourceInfo::getVoteInterval() const
{
	return m_iVoteInterval;
}


int CvVoteSourceInfo::getFreeSpecialist() const
{
	return m_iFreeSpecialist;
}


int CvVoteSourceInfo::getCivic() const
{
	return m_iCivic;
}


int CvVoteSourceInfo::getReligionYield(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_aiReligionYields[i];
}


int CvVoteSourceInfo::getReligionCommerce(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_aiReligionCommerces[i];
}


const CvWString CvVoteSourceInfo::getPopupText() const
{
	return gDLL->getText(m_szPopupText);
}


const CvWString CvVoteSourceInfo::getSecretaryGeneralText() const
{
	return gDLL->getText(m_szSecretaryGeneralText);
}


bool CvVoteSourceInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	CvString szTextVal;
	pXML->GetOptionalChildXmlValByName(szTextVal, L"Civic");
	GC.addDelayedResolution((int*)&m_iCivic, szTextVal);

	return true;
}


void CvVoteSourceInfo::copyNonDefaults(const CvVoteSourceInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	GC.copyNonDefaultDelayedResolution((int*)&m_iCivic, (int*)&pClassInfo->m_iCivic);
}


void CvVoteSourceInfo::getCheckSum(uint32_t &iSum) const
{
	// Explicit (not delegated) to keep the legacy checksum byte-identical: the hand-written
	// m_iCivic sits mid-order, and the two CvStrings ARE part of the legacy checksum
	// (the declarative StringWrapper contributes nothing).
	CheckSum(iSum, m_iVoteInterval);
	CheckSum(iSum, m_iFreeSpecialist);
	CheckSum(iSum, m_iCivic);

	CheckSum(iSum, m_aiReligionYields, NUM_YIELD_TYPES);
	CheckSum(iSum, m_aiReligionCommerces, NUM_COMMERCE_TYPES);

	CheckSumC(iSum, m_szPopupText);
	CheckSumC(iSum, m_szSecretaryGeneralText);
}

