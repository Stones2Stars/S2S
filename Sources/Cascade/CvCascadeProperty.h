#pragma once
#ifndef CV_CASCADE_PROPERTY_H
#define CV_CASCADE_PROPERTY_H

//
//	CascadeProperty -- the #430 modifier machine PROPERTY channel: each PROPERTY_* is its own modifier family
//	(json.md §6, owner 2026-06-16 "classed like any other yield"); the cascade computes the city's PER-TURN
//	SOURCED numbers from the curated deposits, the ENGINE keeps the integration dynamics (decay-toward-
//	targetLevel, the solver's predict/correct ordering). SCOPE BOUNDARY: the #429 spatial distribution
//	(RELATION_NEAR pulses, propagators -- parked as grants.repeatable data) is NOT this channel.
//
//	Sources summed (city scope): ACTIVE buildings' <PROPERTY_X>.city.flat (constant per-turn sources + the
//	construction-time bag the curator folded -- the curated data IS the model, DEC-data-first), the units on
//	the city plot (SAME_PLOT emission, e.g. a criminal's +20 crime), and the property's OWN self-deposits
//	(the per-POPULATION attribute source). The property's decay stays a separate PERCENT number.
//

#include "CvCascadeConditionEval.h"

class CvCity;

class CascadeProperty
{
public:
	// The city's per-turn FLAT source sum for property eProp (curated deposits, current state).
	static int citySourceFlat(int eProp, const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// The property's own DECAY percent at city scope (the toward-targetLevel rate -- engine dynamics apply it).
	static int cityDecayPercent(int eProp);
};

#endif // CV_CASCADE_PROPERTY_H
