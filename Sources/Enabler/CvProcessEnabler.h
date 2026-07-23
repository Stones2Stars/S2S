#pragma once
#ifndef CV_PROCESS_ENABLER_H
#define CV_PROCESS_ENABLER_H

//
//	ProcessEnabler -- the PROCESSES domain's PURE CALCULATORS on the standardized enabler component (enabler.md
//	par.7/7.1; CvEnabler.h): initDomain (the lifecycle sizing, NO content) + the onTechChanged event-delta
//	applier that maintain CvPlayer::m_enabler.processes. The content is built PURELY from DOMAIN events -- the
//	load reseed's in-read per-held-tech emits and the play-time emits are one mechanism (DEC-spine-reseed).
//	The maintain choice stays a city production-list thing (CvCity::canMaintain is the gate), but the domain is
//	PLAYER-held (owner ruling): its ONE axis is team techs (incl. the TECH_GAME_START root, which carries
//	PROCESS_IDLE), so per-city copies would be byte-identical duplicated state that must never drift. The city
//	gate reads through its owner (a dynamic GET_PLAYER(getOwner()) lookup -- conquest-safe, never a stored
//	pointer). A static is a calculator ONLY -- reads are bare member lookups. No removal source exists.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

class CvPlayer;

class ProcessEnabler
{
public:
	static void initDomain(const CvPlayer& kPlayer);   // lifecycle sizing only -- the events build the content
	// MUST run BEFORE TechEnabler::onTechChanged (the player tech domain's held flag is the broad-emit flip guard)
	static void onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas);
};

#endif // CV_PROCESS_ENABLER_H
