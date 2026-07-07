//------------------------------------------------------------------------------------------------
//  FILE:    CvPropertyInfo.cpp
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
#include "CvPropertyInfo.h"

/************************************************************************************************/
/* MODULAR_LOADING_CONTROL				 END												  */
/************************************************************************************************/

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvPropertyInfo
//
//  DESC:   Contains info about generic properties which can be added to buildings
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CvPropertyInfo::CvPropertyInfo() :
								// Hand-written (non-declarative) fields only - the rest is initialized by initDataMembers()
								m_iChar(0), // runtime GameFont char, set later via setChar
								m_eAIScaleType(AISCALE_NONE)
{
	PROFILE_EXTRA_FUNC();
	CvInfoUtil(this).initDataMembers();

	for (int i=0; i < NUM_GAMEOBJECTS; i++)
	{
		for (int j=0; j < NUM_GAMEOBJECTS; j++)
		{
			m_aaiChangePropagator[i][j] = 0;
		}
	}
}


CvPropertyInfo::~CvPropertyInfo()
{
	PROFILE_EXTRA_FUNC();
	foreach_(const PropertyBuilding& propBuilding, m_aPropertyBuildings)
	{
		GC.removeDelayedResolution((int*)&propBuilding.eBuilding);
	}

	foreach_(const PropertyPromotion& propPromotion, m_aPropertyPromotions)
	{
		GC.removeDelayedResolution((int*)&propPromotion.ePromotion);
	}
}


void CvPropertyInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order for the wrapped subset; FontButtonIndex is parked
	// last because the legacy checksum deliberately omits it. The class keeps an explicit
	// getCheckSum: AIScaleType (non-info enum, no InfoClassTraits => no addEnum), the 2D
	// ChangePropagator array, the TargetLevelbyEraTypes pair-vector and the delayed-resolution
	// PropertyBuildings/PropertyPromotions walks stay hand-written and sit mid-order in it.
	// The CvWString display texts also stay hand-written (only CvString is wrapper-expressible).
	util
		.add(m_bSourceDrain, L"bSourceDrain")
		.add(m_bOAType, L"bOAType")
		.add(m_iAIWeight, L"iAIWeight")
		.add(m_iOperationalRangeMin, L"iOperationalRangeMin", -500)
		.add(m_iOperationalRangeMax, L"iOperationalRangeMax", 500)
		.add(m_iTargetLevel, L"iTargetLevel")
		.add(m_iTrainReluctance, L"iTrainReluctance", 1)
		.add(m_PropertyManipulators)
		// Not in the legacy checksum:
		.add(m_iFontButtonIndex, L"FontButtonIndex")
	;
}


bool CvPropertyInfo::read(CvXMLLoadUtility* pXML)
{

	PROFILE_EXTRA_FUNC();
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	// Hand-written: CvWString members (only CvString is wrapper-expressible)
	pXML->GetOptionalChildXmlValByName(m_szValueDisplayText, L"ValueDisplayText");
	pXML->GetOptionalChildXmlValByName(m_szChangeDisplayText, L"ChangeDisplayText");
	pXML->GetOptionalChildXmlValByName(m_szChangeAllCitiesDisplayText, L"ChangeAllCitiesDisplayText");
	pXML->GetOptionalChildXmlValByName(m_szPrereqMinDisplayText, L"PrereqMinDisplayText");
	pXML->GetOptionalChildXmlValByName(m_szPrereqMaxDisplayText, L"PrereqMaxDisplayText");

	// Hand-written: AIScaleTypes is not an info class (no InfoClassTraits), so addEnum can't be used
	CvString szTextVal;
	pXML->GetOptionalChildXmlValByName(szTextVal, L"AIScaleType");
	m_eAIScaleType = (AIScaleTypes) pXML->GetInfoClass(szTextVal);

	if(pXML->TryMoveToXmlFirstChild(L"ChangePropagators"))
	{
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"ChangePropagator"))
			{
				do
				{
					int iChangePercent;
					CvString from;
					pXML->GetChildXmlValByName(from, L"GameObjectTypeFrom");
					int eFrom = pXML->GetInfoClass(from);
					CvString to;
					pXML->GetChildXmlValByName(to, L"GameObjectTypeTo");
					int eTo = pXML->GetInfoClass(to);
					pXML->GetChildXmlValByName(&iChangePercent, L"iChangePercent");
					if (eFrom == -1 || eTo == -1)
					{
						CvXMLLoadUtility::showXMLError("ChangePropagator From (%s) and To (%s) must both be valid", from.c_str(), to.c_str());
					}
					else
					{
						m_aaiChangePropagator[eFrom][eTo] = iChangePercent;
					}
				}
				while(pXML->TryMoveToXmlNextSibling());
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"PropertyBuildings"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"PropertyBuilding" );
		m_aPropertyBuildings.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"PropertyBuilding"))
			{
				do
				{
					pXML->GetChildXmlValByName(&(m_aPropertyBuildings[i].iMinValue), L"iMinValue");
					pXML->GetChildXmlValByName(&(m_aPropertyBuildings[i].iMaxValue), L"iMaxValue");
					pXML->GetChildXmlValByName(szTextVal, L"BuildingType");
					GC.addDelayedResolution((int*)&(m_aPropertyBuildings[i].eBuilding), szTextVal);
					i++;
				}
				while(pXML->TryMoveToXmlNextSibling(L"PropertyBuilding"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	if(pXML->TryMoveToXmlFirstChild(L"PropertyPromotions"))
	{
		int i = 0;
		const int iNum = pXML->GetXmlChildrenNumber(L"PropertyPromotion");
		m_aPropertyPromotions.resize(iNum); // Important to keep the delayed resolution pointers correct

		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"PropertyPromotion"))
			{
				do
				{
					pXML->GetChildXmlValByName(&(m_aPropertyPromotions[i].iMinValue), L"iMinValue");
					pXML->GetChildXmlValByName(&(m_aPropertyPromotions[i].iMaxValue), L"iMaxValue");
					pXML->GetChildXmlValByName(szTextVal, L"PromotionType");
					GC.addDelayedResolution((int*)&(m_aPropertyPromotions[i].ePromotion), szTextVal);
					i++;
				} while(pXML->TryMoveToXmlNextSibling(L"PropertyPromotion"));
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	m_aTargetLevelbyEraTypes.clear();
	if (pXML->TryMoveToXmlFirstChild(L"TargetLevelbyEraTypes"))
	{
		const int iNumSibs = pXML->GetXmlChildrenNumber();

		if (0 < iNumSibs)
		{
			if (pXML->TryMoveToXmlFirstChild())
			{
				for (int j = 0; j < iNumSibs; ++j)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						EraTypes eEra = (EraTypes)pXML->GetInfoClass(szTextVal);
						int iLevel;
						pXML->GetNextXmlVal(&iLevel);
						m_aTargetLevelbyEraTypes.push_back(std::make_pair(eEra, iLevel));

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

	return true;
}


void CvPropertyInfo::copyNonDefaults(const CvPropertyInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const CvWString wDefault = CvWString::format(L"").GetCString();

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if (getAIScaleType() == AISCALE_NONE) m_eAIScaleType = pClassInfo->getAIScaleType();
	if (getValueDisplayText() == NULL || getValueDisplayText() == wDefault) m_szValueDisplayText = pClassInfo->getValueDisplayText();
	if (getChangeDisplayText() == NULL || getChangeDisplayText() == wDefault) m_szChangeDisplayText = pClassInfo->getChangeDisplayText();
	if (getChangeAllCitiesDisplayText() == NULL || getChangeAllCitiesDisplayText() == wDefault) m_szChangeAllCitiesDisplayText = pClassInfo->getChangeAllCitiesDisplayText();
	if (getPrereqMinDisplayText() == NULL || getPrereqMinDisplayText() == wDefault) m_szPrereqMinDisplayText = pClassInfo->getPrereqMinDisplayText();
	if (getPrereqMaxDisplayText() == NULL || getPrereqMaxDisplayText() == wDefault) m_szPrereqMaxDisplayText = pClassInfo->getPrereqMaxDisplayText();

	for (int i=0; i < NUM_GAMEOBJECTS; i++)
	{
		for (int j=0; j < NUM_GAMEOBJECTS; j++)
		{
			if (m_aaiChangePropagator[i][j] == 0)
			{
				m_aaiChangePropagator[i][j] = pClassInfo->getChangePropagator((GameObjectTypes)i,(GameObjectTypes)j);
			}
		}
	}

	if (getNumPropertyBuildings() == 0)
	{
		const int iNum = pClassInfo->getNumPropertyBuildings();
		m_aPropertyBuildings.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aPropertyBuildings[i] = pClassInfo->getPropertyBuilding(i);
			GC.copyNonDefaultDelayedResolution((int*)&(m_aPropertyBuildings[i].eBuilding), (int*)&(pClassInfo->getPropertyBuilding(i).eBuilding));
		}
	}

	if (getNumPropertyPromotions() == 0)
	{
		const int iNum = pClassInfo->getNumPropertyPromotions();
		m_aPropertyPromotions.resize(iNum);
		for (int i=0; i<iNum; i++)
		{
			m_aPropertyPromotions[i] = pClassInfo->getPropertyPromotion(i);
			GC.copyNonDefaultDelayedResolution((int*)&(m_aPropertyPromotions[i].ePromotion), (int*)&(pClassInfo->getPropertyPromotion(i).ePromotion));
		}
	}

	if (getNumTargetLevelbyEraTypes()==0)
	{
		for (int i=0; i < pClassInfo->getNumTargetLevelbyEraTypes(); i++)
		{
			m_aTargetLevelbyEraTypes.push_back(std::make_pair((EraTypes)i, pClassInfo->getTargetLevelbyEraType(i)));
		}
	}
}


// Explicit (not delegated to CvInfoUtil) on purpose: the hand-written fields (AIScaleType, the
// TargetLevelbyEraTypes pair-vector, the 2D ChangePropagator array, PropertyBuildings and
// PropertyPromotions) sit mid-order in the legacy checksum, and the legacy checksum deliberately
// omits the (read) FontButtonIndex. Keep this byte-identical to the legacy order.
void CvPropertyInfo::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	CheckSum(iSum, m_bSourceDrain);
	CheckSum(iSum, m_bOAType);
	CheckSum(iSum, m_iAIWeight);
	CheckSum(iSum, m_eAIScaleType);
	CheckSum(iSum, m_iOperationalRangeMin);
	CheckSum(iSum, m_iOperationalRangeMax);
	CheckSum(iSum, m_iTargetLevel);
	CheckSum(iSum, m_iTrainReluctance);
	CheckSumC(iSum, m_aTargetLevelbyEraTypes);

	for (int i=0; i < NUM_GAMEOBJECTS; i++)
	{
		for (int j=0; j < NUM_GAMEOBJECTS; j++)
		{
			CheckSum(iSum, m_aaiChangePropagator[i][j]);
		}
	}

	foreach_(const PropertyBuilding& propBuilding, m_aPropertyBuildings)
	{
		CheckSum(iSum, propBuilding.iMinValue);
		CheckSum(iSum, propBuilding.iMaxValue);
		CheckSum(iSum, (int)propBuilding.eBuilding);
	}

	foreach_(const PropertyPromotion& propPromotion, m_aPropertyPromotions)
	{
		CheckSum(iSum, propPromotion.iMinValue);
		CheckSum(iSum, propPromotion.iMaxValue);
		CheckSum(iSum, (int)propPromotion.ePromotion);
	}

	m_PropertyManipulators.getCheckSum(iSum);
}


int CvPropertyInfo::getChar() const
{
	return m_iChar;
}


void CvPropertyInfo::setChar(int i)
{
	m_iChar = i;
}


int CvPropertyInfo::getFontButtonIndex() const
{
	return m_iFontButtonIndex;
}


bool CvPropertyInfo::isSourceDrain() const
{
	return m_bSourceDrain;
}


bool CvPropertyInfo::isOAType() const
{
	return m_bOAType;
}


int	CvPropertyInfo::getAIWeight() const
{
	return m_iAIWeight;
}


int	CvPropertyInfo::getOperationalRangeMin() const
{
	return m_iOperationalRangeMin;
}


int	CvPropertyInfo::getOperationalRangeMax() const
{
	return m_iOperationalRangeMax;
}


int	CvPropertyInfo::getTargetLevel() const
{
	return m_iTargetLevel;
}


int	CvPropertyInfo::getTrainReluctance() const
{
	return m_iTrainReluctance;
}


AIScaleTypes CvPropertyInfo::getAIScaleType() const
{
	return m_eAIScaleType;
}


CvWString CvPropertyInfo::getValueDisplayText() const
{
	return m_szValueDisplayText;
}


CvWString CvPropertyInfo::getChangeDisplayText() const
{
	return m_szChangeDisplayText;
}


CvWString CvPropertyInfo::getChangeAllCitiesDisplayText() const
{
	return m_szChangeAllCitiesDisplayText;
}


CvWString CvPropertyInfo::getPrereqMinDisplayText() const
{
	return m_szPrereqMinDisplayText;
}


CvWString CvPropertyInfo::getPrereqMaxDisplayText() const
{
	return m_szPrereqMaxDisplayText;
}


int CvPropertyInfo::getChangePropagator(const GameObjectTypes eFrom, const GameObjectTypes eTo) const
{
	FASSERT_BOUNDS(0, NUM_GAMEOBJECTS, eFrom);
	FASSERT_BOUNDS(0, NUM_GAMEOBJECTS, eTo);
	return m_aaiChangePropagator[eFrom][eTo];
}


const PropertyBuilding& CvPropertyInfo::getPropertyBuilding(int index) const
{
	FASSERT_BOUNDS(0, getNumPropertyBuildings(), index);
	return m_aPropertyBuildings[index];
}


int CvPropertyInfo::getNumPropertyBuildings() const
{
	return (int)m_aPropertyBuildings.size();
}


const PropertyPromotion& CvPropertyInfo::getPropertyPromotion(int index) const
{
	FASSERT_BOUNDS(0, getNumPropertyPromotions(), index);
	return m_aPropertyPromotions[index];
}


int CvPropertyInfo::getNumPropertyPromotions() const
{
	return (int)m_aPropertyPromotions.size();
}


int CvPropertyInfo::getNumTargetLevelbyEraTypes() const
{
	return m_aTargetLevelbyEraTypes.size();
}


int CvPropertyInfo::getTargetLevelbyEraType(int iIndex) const
{
	PROFILE_EXTRA_FUNC();
	for (EraArray::const_iterator it = m_aTargetLevelbyEraTypes.begin(); it != m_aTargetLevelbyEraTypes.end(); ++it)
	{
		if ((*it).first == (EraTypes)iIndex)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPropertyInfo::isTargetLevelbyEraType(int iIndex) const
{
	PROFILE_EXTRA_FUNC();
	for (EraArray::const_iterator it = m_aTargetLevelbyEraTypes.begin(); it != m_aTargetLevelbyEraTypes.end(); ++it)
	{
		if ((*it).first == (EraTypes)iIndex)
		{
			return true;
		}
	}
	return false;
}

