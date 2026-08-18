#pragma once
#ifndef CV_EMPIRE_CONTEXT_H
#define CV_EMPIRE_CONTEXT_H

//
//	EmpireContext -- the per-PLAYER READ SURFACE, the empire-scope sibling of CityContext (same rules, kept symmetric
//	so a reader always knows where to go: city state on CityContext, empire state here). Bound to its CvPlayer by
//	pointer (never a value copy).
//
//	⛔ STORES NOTHING TODAY -- the enacted-policy dictionary moved to its own home on CvPlayer. What follows
//	describes what that store IS, not where it lives: `policies`, a derived UNION over
//	the live civics'/traits' policy blocks that lives nowhere else (the empire analog of CityContext::plotAttrs),
//	event-maintained on civic/trait change. Everything else FORWARDS through the bound player -- incl. the TEAM
//	facts (team techs / projects / member count): team is deliberately NOT a context (contexts.md), so team-held
//	HAVE reads go through the player's team from here.
//
//	⚖ THE HAVE AXIS LIVES HERE (contexts.md): every evaluator atom / enabler gate read of an empire- or team-scope
//	fact (civics, traits, heritages, team techs, ...) goes through these forwards, never an ad-hoc reach into
//	CvPlayer/CvTeam.
//

#include "TraitContext.h"   // TraitContext::HeldTrait -- the id+record pair the held-trait forward hands back
#include "ContextDict.h"
#include "Defines/CvEnums.h"   // NUM_COMMERCE_TYPES -- the realized-commerce group forward's out-array extent
#include <vector>              // the held-trait group forward's out-container

class CvPlayer;
class CvTraitInfo;
struct CvCascadeEvalCtx;

class EmpireContext
{
public:
	EmpireContext() : m_player(NULL) {}
	void bind(const CvPlayer* p) { m_player = p; }   // set once by the owning CvPlayer

	// ⛔ THE POLICY STATE IS NOT HELD HERE, and it is not reached THROUGH here either. It lives in its own
	// dictionary, owned by CvPlayer exactly as the amenity dictionary is owned by CvCity, and that dictionary owns
	// its storage, its maintenance AND the declared set of facts that drives it -- one place responsible
	// (Engine/PolicyContext.h, docs/cascade.md §What a context STORES vs FORWARDS (a dictionary is a spine consumer)). This context FORWARDS the read and stores nothing.
	// ⚠ Its maintainer reaches `player.policies()` DIRECTLY -- never through this context, which owns none of it.
	void clear() const {}
	bool hasPolicy(int ePolicy) const;   // the O(1) enacted-policy read (ev_playerHasPolicy), forwarded

	// --- FORWARDED: read through the bound player / its team. Out-of-line (.cpp). ---
	int  stateReligion() const;               // CvPlayer::getStateReligion (-1 = NO_RELIGION)
	bool hasCivic(int eCivic) const;          // adopted in any civic option (the CIVIC_ presence atom)
	bool hasTrait(int eTrait) const;          // CvPlayer::hasTrait (the TRAIT_ presence atom; active-set semantics ride the id)
	// The empire's HELD TRAITS as their ACTIVE-SET records, APPENDED to a caller-owned vector -- the group read
	// beside the per-id test above, and a genuinely different question: testing one trait is a pointer hop, while
	// enumerating them off the has-array walks the whole registry. So this one is answered by a maintained STORE
	// (Engine/TraitContext.h) rather than by a forward that computes, and it is reached HERE because the HAVE axis
	// is read through the scope's context, never by an ad-hoc reach into CvPlayer (contexts.md).
	// ⚑ Its reader is the keyed-deposit walk: a trait's target-keyed deposits stay source-side (modifier.md §4),
	// so the live sources have to be enumerable cheaply for that read to be the cheap one modifier.md §5 describes.
	void heldTraits(std::vector<TraitContext::HeldTrait>& heldTraits) const;
	bool hasHeritage(int eHeritage) const;    // CvPlayer::hasHeritage
	// The EMPIRE-LEVEL building axis (docs/specs/enabler.md §2 (empire-level buildings)): the held store and the player-side operate
	// verdict, both the object's own O(1) data -- FORWARDED, never stored (the STORES-vs-FORWARDS split).
	bool hasEmpireBuilding(int eBuilding) const;         // CvPlayer::hasEmpireBuilding
	bool isEmpireBuildingActive(int eBuilding) const;    // CvPlayer::isEmpireBuildingActive
	bool isGoldenAge() const;                 // CvPlayer::isGoldenAge (IS_GOLDEN_AGE)
	bool isAnarchy() const;                   // CvPlayer::isAnarchy (IS_ANARCHY)
	bool isRebel() const;                     // CvPlayer::isRebel (IS_REBEL)
	int  numCities() const;                   // CvPlayer::getNumCities (the CITY counter)
	int  currentEra() const;                  // CvPlayer::getCurrentEra (the ERA counter's engine value; 0-based)
	int  commerceRate(int eCommerce) const;   // CvPlayer::getCommercePercent (the <CHANNEL>_RATE slider counters)
	bool teamHasTech(int eTech) const;        // GET_TEAM(player)::isHasTech -- the TECH_ atom (team is not a context)
	int  teamProjectCount(int eProject) const;   // GET_TEAM(player)::getProjectCount -- the PROJECT_ atom
	int  teamMemberCount() const;             // GET_TEAM(player)::getNumMembers (the TEAM counter)
	// The two IDENTITIES a cross-scope read needs as an ARGUMENT rather than as a fact: the count-scope entity
	// (tally reads keyed by empire vs team) and the VIEWER a per-team visibility read is taken from (a foreign
	// tile's bonus reads differently per asking team -- PlotContext::hasBonus / natureYield).
	// ⛔ teamId is the whole of what a reader may know about a team: `CvTeam` is the TECH BRIDGE and holds no
	// live-state surface, so it is reached HERE, through the player, and the eval ctx carries no `CvTeam*`
	// ([contexts.md]; the banner on CvTeam itself).
	int  playerId() const;                    // CvPlayer::getID
	int  teamId() const;                      // CvPlayer::getTeam
	// The §3.9 `ai` AUDIENCE gate: an aiOnly entry applies to AI players only, so the deposit read asks the
	// asking player which side of that split it is on. Raw, object-owned, O(1) -- forwarded like population.
	bool isHuman() const;                     // CvPlayer::isHuman
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

	// Fill the EMPIRE half of a condition-eval context (this silo alone -- it answers the team facts too) -- paired with
	// CityContext::fillEvalCtx (city/plot); together they are the eval state the ONE evaluator reads.
	void fillEvalCtx(CvCascadeEvalCtx& ec) const;

private:
	const CvPlayer* m_player;    // the bound game object; the forward reads it -- never a value copy
};

#endif // CV_EMPIRE_CONTEXT_H
