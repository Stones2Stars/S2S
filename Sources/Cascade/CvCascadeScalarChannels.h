#pragma once
#ifndef CV_CASCADE_SCALAR_CHANNELS_H
#define CV_CASCADE_SCALAR_CHANNELS_H

//
//	CascadeScalarChannels -- the #430 city SCALAR channels (legacy-value-calc-map §4/§5/§9.5): greatPeopleRate,
//	defense, maintenance -- each computed from the curated deposits (buildings via the facts cache + civics +
//	traits), netted against the legacy accumulators on /computed/cities/wellbeing (the wellbeing/property
//	pattern: open the net, reconcile the classes, then slot + flip). DEC-unit-modifiers-on-top holds: nothing
//	unit-sourced enters these (no unit-carried deposits exist in these families).
//

#include "CvCascadeConditionEval.h"

class CvCity;

class CascadeScalarChannels
{
public:
	// greatPeopleRate: the city BASE (building + specialist flats; the player national rate is a live input)
	// and the MODIFIER percent stack (city + empire percents incl. state-religion/golden-age-gated entries).
	static int gpRateBase(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	static int gpRateModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// defense: the building city amount stack (legacy m_iBuildingDefense).
	static int defenseAmount(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// maintenance: the effective modifier percent stack (city + empire + area scopes; building/civic/trait).
	static int maintenanceModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec);
};

#endif // CV_CASCADE_SCALAR_CHANNELS_H
