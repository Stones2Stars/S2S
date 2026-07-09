#pragma once
#ifndef CV_JSON_SIMPLE_TRAIT_INFO_H
#define CV_JSON_SIMPLE_TRAIT_INFO_H

//
//	CvJsonSimpleTraitInfo -- the SIMPLE trait set (StoneBase SimpleTraitInfo). Stored in InfoRepo<CvJsonTraitInfo> (the engine
//	CvJsonTraitInfo tag = the simple repo). Active unless GAMEOPTION_LEADER_COMPLEX_TRAITS selects the complex set.
//

#include "CvJsonTraitInfo.h"

class CvJsonSimpleTraitInfo : public CvJsonTraitInfo {};

#endif // CV_JSON_SIMPLE_TRAIT_INFO_H
