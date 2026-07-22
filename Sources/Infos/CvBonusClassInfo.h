#pragma once

#ifndef CV_BONUS_CLASS_INFO_H
#define CV_BONUS_CLASS_INFO_H

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvBonusClassInfo
//
//  DESC:   A bonus grouping with a map-generation min-spacing. #430: JSON-fed
//          (Assets/Data/bonusclasses/*.json via mapFrom); no XML read.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvBonusClassInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvBonusClassInfo();

	int getUniqueRange() const { return m_iUniqueRange; }

	virtual void mapFrom(const picojson::value& entity);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:
	int m_iUniqueRange;
};

#endif // CV_BONUS_CLASS_INFO_H
