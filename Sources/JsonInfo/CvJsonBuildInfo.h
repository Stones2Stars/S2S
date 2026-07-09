#pragma once
#ifndef CV_JSON_BUILD_INFO_H
#define CV_JSON_BUILD_INFO_H

//
//	CvJsonBuildInfo -- the JSON real poco for worker BUILD actions (BUILD_*). Live-caller surface: what the build
//	PRODUCES (improvement / route / per-feature add-remove-tech-gate, json §9 `produces`) + its cost/time + whether
//	it consumes the worker + its own tech MEANS gate. No hotkey base (no getBuildInfo(...) hotkey caller). No
//	cascade here.
//
//	Live callers (verified 2026-07-07/08): getImprovement / getRoute -> worker-AI target selection; isFeatureRemove ->
//	CvCityAI/CvWorkerAI chop logic; getTime -> build-turn estimate; getType/getDescription -> UI; getTechPrereq ->
//	CvPlot/CvPlayer::canBuild + CascadeAccumulator::enBuildUnlockedFast (the fast-path tech gate) + UI help text;
//	getFeatureTech/getFeatureTime/getFeatureProduction -> the per-feature chop tech-gate/time/hammers
//	(CvPlot::getBuildTime/getFeatureProduction, CvPlayer::canBuild, CvDLLWidgetData help); getPlaceBonusTypes ->
//	CvPlot's place-a-bonus walk (curate_build.py: the struct is DROPPED, 0/304 builds author it, so this is
//	permanently empty pending the #430 outcome-system place-bonus capability); isDisabled/setDisabled -> a RUNTIME
//	toggle (never JSON-authored) read by CascadeAccumulator::enBuildUnlockedFast / CvPlayer::canBuild, written by
//	mod Python settings scripts (Assets/Python/Afforess/ANewDawnSettings.py).
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"    // ImprovementTypes / RouteTypes / FeatureTypes / NO_*
#include "Defines/CvStructs.h"  // FeatureStruct / PlaceBonusTypes
#include <vector>

class CvJsonBuildInfo : public CvJsonInfo
{
public:
	CvJsonBuildInfo();

	ImprovementTypes getImprovement() const { return m_eImprovement; }
	RouteTypes getRoute() const { return m_eRoute; }
	int getTime() const { return m_iTime; }
	int getCost() const { return m_iCost; }
	bool isKill() const { return m_bKill; }                 // consumes the worker
	bool isFeatureRemove(FeatureTypes e) const;             // clears feature e (produces.features[].remove)
	TechTypes getFeatureTech(FeatureTypes e) const;         // per-feature PrereqTech gate (produces.features[].tech)
	int getFeatureTime(FeatureTypes e) const;               // per-feature extra build time (produces.features[].time)
	int getFeatureProduction(FeatureTypes e) const;         // per-feature chop hammers (produces.features[].production)
	// Single-FK plot-type-change outcomes (real data; archived return type is int, consumers cast to the enum + test
	// against NO_TERRAIN/NO_FEATURE, so the enum-default IS the "no change" sentinel).
	int getTerrainChange() const { return m_eTerrainChange; } // produces.terrainChange (terraform-to terrain; NO_TERRAIN default)
	int getFeatureChange() const { return m_eFeatureChange; } // produces.featureChange (feature planted/changed-to; NO_FEATURE default)
	// Per-terrain terraform time + tech gate list (real data; TerrainStructs{eTerrain, ePrereqTech, iTime}).
	const std::vector<TerrainStructs>& getTerrainStructs() const { return m_aTerrainStructs; }   // produces.terraform[]
	// The build's OWN tech MEANS gate (requires.build; reconstructed at load from the team-scoped PRESENCE clause --
	// curate_build.py's _requires() emits exactly one {type:TECH_x, scope:"team"} entry per build, alongside any
	// plot-scoped bonus-connectivity clauses, so a team-scoped presence node IS the tech prereq, unambiguously).
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	// The build's PrereqBonusTypes -- REAL DATA reconstructed at load from requires.build's plot-scoped BONUS_
	// presence clauses (curate_build.py _requires(): {type:BONUS_x, scope:"plot", connection:"trade"} -- the only
	// plot-scoped clauses a build authors; 3 geoglyph builds carry them, all others empty). The connection-gated
	// placement check itself also rides the cascade requires.build evaluation; this is the positive-prereq view.
	const std::vector<BonusTypes>& getPrereqBonuses() const { return m_aePrereqBonusTypes; }
	// ObsoleteTech is store-inverted to tech.obsoletes.builds (curate_build.py:50; the obsolete gate itself rides the
	// cascade -- EnablerKernel::obsoletedByHeldTech, enBuildUnlockedFast). The compat getter FK is reconstructed at LOAD
	// by the cascadeLoadJson tech-FK reverse-index pass (the Route<-bonus pattern), which calls setObsoleteTech.
	TechTypes getObsoleteTech() const { return m_eObsoleteTech; }
	void setObsoleteTech(TechTypes e) { m_eObsoleteTech = e; }   // load-time reverse-index writer (cascadeLoadJson)
	// CURATOR-GAP (deliberate) -- PlaceBonusTypes is DROPPED by curate_build.py (0/304 builds author it; owner
	// 2026-06-16: the place-a-bonus capability becomes canonical #430 outcome tooling, not this XML-era struct). No
	// build-JSON field to map; the member stays a real empty vector so reference-returning callers see "no bonuses placed".
	const std::vector<PlaceBonusTypes>& getPlaceBonusTypes() const { return m_aPlaceBonusTypes; }
	// CURATOR-GAP (deliberate) -- MapCategoryTypes is DROPPED by curate_build.py (0/304; a SPACEMAP placement gate,
	// deferred to the spacemap fix). No build-JSON field to map; the member stays a real empty vector.
	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategoryTypes; }
	// CURATOR-GAP (deliberate) -- Categories is DROPPED by curate_build.py (0/304 builds author it; ZERO
	// getBuildInfo(...).getCategory consumers -- dead). No build-JSON field to map; the member stays a real empty
	// vector (getCategory is bounds-safe, never called).
	int getCategory(int i) const { return (i >= 0 && i < (int)m_aiCategories.size()) ? m_aiCategories[i] : 0; }
	int getNumCategories() const { return (int)m_aiCategories.size(); }
	bool isCategory(int i) const
	{ for (size_t k = 0; k < m_aiCategories.size(); ++k) if (m_aiCategories[k] == i) return true; return false; }
	// RUNTIME toggle (never JSON-authored) -- Python mod settings (ANewDawnSettings.py) flip builds on/off live.
	bool isDisabled() const { return m_bDisabled; }
	void setDisabled(bool bNewVal) { m_bDisabled = bNewVal; }
	// EXE-bound surface (mapscript/EXE map gen -- served by the CvBuildInfo shim leaf, cascade-engine-430.md §3)
	int getEntityEvent() const { return m_iEntityEvent; }   // world.art.entityEvent (the on-map worker animation)
	int getMissionType() const { return m_iMissionType; }   // RUNTIME-assigned (setMissionType at load, CvXMLLoadUtilitySet), NOT JSON
	void setMissionType(int iNewType) { m_iMissionType = iNewType; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonRequires* getRequires() const { return &m_requires; }

protected:
	virtual CvJsonRequires* mutRequires() { return &m_requires; }

private:
	CvJsonRequires m_requires;
	ImprovementTypes m_eImprovement;   // produces.improvement
	RouteTypes m_eRoute;               // produces.route
	TerrainTypes m_eTerrainChange;     // produces.terrainChange (NO_TERRAIN default)
	FeatureTypes m_eFeatureChange;     // produces.featureChange (NO_FEATURE default)
	int m_iTime;                       // cost.time
	int m_iCost;                       // cost.gold
	int m_iEntityEvent;                // world.art.entityEvent (ENTITY_EVENT_* id; default ENTITY_EVENT_NONE)
	int m_iMissionType;                // RUNTIME-assigned via setMissionType (default NO_MISSION), never serialized/JSON
	bool m_bKill;                      // identity.consumesUnit
	bool m_bDisabled;                  // RUNTIME toggle (default false; never JSON-authored)
	TechTypes m_eTechPrereq;           // reconstructed from requires.build's team-scoped PRESENCE clause at load
	TechTypes m_eObsoleteTech;         // store-inverted tech.obsoletes.builds, reconstructed at load (cascadeLoadJson)
	std::vector<FeatureStruct> m_aFeatureStructs;      // produces.features[] {feature, tech?, time?, production?, remove?}
	std::vector<TerrainStructs> m_aTerrainStructs;     // produces.terraform[] {terrain, tech?, time?}
	std::vector<PlaceBonusTypes> m_aPlaceBonusTypes;   // curator-gap: PlaceBonusTypes dropped (0/304) -- always empty
	std::vector<BonusTypes> m_aePrereqBonusTypes;      // reconstructed from requires.build plot-scoped BONUS_ clauses
	std::vector<MapCategoryTypes> m_aeMapCategoryTypes; // curator-gap: MapCategoryTypes dropped (0/304) -- always empty
	std::vector<int> m_aiCategories;                   // curator-gap: Categories dropped (0/304, dead) -- always empty
};

#endif // CV_JSON_BUILD_INFO_H
