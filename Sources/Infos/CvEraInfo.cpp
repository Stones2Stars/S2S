//------------------------------------------------------------------------------------------------
//  FILE:    CvEraInfo.cpp
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
#include "CvEraInfo.h"


//------------------------------------------------------------------------------------------------------
//
//  CvEraInfo
//

CvEraInfo::CvEraInfo() :
m_iNumSoundtracks(0),
m_paiCitySoundscapeSciptIds(NULL),
m_paiSoundtracks(NULL)
{
	CvInfoUtil(this).initDataMembers();
}


// Every scalar XML field is declared here (#196); read/copy derive from it.
// Still hand-written: the soundtrack/city-soundscape arrays (resolved through the runtime
// audio-tag table, gDLL->getAudioTagIndex / SetVariableListTagPairForAudioScripts), and
// getCheckSum (explicit — the legacy checksum omits several read fields; see below).
void CvEraInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iHistoricalStartYear, L"iHistoricalStartYear")
		.add(m_iHistoricalEndYear, L"iHistoricalEndYear")
		.add(m_iNormalSpeedTurns, L"iNormalSpeedTurns")
		.add(m_bNoGoodies, L"bNoGoodies")
		.add(m_bNoAnimals, L"bNoAnimals")
		.add(m_bNoBarbUnits, L"bNoBarbUnits")
		.add(m_bNoBarbCities, L"bNoBarbCities")
		.add(m_iStartingUnitMultiplier, L"iStartingUnitMultiplier")
		.add(m_iStartingDefenseUnits, L"iStartingDefenseUnits")
		.add(m_iStartingWorkerUnits, L"iStartingWorkerUnits")
		.add(m_iStartingExploreUnits, L"iStartingExploreUnits")
		.add(m_iAdvancedStartPoints, L"iAdvancedStartPoints")
		.add(m_iStartingGold, L"iStartingGold")
		.add(m_iFreePopulation, L"iFreePopulation")
		.add(m_iGrowthPercent, L"iGrowthPercent")
		.add(m_iTrainPercent, L"iTrainPercent")
		.add(m_iConstructPercent, L"iConstructPercent")
		.add(m_iCreatePercent, L"iCreatePercent")
		.add(m_iResearchPercent, L"iResearchPercent")
		.add(m_iBuildPercent, L"iBuildPercent")
		.add(m_iImprovementPercent, L"iImprovementPercent")
		.add(m_iGreatPeoplePercent, L"iGreatPeoplePercent")
		.add(m_iAnarchyPercent, L"iAnarchyPercent")
		.add(m_iEventChancePerTurn, L"iEventChancePerTurn")
		.add(m_iSoundtrackSpace, L"iSoundtrackSpace")
		.add(m_bFirstSoundtrackFirst, L"bFirstSoundtrackFirst")
		.add(m_iInitialCityMaintenancePercent, L"iInitialCityMaintenancePercent")
		.add(m_iCuttingEdgeCutsTechCostModifier, L"iCuttingEdgeCutsTechCostModifier")
		.add(m_szAudioUnitVictoryScript, L"AudioUnitVictoryScript")
		.add(m_szAudioUnitDefeatScript, L"AudioUnitDefeatScript")
	;
}


CvEraInfo::~CvEraInfo()
{
	SAFE_DELETE_ARRAY(m_paiCitySoundscapeSciptIds);
	SAFE_DELETE_ARRAY(m_paiSoundtracks);
}


int CvEraInfo::getStartingUnitMultiplier() const
{
	return m_iStartingUnitMultiplier;
}


int CvEraInfo::getStartingDefenseUnits() const
{
	return m_iStartingDefenseUnits;
}


int CvEraInfo::getStartingWorkerUnits() const
{
	return m_iStartingWorkerUnits;
}


int CvEraInfo::getStartingExploreUnits() const
{
	return m_iStartingExploreUnits;
}


int CvEraInfo::getAdvancedStartPoints() const
{
	return m_iAdvancedStartPoints;
}


int CvEraInfo::getStartingGold() const
{
	return m_iStartingGold;
}


int CvEraInfo::getFreePopulation() const
{
	return m_iFreePopulation;
}


int CvEraInfo::getHistoricalStartYear() const
{
	return m_iHistoricalStartYear;
}


int CvEraInfo::getHistoricalEndYear() const
{
	return m_iHistoricalEndYear;
}


int CvEraInfo::getNormalSpeedTurns() const
{
	return m_iNormalSpeedTurns;
}


int CvEraInfo::getGrowthPercent() const
{
	return m_iGrowthPercent;
}


int CvEraInfo::getTrainPercent() const
{
	return m_iTrainPercent;
}


int CvEraInfo::getConstructPercent() const
{
	return m_iConstructPercent;
}


int CvEraInfo::getCreatePercent() const
{
	return m_iCreatePercent;
}


int CvEraInfo::getResearchPercent() const
{
	return m_iResearchPercent;
}


int CvEraInfo::getBuildPercent() const
{
	return m_iBuildPercent;
}


int CvEraInfo::getImprovementPercent() const
{
	return m_iImprovementPercent;
}


int CvEraInfo::getGreatPeoplePercent() const
{
	return m_iGreatPeoplePercent;
}


int CvEraInfo::getAnarchyPercent() const
{
	return m_iAnarchyPercent;
}


int CvEraInfo::getEventChancePerTurn() const
{
	return m_iEventChancePerTurn;
}


int CvEraInfo::getSoundtrackSpace() const
{
	return m_iSoundtrackSpace;
}


bool CvEraInfo::isFirstSoundtrackFirst() const
{
	return m_bFirstSoundtrackFirst;
}


int CvEraInfo::getNumSoundtracks() const
{
	return m_iNumSoundtracks;
}


int CvEraInfo::getCuttingEdgeCutsTechCostModifier() const
{
	return m_iCuttingEdgeCutsTechCostModifier;
}


const char* CvEraInfo::getAudioUnitVictoryScript() const
{
	return m_szAudioUnitVictoryScript;
}


const char* CvEraInfo::getAudioUnitDefeatScript() const
{
	return m_szAudioUnitDefeatScript;
}


bool CvEraInfo::isNoGoodies() const
{
	return m_bNoGoodies;
}


bool CvEraInfo::isNoAnimals() const
{
	return m_bNoAnimals;
}


bool CvEraInfo::isNoBarbUnits() const
{
	return m_bNoBarbUnits;
}


bool CvEraInfo::isNoBarbCities() const
{
	return m_bNoBarbCities;
}


int CvEraInfo::getInitialCityMaintenancePercent() const
{
	return m_iInitialCityMaintenancePercent;
}



// Arrays

int CvEraInfo::getSoundtracks(int i) const
{
	FASSERT_BOUNDS(0, getNumSoundtracks(), i);
	return m_paiSoundtracks ? m_paiSoundtracks[i] : -1;
}


int CvEraInfo::getCitySoundscapeSciptId(int i) const
{
//	FAssertMsg(i < ?, "Index out of bounds");
	FAssertMsg(i > -1, "Index out of bounds");
	return m_paiCitySoundscapeSciptIds ? m_paiCitySoundscapeSciptIds[i] : -1;
}


bool CvEraInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	if (m_iInitialCityMaintenancePercent < 0)
	{
		m_iInitialCityMaintenancePercent = 0;
	}

	if (pXML->TryMoveToXmlFirstChild(L"EraInfoSoundtracks"))
	{
		CvString* pszSoundTrackNames = NULL;
		pXML->SetStringList(&pszSoundTrackNames, &m_iNumSoundtracks);

		if (m_iNumSoundtracks > 0)
		{
			m_paiSoundtracks = new int[m_iNumSoundtracks];

			int j;
			for (j=0;j<m_iNumSoundtracks;j++)
			{
				m_paiSoundtracks[j] = ((!gDLL->getAudioDisabled()) ? gDLL->getAudioTagIndex(pszSoundTrackNames[j], AUDIOTAG_2DSCRIPT) : -1);
			}
		}
		else
		{
			m_paiSoundtracks = NULL;
		}

		pXML->MoveToXmlParent();

		SAFE_DELETE_ARRAY(pszSoundTrackNames);
	}

	pXML->SetVariableListTagPairForAudioScripts(&m_paiCitySoundscapeSciptIds, L"CitySoundscapes", GC.getNumCitySizeTypes());

	return true;
}


void CvEraInfo::copyNonDefaults(const CvEraInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const int iAudioDefault = -1;  //all audio is default -1

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if (m_iInitialCityMaintenancePercent < 0)
	{
		m_iInitialCityMaintenancePercent = 0;
	}

	if ( pClassInfo->getNumSoundtracks() != 0 )
	{
		int iNumSoundTracks = getNumSoundtracks() + pClassInfo->getNumSoundtracks();
		int* m_paiSoundtracksTemp = new int[iNumSoundTracks];
		for ( int i = 0; i < iNumSoundTracks; i++)
		{
			if ( i < getNumSoundtracks() )
			{
				m_paiSoundtracksTemp[i] = getSoundtracks(i);
			}
			else
			{
				m_paiSoundtracksTemp[i] = pClassInfo->getSoundtracks(i - getNumSoundtracks());
			}
		}
		SAFE_DELETE_ARRAY(m_paiSoundtracks);
		m_paiSoundtracks = new int[iNumSoundTracks];
		for ( int i = 0; i < iNumSoundTracks; i++)
		{
			m_paiSoundtracks[i] = m_paiSoundtracksTemp[i];
		}
		SAFE_DELETE_ARRAY(m_paiSoundtracksTemp);
	}

	for ( int i = 0; i < GC.getNumCitySizeTypes(); i++)
	{
		if ( getCitySoundscapeSciptId(i) == iAudioDefault && pClassInfo->getCitySoundscapeSciptId(i) != iAudioDefault)
		{
			if ( NULL == m_paiCitySoundscapeSciptIds )
			{
				CvXMLLoadUtility::InitList(&m_paiCitySoundscapeSciptIds,GC.getNumCitySizeTypes(),iAudioDefault);
			}
			m_paiCitySoundscapeSciptIds[i] = pClassInfo->getCitySoundscapeSciptId(i);
		}
	}
}


// Explicit, NOT delegated to CvInfoUtil (#196): the legacy checksum omits several read fields
// (m_iSoundtrackSpace, m_bFirstSoundtrackFirst, m_iInitialCityMaintenancePercent); delegating
// would fold them in and change the savegame asset checksum. The order below reproduces the
// legacy composition exactly: hand-written ints, bools, then the three calendar-pacing fields
// that the old delegated CvInfoUtil tail appended.
void CvEraInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_iStartingUnitMultiplier);
	CheckSum(iSum, m_iStartingDefenseUnits);
	CheckSum(iSum, m_iStartingWorkerUnits);
	CheckSum(iSum, m_iStartingExploreUnits);
	CheckSum(iSum, m_iAdvancedStartPoints);
	CheckSum(iSum, m_iStartingGold);
	CheckSum(iSum, m_iFreePopulation);
	CheckSum(iSum, m_iGrowthPercent);
	CheckSum(iSum, m_iTrainPercent);
	CheckSum(iSum, m_iConstructPercent);
	CheckSum(iSum, m_iCreatePercent);
	CheckSum(iSum, m_iResearchPercent);
	CheckSum(iSum, m_iBuildPercent);
	CheckSum(iSum, m_iImprovementPercent);
	CheckSum(iSum, m_iGreatPeoplePercent);
	CheckSum(iSum, m_iAnarchyPercent);
	CheckSum(iSum, m_iEventChancePerTurn);
	CheckSum(iSum, m_iCuttingEdgeCutsTechCostModifier);

	CheckSum(iSum, m_bNoGoodies);
	CheckSum(iSum, m_bNoAnimals);
	CheckSum(iSum, m_bNoBarbUnits);
	CheckSum(iSum, m_bNoBarbCities);

	CheckSum(iSum, m_iHistoricalStartYear);
	CheckSum(iSum, m_iHistoricalEndYear);
	CheckSum(iSum, m_iNormalSpeedTurns);
}

