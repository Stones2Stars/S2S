#pragma once
#ifndef CV_PROJECT_ENABLER_H
#define CV_PROJECT_ENABLER_H

//
//	ProjectEnabler -- the PROJECTS domain's PURE CALCULATORS on the standardized enabler component (enabler.md
//	par.7/7.1; CvEnabler.h): initDomain (the lifecycle sizing, NO content) + the tech/project event-delta
//	appliers that maintain CvPlayer::m_enabler.projects. The content is built PURELY from DOMAIN events -- the
//	load reseed's in-read emits and the play-time emits are one mechanism (docs/spine.md §5 (the load reseed)). Project CREATION
//	stays a city production-list thing (CvCity::canCreate is the gate), but the domain is PLAYER-held (owner
//	ruling): its axes are team-scope (a project builds like a wonder -- one city's queue, team-wide effect; the
//	designed multi-city production feed never worked), so per-city copies would be byte-identical duplicated
//	state that must never drift. The city gate reads through its owner (a dynamic GET_PLAYER(getOwner()) lookup
//	-- conquest-safe, never a stored pointer); the one city-local fact (the plot map-category gate) stays a
//	live check when the requires stage lands. HAVE axes: team techs (incl. the TECH_GAME_START root) + the
//	team's completed projects (project->project enables, the Apollo->SS-parts chain). A created project does
//	not leave the frontier (multi-instance parts; caps are the gate stage). No removal source exists (nothing
//	authors obsoletes/disables.projects).
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

class CvPlayer;

class ProjectEnabler
{
public:
	static void initDomain(const CvPlayer& kPlayer);   // lifecycle sizing only -- the events build the content
	// MUST run BEFORE TechEnabler::onTechChanged (the player tech domain's held flag is the broad-emit flip guard)
	static void onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas);
	// SEVT_PROJECT_CHANGED: PER-MEMBER emits (one per alive team member, the tech-emit precedent) -- applies to
	// ePlayer's domain ONLY, on the team count's 0-crossing (iDelta = the applied count delta).
	static void onProjectChanged(PlayerTypes ePlayer, ProjectTypes eProject, int iDelta);
};

#endif // CV_PROJECT_ENABLER_H
