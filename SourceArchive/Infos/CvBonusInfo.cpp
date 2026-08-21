#include "CvGameCoreDLL.h"
#include "Tools/FProfiler.h"

#include "UI/CvArtFileMgr.h"
#include "CvBonusInfo.h"
#include "Defines/CvDefines.h"
#include "CvImprovementInfo.h"
#include "CvInfoUtil.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Tools/CheckSum.h"
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"

CvBonusInfo::CvBonusInfo() :
	// Only non-declarative fields here; everything else defaults via initDataMembers().
	m_iRandAppearance1(0)
	, m_iRandAppearance2(0)
	, m_iRandAppearance3(0)
	, m_iRandAppearance4(0)
	, m_iChar(0)
	, m_piImprovementChange(NULL)
	, m_tradeProvidingImprovements(NULL)
{
	CvInfoUtil(this).initDataMembers();
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvBonusInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvBonusInfo::~CvBonusInfo()
{
	CvInfoUtil(this).uninitDataMembers(); // frees m_piYieldChange (owned by addYields)

	SAFE_DELETE_ARRAY(m_piImprovementChange);
}

int CvBonusInfo::getBonusClassType() const
{
	return m_iBonusClassType;
}

int CvBonusInfo::getChar() const
{
	return m_iChar;
}

void CvBonusInfo::setChar(int i)
{
	m_iChar = i;
}

int CvBonusInfo::getTechReveal() const
{
	return m_iTechReveal;
}

int CvBonusInfo::getTechCityTrade() const
{
	return m_iTechCityTrade;
}

int CvBonusInfo::getTechObsolete() const
{
	return m_iTechObsolete;
}

int CvBonusInfo::getAITradeModifier() const
{
	return m_iAITradeModifier;
}

int CvBonusInfo::getAIObjective() const
{
	return m_iAIObjective;
}

int CvBonusInfo::getHealth() const
{
	return m_iHealth;
}

int CvBonusInfo::getHappiness() const
{
	return m_iHappiness;
}

int CvBonusInfo::getMinAreaSize() const
{
	return m_iMinAreaSize;
}

int CvBonusInfo::getMinLatitude() const
{
	return m_iMinLatitude;
}

int CvBonusInfo::getMaxLatitude() const
{
	return m_iMaxLatitude;
}

int CvBonusInfo::getPlacementOrder() const
{
	return m_iPlacementOrder;
}

int CvBonusInfo::getRandAppearance() const
{
	return m_iConstAppearance +
		GC.getGame().getMapRandNum(m_iRandAppearance1, "random1") +
		GC.getGame().getMapRandNum(m_iRandAppearance2, "random2") +
		GC.getGame().getMapRandNum(m_iRandAppearance3, "random3") +
		GC.getGame().getMapRandNum(m_iRandAppearance4, "random4");
}

bool CvBonusInfo::isMapBonus() const
{
	return (
			m_iConstAppearance > 0
		||	m_iRandAppearance1 > 0
		||	m_iRandAppearance2 > 0
		||	m_iRandAppearance3 > 0
		||	m_iRandAppearance4 > 0
	);
}

int CvBonusInfo::getPercentPerPlayer() const
{
	return m_iPercentPerPlayer;
}

int CvBonusInfo::getTilesPer() const
{
	return m_iTilesPer;
}

int CvBonusInfo::getMinLandPercent() const
{
	return m_iMinLandPercent;
}

int CvBonusInfo::getUniqueRange() const
{
	return m_iUniqueRange;
}

int CvBonusInfo::getGroupRange() const
{
	return m_iGroupRange;
}

int CvBonusInfo::getGroupRand() const
{
	return m_iGroupRand;
}

bool CvBonusInfo::isOneArea() const
{
	return m_bOneArea;
}

bool CvBonusInfo::isHills() const
{
	return m_bHills;
}

bool CvBonusInfo::isFlatlands() const
{
	return m_bFlatlands;
}

bool CvBonusInfo::isBonusCoastalOnly() const
{
	return m_bBonusCoastalOnly;
}

bool CvBonusInfo::isNoRiverSide() const
{
	return m_bNoRiverSide;
}

bool CvBonusInfo::isNormalize() const
{
	return m_bNormalize;
}

const char* CvBonusInfo::getArtDefineTag() const
{
	return m_szArtDefineTag;
}

// Arrays

int CvBonusInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldChange ? m_piYieldChange[i] : 0;
}

int* CvBonusInfo::getYieldChangeArray() const
{
	return m_piYieldChange;
}

int CvBonusInfo::getImprovementChange(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumImprovementInfos(), i);
	return m_piImprovementChange ? m_piImprovementChange[i] : 0;
}

bool CvBonusInfo::isTerrain(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return algo::any_of_equal(m_aeTerrain, static_cast<TerrainTypes>(i));
}

bool CvBonusInfo::isFeature(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), i);
	return algo::any_of_equal(m_aeFeature, static_cast<FeatureTypes>(i));
}

bool CvBonusInfo::isFeatureTerrain(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), i);
	return algo::any_of_equal(m_aeFeatureTerrain, static_cast<TerrainTypes>(i));
}

int CvBonusInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}

int CvBonusInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}

bool CvBonusInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


const char* CvBonusInfo::getButton() const
{
	const CvArtInfoBonus* pBonusArtInfo = getArtInfo();
	return pBonusArtInfo ? pBonusArtInfo->getButton() : NULL;
}

bool CvBonusInfo::isPeaks() const
{
	return m_bPeaks;
}


ImprovementTypes CvBonusInfo::getProvidedByImprovementType(const int i) const
{
	return m_providedByImprovementTypes[i];
}

int CvBonusInfo::getNumProvidedByImprovementTypes() const
{
	return (int)m_providedByImprovementTypes.size();
}

bool CvBonusInfo::isProvidedByImprovementType(const ImprovementTypes i) const
{
	return algo::any_of_equal(m_providedByImprovementTypes, i);
}

void CvBonusInfo::setProvidedByImprovementTypes(const ImprovementTypes eType)
{
	m_providedByImprovementTypes.push_back(eType);
}


void CvBonusInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. The checksum is NOT delegated (see getCheckSum),
	// but keeping the order aligned makes the two trivially comparable.
	// Stays hand-written: m_iRandAppearance1-4 (nested under <Rands>, no wrapper for nested scalar
	// groups), m_piImprovementChange (dead member - never read from XML), m_iChar (runtime GameFont).
	util
		.addEnumAsInt(m_iBonusClassType, L"BonusClassType")
		.addEnumAsInt(m_iTechReveal, L"TechReveal")
		.addEnumAsInt(m_iTechCityTrade, L"TechCityTrade")
		.addEnumAsInt(m_iTechObsolete, L"TechObsolete")
		.add(m_iAITradeModifier, L"iAITradeModifier")
		.add(m_iAIObjective, L"iAIObjective")
		.add(m_iHealth, L"iHealth")
		.add(m_iHappiness, L"iHappiness")
		.add(m_iMinAreaSize, L"iMinAreaSize")
		.add(m_iMinLatitude, L"iMinLatitude")
		.add(m_iMaxLatitude, L"iMaxLatitude", 90)
		.add(m_iPlacementOrder, L"iPlacementOrder", -1)
		.add(m_iConstAppearance, L"iConstAppearance")
		.add(m_iPercentPerPlayer, L"iPlayer")
		.add(m_iTilesPer, L"iTilesPer")
		.add(m_iMinLandPercent, L"iMinLandPercent")
		.add(m_iUniqueRange, L"iUniqueRange")
		.add(m_iGroupRange, L"iGroupRange")
		.add(m_iGroupRand, L"iGroupRand")
		.add(m_bOneArea, L"bArea")
		.add(m_bHills, L"bHills")
		.add(m_bFlatlands, L"bFlatlands")
		.add(m_bBonusCoastalOnly, L"bBonusCoastalOnly")
		.add(m_bNoRiverSide, L"bNoRiverSide")
		.add(m_bNormalize, L"bNormalize")
		.addYields(m_piYieldChange, L"YieldChanges")
		.add(m_aeTerrain, L"TerrainBooleans")
		.add(m_aeFeature, L"FeatureBooleans")
		.add(m_aeFeatureTerrain, L"FeatureTerrainBooleans")
		.add(m_aeMapCategoryTypes, L"MapCategoryTypes")
		.add(m_bPeaks, L"bPeaks")
		.add(m_aiCategories, L"Categories")
		.add(m_PropertyManipulators)
		.add(m_szArtDefineTag, L"ArtDefineTag")
	;
}

// Explicit (not delegated to CvInfoUtil) because the hand-written m_iRandAppearance1-4 sit
// mid-order; delegating would drop them and change the asset checksum. Body kept byte-identical
// to the legacy checksum (m_piImprovementChange is always NULL and contributes nothing).
void CvBonusInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	CheckSum(iSum, m_iBonusClassType);
	CheckSum(iSum, m_iTechReveal);
	CheckSum(iSum, m_iTechCityTrade);
	CheckSum(iSum, m_iTechObsolete);
	CheckSum(iSum, m_iAITradeModifier);
	CheckSum(iSum, m_iAIObjective);
	CheckSum(iSum, m_iHealth);
	CheckSum(iSum, m_iHappiness);
	CheckSum(iSum, m_iMinAreaSize);
	CheckSum(iSum, m_iMinLatitude);
	CheckSum(iSum, m_iMaxLatitude);
	CheckSum(iSum, m_iPlacementOrder);
	CheckSum(iSum, m_iConstAppearance);
	CheckSum(iSum, m_iRandAppearance1);
	CheckSum(iSum, m_iRandAppearance2);
	CheckSum(iSum, m_iRandAppearance3);
	CheckSum(iSum, m_iRandAppearance4);
	CheckSum(iSum, m_iPercentPerPlayer);
	CheckSum(iSum, m_iTilesPer);
	CheckSum(iSum, m_iMinLandPercent);
	CheckSum(iSum, m_iUniqueRange);
	CheckSum(iSum, m_iGroupRange);
	CheckSum(iSum, m_iGroupRand);

	CheckSum(iSum, m_bOneArea);
	CheckSum(iSum, m_bHills);
	CheckSum(iSum, m_bFlatlands);
	CheckSum(iSum, m_bBonusCoastalOnly);
	CheckSum(iSum, m_bNoRiverSide);
	CheckSum(iSum, m_bNormalize);
	CheckSumI(iSum, NUM_YIELD_TYPES, m_piYieldChange);
	CheckSumI(iSum, GC.getNumImprovementInfos(), m_piImprovementChange);
	CheckSumC(iSum, m_aeTerrain);
	CheckSumC(iSum, m_aeFeature);
	CheckSumC(iSum, m_aeFeatureTerrain);
	CheckSumC(iSum, m_aeMapCategoryTypes);
	CheckSum(iSum, m_bPeaks);

	CheckSumC(iSum, m_aiCategories);

	m_PropertyManipulators.getCheckSum(iSum);
}

bool CvBonusInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	// Appearance rands are nested under <Rands>; no wrapper for nested scalar groups.
	if (pXML->TryMoveToXmlFirstChild(L"Rands"))
	{
		pXML->GetOptionalChildXmlValByName(&m_iRandAppearance1, L"iRandApp1");
		pXML->GetOptionalChildXmlValByName(&m_iRandAppearance2, L"iRandApp2");
		pXML->GetOptionalChildXmlValByName(&m_iRandAppearance3, L"iRandApp3");
		pXML->GetOptionalChildXmlValByName(&m_iRandAppearance4, L"iRandApp4");

		pXML->MoveToXmlParent();
	}

	return true;
}

void CvBonusInfo::copyNonDefaults(const CvBonusInfo* pClassInfo)
{
	// The art tag must merge BEFORE the base copy: CvInfoBase::copyNonDefaults calls the virtual
	// getButton(), which resolves through getArtDefineTag() — an empty tag asserts in
	// ARTFILEMGR.getBonusArtInfo. The declared StringWrapper copy below then no-ops.
	if (m_szArtDefineTag.empty()) m_szArtDefineTag = pClassInfo->getArtDefineTag();

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if (m_iRandAppearance1 == 0) m_iRandAppearance1 = pClassInfo->m_iRandAppearance1;
	if (m_iRandAppearance2 == 0) m_iRandAppearance2 = pClassInfo->m_iRandAppearance2;
	if (m_iRandAppearance3 == 0) m_iRandAppearance3 = pClassInfo->m_iRandAppearance3;
	if (m_iRandAppearance4 == 0) m_iRandAppearance4 = pClassInfo->m_iRandAppearance4;
}

const std::vector<std::pair<ImprovementTypes, BuildTypes> >* CvBonusInfo::getTradeProvidingImprovements()
{
	PROFILE_EXTRA_FUNC();
	if (m_tradeProvidingImprovements == NULL)
	{

		if (m_tradeProvidingImprovements == NULL)
		{
			const int eBonus = GC.getInfoTypeForString(m_szType);
			std::vector<std::pair<ImprovementTypes, BuildTypes> >* tradeProvidingImprovements = new std::vector<std::pair<ImprovementTypes, BuildTypes> >();

			for (int iJ = 0; iJ < GC.getNumBuildInfos(); iJ++)
			{
				const BuildTypes eBuild = static_cast<BuildTypes>(iJ);
				const ImprovementTypes eImp = GC.getBuildInfo(eBuild).getImprovement();

				if (eImp != NO_IMPROVEMENT && GC.getImprovementInfo(eImp).isImprovementBonusTrade(eBonus))
				{
					tradeProvidingImprovements->push_back(std::make_pair(eImp, eBuild));
				}
			}

			m_tradeProvidingImprovements = (volatile std::vector<std::pair<ImprovementTypes, BuildTypes> >*)tradeProvidingImprovements;
		}
	}

	return (const std::vector<std::pair<ImprovementTypes, BuildTypes> >*)m_tradeProvidingImprovements;
}

// ===== Methods relocated from CvInfos.cpp =====

const CvArtInfoBonus* CvBonusInfo::getArtInfo() const
{
	return ARTFILEMGR.getBonusArtInfo( getArtDefineTag());
}

