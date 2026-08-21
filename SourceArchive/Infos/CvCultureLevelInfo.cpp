//------------------------------------------------------------------------------------------------
//  FILE:    CvCultureLevelInfo.cpp
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
#include "CvCultureLevelInfo.h"


//------------------------------------------------------------------------------------------------------
//
//  CvCultureLevelInfo
//

CvCultureLevelInfo::CvCultureLevelInfo() :
// Only the non-XML runtime field (m_iLevel, assigned post-load by CvGlobals) and the hand-written
// array need explicit init here; every declared field is defaulted by initDataMembers() below.
m_iLevel(-1),
m_paiSpeedThreshold(NULL)
{
	CvInfoUtil(this).initDataMembers();
}


CvCultureLevelInfo::~CvCultureLevelInfo()
{
	SAFE_DELETE_ARRAY(m_paiSpeedThreshold);
}


int CvCultureLevelInfo::getCityDefenseModifier() const
{
	return m_iCityDefenseModifier;
}


int CvCultureLevelInfo::getSpeedThreshold(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameSpeedInfos(), i);
	return m_paiSpeedThreshold ? m_paiSpeedThreshold[i] : 0;
}



int CvCultureLevelInfo::getCityRadius() const
{
	return m_iCityRadius;
}


int CvCultureLevelInfo::getMaxWorldWonders() const
{
	return m_iMaxWorldWonders;
}


int CvCultureLevelInfo::getMaxTeamWonders() const
{
	return m_iMaxTeamWonders;
}


int CvCultureLevelInfo::getMaxNationalWonders() const
{
	return m_iMaxNationalWonders;
}


int CvCultureLevelInfo::getMaxNationalWondersOCC() const
{
	return m_iMaxNationalWondersOCC;
}


int CvCultureLevelInfo::getPrereqGameOption() const
{
	return m_iPrereqGameOption;
}



void CvCultureLevelInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. m_paiSpeedThreshold (dynamic GameSpeed-length int array
	// via SetVariableListTagPair) has no wrapper yet and stays hand-written in read/copyNonDefaults;
	// it is appended after the delegated checksum below, preserving the legacy checksum byte-for-byte.
	util
		.add(m_iCityDefenseModifier, L"iCityDefenseModifier")
		.add(m_iCityRadius, L"iCityRadius", 1)
		.add(m_iMaxWorldWonders, L"iMaxWorldWonders", 1)
		.add(m_iMaxTeamWonders, L"iMaxTeamWonders", 1)
		.add(m_iMaxNationalWonders, L"iMaxNationalWonders", 1)
		.add(m_iMaxNationalWondersOCC, L"iMaxNationalWondersOCC", 1)
		.addEnumAsInt(m_iPrereqGameOption, L"PrereqGameOption")
	;
}


bool CvCultureLevelInfo::read(CvXMLLoadUtility* pXml)
{
	if (!CvInfoBase::read(pXml))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXml);

	pXml->SetVariableListTagPair(&m_paiSpeedThreshold, L"SpeedThresholds", GC.getNumGameSpeedInfos());

	return true;
}


void CvCultureLevelInfo::copyNonDefaults(const CvCultureLevelInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const int iDefault = 0;

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	for ( int i = 0; i < GC.getNumGameSpeedInfos(); i++ )
	{
		if ( getSpeedThreshold(i) == iDefault && pClassInfo->getSpeedThreshold(i) != iDefault)
		{
			if ( NULL == m_paiSpeedThreshold )
			{
				CvXMLLoadUtility::InitList(&m_paiSpeedThreshold,GC.getNumGameSpeedInfos(),iDefault);
			}
			m_paiSpeedThreshold[i] = pClassInfo->getSpeedThreshold(i);
		}
	}
}


void CvCultureLevelInfo::getCheckSum(uint32_t &iSum) const
{
	CvInfoUtil(this).checkSum(iSum);

	CheckSum(iSum, m_paiSpeedThreshold, GC.getNumGameSpeedInfos());
}

