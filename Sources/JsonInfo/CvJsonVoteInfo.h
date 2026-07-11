#pragma once
#ifndef CV_JSON_VOTE_INFO_H
#define CV_JSON_VOTE_INFO_H

//
//	CvJsonVoteInfo -- the JSON poco for VOTES (the diplomatic-vote resolutions; uniformity ruling: every info type
//	has its own CvJson<X>Info home, even when empty). Its sections are intrinsic/bespoke (voteSource/threshold);
//	it composes no section units today. Type + description are served by the CvInfo base; this is where any
//	future vote-level typed member would land.
//

#include "CvInfo.h"

class CvJsonVoteInfo : public CvInfo
{
public:
	CvJsonVoteInfo();
};

#endif // CV_JSON_VOTE_INFO_H
