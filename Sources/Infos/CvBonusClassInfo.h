#pragma once
#ifndef CV_BONUS_CLASS_INFO_H
#define CV_BONUS_CLASS_INFO_H

//
//	CvBonusClassInfo -- the BONUSCLASS poco (a bonus grouping with a map-generation min-spacing). Pure
//	intrinsic self-description on the exemplar surface (patterns.md § THE GETTER SETUP category 4): the census
//	authors NO modifier family and no availability section on this type -- one mapGeneration config value.
//	JSON-fed (Assets/Data/bonusclasses/*.json via mapFrom); no XML read.
//

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

class CvBonusClassInfo : public CvInfo
{
public:
	CvBonusClassInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= INTRINSIC -- bare typed reads =======================
	// mapGeneration.uniqueRange -- the min-spacing that prevents same-class bonus stacking (CvMapGenerator).
	int getUniqueRange() const { return m_iUniqueRange; }

private:
	int m_iUniqueRange;
};

#endif // CV_BONUS_CLASS_INFO_H
