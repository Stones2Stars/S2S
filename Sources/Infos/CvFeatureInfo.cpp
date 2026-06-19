//------------------------------------------------------------------------------------------------
//  FILE:    CvFeatureInfo.cpp
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
#include "CvFeatureInfo.h"


//======================================================================================================
//					CvBonusInfo
//======================================================================================================
//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvBonusInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------



//======================================================================================================
//					CvFeatureInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvFeatureInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvFeatureInfo::CvFeatureInfo() :
// Only non-declarative fields here; everything else defaults via initDataMembers().
m_iWorldSoundscapeScriptId(0),
m_pi3DAudioScriptFootstepIndex(NULL)
{
	CvInfoUtil(this).initDataMembers();

	m_zobristValue = GC.getGame().getSorenRand().getInt();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvFeatureInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvFeatureInfo::~CvFeatureInfo()
{
	CvInfoUtil(this).uninitDataMembers(); // frees m_piYieldChange / m_piRiverYieldChange (owned by addYields)

	SAFE_DELETE_ARRAY(m_pi3DAudioScriptFootstepIndex);
}


int CvFeatureInfo::getMovementCost() const
{
	return m_iMovementCost;
}


int CvFeatureInfo::getSeeThroughChange() const
{
	return m_iSeeThroughChange;
}


int CvFeatureInfo::getHealthPercent() const
{
	return m_iHealthPercent;
}


int CvFeatureInfo::getAppearanceProbability() const
{
	return m_iAppearanceProbability;
}


int CvFeatureInfo::getDisappearanceProbability() const
{
	return m_iDisappearanceProbability;
}


int CvFeatureInfo::getGrowthProbability() const
{
	return m_iGrowthProbability;
}


int CvFeatureInfo::getDefenseModifier() const
{
	return m_iDefenseModifier;
}


int CvFeatureInfo::getAdvancedStartRemoveCost() const
{
	return m_iAdvancedStartRemoveCost;
}


int CvFeatureInfo::getWarmingDefense() const			//GWMod new xml field M.A.
{
	return m_iWarmingDefense;
}


int CvFeatureInfo::getPopDestroys() const			//GWMod new xml field M.A.
{
	return m_iPopDestroys;
}


bool CvFeatureInfo::isNoCoast() const
{
	return m_bNoCoast;
}


bool CvFeatureInfo::isNoRiver() const
{
	return m_bNoRiver;
}


bool CvFeatureInfo::isNoAdjacent() const
{
	return m_bNoAdjacent;
}


bool CvFeatureInfo::isRequiresFlatlands() const
{
	return m_bRequiresFlatlands;
}


bool CvFeatureInfo::isRequiresRiver() const
{
	return m_bRequiresRiver;
}


bool CvFeatureInfo::isAddsFreshWater() const
{
	return m_bAddsFreshWater;
}


bool CvFeatureInfo::isImpassable() const
{
	return m_bImpassable;
}


bool CvFeatureInfo::isNoCity() const
{
	return m_bNoCity;
}


bool CvFeatureInfo::isNoImprovement() const
{
	return m_bNoImprovement;
}


bool CvFeatureInfo::isVisibleAlways() const
{
	return m_bVisibleAlways;
}


bool CvFeatureInfo::isNukeImmune() const
{
	return m_bNukeImmune;
}


bool CvFeatureInfo::isCountsAsPeak() const
{
	return m_bCountsAsPeak;
}


// BUG - City Plot Status - start
bool CvFeatureInfo::isOnlyBad() const
{
	PROFILE_EXTRA_FUNC();
	if (getHealthPercent() > 0 || isAddsFreshWater())
	{
		return false;
	}
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		if (getYieldChange(iI) > 0)
		{
			return false;
		}
	}

	return true;
}

// BUG - City Plot Status - end

const char* CvFeatureInfo::getOnUnitChangeTo() const
{
	return m_szOnUnitChangeTo;
}


const char* CvFeatureInfo::getArtDefineTag() const
{
	return m_szArtDefineTag;
}


int CvFeatureInfo::getWorldSoundscapeScriptId() const
{
	return m_iWorldSoundscapeScriptId;
}


const char* CvFeatureInfo::getEffectType() const
{
	return m_szEffectType;
}


int CvFeatureInfo::getEffectProbability() const
{
	return m_iEffectProbability;
}


// Arrays

int CvFeatureInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldChange ? m_piYieldChange[i] : 0;
}


int CvFeatureInfo::getRiverYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piRiverYieldChange ? m_piRiverYieldChange[i] : 0;
}


int CvFeatureInfo::get3DAudioScriptFootstepIndex(int i) const
{
	//	FAssertMsg(i < ?, "Index out of bounds");
	FAssertMsg(i > -1, "Index out of bounds");
	return m_pi3DAudioScriptFootstepIndex ? m_pi3DAudioScriptFootstepIndex[i] : -1;
}


bool CvFeatureInfo::isTerrain(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return algo::any_of_equal(m_aeTerrain, static_cast<TerrainTypes>(i));
}


int CvFeatureInfo::getNumVarieties() const
{
	return getArtInfo()->getNumVarieties();
}


const char* CvFeatureInfo::getButton() const
{
	const CvArtInfoFeature* pFeatureArtInfo = getArtInfo();
	return pFeatureArtInfo ? pFeatureArtInfo->getButton() : NULL;
}


const CvArtInfoFeature* CvFeatureInfo::getArtInfo() const
{
	return ARTFILEMGR.getFeatureArtInfo( getArtDefineTag());
}


int CvFeatureInfo::getSpreadProbability() const
{
	return m_iSpreadProbability;
}


int CvFeatureInfo::getCultureDistance() const
{
	return m_iCultureDistance;
}


const char* CvFeatureInfo::getGrowthSound() const
{
	return m_szGrowthSound;
}


bool CvFeatureInfo::isIgnoreTerrainCulture() const
{
	return m_bIgnoreTerrainCulture;
}


bool CvFeatureInfo::isCanGrowAnywhere() const
{
	return m_bCanGrowAnywhere;
}


// AIAndy: Returns true if the feature can be displayed properly as secondary feature
bool CvFeatureInfo::canBeSecondary() const
{
	return !(getArtInfo()->isRiverArt() || (getArtInfo()->getTileArtType() != TILE_ART_TYPE_NONE));
}


int CvFeatureInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvFeatureInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvFeatureInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}



void CvFeatureInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. The checksum is NOT delegated (see getCheckSum),
	// but keeping the order aligned makes the two trivially comparable.
	// Stays hand-written: m_pi3DAudioScriptFootstepIndex (SetVariableListTagPair-style dynamic
	// array), m_iWorldSoundscapeScriptId (audio tag index lookup), m_zobristValue (runtime).
	util
		.add(m_iSpreadProbability, L"iSpread")
		.add(m_iCultureDistance, L"iCultureDistance")
		.add(m_bIgnoreTerrainCulture, L"bIgnoreTerrainCulture")
		.add(m_bCanGrowAnywhere, L"bCanGrowAnywhere")
		.add(m_iMovementCost, L"iMovement")
		.add(m_iSeeThroughChange, L"iSeeThrough")
		.add(m_iHealthPercent, L"iHealthPercent")
		.add(m_iAppearanceProbability, L"iAppearance")
		.add(m_iDisappearanceProbability, L"iDisappearance")
		.add(m_iGrowthProbability, L"iGrowth")
		.add(m_iDefenseModifier, L"iDefense")
		.add(m_iAdvancedStartRemoveCost, L"iAdvancedStartRemoveCost")
		.add(m_iWarmingDefense, L"iWarmingDefense") //GWMod new xml field M.A.
		.add(m_iPopDestroys, L"iPopDestroys", -1)
		.add(m_bNoCoast, L"bNoCoast")
		.add(m_bNoRiver, L"bNoRiver")
		.add(m_bNoAdjacent, L"bNoAdjacent")
		.add(m_bRequiresFlatlands, L"bRequiresFlatlands")
		.add(m_bRequiresRiver, L"bRequiresRiver")
		.add(m_bCoastalOnly, L"bCoastalOnly")
		.add(m_bAddsFreshWater, L"bAddsFreshWater")
		.add(m_bImpassable, L"bImpassable")
		.add(m_bNoCity, L"bNoCity")
		.add(m_bNoImprovement, L"bNoImprovement")
		.add(m_bNoBonus, L"bNoBonus")
		.add(m_bVisibleAlways, L"bVisibleAlways")
		.add(m_bNukeImmune, L"bNukeImmune")
		.add(m_bCountsAsPeak, L"bCountsAsPeak")
		.add(m_szOnUnitChangeTo, L"OnUnitChangeTo") // checksummed by the legacy hand-written getCheckSum
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")
		.add(m_aiCategories, L"Categories")
		.addYields(m_piYieldChange, L"YieldChanges")
		.addYields(m_piRiverYieldChange, L"RiverYieldChange")
		.add(m_aeTerrain, L"TerrainBooleans")
		.add(m_PropertyManipulators)
		.add(m_iEffectProbability, L"iEffectProbability") // read but not checksummed (legacy omission, preserved)
		.add(m_szEffectType, L"EffectType")
		.add(m_szGrowthSound, L"GrowthSound")
		.add(m_szArtDefineTag, L"ArtDefineTag")
	;
}


bool CvFeatureInfo::read(CvXMLLoadUtility* pXML)
{
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->SetVariableListTagPairForAudioScripts(&m_pi3DAudioScriptFootstepIndex, L"FootstepSounds", GC.getNumFootstepAudioTypes());

	pXML->GetOptionalChildXmlValByName(szTextVal, L"WorldSoundscapeAudioScript");
	if ( szTextVal.GetLength() > 0 )
		m_iWorldSoundscapeScriptId = gDLL->getAudioTagIndex( szTextVal.GetCString(), AUDIOTAG_SOUNDSCAPE );
	else
		m_iWorldSoundscapeScriptId = -1;

	return true;
}


void CvFeatureInfo::copyNonDefaults(const CvFeatureInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	int iDefault = 0;
	int iTextDefault = -1;  //all integers which are TEXT_KEYS in the xml are -1 by default

	// The art tag must merge BEFORE the base copy: CvInfoBase::copyNonDefaults calls the virtual
	// getButton(), which resolves through getArtDefineTag(). The declared StringWrapper copy
	// then no-ops.
	if (m_szArtDefineTag.empty()) m_szArtDefineTag = pClassInfo->getArtDefineTag();

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	for ( int i = 0; i < GC.getNumFootstepAudioTypes(); i++ )
	{
		if ( get3DAudioScriptFootstepIndex(i) == iDefault && pClassInfo->get3DAudioScriptFootstepIndex(i) != iDefault)
		{
			if ( NULL == m_pi3DAudioScriptFootstepIndex )
			{
				CvXMLLoadUtility::InitList(&m_pi3DAudioScriptFootstepIndex,GC.getNumFootstepAudioTypes(),iDefault);
			}
			m_pi3DAudioScriptFootstepIndex[i] = pClassInfo->get3DAudioScriptFootstepIndex(i);
		}
	}
	if (getWorldSoundscapeScriptId() == iTextDefault) m_iWorldSoundscapeScriptId = pClassInfo->getWorldSoundscapeScriptId();
}


// Explicit (not delegated to CvInfoUtil) because the legacy checksum folds the STRING
// m_szOnUnitChangeTo mid-order (StringWrapper checksums are no-ops) and omits the read fields
// m_iEffectProbability / m_szEffectType / m_szGrowthSound. Body kept byte-identical to legacy.
void CvFeatureInfo::getCheckSum(uint32_t &iSum) const
{
	PROFILE_EXTRA_FUNC();
	CheckSum(iSum, m_iSpreadProbability);
	CheckSum(iSum, m_iCultureDistance);
	CheckSum(iSum, m_bIgnoreTerrainCulture);
	CheckSum(iSum, m_bCanGrowAnywhere);
	CheckSum(iSum, m_iMovementCost);
	CheckSum(iSum, m_iSeeThroughChange);
	CheckSum(iSum, m_iHealthPercent);
	CheckSum(iSum, m_iAppearanceProbability);
	CheckSum(iSum, m_iDisappearanceProbability);
	CheckSum(iSum, m_iGrowthProbability);
	CheckSum(iSum, m_iDefenseModifier);
	CheckSum(iSum, m_iAdvancedStartRemoveCost);
	CheckSum(iSum, m_iWarmingDefense);
	CheckSum(iSum, m_iPopDestroys);

	CheckSum(iSum, m_bNoCoast);
	CheckSum(iSum, m_bNoRiver);
	CheckSum(iSum, m_bNoAdjacent);
	CheckSum(iSum, m_bRequiresFlatlands);
	CheckSum(iSum, m_bRequiresRiver);
	CheckSum(iSum, m_bCoastalOnly);
	CheckSum(iSum, m_bAddsFreshWater);
	CheckSum(iSum, m_bImpassable);
	CheckSum(iSum, m_bNoCity);
	CheckSum(iSum, m_bNoImprovement);
	CheckSum(iSum, m_bNoBonus);
	CheckSum(iSum, m_bVisibleAlways);
	CheckSum(iSum, m_bNukeImmune);
	CheckSum(iSum, m_bCountsAsPeak);
	CheckSumC(iSum, m_szOnUnitChangeTo);
	CheckSumC(iSum, m_aeMapCategoryTypes);
	CheckSumC(iSum, m_aiCategories);

	// Arrays

	CheckSum(iSum, m_piYieldChange, NUM_YIELD_TYPES);
	CheckSum(iSum, m_piRiverYieldChange, NUM_YIELD_TYPES);

	CheckSumC(iSum, m_aeTerrain);
	m_PropertyManipulators.getCheckSum(iSum);
}

