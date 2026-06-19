//------------------------------------------------------------------------------------------------
//  FILE:    CvGoodyInfo.cpp
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
#include "CvGoodyInfo.h"


//======================================================================================================
//					CvGoodyInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvGoodyInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvGoodyInfo::CvGoodyInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvGoodyInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvGoodyInfo::~CvGoodyInfo()
{
}


int CvGoodyInfo::getGold() const
{
	return m_iGold;
}


int CvGoodyInfo::getGoldRand1() const
{
	return m_iGoldRand1;
}


int CvGoodyInfo::getGoldRand2() const
{
	return m_iGoldRand2;
}


int CvGoodyInfo::getMapOffset() const
{
	return m_iMapOffset;
}


int CvGoodyInfo::getMapRange() const
{
	return m_iMapRange;
}


int CvGoodyInfo::getMapProb() const
{
	return m_iMapProb;
}


int CvGoodyInfo::getExperience() const
{
	return m_iExperience;
}


int CvGoodyInfo::getHealing() const
{
	return m_iHealing;
}


int CvGoodyInfo::getDamagePrereq() const
{
	return m_iDamagePrereq;
}


int CvGoodyInfo::getBarbarianUnitProb() const
{
	return m_iBarbarianUnitProb;
}


int CvGoodyInfo::getMinBarbarians() const
{
	return m_iMinBarbarians;
}


int CvGoodyInfo::getGoodyUnit() const
{
	return m_iGoodyUnit;
}


int CvGoodyInfo::getBarbarianUnit() const
{
	return m_iBarbarianUnit;
}


int CvGoodyInfo::getEraType() const
{
	return m_iEraType;
}


int CvGoodyInfo::getNotEraType() const
{
	return m_iNotEraType;
}


int CvGoodyInfo::getResearch() const
{
	return m_iResearch;
}


bool CvGoodyInfo::isTech() const
{
	return m_bTech;
}


bool CvGoodyInfo::isBad() const
{
	return m_bBad;
}


bool CvGoodyInfo::isNaval() const
{
	return m_bNaval;
}


const char* CvGoodyInfo::getSound() const
{
	return m_szSound;
}


void CvGoodyInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order so the delegated checksum stays byte-identical; m_szSound
	// is last (StringWrapper contributes nothing to the checksum).
	util
		.add(m_iGold, L"iGold")
		.add(m_iGoldRand1, L"iGoldRand1")
		.add(m_iGoldRand2, L"iGoldRand2")
		.add(m_iMapOffset, L"iMapOffset")
		.add(m_iMapRange, L"iMapRange")
		.add(m_iMapProb, L"iMapProb")
		.add(m_iExperience, L"iExperience")
		.add(m_iHealing, L"iHealing")
		.add(m_iDamagePrereq, L"iDamagePrereq")
		.add(m_iBarbarianUnitProb, L"iBarbarianUnitProb")
		.add(m_iMinBarbarians, L"iMinBarbarians")
		.addEnumAsInt(m_iGoodyUnit, L"FreeUnit")
		.addEnumAsInt(m_iBarbarianUnit, L"BarbarianUnit")
		.addEnumAsInt(m_iEraType, L"EraType")
		.addEnumAsInt(m_iNotEraType, L"NotEraType")
		.add(m_iResearch, L"iResearch")
		.add(m_bTech, L"bTech")
		.add(m_bBad, L"bBad")
		.add(m_bNaval, L"bNaval")
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")
		.add(m_szSound, L"Sound")
	;
}


bool CvGoodyInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvGoodyInfo::copyNonDefaults(const CvGoodyInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvGoodyInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

