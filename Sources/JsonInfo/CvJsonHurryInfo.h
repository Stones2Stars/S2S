#pragma once
#ifndef CV_JSON_HURRY_INFO_H
#define CV_JSON_HURRY_INFO_H

//
//	CvJsonHurryInfo -- the JSON poco for HURRIES (the production-hurry kinds; uniformity ruling: every info type
//	has its own CvJson<X>Info home, even when empty). Its sections are intrinsic/bespoke (the hurry cost params);
//	availability rides the enablers' `enables.hurries` edges, not this poco. Type + description are served by the
//	CvJsonInfo base; this is where any future hurry-level typed member would land.
//

#include "CvJsonInfo.h"

class CvJsonHurryInfo : public CvJsonInfo
{
public:
	CvJsonHurryInfo();
};

#endif // CV_JSON_HURRY_INFO_H
