//------------------------------------------------------------------------------------------------
//  FILE:    CvBuildInfo.cpp
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
#include "CvBuildInfo.h"


//======================================================================================================
//					CvBuildInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvBuildInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvBuildInfo::CvBuildInfo() :
// Only the two non-XML, runtime-assigned fields need explicit init here; every XML-backed field is
// declared in getDataMembers() and set to its default by initDataMembers() below.
m_iMissionType(NO_MISSION),
m_bDisabled(false)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvBuildInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvBuildInfo::~CvBuildInfo()
{
	PROFILE_EXTRA_FUNC();
	CvInfoUtil(this).uninitDataMembers();
}


void CvBuildInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iTime, L"iTime")
		.add(m_iCost, L"iCost")
		.add(m_bKill, L"bKill")
		.addEnum(m_iTechPrereq, L"PrereqTech")
		.addEnum(m_iObsoleteTech, L"ObsoleteTech")
		.addEnum(m_iImprovement, L"ImprovementType")
		.addEnum(m_iRoute, L"RouteType")
		.addEnum(m_iTerrainChange, L"TerrainChange")
		.addEnum(m_iFeatureChange, L"FeatureChange")
		.addEnum(m_iEntityEvent, L"EntityEvent")
		.add(m_aiPrereqBonusTypes, L"PrereqBonusTypes")
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")
		.add(m_aiCategories, L"Categories")
		.addStruct(m_aFeatureStructs, L"FeatureStructs", L"FeatureStruct")
		.addStruct(m_aTerrainStructs, L"TerrainStructs", L"TerrainStruct")
		.addStruct(m_aPlaceBonusTypes, L"PlaceBonusTypes", L"PlaceBonusType")
	;
}


int CvBuildInfo::getTime() const
{
	return m_iTime;
}


int CvBuildInfo::getCost() const
{
	return m_iCost;
}


int CvBuildInfo::getRoute() const
{
	return m_iRoute;
}


int CvBuildInfo::getTerrainChange() const
{
	return m_iTerrainChange;
}


int CvBuildInfo::getFeatureChange() const
{
	return m_iFeatureChange;
}


TechTypes CvBuildInfo::getObsoleteTech() const
{
	return m_iObsoleteTech;
}


bool CvBuildInfo::isDisabled() const
{
	return m_bDisabled;
}

void CvBuildInfo::setDisabled(bool bNewVal)
{
	m_bDisabled = bNewVal;
}


int CvBuildInfo::getEntityEvent() const
{
	return m_iEntityEvent;
}


int CvBuildInfo::getMissionType() const
{
	return m_iMissionType;
}


void CvBuildInfo::setMissionType(int iNewType)
{
	m_iMissionType = iNewType;
}


bool CvBuildInfo::isKill() const
{
	return m_bKill;
}


// Arrays

namespace {
	const FeatureStruct* findFeatureStruct(const std::vector<FeatureStruct>& vec, FeatureTypes e)
	{
		for (uint32_t i = 0, n = vec.size(); i < n; i++)
		{
			if (vec[i].eFeature == e)
				return &vec[i];
		}
		return NULL;
	}
}

TechTypes CvBuildInfo::getFeatureTech(FeatureTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), e);
	const FeatureStruct* p = findFeatureStruct(m_aFeatureStructs, e);
	return p ? p->ePrereqTech : NO_TECH;
}


int CvBuildInfo::getFeatureTime(FeatureTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), e);
	const FeatureStruct* p = findFeatureStruct(m_aFeatureStructs, e);
	return p ? p->iTime : 0;
}


int CvBuildInfo::getFeatureProduction(FeatureTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), e);
	const FeatureStruct* p = findFeatureStruct(m_aFeatureStructs, e);
	return p ? p->iProduction : 0;
}


bool CvBuildInfo::isFeatureRemove(FeatureTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), e);
	const FeatureStruct* p = findFeatureStruct(m_aFeatureStructs, e);
	return p ? p->bRemove : false;
}


int CvBuildInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvBuildInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvBuildInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


bool CvBuildInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	if (!CvHotkeyInfo::read(pXML))
	{
		return false;
	}

	// Every XML-backed field is declared in getDataMembers(); this one call reads them all.
	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvBuildInfo::copyNonDefaults(const CvBuildInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvHotkeyInfo::copyNonDefaults(pClassInfo);

	// Declarative modular merge of every XML-backed field.
	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvBuildInfo::getCheckSum(uint32_t &iSum) const
{
	PROFILE_EXTRA_FUNC();
	// Declarative checksum of every XML-backed field.
	CvInfoUtil(this).checkSum(iSum);

	// Non-XML, runtime-assigned fields (not owned by the declarative layer).
	CheckSum(iSum, m_bDisabled);
	CheckSum(iSum, m_iMissionType);
}


void CvBuildInfo::doPostLoadCaching(uint32_t iThis)
{
}

