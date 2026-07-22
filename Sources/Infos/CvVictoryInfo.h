#pragma once

#ifndef CV_VICTORY_INFO_H
#define CV_VICTORY_INFO_H

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvVictoryInfo
//
//  DESC:   A victory condition. #430: JSON-fed (Assets/Data/victories/*.json via
//          mapFrom); no XML read.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvVictoryInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvVictoryInfo();

	int getPopulationPercentLead() const { return m_iPopulationPercentLead; }
	int getLandPercent() const { return m_iLandPercent; }
	int getMinLandPercent() const { return m_iMinLandPercent; }
	int getReligionPercent() const { return m_iReligionPercent; }
	int getCityCulture() const { return m_iCityCulture; }
	int getNumCultureCities() const { return m_iNumCultureCities; }
	int getTotalCultureRatio() const { return m_iTotalCultureRatio; }
	int getVictoryDelayTurns() const { return m_iVictoryDelayTurns; }

	bool isTotalVictory() const { return m_bTotalVictory; }
	bool isTargetScore() const { return m_bTargetScore; }
	bool isEndScore() const { return m_bEndScore; }
	bool isConquest() const { return m_bConquest; }
	bool isDiploVote() const { return m_bDiploVote; }
	DllExport bool isPermanent() const;   // EXE-bound (DllExport) + Python-bound -- kept out-of-line

	const char* getMovie() const { return m_szMovie; }

	virtual void mapFrom(const picojson::value& entity);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	int m_iPopulationPercentLead;
	int m_iLandPercent;
	int m_iMinLandPercent;
	int m_iReligionPercent;
	int m_iCityCulture;
	int m_iNumCultureCities;
	int m_iTotalCultureRatio;
	int m_iVictoryDelayTurns;

	bool m_bTargetScore;
	bool m_bEndScore;
	bool m_bConquest;
	bool m_bDiploVote;
	bool m_bPermanent;
	bool m_bTotalVictory;

	CvString m_szMovie;
};

#endif // CV_VICTORY_INFO_H
