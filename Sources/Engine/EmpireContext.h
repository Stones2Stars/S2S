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
//	event-maintained on civic/trait change. Everything else FORWARDS through the bound player -- incl. the TEAM
//	facts (team techs / projects / member count): team is deliberately NOT a context (contexts.md), so team-held
//	HAVE reads go through the player's team from here.
//
//	⚖ THE HAVE AXIS LIVES HERE (contexts.md): every evaluator atom / enabler gate read of an empire- or team-scope
//	fact (civics, traits, heritages, team techs, ...) goes through these forwards, never an ad-hoc reach into
//	CvPlayer/CvTeam.
//

#include "ContextDict.h"

class CvPlayer;
struct CvCascadeEvalCtx;

class EmpireContext
{
public:
	EmpireContext() : m_player(NULL) {}
	void bind(const CvPlayer* p) { m_player = p; }   // set once by the owning CvPlayer

	// --- STORED aggregate: POLICY id -> the empire ENACTS this policy (json §9), keyed by the ClassificationRegistry
	// domain-local POLICY id (the CvClassificationBlock::hasId space). The derived UNION over the player's LIVE grantors --
	// adopted civics + held (active-set) traits -- rebuilt WHOLE on civic/trait change + at load, never per read. ---
	ContextDict policies;
	void rebuildPolicies();   // walk m_player's live civics + traits, refill `policies` (out-of-line, EmpireContext.cpp)
	void clear() { policies.clear(); }
	bool hasPolicy(int ePolicy) const { return policies.has(ePolicy); }   // the O(1) enacted-policy read (ev_playerHasPolicy)

	// --- FORWARDED: read through the bound player / its team. Out-of-line (.cpp). ---
	int  stateReligion() const;               // CvPlayer::getStateReligion (-1 = NO_RELIGION)
	bool hasCivic(int eCivic) const;          // adopted in any civic option (the CIVIC_ presence atom)
	bool hasTrait(int eTrait) const;          // CvPlayer::hasTrait (the TRAIT_ presence atom; active-set semantics ride the id)
	bool hasHeritage(int eHeritage) const;    // CvPlayer::hasHeritage
	bool isGoldenAge() const;                 // CvPlayer::isGoldenAge (IS_GOLDEN_AGE)
	bool isAnarchy() const;                   // CvPlayer::isAnarchy (IS_ANARCHY)
	int  numCities() const;                   // CvPlayer::getNumCities (the CITY counter)
	int  currentEra() const;                  // CvPlayer::getCurrentEra (the ERA counter's engine value; 0-based)
	int  commerceRate(int eCommerce) const;   // CvPlayer::getCommercePercent (the <CHANNEL>_RATE slider counters)
	bool teamHasTech(int eTech) const;        // GET_TEAM(player)::isHasTech -- the TECH_ atom (team is not a context)
	int  teamProjectCount(int eProject) const;   // GET_TEAM(player)::getProjectCount -- the PROJECT_ atom
	int  teamMemberCount() const;             // GET_TEAM(player)::getNumMembers (the TEAM counter)

	// Fill the EMPIRE half of a condition-eval context (ec.player + ec.team) from the bound player -- paired with
	// CityContext::fillEvalCtx (city/plot); together they are the eval state the ONE evaluator reads.
	void fillEvalCtx(CvCascadeEvalCtx& ec) const;

private:
	const CvPlayer* m_player;    // the bound game object; the forward reads it -- never a value copy
};

#endif // CV_EMPIRE_CONTEXT_H
