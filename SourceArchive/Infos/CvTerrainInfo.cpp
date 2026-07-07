//------------------------------------------------------------------------------------------------
//  FILE:    CvTerrainInfo.cpp
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
#include "CvTerrainInfo.h"


//======================================================================================================
//					CvTerrainInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvTerrainInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvTerrainInfo::CvTerrainInfo() :
// Only non-declarative fields here; everything else defaults via initDataMembers().
m_eClimate(NO_CLIMATE_ZONE),
m_iWorldSoundscapeScriptId(0),
m_pi3DAudioScriptFootstepIndex(NULL)
{
	CvInfoUtil(this).initDataMembers();

	m_zobristValue = GC.getGame().getSorenRand().getInt();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvTerrainInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvTerrainInfo::~CvTerrainInfo()
{
	CvInfoUtil(this).uninitDataMembers(); // frees m_piYields (owned by addYields)

	SAFE_DELETE_ARRAY(m_pi3DAudioScriptFootstepIndex);
}


int CvTerrainInfo::getMovementCost() const
{
	return m_iMovementCost;
}


int CvTerrainInfo::getBuildModifier() const
{
	return m_iBuildModifier;
}


int CvTerrainInfo::getDefenseModifier() const
{
	return m_iDefenseModifier;
}


bool CvTerrainInfo::isImpassable() const
{
	return m_bImpassable;
}


bool CvTerrainInfo::isFound() const
{
	return m_bFound;
}


bool CvTerrainInfo::isFoundCoast() const
{
	return m_bFoundCoast;
}


bool CvTerrainInfo::isFoundFreshWater() const
{
	return m_bFoundFreshWater;
}


bool CvTerrainInfo::isFreshWaterTerrain() const
{
	return m_bFreshWaterTerrain;
}


const char* CvTerrainInfo::getArtDefineTag() const
{
	return m_szArtDefineTag;
}


int CvTerrainInfo::getWorldSoundscapeScriptId() const
{
	return m_iWorldSoundscapeScriptId;
}


// Arrays

int CvTerrainInfo::getYield(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYields ? m_piYields[i] : 0;
}


int CvTerrainInfo::get3DAudioScriptFootstepIndex(int i) const
{
	FAssertMsg(i > -1, "Index out of bounds");
	return m_pi3DAudioScriptFootstepIndex ? m_pi3DAudioScriptFootstepIndex[i] : 0;
}


int CvTerrainInfo::getCultureDistance() const
{
	return m_iCultureDistance;
}

int CvTerrainInfo::getHealthPercent() const
{
	return m_iHealthPercent;
}


//TB Combat Mod begin

//TB Combat Mod end


int CvTerrainInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvTerrainInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvTerrainInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}




void CvTerrainInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. The checksum is NOT delegated (see getCheckSum),
	// but keeping the order aligned makes the two trivially comparable.
	// Stays hand-written: m_eClimate (ClimateZoneTypes is a registered enum with NO InfoClassTraits
	// specialization, so addEnum would index m_infoClassXmlLoadOrder[NO_INFO_CLASS] out of bounds),
	// m_pi3DAudioScriptFootstepIndex (SetVariableListTagPair-style dynamic array),
	// m_iWorldSoundscapeScriptId (audio tag index lookup), m_zobristValue (runtime).
	util
		.add(m_iMovementCost, L"iMovement")
		.add(m_iBuildModifier, L"iBuildModifier")
		.add(m_iDefenseModifier, L"iDefense")
		.add(m_iDistanceToLand, L"iDistanceToLand")
		.add(m_bImpassable, L"bImpassable")
		.add(m_bFound, L"bFound")
		.add(m_bFoundCoast, L"bFoundCoast")
		.add(m_bFoundFreshWater, L"bFoundFreshWater")
		.add(m_bFreshWaterTerrain, L"bFreshWaterTerrain")
		.addYields(m_piYields, L"Yields")
		.add(m_iCultureDistance, L"iCultureDistance")
		.add(m_iHealthPercent, L"iHealthPercent")
		.add(m_PropertyManipulators)
		.add(m_aiCategories, L"Categories")
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")
		.add(m_szArtDefineTag, L"ArtDefineTag")
	;
}


bool CvTerrainInfo::read(CvXMLLoadUtility* pXML)
{
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->GetOptionalChildXmlValByName(szTextVal, L"ClimateZoneType");
	m_eClimate = (ClimateZoneTypes) pXML->GetInfoClass(szTextVal);

	pXML->SetVariableListTagPairForAudioScripts(&m_pi3DAudioScriptFootstepIndex, L"FootstepSounds", GC.getNumFootstepAudioTypes());

	if (pXML->GetOptionalChildXmlValByName(szTextVal, L"WorldSoundscapeAudioScript"))
		m_iWorldSoundscapeScriptId = gDLL->getAudioTagIndex( szTextVal.GetCString(), AUDIOTAG_SOUNDSCAPE );
	else
		m_iWorldSoundscapeScriptId = -1;

	return true;
}


void CvTerrainInfo::copyNonDefaults(const CvTerrainInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	int iTextDefault = -1;  //all integers which are TEXT_KEYS in the xml are -1 by default

	// The art tag must merge BEFORE the base copy: CvInfoBase::copyNonDefaults calls the virtual
	// getButton(), which resolves through getArtDefineTag(). The declared StringWrapper copy
	// then no-ops.
	if (m_szArtDefineTag.empty()) m_szArtDefineTag = pClassInfo->getArtDefineTag();

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	// Legacy quirk preserved: compares against CLIMATE_ZONE_TEMPERATE instead of NO_CLIMATE_ZONE
	// (-1, the read default), so a module override lacking <ClimateZoneType> does NOT inherit it.
	if (m_eClimate == CLIMATE_ZONE_TEMPERATE) m_eClimate = pClassInfo->getClimate();

	for ( int i = 0; i < GC.getNumFootstepAudioTypes(); i++)
	{
		if (get3DAudioScriptFootstepIndex(i) == -1 && pClassInfo->get3DAudioScriptFootstepIndex(i) != -1)
		{
			if ( NULL == m_pi3DAudioScriptFootstepIndex )
			{
				CvXMLLoadUtility::InitList(&m_pi3DAudioScriptFootstepIndex,GC.getNumFootstepAudioTypes(),-1);
			}
			m_pi3DAudioScriptFootstepIndex[i] = pClassInfo->get3DAudioScriptFootstepIndex(i);
		}
	}

	if (getWorldSoundscapeScriptId() == iTextDefault) m_iWorldSoundscapeScriptId = pClassInfo->getWorldSoundscapeScriptId();
}



// Explicit (not delegated to CvInfoUtil) because the hand-written m_eClimate sits mid-order;
// delegating would drop it and change the asset checksum. Body kept byte-identical to legacy.
void CvTerrainInfo::getCheckSum(uint32_t &iSum) const
{
	PROFILE_EXTRA_FUNC();
	CheckSum(iSum, m_iMovementCost);
	CheckSum(iSum, m_iBuildModifier);
	CheckSum(iSum, m_iDefenseModifier);
	CheckSum(iSum, m_iDistanceToLand);
	CheckSum(iSum, m_eClimate);

	CheckSum(iSum, m_bImpassable);
	CheckSum(iSum, m_bFound);
	CheckSum(iSum, m_bFoundCoast);
	CheckSum(iSum, m_bFoundFreshWater);
	CheckSum(iSum, m_bFreshWaterTerrain);

	// Arrays

	CheckSum(iSum, m_piYields, NUM_YIELD_TYPES);

	CheckSum(iSum, m_iCultureDistance);
	CheckSum(iSum, m_iHealthPercent);

	m_PropertyManipulators.getCheckSum(iSum);
	//TB Combat Mods begin
	CheckSumC(iSum, m_aiCategories);
	CheckSumC(iSum, m_aeMapCategoryTypes);

	//TB Combat Mods end
}


const char* CvTerrainInfo::getButton() const
{
	const CvArtInfoTerrain* pTerrainArtInfo = getArtInfo();
	return pTerrainArtInfo ? pTerrainArtInfo->getButton() : NULL;
}


const CvArtInfoTerrain* CvTerrainInfo::getArtInfo() const
{
	return ARTFILEMGR.getTerrainArtInfo(getArtDefineTag());
}

