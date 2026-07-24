#pragma once
#ifndef CV_EMPIRE_CONTEXT_H
#define CV_EMPIRE_CONTEXT_H

//
//	EmpireContext -- the per-PLAYER ISOLATED live-state object, the empire-scope sibling of CityContext. Owned by
//	CvPlayer, kept current by events. Holds the EMPIRE-scope facts so they are NOT mirrored into every city (owner):
//	a city's eval reaches its owner's EmpireContext up the scope chain for these, rather than each CityContext
//	carrying a copy.
//
//	Same shape rules as CityContext: COUNTS not objects, self-contained (a raw pointer passed directly), and the
//	same ContextDict for anything keyed. First cut carries the empire facts we have a use for:
//	  - stateReligion -- a SINGLE enum (there is exactly one), not a dictionary (owner);
//	  - policies       -- the empire policy dict (json §9): POLICY id -> the empire ENACTS this policy.
//	Team-scope facts (techs, team wonders, ...) are the TeamContext sibling when needed.
//

#include "ContextDict.h"

class EmpireContext
{
public:
	EmpireContext() : m_stateReligion(-1) {}

	// The empire STATE RELIGION -- a SINGLE enum (a RELIGION id; -1 = NO_RELIGION), not a dictionary. {STATE_RELIGION: R}
	// = stateReligion() == R; a city's STATE_RELIGION_IN_CITY = cityCtx.religions.has(empireCtx.stateReligion()).
	int  stateReligion() const { return m_stateReligion; }
	void setStateReligion(int r) { m_stateReligion = r; }

	// Empire POLICIES (json §9): POLICY id -> the empire enacts this policy. Enacted by a civic (while adopted) or a
	// trait (while held) -- fed by those events, read via policies.has(POLICY_X).
	ContextDict policies;

	void clear() { policies.clear(); m_stateReligion = -1; }

private:
	int m_stateReligion;   // the empire's state religion -- a SINGLE RELIGION enum id (-1 = NO_RELIGION)
};

#endif // CV_EMPIRE_CONTEXT_H
