//------------------------------------------------------------------------------------------------
//  FILE:    CvSpawnInfo.cpp
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
#include "CvSpawnInfo.h"
#include "CvTagReads.h"   // the ONE shared surface for the json.md §8 tag reads

// The json.md §8 TAG read this file makes. `wild` is its own tag (UNITCOMBAT_WILD, 198 units), so the
// spawn test stays EXACT rather than widening to `animal` and sweeping tamed animals in with it. The
// info's tag bitset is already the fold of the unit's own tags with its combat classes'
// (deriveAtRegistryComplete), so the combat-class-authored tag reads straight off the unit.


//======================================================================================================
//					CvSpawnInfo
//======================================================================================================
CvSpawnInfo::CvSpawnInfo()
	:
	m_iRateOverride(100)
{
	CvInfoUtil(this).initDataMembers();
}


CvSpawnInfo::~CvSpawnInfo()
{
	CvInfoUtil(this).uninitDataMembers();
}


void CvSpawnInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. Hand-written fields (see read()):
	// - m_iRateOverride: indirect read - <rateOverrideDefineName> names a GlobalDefines entry
	//   whose value is looked up via GC.getDefineINT; not a plain tag-value read.
	// - The five type lists (m_bonusTypes, m_terrainTypes, m_featureTypes, m_featureTerrainTypes,
	//   m_spawnGroup): the legacy walks preserve duplicates and document order; SetOptionalVector
	//   (the vector wrapper) dedups and sorts. SpawnGroups legitimately list the same unit twice
	//   in CIV4SpawnInfos.xml (UNIT_BISON, UNIT_WOLF), and one unit is spawned per entry, so
	//   dedup would change gameplay; sorting would change the asset checksum.
	// getCheckSum stays explicit because these fields sit mid-order in the legacy checksum.
	util
		.addEnum(m_eUnitType, L"UnitType")
		.addEnum(m_ePrereqTech, L"PrereqTech")
		.addEnum(m_eObsoleteTechType, L"ObsoleteTech")
		.addEnumAsInt(m_iPlayerType, L"PlayerType")
		.add(m_iTurns, L"iTurns")
		.add(m_iGlobalTurns, L"iGlobalTurns", -1)
		.add(m_iMaxLocalDensity, L"iMaxLocalDensity")
		.add(m_iMinAreaPlotsPerPlayerUnit, L"iMinAreaPlotsPerPlayerUnit")
		.add(m_iMinAreaPlotsPerUnitType, L"iMinAreaPlotsPerUnitType")
		.add(m_iStartDate, L"iStartDate", -200000)
		.add(m_iEndDate, L"iEndDate", 50000)
		.add(m_bTreatAsBarbarian, L"bTreatAsBarbarian")
		.add(m_bNeutralOnly, L"bNeutralOnly", true)
		.add(m_bNotInView, L"bNotInView")
		.add(m_bNoSpeedNormalization, L"bNoSpeedNormalization")
		.add(m_bHills, L"bHills")
		.add(m_bFlatlands, L"bFlatlands", true)
		.add(m_bPeaks, L"bPeaks")
		.add(m_bFreshWaterOnly, L"bFreshWaterOnly")
		.add(m_bLatitudeAbs, L"bLatitudeAbs", true)
		.add(m_iMinLatitude, L"iMinLatitude", -90)
		.add(m_iMaxLatitude, L"iMaxLatitude", 90)
		.add(m_iMinLongitude, L"iMinLongitude", -180)
		.add(m_iMaxLongitude, L"iMaxLongitude", 180)
		.addBoolExpr(m_pExprSpawnCondition, L"SpawnCondition")
	;
}


bool CvSpawnInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;

	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->GetOptionalChildXmlValByName(szTextVal, L"rateOverrideDefineName");
	if ( szTextVal.GetLength() != 0 )
	{
		m_iRateOverride = std::max(GC.getDefineINT(szTextVal, 100),0);
	}

	if(pXML->TryMoveToXmlFirstChild(L"BonusTypes"))
	{
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"BonusType"))
			{
				do
				{
					int iBonusType;

					pXML->GetXmlVal(szTextVal);
					iBonusType = pXML->GetInfoClass(szTextVal);

					m_bonusTypes.push_back((BonusTypes)iBonusType);
				} while(pXML->TryMoveToXmlNextSibling());
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"TerrainTypes"))
	{
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"TerrainType"))
			{
				do
				{
					int iTerrainType;

					pXML->GetXmlVal(szTextVal);
					iTerrainType = pXML->GetInfoClass(szTextVal);

					m_terrainTypes.push_back((TerrainTypes)iTerrainType);
				} while(pXML->TryMoveToXmlNextSibling());
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"FeatureTypes"))
	{
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"FeatureType"))
			{
				do
				{
					int iFeatureType;

					pXML->GetXmlVal(szTextVal);
					iFeatureType = pXML->GetInfoClass(szTextVal);

					m_featureTypes.push_back((FeatureTypes)iFeatureType);
				} while(pXML->TryMoveToXmlNextSibling());
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"FeatureTerrainTypes"))
	{
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"TerrainType"))
			{
				do
				{
					int iTerrainType;

					pXML->GetXmlVal(szTextVal);
					iTerrainType = pXML->GetInfoClass(szTextVal);

					m_featureTerrainTypes.push_back((TerrainTypes)iTerrainType);
				} while(pXML->TryMoveToXmlNextSibling());
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"SpawnGroup"))
	{
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"UnitType"))
			{
				do
				{
					int iUnitType;

					pXML->GetXmlVal(szTextVal);
					iUnitType = pXML->GetInfoClass(szTextVal);

					m_spawnGroup.push_back((UnitTypes)iUnitType);
				} while(pXML->TryMoveToXmlNextSibling());
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	return true;
}


void CvSpawnInfo::copyNonDefaults(const CvSpawnInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	// The hand-written fields (m_iRateOverride and the five type lists) were never merged by the
	// legacy copyNonDefaults ("not yet implemented"); that behaviour is kept unchanged.
}


const BoolExpr* CvSpawnInfo::getSpawnCondition() const
{
	return m_pExprSpawnCondition;
}


int CvSpawnInfo::getTurnRate() const
{
	return m_iTurns;
}


int CvSpawnInfo::getGlobalTurnRate() const
{
	return m_iGlobalTurns;
}


int CvSpawnInfo::getMaxLocalDensity() const
{
	return m_iMaxLocalDensity;
}


int CvSpawnInfo::getMinAreaPlotsPerPlayerUnit() const
{
	return m_iMinAreaPlotsPerPlayerUnit;
}


int CvSpawnInfo::getMinAreaPlotsPerUnitType() const
{
	return m_iMinAreaPlotsPerUnitType;
}


int CvSpawnInfo::getStartDate() const
{
	return m_iStartDate;
}


int	CvSpawnInfo::getEndDate() const
{
	return m_iEndDate;
}


UnitTypes CvSpawnInfo::getUnitType() const
{
	return m_eUnitType;
}


PlayerTypes	CvSpawnInfo::getPlayer() const
{
	return (PlayerTypes)m_iPlayerType;
}


bool CvSpawnInfo::getTreatAsBarbarian() const
{
	return m_bTreatAsBarbarian;
}


bool CvSpawnInfo::getNeutralOnly() const
{
	if (CvTagReads::wild(GC.getUnitInfo(getUnitType()).getTags()) && GC.getGame().isOption(GAMEOPTION_ANIMAL_DANGEROUS))
	{
		return false;
	}
	return m_bNeutralOnly;
}


bool CvSpawnInfo::getNoSpeedNormalization() const
{
	return m_bNoSpeedNormalization;
}


bool CvSpawnInfo::getNotInView() const
{
	return m_bNotInView;
}


bool CvSpawnInfo::getHills() const
{
	return m_bHills;
}


bool CvSpawnInfo::getFlatlands() const
{
	return m_bFlatlands;
}


bool CvSpawnInfo::getPeaks() const
{
	return m_bPeaks;
}


bool CvSpawnInfo::getFreshWaterOnly() const
{
	return m_bFreshWaterOnly;
}


bool CvSpawnInfo::getLatitudeAbs() const
{
	return m_bLatitudeAbs;
}


int CvSpawnInfo::getMinLatitude() const
{
	return m_iMinLatitude;
}


int CvSpawnInfo::getMaxLatitude() const
{
	return m_iMaxLatitude;
}


int CvSpawnInfo::getMinLongitude() const
{
	return m_iMinLongitude;
}


int CvSpawnInfo::getMaxLongitude() const
{
	return m_iMaxLongitude;
}


int CvSpawnInfo::getRateOverride() const
{
	return m_iRateOverride;
}


void CvSpawnInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_eUnitType);
	CheckSum(iSum, m_ePrereqTech);
	CheckSum(iSum, m_eObsoleteTechType);
	CheckSum(iSum, m_iPlayerType);
	CheckSum(iSum, m_iTurns);
	CheckSum(iSum, m_iGlobalTurns);
	CheckSum(iSum, m_iMaxLocalDensity);
	CheckSum(iSum, m_iMinAreaPlotsPerPlayerUnit);
	CheckSum(iSum, m_iMinAreaPlotsPerUnitType);
	CheckSum(iSum, m_iStartDate);
	CheckSum(iSum, m_iEndDate);
	CheckSum(iSum, m_bTreatAsBarbarian);
	CheckSum(iSum, m_bNeutralOnly);
	CheckSum(iSum, m_bNotInView);
	CheckSum(iSum, m_bNoSpeedNormalization);
	CheckSum(iSum, m_bHills);
	CheckSum(iSum, m_bFlatlands);
	CheckSum(iSum, m_bPeaks);
	CheckSum(iSum, m_bFreshWaterOnly);

	CheckSum(iSum, m_bLatitudeAbs);
	CheckSum(iSum, m_iMinLatitude);
	CheckSum(iSum, m_iMaxLatitude);
	CheckSum(iSum, m_iMinLongitude);
	CheckSum(iSum, m_iMaxLongitude);

	CheckSum(iSum, m_iRateOverride);
	CheckSumC(iSum, m_bonusTypes);
	CheckSumC(iSum, m_terrainTypes);
	CheckSumC(iSum, m_featureTypes);
	CheckSumC(iSum, m_featureTerrainTypes);
	CheckSumC(iSum, m_spawnGroup);

	if (m_pExprSpawnCondition)
	{
		m_pExprSpawnCondition->getCheckSum(iSum);
	}
}

