#pragma once
#ifndef CV_JSON_PROJECT_INFO_H
#define CV_JSON_PROJECT_INFO_H

//
//	CvProjectInfo -- the PROJECT poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP: the four
//	read categories, nothing else). Team-built endeavours: spaceship parts, Manhattan/SDI class, the Internet.
//	Styled for the JSON anatomy (json.md §2); every magnitude read is a load-compiled fetch
//	(docs/architecture/patterns.md §Materialize at mapFrom); kind and scope are separate parameters (docs/architecture/patterns.md §The coherent surface (scope is a separate axis)); no legacy
//	getter name returns (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)).
//
//	Availability rides the base: the tech prereq (tech.enables.projects) + the prerequisite projects (their
//	enables.projects) are store-inverted and reconstructed at LOAD by the readJson reverse pass (the setters
//	below); the world/team instance caps ride the composed `allowed` (base allowedCap); the game-wide
//	specialBuilding unlock rides the composed edges (EDGEF_ENABLES/EDGEB_SPECIAL_BUILDINGS).
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // MapCategoryTypes / VictoryTypes FKs
#include <map>
#include <set>
#include <vector>

class CvProjectInfo : public CvInfo
{
public:
	CvProjectInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvAllowed*   getAllowed()   const { return &m_allowed; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 2. CLASSIFICATION -- none (projects author no §8 block) =========================

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. The census
	// participation: maintenance.empire %, {gold/research/culture}.empire %, happiness/health at empire,
	// diplomacy.team.techShare -- all unconditioned; buildRate.self bonus-gated entries are conditioned and
	// stay entry-list/valuation reads.)
	// ⛔ The `world.empires` FAN is NOT one of these point reads, and expecting it here reads 0 silently: a
	// plural target ([json.md §3.3]) carries a target segment, which makes the entry unfoldable by
	// construction, so the health/tradeRoutes rows authored there are an ENTRY-LIST read
	// (`InfoValuation::keyedTarget` over the `empires` segment, [modifier.md §5]). The gather is what lands
	// the fan in each player's package.
	int getCommerceModifier(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT); }
	// The authored wellbeing families' SIGNED sums. ⛔ ANGER/UNHEALTH read 0 here BY CONSTRUCTION and that is
	// never a gap to chase: an INFO keeps a negative in its POSITIVE family (happiness -1, not anger +1) --
	// the sign ROUTING to the opposing channel happens at FILL, on the city PACKAGE, not on authored data
	// (modifier.md §2b). So this read already carries the negatives; there is nothing to verify in the JSON.
	int getMaintenanceModifier(MaintenanceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MAINTENANCE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getDiplomacy(DiplomacyKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DIPLOMACY, eKind, eScope, infoKindUnit(MODFAM_DIPLOMACY, eKind)); }
	int getTradeRoute(TradeRouteKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_TRADE_ROUTES, eKind, eScope, infoKindUnit(MODFAM_TRADE_ROUTES, eKind)); }
	// (combat.team.nukeInterception is OUTSIDE the kind vocabulary -- ruling 16 classes it trigger-plane chance
	// data pending the curator re-home, so it compiles as an unkinded entry (kind-coverage diagnostic) and has
	// no point read here; the buildRate.self bonus-gated entries are conditioned-list reads by design.)

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getProductionCost() const { return m_iProductionCost; }             // cost.create (hammers)
	bool isSpaceship() const { return m_bSpaceship; }                       // identity.spaceship
	int getLaunchesVictory() const { return m_eLaunchesVictory; }           // identity.launchesVictory (VICTORY_* FK; -1 none)
	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }   // identity.mapCategories
	const char* getMovieDefineTag() const { return m_szMovieDefineTag.c_str(); }                  // ui.art.movie.defineTag
	// bespoke non-cascade `victory` launch params (json.md §9)
	int getVictoryThreshold(int iVictory) const { return mapGet(m_victoryThreshold, iVictory); }  // victory.thresholds
	// when the min is 0/unset, FALL BACK to the plain victory threshold (a project with a threshold but no
	// explicit min must not read 0 -- the Space-victory min-project gate).
	int getVictoryMinThreshold(int iVictory) const
	{
		const int iMin = mapGet(m_victoryMinThreshold, iVictory);
		return iMin != 0 ? iMin : getVictoryThreshold(iVictory);
	}
	int getVictoryDelayPercent() const { return m_iVictoryDelayPercent; }   // victory.delayPercent
	int getSuccessRate() const { return m_iSuccessRate; }                   // victory.successRate

	// --- store-inverted availability FKs (tech.enables.projects / the prerequisite project's enables.projects,
	// set-based -- the per-edge iNeeded count is dropped, all 1 today), reconstructed at LOAD by the readJson
	// reverse pass (CvReversePass), which calls the writers below. LOAD-ONLY writers. ---
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	int getProjectsNeeded(int iProject) const { return m_projectsNeeded.count(iProject) ? 1 : 0; }
	void setTechPrereq(TechTypes eTech) { m_eTechPrereq = eTech; }
	void addProjectNeeded(int iProject) { m_projectsNeeded.insert(iProject); }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvAllowed*   mutAllowed()   { return &m_allowed; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	static int mapGet(const std::map<int, int>& valueMap, int iKey)
	{
		std::map<int, int>::const_iterator valueIt = valueMap.find(iKey);
		return valueIt != valueMap.end() ? valueIt->second : 0;
	}

	// --- the composed section units ---
	CvEdges     m_edges;
	CvAllowed   m_allowed;
	CvModifiers m_modifiers;

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	int m_iProductionCost;
	bool m_bSpaceship;
	int m_eLaunchesVictory;
	std::vector<MapCategoryTypes> m_aeMapCategories;
	std::string m_szMovieDefineTag;
	std::map<int, int> m_victoryThreshold;
	std::map<int, int> m_victoryMinThreshold;
	int m_iVictoryDelayPercent;
	int m_iSuccessRate;

	// --- reverse-pass-fed availability FKs ---
	TechTypes m_eTechPrereq;
	std::set<int> m_projectsNeeded;
};

#endif // CV_JSON_PROJECT_INFO_H
