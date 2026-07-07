#pragma once
#ifndef CV_JSON_SPECIALUNIT_INFO_H
#define CV_JSON_SPECIALUNIT_INFO_H

//
//	CvJsonSpecialUnitInfo -- the JSON poco for SPECIAL UNITS (the unit group/slot axis; uniformity ruling: every
//	info type has its own CvJson<X>Info home, even when empty). Its sections are intrinsic/bespoke; it composes no
//	section units today. Type + description are served by the CvJsonInfo base; this is where any future
//	special-unit-level typed member would land.
//

#include "CvJsonInfo.h"

class CvJsonSpecialUnitInfo : public CvJsonInfo
{
public:
	CvJsonSpecialUnitInfo();
};

#endif // CV_JSON_SPECIALUNIT_INFO_H
