#pragma once
#ifndef CV_PROMOTION_ENABLER_H
#define CV_PROMOTION_ENABLER_H

//
//	PromotionEnabler -- the PROMOTIONS domain's PURE CALCULATORS on the standardized enabler component
//	(enabler.md par.7/7.1; CvEnabler.h): initDomain (the lifecycle sizing, NO content) + the onTechChanged
//	event-delta applier that maintain CvPlayer::m_enabler.promotions -- the par.7.1 player unlocked-promotions
//	set. The content is built PURELY from DOMAIN events (the load reseed's in-read per-held-tech emits and the
//	play-time emits, one mechanism -- docs/spine.md §5 (the load reseed)). The ONE maintained HAVE axis is team techs (incl. the
//	TECH_GAME_START root, which carries the from-start promotions): enables.promotions feeds the enable plane,
//	the removal families the remove plane, through the ONE kernel applier. Per par.7.1 there are NO per-unit
//	maintained sets (thousands of units x hundreds of promotions, churned on every tech, for a decision that
//	only happens at level-up) -- the per-unit gate (enPromotionValid) OVERLAYS the unit's own held-promo +
//	unitcombat planes on this domain's planes at level-up, on demand.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

class CvPlayer;

class PromotionEnabler
{
public:
	static void initDomain(const CvPlayer& kPlayer);   // lifecycle sizing only -- the events build the content
	// MUST run BEFORE TechEnabler::onTechChanged (the player tech domain's held flag is the broad-emit flip guard)
	static void onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas);
};

#endif // CV_PROMOTION_ENABLER_H
