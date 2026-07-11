#pragma once
#ifndef CV_JSON_VICTORY_INFO_H
#define CV_JSON_VICTORY_INFO_H

//
//	CvJsonVictoryInfo -- the JSON poco for VICTORIES (uniformity ruling: every info type has its own CvJson<X>Info
//	home, even when empty). Its sections are intrinsic/bespoke (victory conditions/thresholds); it composes no
//	section units today. Type + description are served by the CvInfo base; this is where any future
//	victory-level typed member would land.
//

#include "CvInfo.h"

class CvJsonVictoryInfo : public CvInfo
{
public:
	CvJsonVictoryInfo();
};

#endif // CV_JSON_VICTORY_INFO_H
