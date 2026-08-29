//------------------------------------------------------------------------------------------------
//  FILE:    CvEventTriggerInfo.cpp
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
#include "CvEventTriggerInfo.h"


//////////////////////////////////////////////////////////////////////////
//
//	CvEventTriggerInfo
//	Event triggers
//
//
CvEventTriggerInfo::CvEventTriggerInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvEventTriggerInfo::~CvEventTriggerInfo()
{
}


int CvEventTriggerInfo::getPercentGamesActive() const
{
	return m_iPercentGamesActive;
}


int CvEventTriggerInfo::getProbability() const
{
	return m_iProbability;
}


int CvEventTriggerInfo::getUnitRequired(int i) const
{
	return m_aiUnitsRequired[i];
}


int CvEventTriggerInfo::getNumUnitsRequired() const
{
	return (int)m_aiUnitsRequired.size();
}


int CvEventTriggerInfo::getBuildingRequired(int i) const
{
	return m_aiBuildingsRequired[i];
}


int CvEventTriggerInfo::getNumBuildingsRequired() const
{
	return (int)m_aiBuildingsRequired.size();
}


int CvEventTriggerInfo::getNumUnits() const
{
	return m_iNumUnits;
}


int CvEventTriggerInfo::getNumBuildings() const
{
	return m_iNumBuildings;
}


int CvEventTriggerInfo::getNumUnitsGlobal() const
{
	return m_iNumUnitsGlobal;
}


int CvEventTriggerInfo::getNumBuildingsGlobal() const
{
	return m_iNumBuildingsGlobal;
}


int CvEventTriggerInfo::getNumPlotsRequired() const
{
	return m_iNumPlotsRequired;
}


int CvEventTriggerInfo::getPlotType() const
{
	return m_iPlotType;
}


int CvEventTriggerInfo::getNumReligions() const
{
	return m_iNumReligions;
}


int CvEventTriggerInfo::getNumCorporations() const
{
	return m_iNumCorporations;
}


int CvEventTriggerInfo::getOtherPlayerShareBorders() const
{
	return m_iOtherPlayerShareBorders;
}


int CvEventTriggerInfo::getOtherPlayerHasTech() const
{
	return m_iOtherPlayerHasTech;
}


int CvEventTriggerInfo::getCivic() const
{
	return m_iCivic;
}


int CvEventTriggerInfo::getMinPopulation() const
{
	return m_iMinPopulation;
}


int CvEventTriggerInfo::getMaxPopulation() const
{
	return m_iMaxPopulation;
}


int CvEventTriggerInfo::getMinMapLandmass() const
{
	return m_iMinMapLandmass;
}


int CvEventTriggerInfo::getMinOurLandmass() const
{
	return m_iMinOurLandmass;
}


int CvEventTriggerInfo::getMaxOurLandmass() const
{
	return m_iMaxOurLandmass;
}


int CvEventTriggerInfo::getMinDifficulty() const
{
	return m_iMinDifficulty;
}


int CvEventTriggerInfo::getAngry() const
{
	return m_iAngry;
}


int CvEventTriggerInfo::getUnhealthy() const
{
	return m_iUnhealthy;
}


int CvEventTriggerInfo::getUnitDamagedWeight() const
{
	return m_iUnitDamagedWeight;
}


int CvEventTriggerInfo::getUnitDistanceWeight() const
{
	return m_iUnitDistanceWeight;
}


int CvEventTriggerInfo::getUnitExperienceWeight() const
{
	return m_iUnitExperienceWeight;
}


int CvEventTriggerInfo::getMinTreasury() const
{
	return m_iMinTreasury;
}


int CvEventTriggerInfo::getEvent(int i) const
{
	return m_aiEvents[i];
}


int CvEventTriggerInfo::getNumEvents() const
{
	return (int)m_aiEvents.size();
}


int CvEventTriggerInfo::getPrereqEvent(int i) const
{
	return m_aiPrereqEvents[i];
}


int CvEventTriggerInfo::getNumPrereqEvents() const
{
	return (int)m_aiPrereqEvents.size();
}


int CvEventTriggerInfo::getPrereqOrTechs(int i) const
{
	return m_aiPrereqOrTechs[i];
}


int CvEventTriggerInfo::getNumPrereqOrTechs() const
{
	return (int)m_aiPrereqOrTechs.size();
}


int CvEventTriggerInfo::getPrereqAndTechs(int i) const
{
	return m_aiPrereqAndTechs[i];
}


int CvEventTriggerInfo::getNumPrereqAndTechs() const
{
	return (int)m_aiPrereqAndTechs.size();
}


int CvEventTriggerInfo::getObsoleteTech(int i) const
{
	return m_aiObsoleteTechs[i];
}


int CvEventTriggerInfo::getNumObsoleteTechs() const
{
	return (int)m_aiObsoleteTechs.size();
}


int CvEventTriggerInfo::getFeatureRequired(int i) const
{
	return m_aiFeaturesRequired[i];
}


int CvEventTriggerInfo::getNumFeaturesRequired() const
{
	return (int)m_aiFeaturesRequired.size();
}


int CvEventTriggerInfo::getTerrainRequired(int i) const
{
	return m_aiTerrainsRequired[i];
}


int CvEventTriggerInfo::getNumTerrainsRequired() const
{
	return (int)m_aiTerrainsRequired.size();
}


int CvEventTriggerInfo::getImprovementRequired(int i) const
{
	return m_aiImprovementsRequired[i];
}


int CvEventTriggerInfo::getNumImprovementsRequired() const
{
	return (int)m_aiImprovementsRequired.size();
}


int CvEventTriggerInfo::getBonusRequired(int i) const
{
	return m_aiBonusesRequired[i];
}


int CvEventTriggerInfo::getNumBonusesRequired() const
{
	return (int)m_aiBonusesRequired.size();
}


int CvEventTriggerInfo::getRouteRequired(int i) const
{
	return m_aiRoutesRequired[i];
}


int CvEventTriggerInfo::getNumRoutesRequired() const
{
	return (int)m_aiRoutesRequired.size();
}


int CvEventTriggerInfo::getReligionRequired(int i) const
{
	return m_aiReligionsRequired[i];
}


int CvEventTriggerInfo::getNumReligionsRequired() const
{
	return (int)m_aiReligionsRequired.size();
}


int CvEventTriggerInfo::getCorporationRequired(int i) const
{
	return m_aiCorporationsRequired[i];
}


int CvEventTriggerInfo::getNumCorporationsRequired() const
{
	return (int)m_aiCorporationsRequired.size();
}


// Begin EmperorFool: Events with Images
const char* CvEventTriggerInfo::getEventArt() const
{
	if (m_szEventArt.empty())
	{
		return NULL;
	}

	return m_szEventArt;
}

// End EmperorFool: Events with Images

bool CvEventTriggerInfo::isSinglePlayer() const
{
	return m_bSinglePlayer;
}


bool CvEventTriggerInfo::isTeam() const
{
	return m_bTeam;
}


const CvWString& CvEventTriggerInfo::getText(int i) const
{
	FASSERT_BOUNDS(0, (int)m_aszText.size(), i);
	return m_aszText[i];
}


int CvEventTriggerInfo::getTextEra(int i) const
{
	FASSERT_BOUNDS(0, (int)m_aiTextEra.size(), i);
	return m_aiTextEra[i];
}


int CvEventTriggerInfo::getNumTexts() const
{
	FAssert(m_aiTextEra.size() == m_aszText.size());
	return m_aszText.size();
}


const CvWString& CvEventTriggerInfo::getWorldNews(int i) const
{
	FASSERT_BOUNDS(0, getNumWorldNews(), i);
	return m_aszWorldNews[i];
}


int CvEventTriggerInfo::getNumWorldNews() const
{
	return m_aszWorldNews.size();
}


bool CvEventTriggerInfo::isRecurring() const
{
	return m_bRecurring;
}


bool CvEventTriggerInfo::isGlobal() const
{
	return m_bGlobal;
}


bool CvEventTriggerInfo::isPickPlayer() const
{
	return m_bPickPlayer;
}


bool CvEventTriggerInfo::isOtherPlayerWar() const
{
	return m_bOtherPlayerWar;
}


bool CvEventTriggerInfo::isOtherPlayerHasReligion() const
{
	return m_bOtherPlayerHasReligion;
}


bool CvEventTriggerInfo::isOtherPlayerHasOtherReligion() const
{
	return m_bOtherPlayerHasOtherReligion;
}


bool CvEventTriggerInfo::isOtherPlayerAI() const
{
	return m_bOtherPlayerAI;
}


bool CvEventTriggerInfo::isPickCity() const
{
	return m_bPickCity;
}


bool CvEventTriggerInfo::isPickOtherPlayerCity() const
{
	return m_bPickOtherPlayerCity;
}


bool CvEventTriggerInfo::isShowPlot() const
{
	return m_bShowPlot;
}


int CvEventTriggerInfo::getCityFoodWeight() const
{
	return m_iCityFoodWeight;
}


bool CvEventTriggerInfo::isUnitsOnPlot() const
{
	return m_bUnitsOnPlot;
}


bool CvEventTriggerInfo::isOwnPlot() const
{
	return m_bOwnPlot;
}


bool CvEventTriggerInfo::isPickReligion() const
{
	return m_bPickReligion;
}


bool CvEventTriggerInfo::isStateReligion() const
{
	return m_bStateReligion;
}


bool CvEventTriggerInfo::isHolyCity() const
{
	return m_bHolyCity;
}


bool CvEventTriggerInfo::isPickCorporation() const
{
	return m_bPickCorporation;
}


bool CvEventTriggerInfo::isHeadquarters() const
{
	return m_bHeadquarters;
}


bool CvEventTriggerInfo::isProbabilityUnitMultiply() const
{
	return m_bProbabilityUnitMultiply;
}


bool CvEventTriggerInfo::isProbabilityBuildingMultiply() const
{
	return m_bProbabilityBuildingMultiply;
}


bool CvEventTriggerInfo::isPrereqEventCity() const
{
	return m_bPrereqEventCity;
}


const CvProperties* CvEventTriggerInfo::getPrereqMinProperties() const
{
	return &m_PrereqMinProperties;
}


const CvProperties* CvEventTriggerInfo::getPrereqMaxProperties() const
{
	return &m_PrereqMaxProperties;
}


const CvProperties* CvEventTriggerInfo::getPrereqPlayerMinProperties() const
{
	return &m_PrereqPlayerMinProperties;
}


const CvProperties* CvEventTriggerInfo::getPrereqPlayerMaxProperties() const
{
	return &m_PrereqPlayerMaxProperties;
}


const char* CvEventTriggerInfo::getPythonCallback() const
{
	return m_szPythonCallback;
}


const char* CvEventTriggerInfo::getPythonCanDo() const
{
	return m_szPythonCanDo;
}


const char* CvEventTriggerInfo::getPythonCanDoCity() const
{
	return m_szPythonCanDoCity;
}


const char* CvEventTriggerInfo::getPythonCanDoUnit() const
{
	return m_szPythonCanDoUnit;
}


// bool vector without delayed resolution
int CvEventTriggerInfo::getNotOnGameOption(int i) const
{
	return m_aiNotOnGameOptions[i];
}


int CvEventTriggerInfo::getNumNotOnGameOptions() const
{
	return (int)m_aiNotOnGameOptions.size();
}


bool CvEventTriggerInfo::isNotOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiNotOnGameOptions, i);
}


int CvEventTriggerInfo::getOnGameOption(int i) const
{
	return m_aiOnGameOptions[i];
}


int CvEventTriggerInfo::getNumOnGameOptions() const
{
	return (int)m_aiOnGameOptions.size();
}


bool CvEventTriggerInfo::isOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiOnGameOptions, i);
}


void CvEventTriggerInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order for readability; the checksum itself stays
	// hand-written (see getCheckSum below), so declaration order carries no checksum weight here.
	util
		.add(m_iPercentGamesActive, L"iPercentGamesActive")
		.add(m_iProbability, L"iWeight")
		.add(m_iNumUnits, L"iNumUnits")
		.add(m_iNumBuildings, L"iNumBuildings")
		.add(m_iNumUnitsGlobal, L"iNumUnitsGlobal")
		.add(m_iNumBuildingsGlobal, L"iNumBuildingsGlobal")
		.add(m_iNumPlotsRequired, L"iNumPlotsRequired")
		.add(m_iPlotType, L"iPlotType", NO_PLOT)
		.add(m_iNumReligions, L"iNumReligions")
		.add(m_iNumCorporations, L"iNumCorporations")
		.add(m_iOtherPlayerShareBorders, L"iOtherPlayerShareBorders")
		.addEnumAsInt(m_iOtherPlayerHasTech, L"OtherPlayerHasTech")
		.addEnumAsInt(m_iCivic, L"Civic")
		.add(m_iMinPopulation, L"iMinPopulation")
		.add(m_iMaxPopulation, L"iMaxPopulation")
		.add(m_iMinMapLandmass, L"iMinMapLandmass")
		.add(m_iMinOurLandmass, L"iMinOurLandmass")
		.add(m_iMaxOurLandmass, L"iMaxOurLandmass", -1)
		.addEnumAsInt(m_iMinDifficulty, L"MinDifficulty")
		.add(m_iAngry, L"iAngry")
		.add(m_iUnhealthy, L"iUnhealthy")
		.add(m_iUnitDamagedWeight, L"iUnitDamagedWeight")
		.add(m_iUnitDistanceWeight, L"iUnitDistanceWeight")
		.add(m_iUnitExperienceWeight, L"iUnitExperienceWeight")
		.add(m_iMinTreasury, L"iMinTreasury")
		.add(m_aiUnitsRequired, L"UnitsRequired")
		.add(m_aiBuildingsRequired, L"BuildingsRequired")
		.add(m_aiPrereqOrTechs, L"OrPreReqs")
		.add(m_aiPrereqAndTechs, L"AndPreReqs")
		.add(m_aiObsoleteTechs, L"ObsoleteTechs")
		.add(m_aiEvents, L"Events")
		.add(m_aiPrereqEvents, L"PrereqEvents")
		.add(m_aiFeaturesRequired, L"FeaturesRequired")
		.add(m_aiTerrainsRequired, L"TerrainsRequired")
		.add(m_aiImprovementsRequired, L"ImprovementsRequired")
		.add(m_aiBonusesRequired, L"BonusesRequired")
		.add(m_aiRoutesRequired, L"RoutesRequired")
		.add(m_aiReligionsRequired, L"ReligionsRequired")
		.add(m_aiCorporationsRequired, L"CorporationsRequired")
		.add(m_bSinglePlayer, L"bSinglePlayer")
		.add(m_bTeam, L"bTeam")
		.add(m_bRecurring, L"bRecurring")
		.add(m_bGlobal, L"bGlobal")
		.add(m_bPickPlayer, L"bPickPlayer")
		.add(m_bOtherPlayerWar, L"bOtherPlayerWar")
		.add(m_bOtherPlayerHasReligion, L"bOtherPlayerHasReligion")
		.add(m_bOtherPlayerHasOtherReligion, L"bOtherPlayerHasOtherReligion")
		.add(m_bOtherPlayerAI, L"bOtherPlayerAI")
		.add(m_bPickCity, L"bPickCity")
		.add(m_bPickOtherPlayerCity, L"bPickOtherPlayerCity")
		.add(m_bShowPlot, L"bShowPlot", true)
		.add(m_iCityFoodWeight, L"iCityFoodWeight")
		.add(m_bUnitsOnPlot, L"bUnitsOnPlot")
		.add(m_bOwnPlot, L"bOwnPlot")
		.add(m_bPickReligion, L"bPickReligion")
		.add(m_bStateReligion, L"bStateReligion")
		.add(m_bHolyCity, L"bHolyCity")
		.add(m_bPickCorporation, L"bPickCorporation")
		.add(m_bHeadquarters, L"bHeadquarters")
		.add(m_bProbabilityUnitMultiply, L"bProbabilityUnitMultiply")
		.add(m_bProbabilityBuildingMultiply, L"bProbabilityBuildingMultiply")
		.add(m_bPrereqEventCity, L"bPrereqEventPlot")
		.add(m_aiNotOnGameOptions, L"NotOnGameOptions")
		.add(m_aiOnGameOptions, L"OnGameOptions")
		.add(m_szEventArt, L"EventArt")
		.add(m_szPythonCallback, L"PythonCallback")
		.add(m_szPythonCanDo, L"PythonCanDo")
		.add(m_szPythonCanDoCity, L"PythonCanDoCity")
		.add(m_szPythonCanDoUnit, L"PythonCanDoUnit")
	;
}


// Kept fully hand-written (NOT delegated to CvInfoUtil) to preserve the legacy asset checksum
// byte-for-byte:
//  - the CvProperties blocks sit mid-order between declarative fields;
//  - the python callback CvStrings ARE folded in here via CheckSumC, which the declarative
//    StringWrapper cannot reproduce (its checkSum is a no-op).
void CvEventTriggerInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_iPercentGamesActive);
	CheckSum(iSum, m_iProbability);
	CheckSum(iSum, m_iNumUnits);
	CheckSum(iSum, m_iNumBuildings);
	CheckSum(iSum, m_iNumUnitsGlobal);
	CheckSum(iSum, m_iNumBuildingsGlobal);
	CheckSum(iSum, m_iNumPlotsRequired);
	CheckSum(iSum, m_iPlotType);
	CheckSum(iSum, m_iNumReligions);
	CheckSum(iSum, m_iNumCorporations);
	CheckSum(iSum, m_iOtherPlayerShareBorders);
	CheckSum(iSum, m_iOtherPlayerHasTech);
	CheckSum(iSum, m_iCivic);
	CheckSum(iSum, m_iMinPopulation);
	CheckSum(iSum, m_iMaxPopulation);
	CheckSum(iSum, m_iMinMapLandmass);
	CheckSum(iSum, m_iMinOurLandmass);
	CheckSum(iSum, m_iMaxOurLandmass);
	CheckSum(iSum, m_iMinDifficulty);
	CheckSum(iSum, m_iAngry);
	CheckSum(iSum, m_iUnhealthy);
	CheckSum(iSum, m_iUnitDamagedWeight);
	CheckSum(iSum, m_iUnitDistanceWeight);
	CheckSum(iSum, m_iUnitExperienceWeight);
	CheckSum(iSum, m_iMinTreasury);

	CheckSumC(iSum, m_aiUnitsRequired);
	CheckSumC(iSum, m_aiBuildingsRequired);
	CheckSumC(iSum, m_aiPrereqOrTechs);
	CheckSumC(iSum, m_aiPrereqAndTechs);
	CheckSumC(iSum, m_aiObsoleteTechs);
	CheckSumC(iSum, m_aiEvents);
	CheckSumC(iSum, m_aiPrereqEvents);
	CheckSumC(iSum, m_aiFeaturesRequired);
	CheckSumC(iSum, m_aiTerrainsRequired);
	CheckSumC(iSum, m_aiImprovementsRequired);
	CheckSumC(iSum, m_aiBonusesRequired);
	CheckSumC(iSum, m_aiRoutesRequired);
	CheckSumC(iSum, m_aiReligionsRequired);
	CheckSumC(iSum, m_aiCorporationsRequired);

	CheckSum(iSum, m_bSinglePlayer);
	CheckSum(iSum, m_bTeam);
	CheckSum(iSum, m_bRecurring);
	CheckSum(iSum, m_bGlobal);
	CheckSum(iSum, m_bPickPlayer);
	CheckSum(iSum, m_bOtherPlayerWar);
	CheckSum(iSum, m_bOtherPlayerHasReligion);
	CheckSum(iSum, m_bOtherPlayerHasOtherReligion);
	CheckSum(iSum, m_bOtherPlayerAI);
	CheckSum(iSum, m_bPickCity);
	CheckSum(iSum, m_bPickOtherPlayerCity);
	CheckSum(iSum, m_bShowPlot);
	CheckSum(iSum, m_iCityFoodWeight);
	CheckSum(iSum, m_bUnitsOnPlot);
	CheckSum(iSum, m_bOwnPlot);
	CheckSum(iSum, m_bPickReligion);
	CheckSum(iSum, m_bStateReligion);
	CheckSum(iSum, m_bHolyCity);
	CheckSum(iSum, m_bPickCorporation);
	CheckSum(iSum, m_bHeadquarters);
	CheckSum(iSum, m_bProbabilityUnitMultiply);
	CheckSum(iSum, m_bProbabilityBuildingMultiply);
	CheckSum(iSum, m_bPrereqEventCity);

	m_PrereqMinProperties.getCheckSum(iSum);
	m_PrereqMaxProperties.getCheckSum(iSum);
	m_PrereqPlayerMinProperties.getCheckSum(iSum);
	m_PrereqPlayerMaxProperties.getCheckSum(iSum);

	CheckSumC(iSum, m_szPythonCallback);
	CheckSumC(iSum, m_szPythonCanDo);
	CheckSumC(iSum, m_szPythonCanDoCity);
	CheckSumC(iSum, m_szPythonCanDoUnit);

	CheckSumC(iSum, m_aiNotOnGameOptions);
	CheckSumC(iSum, m_aiOnGameOptions);
}


bool CvEventTriggerInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;

	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	m_aszText.clear();
	m_aiTextEra.clear();
	if (pXML->TryMoveToXmlFirstChild(L"TriggerTexts"))
	{
		int iNumSibs = pXML->GetXmlChildrenNumber();

		if (0 < iNumSibs)
		{
			if (pXML->TryMoveToXmlFirstChild())
			{
				for (int j = 0; j < iNumSibs; ++j)
				{
					if (pXML->GetChildXmlVal(szTextVal))
					{
						m_aszText.push_back(szTextVal);
						pXML->GetNextXmlVal(szTextVal);
						m_aiTextEra.push_back(pXML->GetInfoClass(szTextVal));

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

	m_aszWorldNews.clear();
	if (pXML->TryMoveToXmlFirstChild(L"WorldNewsTexts"))
	{
		int iNumSibs = pXML->GetXmlChildrenNumber();

		if (0 < iNumSibs)
		{
			if (pXML->GetChildXmlVal(szTextVal))
			{
				for (int j=0; j<iNumSibs; ++j)
				{
					m_aszWorldNews.push_back(szTextVal);
					if (!pXML->GetNextXmlVal(szTextVal))
					{
						break;
					}
				}

				pXML->MoveToXmlParent();
			}
		}

		pXML->MoveToXmlParent();
	}

	m_PrereqMinProperties.read(pXML, L"PrereqMinProperties");
	m_PrereqMaxProperties.read(pXML, L"PrereqMaxProperties");
	m_PrereqPlayerMinProperties.read(pXML, L"PrereqPlayerMinProperties");
	m_PrereqPlayerMaxProperties.read(pXML, L"PrereqPlayerMaxProperties");

	return true;
}


void CvEventTriggerInfo::copyNonDefaults(const CvEventTriggerInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	// (text, era) are PARALLEL BY INDEX -- getNumTexts asserts the two sizes match, and the consumer
	// reads getText(i) against getTextEra(i) to pick era-appropriate event text. So they merge as PAIRS:
	// the generic CopyNonDefaultsFromVector dedups and sorts each vector independently, which reorders
	// one list against the other and can leave the two different lengths.
	for (size_t i = 0; i < pClassInfo->m_aszText.size(); ++i)
	{
		if (algo::none_of_equal(m_aszText, pClassInfo->m_aszText[i]))
		{
			m_aszText.push_back(pClassInfo->m_aszText[i]);
			m_aiTextEra.push_back(i < pClassInfo->m_aiTextEra.size() ? pClassInfo->m_aiTextEra[i] : NO_ERA);
		}
	}

	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aszWorldNews, pClassInfo->m_aszWorldNews);

	m_PrereqMinProperties.copyNonDefaults(pClassInfo->getPrereqMinProperties());
	m_PrereqMaxProperties.copyNonDefaults(pClassInfo->getPrereqMaxProperties());
	m_PrereqPlayerMinProperties.copyNonDefaults(pClassInfo->getPrereqPlayerMinProperties());
	m_PrereqPlayerMaxProperties.copyNonDefaults(pClassInfo->getPrereqPlayerMaxProperties());
}

