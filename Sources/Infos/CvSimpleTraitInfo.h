#pragma once
#ifndef CV_JSON_SIMPLE_TRAIT_INFO_H
#define CV_JSON_SIMPLE_TRAIT_INFO_H

//
//	CvSimpleTraitInfo -- the SIMPLE trait set (StoneBase SimpleTraitInfo). Stored in InfoRepo<CvTraitInfo> (the engine
//	CvTraitInfo tag = the simple repo). Active unless GAMEOPTION_LEADER_COMPLEX_TRAITS selects the complex set.
//

#include "CvTraitInfo.h"

class CvSimpleTraitInfo : public CvTraitInfo {};

#endif // CV_JSON_SIMPLE_TRAIT_INFO_H
