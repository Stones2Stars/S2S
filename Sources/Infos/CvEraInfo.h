#pragma once

#ifndef CV_ERA_INFO_H
#define CV_ERA_INFO_H

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvEraInfo
//
//  DESC:   A game era (pacing identity + world-scope cost/growth modifiers + one-shot
//          starting grants + era audio). #430: JSON-fed (Assets/Data/eras/*.json via
//          mapFrom); no XML read.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvEraInfo : public CvInfo
{
public:

	CvEraInfo();
	virtual ~CvEraInfo();

	int getStartingUnitMultiplier() const { return m_iStartingUnitMultiplier; }
	int getStartingDefenseUnits() const { return m_iStartingDefenseUnits; }
	int getStartingWorkerUnits() const { return m_iStartingWorkerUnits; }
	int getStartingExploreUnits() const { return m_iStartingExploreUnits; }
	int getAdvancedStartPoints() const { return m_iAdvancedStartPoints; }
	int getStartingGold() const { return m_iStartingGold; }
	int getFreePopulation() const { return m_iFreePopulation; }
	int getHistoricalStartYear() const { return m_iHistoricalStartYear; }
	int getHistoricalEndYear() const { return m_iHistoricalEndYear; }
	int getNormalSpeedTurns() const { return m_iNormalSpeedTurns; }
	int getGrowthPercent() const { return m_iGrowthPercent; }
	int getTrainPercent() const { return m_iTrainPercent; }
	int getConstructPercent() const { return m_iConstructPercent; }
	int getCreatePercent() const { return m_iCreatePercent; }
	int getResearchPercent() const { return m_iResearchPercent; }
	int getBuildPercent() const { return m_iBuildPercent; }
	int getImprovementPercent() const { return m_iImprovementPercent; }
	int getGreatPeoplePercent() const { return m_iGreatPeoplePercent; }
	int getAnarchyPercent() const { return m_iAnarchyPercent; }
	int getEventChancePerTurn() const { return m_iEventChancePerTurn; }
	int getSoundtrackSpace() const { return m_iSoundtrackSpace; }
	int getNumSoundtracks() const { return m_iNumSoundtracks; }
	int getCuttingEdgeCutsTechCostModifier() const { return m_iCuttingEdgeCutsTechCostModifier; }
	int getInitialCityMaintenancePercent() const { return m_iInitialCityMaintenancePercent; }

	const char* getAudioUnitVictoryScript() const { return m_szAudioUnitVictoryScript; }
	const char* getAudioUnitDefeatScript() const { return m_szAudioUnitDefeatScript; }

	bool isNoGoodies() const { return m_bNoGoodies; }
	bool isNoAnimals() const { return m_bNoAnimals; }
	bool isNoBarbUnits() const { return m_bNoBarbUnits; }
	bool isNoBarbCities() const { return m_bNoBarbCities; }
	bool isFirstSoundtrackFirst() const { return m_bFirstSoundtrackFirst; }

	// Arrays (bounds-checked; kept out-of-line)

	int getSoundtracks(int i) const;
	int getCitySoundscapeSciptId(int i) const;

	// The COMPOSED grants unit: `grants.<channel>: N` is a §5 numeric PULSE, so the base dispatch parses it here and
	// the scalars below are a view of it. Without this the section reached only those private scalars, and the grants
	// machine's game-start resolution read 0 for every era pulse and silently no-op'd.
	virtual const CvJsonGrants* getGrants() const { return &m_grants; }
	virtual CvJsonGrants*       mutGrants()       { return &m_grants; }

	virtual void mapFrom(const picojson::value& entity);

protected:
	CvJsonGrants m_grants;   // §5 -- the game-start starting-gold/units pulses


	int m_iStartingUnitMultiplier;
	int m_iStartingDefenseUnits;
	int m_iStartingWorkerUnits;
	int m_iStartingExploreUnits;
	int m_iAdvancedStartPoints;
	int m_iStartingGold;
	int m_iFreePopulation;
	// Calendar pacing: the era's real-history year span and how many game turns it lasts
	// at Normal (100%) speed. Other speeds scale the turn count by CvGameSpeedInfo::getSpeedPercent;
	// CvDate interpolates dates from the year span. Eras must be contiguous (start == previous era's end).
	int m_iHistoricalStartYear;
	int m_iHistoricalEndYear;
	int m_iNormalSpeedTurns;
	int m_iGrowthPercent;
	int m_iTrainPercent;
	int m_iConstructPercent;
	int m_iCreatePercent;
	int m_iResearchPercent;
	int m_iBuildPercent;
	int m_iImprovementPercent;
	int m_iGreatPeoplePercent;
	int m_iAnarchyPercent;
	int m_iEventChancePerTurn;
	int m_iSoundtrackSpace;
	int m_iNumSoundtracks;
	int m_iCuttingEdgeCutsTechCostModifier;
	int m_iInitialCityMaintenancePercent;

	CvString m_szAudioUnitVictoryScript;
	CvString m_szAudioUnitDefeatScript;

	bool m_bNoGoodies;
	bool m_bNoAnimals;
	bool m_bNoBarbUnits;
	bool m_bNoBarbCities;
	bool m_bFirstSoundtrackFirst;

	// Arrays

	int* m_paiSoundtracks;
	int* m_paiCitySoundscapeSciptIds;
};

#endif // CV_ERA_INFO_H
