#pragma once
#ifndef CV_JSON_COMPLEX_TRAIT_INFO_H
#define CV_JSON_COMPLEX_TRAIT_INFO_H

//
//	CvComplexTraitInfo -- the COMPLEX/Thunderbrd trait set (StoneBase ComplexTraitInfo). Stored in its OWN repo
//	InfoRepo<CvComplexTraitTag> (the simple set collides with it on the engine id, so it needs a distinct repo tag).
//	Active under GAMEOPTION_LEADER_COMPLEX_TRAITS.
//

#include "CvTraitInfo.h"

// The phantom repo TAG for the complex trait set (both sets store CvTraitInfo subclasses keyed by the SAME engine id).
struct CvComplexTraitTag {};

class CvComplexTraitInfo : public CvTraitInfo {};

#endif // CV_JSON_COMPLEX_TRAIT_INFO_H
