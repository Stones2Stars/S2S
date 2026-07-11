#pragma once
#ifndef CV_JSON_SPECIALUNIT_INFO_H
#define CV_JSON_SPECIALUNIT_INFO_H

//
//	CvJsonSpecialUnitInfo -- the JSON poco for SPECIAL UNITS (the unit group/slot axis; uniformity ruling: every
//	info type has its own CvJson<X>Info home, even when empty). Its sections are intrinsic/bespoke; it composes no
//	section units today. Type + description are served by the CvInfo base; this is where any future
//	special-unit-level typed member would land.
//

#include "CvInfo.h"

class CvJsonSpecialUnitInfo : public CvInfo
{
public:
	CvJsonSpecialUnitInfo();
};

#endif // CV_JSON_SPECIALUNIT_INFO_H
