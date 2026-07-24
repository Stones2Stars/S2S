#pragma once
#ifndef CV_EMPIRE_CONTEXT_H
#define CV_EMPIRE_CONTEXT_H

//
//	EmpireContext -- the per-PLAYER READ SURFACE, the empire-scope sibling of CityContext (same rules, kept symmetric
//	so a reader always knows where to go: city state on CityContext, empire state here). Bound to its CvPlayer by
//	pointer (never a value copy).
//
//	⛔ STORES only the uniquely-owned AGGREGATE -- `policies`: the empire's enacted-policy set, a derived UNION over
//	the live civics'/traits' policy blocks that lives nowhere else (the empire analog of CityContext::plotAttrs),
//	event-maintained on civic/trait change. `stateReligion` FORWARDS (a single enum already on CvPlayer -- not
//	duplicated).
//

#include "ContextDict.h"

class CvPlayer;

class EmpireContext
{
public:
	EmpireContext() : m_player(NULL) {}
	void bind(const CvPlayer* p) { m_player = p; }   // set once by the owning CvPlayer

	// --- STORED aggregate: POLICY id -> the empire ENACTS this policy (json §9). Rebuilt on civic/trait change. ---
	ContextDict policies;
	void clear() { policies.clear(); }

	// --- FORWARDED: the empire's state religion (single enum), read through the bound player. Out-of-line (.cpp). ---
	int stateReligion() const;   // CvPlayer::getStateReligion (-1 = NO_RELIGION)

private:
	const CvPlayer* m_player;    // the bound game object; the forward reads it -- never a value copy
};

#endif // CV_EMPIRE_CONTEXT_H
