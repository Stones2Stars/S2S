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
#include "Defines/CvEnums.h"   // NUM_COMMERCE_TYPES -- the realized-commerce group forward's out-array extent

class CvPlayer;
struct CvCascadeEvalCtx;

class EmpireContext
{
public:
	EmpireContext() : m_player(NULL) {}
	void bind(const CvPlayer* p) { m_player = p; }   // set once by the owning CvPlayer

	// --- STORED aggregate: POLICY id -> the empire ENACTS this policy (json §9), keyed by the ClassificationRegistry
	// domain-local POLICY id (the CvClassificationBlock::hasId space). The derived UNION over the player's LIVE grantors --
	// adopted civics + held (active-set) traits -- rebuilt WHOLE on the civic/trait/player-init DOMAIN facts, never per read. ---
	// `mutable` + a const refresh (the CvDerivedCache / PlotContext shape) so the maintainer drives it through the
	// bound player's CONST accessor -- there is no second, mutable path onto the player.
	mutable ContextDict policies;
	// THE ONE MAINTENANCE ENTRY -- called ONLY by the contexts' spine consumer (Engine/ContextConsumer). Walks
	// m_player's live civics + held traits and refills `policies` whole (out-of-line, EmpireContext.cpp).
	// CONSTRAINT: no choke point may call this directly. A civic/trait change emits its own DOMAIN fact, so the
	// consumer is the single trigger path; a direct call beside the event would be a second maintenance surface.
	void rebuildPolicies() const;
	void clear() const { policies.clear(); }
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
	// The empire's CURRENT REALIZED COMMERCE -- CvPlayer::getCommerces, the player's own O(1) group read, handed
	// on unchanged. The empire-scope twin of CityContext::yields and forwarded for the same reason: it is the
	// base an empire-scope percent deposit resolves against (contexts.md: THE CONTEXT *IS* THE CURRENT VALUE --
	// a valuation reads it HERE rather than taking current amounts as a separate parameter). All four channels
	// are EMPIRE RECEIVERS, so every slot is a maintained realized total rather than a channel aggregate.
	// FORWARDED, never stored: a copy here would duplicate the bound object's own maintained data AND would need
	// an invalidation the forward does not have. ×100 native, indexed by CommerceTypes.
	void commerces(int (&realizedCommerces)[NUM_COMMERCE_TYPES]) const;
	// The COMMERCE SLIDER PERCENTAGES -- the empire's gold / research / culture / espionage rates as ONE group
	// keyed by CommerceTypes (contexts.md's EmpireContext forward row). These are the percentages the per-commerce
	// SPLIT runs on: a city receives the COMMERCE yield and the empire's sliders divide it across the four
	// channels (InfoValuation::commerceSplit), which is why they live on the PLAYER's context and nowhere else.
	// FORWARDED, never stored: CvPlayer owns them O(1) and normalizes the four to total 100 on every set, so a
	// mirror here would need an invalidation the forward does not have.
	// SCALE: plain 0..100 counters, NOT ×100 magnitudes -- json §3.1 lists GOLD_RATE / RESEARCH_RATE /
	// CULTURE_RATE / ESPIONAGE_RATE among the catch-all counter tokens, and the same values answer those tokens
	// through commerceRate above (the single forward this group fans out over).
	void commerceRates(int (&commerceRates)[NUM_COMMERCE_TYPES]) const;

	// Fill the EMPIRE half of a condition-eval context (ec.player + ec.team) from the bound player -- paired with
	// CityContext::fillEvalCtx (city/plot); together they are the eval state the ONE evaluator reads.
	void fillEvalCtx(CvCascadeEvalCtx& ec) const;

private:
	const CvPlayer* m_player;    // the bound game object; the forward reads it -- never a value copy
};

#endif // CV_EMPIRE_CONTEXT_H
