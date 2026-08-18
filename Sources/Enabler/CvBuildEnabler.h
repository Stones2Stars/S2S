#pragma once
#ifndef CV_BUILD_ENABLER_H
#define CV_BUILD_ENABLER_H

//
//	BuildEnabler -- the worker-BUILDS domain's PURE CALCULATORS on the standardized enabler component
//	(enabler.md par.7/7.1; CvEnabler.h): initDomain (the lifecycle sizing, NO content) + the onTechChanged
//	event-delta applier that maintain CvPlayer::m_enabler.builds -- the par.7.1 player unlocked-builds set. The
//	content is built PURELY from DOMAIN events (the load reseed's in-read per-held-tech emits and the play-time
//	emits, one mechanism -- docs/spine.md §5 (the load reseed)). The ONE HAVE axis is team techs (incl. the TECH_GAME_START root,
//	which carries the from-start builds): enables.builds feeds the enable plane, obsoletes.builds the remove
//	plane, through the ONE kernel applier. Per par.7.1 the PLOT-VALIDITY half of canBuild stays a live per-plot
//	gate (improvement/feature/terrain/gold -- worker decisions already iterate plots); the isDisabled runtime
//	toggle (Python settings scripts) stays a live check beside the bare read. A static is a calculator ONLY --
//	the gate read is the owner's bare member lookup (canBuild reads m_enabler.builds.listed directly).
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

class CvPlayer;

class BuildEnabler
{
public:
	static void initDomain(const CvPlayer& kPlayer);   // lifecycle sizing only -- the events build the content
	// MUST run BEFORE TechEnabler::onTechChanged (the player tech domain's held flag is the broad-emit flip guard)
	static void onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas);
};

#endif // CV_BUILD_ENABLER_H
