#pragma once
#ifndef CV_JSON_LEADERHEAD_INFO_H
#define CV_JSON_LEADERHEAD_INFO_H

//
//	CvJsonLeaderHeadInfo -- the JSON poco for LEADERHEADS (uniformity ruling: every info type has its own
//	CvJson<X>Info home, even when empty). A leaderhead's sections are intrinsic/bespoke (personality/AI blocks);
//	it composes no section units today. Type + description are served by the CvJsonInfo base; this is where any
//	future leaderhead-level typed member would land.
//

#include "CvJsonInfo.h"

class CvJsonLeaderHeadInfo : public CvJsonInfo
{
public:
	CvJsonLeaderHeadInfo();
};

#endif // CV_JSON_LEADERHEAD_INFO_H
