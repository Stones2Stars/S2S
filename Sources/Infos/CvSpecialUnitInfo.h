#pragma once

#ifndef CV_SPECIAL_UNIT_INFO_H
#define CV_SPECIAL_UNIT_INFO_H

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvSpecialUnitInfo
//
//  DESC:   A special-unit class (captive / people / missile / fighter / ...).
//          #430: JSON-fed (Assets/Data/specialunits/*.json via mapFrom); no XML read.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvSpecialUnitInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvSpecialUnitInfo();

	bool isValid() const { return m_bValid; }
	bool isCityLoad() const { return m_bCityLoad; }
	bool isSMLoadSame() const { return m_bSMLoadSame; }

	int getCombatPercent() const { return m_iCombatPercent; }
	int getWithdrawalChange() const { return m_iWithdrawalChange; }

	virtual void mapFrom(const picojson::value& entity);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	bool m_bValid;
	bool m_bCityLoad;
	bool m_bSMLoadSame;

	int m_iCombatPercent;
	int m_iWithdrawalChange;
};

#endif // CV_SPECIAL_UNIT_INFO_H
