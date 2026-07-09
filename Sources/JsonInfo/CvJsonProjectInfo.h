#pragma once
#ifndef CV_JSON_PROJECT_INFO_H
#define CV_JSON_PROJECT_INFO_H

//
//	CvJsonProjectInfo -- the JSON real poco for PROJECTS (team-built endeavours: spaceship parts, Manhattan/SDI class,
//	the Internet, …). Carries the project's team/empire/world-scope values + the bespoke non-cascade `victory` launch
//	params + placement flags. Availability rides the base: the tech prereq (tech.enables.projects), the prerequisite
//	projects (their enables.projects), the world-scope "anyone built one" gate (requires.build {type,scope:world}), the
//	game-wide specialBuilding/specialUnit unlocks (enables.specialBuildings / grants.grantsSpecialUnit), and the
//	world/team instance caps (allowed). HUMAN-native (no ×100 in this type). No cascade here.
//
//	Live callers (verified 2026-07-07): the maintenance/inflation/happiness/health/commerce/tradeRoutes/nukeInter/
//	techShare mods -> CvTeam::processProjectChange + CvCityAI valuation + pedia; victory* -> CvTeam/CvGame victory;
//	isSpaceship/isAllowsNukes/getVictoryPrereq/getMapCategories -> build + placement; getMax{Global,Team}Instances -> caps.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // NUM_COMMERCE_TYPES / COMMERCE_* / MapCategoryTypes
#include <vector>
#include <map>
#include <set>

class CvJsonProjectInfo : public CvJsonInfo
{
public:
	CvJsonProjectInfo();

	int getProductionCost() const { return m_iProductionCost; }                              // cost.create (hammers)
	int getNukeInterception() const { return m_iNukeInterception; }                          // combat.team.nukeInterception.percent (family name PROVISIONAL)
	int getTechShare() const { return m_iTechShare; }                                        // diplomacy.team.techShare.flat        (family name PROVISIONAL)
	int getGlobalMaintenanceModifier() const { return m_iGlobalMaintenanceModifier; }        // maintenance.empire.all.percent
	int getDistanceMaintenanceModifier() const { return m_iDistanceMaintenanceModifier; }    // maintenance.empire.distance.percent
	int getNumCitiesMaintenanceModifier() const { return m_iNumCitiesMaintenanceModifier; }  // maintenance.empire.numCities.percent
	int getConnectedCityMaintenanceModifier() const { return m_iConnectedCityMaintenanceModifier; } // maintenance.empire.connectedCity.percent
	int getInflationModifier() const { return m_iInflationModifier; }                        // upkeep.empire.inflation.percent
	int getGlobalHappiness() const { return m_iGlobalHappiness; }                            // happiness.empire.flat
	int getGlobalHealth() const { return m_iGlobalHealth; }                                  // health.empire.flat
	int getWorldHappiness() const { return m_iWorldHappiness; }                              // happiness.world.flat
	int getWorldHealth() const { return m_iWorldHealth; }                                    // health.world.flat
	int getWorldTradeRoutes() const { return m_iWorldTradeRoutes; }                          // tradeRoutes.world.flat
	int getCommerceModifier(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceModifier[i] : 0; }  // {c}.empire.percent
	const int* getCommerceModifierArray() const { return m_aiCommerceModifier; }

	// bespoke non-cascade `victory` launch params (json.md §9)
	int getVictoryThreshold(int iVictory) const { return mapGet(m_victoryThreshold, iVictory); }        // victory.thresholds
	int getVictoryMinThreshold(int iVictory) const { return mapGet(m_victoryMinThreshold, iVictory); }  // victory.minThresholds
	int getVictoryDelayPercent() const { return m_iVictoryDelayPercent; }                    // victory.delayPercent
	int getSuccessRate() const { return m_iSuccessRate; }                                    // victory.successRate
	int getVictoryPrereq() const { return m_eLaunchesVictory; }                              // identity.launchesVictory (VICTORY_* completion LAUNCHES; -1 none)

	// build-this-project-faster while a bonus is present (kept on source; the enabled-gate is cascade-side)
	int getBonusProductionModifier(int iBonus) const { return mapGet(m_bonusProduction, iBonus); }      // buildRate.self.percent (bonus-keyed)

	bool isSpaceship() const { return m_bSpaceship; }        // identity.spaceship
	bool isAllowsNukes() const { return m_bAllowsNukes; }    // identity.allowsNukes

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }
	const char* getCreateSound() const { return m_szCreateSound.c_str(); }                   // sound.onCompletion

	// --- mirrored legacy CvProjectInfo getters (consumer surface) ---
	int getAnyoneProjectPrereq() const { return m_iAnyoneProjectPrereq; }   // requires.build{type,scope:world} (no current project authors it -> NO_PROJECT)
	int getEveryoneSpecialUnit() const { return m_iEveryoneSpecialUnit; }   // grants.grantsSpecialUnit (no current project authors it -> NO_SPECIALUNIT)
	const char* getMovieArtDef() const { return m_szMovieArtDef.c_str(); }  // ui.art.movie.defineTag
	int getEveryoneSpecialBuilding() const                                 // enables.specialBuildings (first; legacy carried one) -> NO_SPECIALBUILDING when absent
	{ const std::vector<int>* v = edge("enables.specialBuildings"); return (v && !v->empty()) ? (*v)[0] : -1; }
	// TechPrereq and PrereqProjects are store-INVERTED onto the OTHER entity (tech.enables.projects / the prerequisite
	// project's enables.projects, set-based -- the per-edge iNeeded count is dropped, all 1 today, curate_project.py:27).
	// The project's own JSON carries NO back-reference, so they are reconstructed at LOAD by the cascadeLoadJson tech-FK
	// reverse-index pass (the Route<-bonus pattern), which calls the setters below.
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	int getProjectsNeeded(int i) const { return m_projectsNeeded.count(i) ? 1 : 0; }   // 1 if project i is a prereq (count dropped)
	void setTechPrereq(TechTypes e) { m_eTechPrereq = e; }               // load-time reverse-index writers (cascadeLoadJson)
	void addProjectNeeded(int iProject) { m_projectsNeeded.insert(iProject); }

	// instance caps -- the base allowedCap read-through over the composed `allowed` unit; -1 = unlimited (absent,
	// per the legacy convention -- the base helper's own convention).
	int getMaxGlobalInstances() const { return allowedCap("world"); }
	int getMaxTeamInstances() const { return allowedCap("team"); }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonAllowed*   getAllowed()   const { return &m_allowed; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonAllowed*   mutAllowed()   { return &m_allowed; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	static int mapGet(const std::map<int, int>& m, int k) { std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }

	CvJsonEdges     m_edges;
	CvJsonAllowed   m_allowed;
	CvJsonModifiers m_modifiers;
	int m_iProductionCost;
	int m_iNukeInterception, m_iTechShare;
	int m_iGlobalMaintenanceModifier, m_iDistanceMaintenanceModifier, m_iNumCitiesMaintenanceModifier, m_iConnectedCityMaintenanceModifier;
	int m_iInflationModifier;
	int m_iGlobalHappiness, m_iGlobalHealth, m_iWorldHappiness, m_iWorldHealth, m_iWorldTradeRoutes;
	int m_aiCommerceModifier[NUM_COMMERCE_TYPES];
	std::map<int, int> m_victoryThreshold, m_victoryMinThreshold;
	int m_iVictoryDelayPercent, m_iSuccessRate;
	int m_eLaunchesVictory;
	std::map<int, int> m_bonusProduction;
	bool m_bSpaceship, m_bAllowsNukes;
	std::vector<MapCategoryTypes> m_aeMapCategories;
	std::string m_szCreateSound;
	std::string m_szMovieArtDef;      // ui.art.movie.defineTag
	int m_iEveryoneSpecialUnit;       // grants.grantsSpecialUnit (NO_SPECIALUNIT default)
	int m_iAnyoneProjectPrereq;       // requires.build{type,scope:world} (NO_PROJECT default)
	TechTypes m_eTechPrereq;          // store-inverted tech.enables.projects, reconstructed at load (cascadeLoadJson)
	std::set<int> m_projectsNeeded;   // store-inverted PrereqProjects (prereq project's enables.projects), reconstructed at load
};

#endif // CV_JSON_PROJECT_INFO_H
