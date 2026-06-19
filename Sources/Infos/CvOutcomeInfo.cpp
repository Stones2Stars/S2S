//------------------------------------------------------------------------------------------------
//  FILE:    CvOutcomeInfo.cpp
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
#include "CvOutcomeInfo.h"



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvOutcomeInfo
//
//  DESC:   Contains info about outcome types which can be the result of a kill or of actions
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CvOutcomeInfo::CvOutcomeInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvOutcomeInfo::~CvOutcomeInfo()
{
	GC.removeDelayedResolutionVector(m_aeReplaceOutcomes);
}


void CvOutcomeInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. Hand-written fields (see read()):
	// - m_szMessageText: CvWString, no CvInfoUtil wrapper exists (StringWrapper is CvString-only).
	// - m_aeiExtraChancePromotions: bespoke std::vector<std::pair> walk, no wrapper shape fits.
	// - m_aeReplaceOutcomes: self-referential FK list (SetOptionalVectorWithDelayedResolution).
	// getCheckSum stays explicit because those fields sit mid-order in the legacy checksum.
	util
		.addEnum(m_ePrereqTech, L"PrereqTech")
		.addEnum(m_eObsoleteTech, L"ObsoleteTech")
		.add(m_aePrereqBuildings, L"PrereqBuildings")
		.add(m_bToCoastalCity, L"bToCoastalCity")
		.add(m_bFriendlyTerritory, L"bFriendlyTerritory", true)
		.add(m_bNeutralTerritory, L"bNeutralTerritory", true)
		.add(m_bHostileTerritory, L"bHostileTerritory", true)
		.add(m_bBarbarianTerritory, L"bBarbarianTerritory")
		.add(m_bCity, L"bCity")
		.add(m_bNotCity, L"bNotCity")
		.add(m_bCapture, L"bCapture")
		.addEnum(m_ePrereqCivic, L"PrereqCivic")
	;
}


bool CvOutcomeInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->GetOptionalChildXmlValByName(m_szMessageText, L"Message");

	if(pXML->TryMoveToXmlFirstChild(L"ExtraChancePromotions"))
	{
		if(pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"ExtraChancePromotion"))
			{
				CvString szTextVal;
				do
				{
					int iExtraChance;
					pXML->GetChildXmlValByName(szTextVal, L"PromotionType");
					PromotionTypes ePromotion = (PromotionTypes) pXML->GetInfoClass(szTextVal);
					pXML->GetChildXmlValByName(&iExtraChance, L"iExtraChance");
					m_aeiExtraChancePromotions.push_back(std::pair<PromotionTypes,int>(ePromotion, iExtraChance));
				} while(pXML->TryMoveToXmlNextSibling());
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	pXML->SetOptionalVectorWithDelayedResolution(m_aeReplaceOutcomes, L"ReplaceOutcomes");

	return true;
}


void CvOutcomeInfo::copyNonDefaults(const CvOutcomeInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if (getMessageText().empty()) m_szMessageText = pClassInfo->getMessageText();
	if (getNumExtraChancePromotions() == 0) m_aeiExtraChancePromotions = pClassInfo->m_aeiExtraChancePromotions;

	GC.copyNonDefaultDelayedResolutionVector(m_aeReplaceOutcomes, pClassInfo->getReplaceOutcomes());
}


void CvOutcomeInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_ePrereqTech);
	CheckSum(iSum, m_eObsoleteTech);
	CheckSumC(iSum, m_aeiExtraChancePromotions);
	CheckSumC(iSum, m_aePrereqBuildings);
	CheckSum(iSum, m_bToCoastalCity);
	CheckSum(iSum, m_bFriendlyTerritory);
	CheckSum(iSum, m_bNeutralTerritory);
	CheckSum(iSum, m_bHostileTerritory);
	CheckSum(iSum, m_bBarbarianTerritory);
	CheckSum(iSum, m_bCity);
	CheckSum(iSum, m_bNotCity);
	CheckSum(iSum, m_bCapture);
	CheckSumC(iSum, m_aeReplaceOutcomes);
	CheckSum(iSum, m_ePrereqCivic);
}


CvWString CvOutcomeInfo::getMessageText() const
{
	return m_szMessageText;
}


bool CvOutcomeInfo::getToCoastalCity() const
{
	return m_bToCoastalCity;
}


bool CvOutcomeInfo::getFriendlyTerritory() const
{
	return m_bFriendlyTerritory;
}


bool CvOutcomeInfo::getNeutralTerritory() const
{
	return m_bNeutralTerritory;
}


bool CvOutcomeInfo::getHostileTerritory() const
{
	return m_bHostileTerritory;
}


bool CvOutcomeInfo::getBarbarianTerritory() const
{
	return m_bBarbarianTerritory;
}


bool CvOutcomeInfo::getCity() const
{
	return m_bCity;
}


bool CvOutcomeInfo::getNotCity() const
{
	return m_bNotCity;
}


bool CvOutcomeInfo::isCapture() const
{
	return m_bCapture;
}


TechTypes CvOutcomeInfo::getObsoleteTech() const
{
	return m_eObsoleteTech;
}


CivicTypes CvOutcomeInfo::getPrereqCivic() const
{
	return m_ePrereqCivic;
}


int CvOutcomeInfo::getNumExtraChancePromotions() const
{
	return m_aeiExtraChancePromotions.size();
}


PromotionTypes CvOutcomeInfo::getExtraChancePromotion(int i) const
{
	FASSERT_BOUNDS(0, getNumExtraChancePromotions(), i);
	return m_aeiExtraChancePromotions[i].first;
}


int CvOutcomeInfo::getExtraChancePromotionChance(int i) const
{
	FASSERT_BOUNDS(0, getNumExtraChancePromotions(), i);
	return m_aeiExtraChancePromotions[i].second;
}

