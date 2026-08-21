#pragma once
#ifndef CV_JSON_COMPLEX_TRAIT_INFO_H
#define CV_JSON_COMPLEX_TRAIT_INFO_H

//
//	CvComplexTraitInfo -- the COMPLEX/Thunderbrd trait set (StoneBase ComplexTraitInfo). Stored in its OWN repo
//	InfoRepo<CvComplexTraitTag>, because the two sets are two genuinely different entity populations that a single
//	repo would interleave. Active under GAMEOPTION_LEADER_COMPLEX_TRAITS.
//
//	The two sets are separated BY ID as well as by repo: a complex trait keeps its own TRAIT_COMPLEX_ identity
//	(naming.md) and is never re-keyed onto the base trait's id (modifier.md par.4). So a given trait id resolves in
//	exactly ONE repo and no reader has to consult a game option to know which. The sets share NO id -- a simple
//	trait with no complex variant is copied into complex/ under its own TRAIT_COMPLEX_ id.
//

#include "CvTraitInfo.h"

// The phantom repo TAG for the complex trait set (both sets store CvTraitInfo subclasses in the one engine id space).
struct CvComplexTraitTag {};

class CvComplexTraitInfo : public CvTraitInfo {};

#endif // CV_JSON_COMPLEX_TRAIT_INFO_H
