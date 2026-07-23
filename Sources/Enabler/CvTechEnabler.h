#pragma once
#ifndef CV_TECH_ENABLER_H
#define CV_TECH_ENABLER_H

//
//	TechEnabler -- the TECH domain on the standardized enabler component (enabler.md par.7/7.1; CvEnabler.h):
//	the player's researchable list as a maintained tri-state vector, built PURELY from DOMAIN events (one
//	mechanism for load and play -- the load reseed's in-read emits ARE the same events, DEC-spine-reseed).
//	initDomain() is the lifecycle sizing (arrays + static exclusions, NO content); onTechChanged() is the
//	O(delta) applier the spine consumer routes (play emits + the reseed's per-held-tech emits alike); the read
//	is the owner's bare member lookup (canResearch reads m_enabler.techs.listed directly).
//
//	available() is the PURE FUNCTION -- the VALIDATION ORACLE ONLY, never the read path (enabler.md par.7:
//	a static is a pure calculator; the read path never calls one). Its diff against the maintained vector is
//	the missed-emit tripwire: the enabler consumes ONLY events precisely so a missed emit surfaces as a
//	visibly wrong enabler.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state
//	(the maintained state lives on the OWNER -- CvPlayer::m_enabler).
//

#include <set>

class CvPlayer;
class CvTeam;

class TechEnabler
{
public:
	// The lifecycle INIT (size + static exclusions, NO content): at the start of CvPlayer::read (before the
	// in-read emits stream) and at CvPlayer::init/initInGame. The events build the content.
	static void initDomain(const CvPlayer& kPlayer);

	// The O(delta) applier (the spine consumer's SEVT_TECH_CHANGED entry): apply eTech's tech edges to every
	// initialized player of eTeam. Idempotent per player (the held flag guards re-emits).
	static void onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas);

	// THE VALIDATION ORACLE (the pure function, enable-side): CAN GET = union(enables.techs) - removals over
	// HAVE, minus held/disabled. Never the read path -- diff the maintained vector against this to catch a
	// missed/mis-ordered delta.
	static void available(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& avail);
};

#endif // CV_TECH_ENABLER_H
