#pragma once

#ifndef CV_GAME_SPEED_INFO_H
#define CV_GAME_SPEED_INFO_H

#include "CvInfo.h"   // the JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvGameSpeedInfo
//
//  DESC:   A game speed scales costs/durations by iSpeedPercent and stretches the
//          game over proportionally more turns. Turn counts and calendar pacing
//          are derived per era from CvEraInfo's historical year span and
//          Normal-speed turn count (see CvDate) -- nothing calendar-related is
//          stored here.
//
//  #430: JSON-fed (Assets/Data/gamespeeds/*.json via mapFrom); no XML read
//        (DEC-no-xml-into-game). speed.world.percent -> iSpeedPercent;
//        missionYieldMultiplier.world.percent -> iUnitYieldScalePercent.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvGameSpeedInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvGameSpeedInfo();

	int getSpeedPercent() const;
	int getHammerCostPercent() const;
	int getUnitYieldScalePercent() const;

	// Era pacing at this speed, derived from CvEraInfo (not JSON-backed).
	int getTurnsInEra(int iEra) const;
	int getEraStartTurn(int iEra) const;
	int getTotalTurns() const;
	int getTicksPerTurnInEra(int iEra) const;

	virtual void mapFrom(const picojson::value& entity);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	int m_iSpeedPercent;
	// Scale for unit-produced yields (e.g. subdued-animal food/production), the
	// <AdaptUnitYield> expression channel. Grows slower than iSpeedPercent
	// (~sqrt) so yields don't outpace the longer research/build times.
	int m_iUnitYieldScalePercent;
};

#endif // CV_GAME_SPEED_INFO_H
