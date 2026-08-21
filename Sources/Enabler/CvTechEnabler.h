#pragma once
#ifndef CV_TECH_ENABLER_H
#define CV_TECH_ENABLER_H

//
//	TechEnabler -- the TECH domain on the standardized enabler component (enabler.md par.7/7.1; CvEnabler.h):
//	the player's researchable list as a maintained tri-state vector, built PURELY from DOMAIN events (one
//	mechanism for load and play -- the load reseed's in-read emits ARE the same events, docs/spine.md §5 (the load reseed)).
//	initDomain() is the lifecycle sizing (arrays + static exclusions, NO content); onTechChanged() is the
//	O(delta) applier the spine consumer routes (play emits + the reseed's per-held-tech emits alike); the read
//	is the owner's bare member lookup (CvPlayer::getTechAvailability reads m_enabler.techs directly).
//
//	⛔ There is deliberately NO from-source recompute here to diff the maintained vector against. The enabler
//	consumes ONLY events so that a missed emit surfaces as a visibly WRONG enabler, and what catches one is the
//	THREE-LEG check -- the logs, the JSON info, and what state expects
//	([http-endpoints.md](../../docs/specs/http-endpoints.md)). A recompute served beside the maintained set
//	answers a number that was never comparable, and the replay it would need is minutes of work
//	([superseded-ideas #33](../../docs/architecture/superseded-ideas.md)).
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

};

#endif // CV_TECH_ENABLER_H
