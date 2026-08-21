#pragma once
#ifndef CV_CIVIC_ENABLER_H
#define CV_CIVIC_ENABLER_H

//
//	CivicEnabler -- the CIVICS domain's PURE CALCULATORS on the standardized enabler component (enabler.md
//	par.7/7.1; CvEnabler.h): initDomain (the lifecycle sizing, NO content) + the onTechChanged event-delta
//	applier that maintain CvPlayer::m_enabler.civics. The content is built PURELY from DOMAIN events -- the
//	load reseed's in-read per-held-tech emits and the play-time emits are one mechanism (docs/spine.md §5 (the load reseed)).
//	A static is a calculator ONLY -- reads are the owner's bare member lookups (canDoCivics reads
//	m_enabler.civics.listed directly). The domain's ONE HAVE axis is team techs (incl. the TECH_GAME_START
//	root, which carries the 15 start civics); no removal source exists (nothing authors
//	obsoletes/disables.civics today), and an adopted civic stays listed (legacy canDoCivics semantics).
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

class CvPlayer;

class CivicEnabler
{
public:
	static void initDomain(const CvPlayer& kPlayer);   // lifecycle sizing only -- the events build the content
	// MUST run BEFORE TechEnabler::onTechChanged (the player tech domain's held flag is the broad-emit flip guard)
	static void onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas);
};

#endif // CV_CIVIC_ENABLER_H
