#pragma once
#ifndef CV_JSON_BUILD_INFO_H
#define CV_JSON_BUILD_INFO_H

//
//	CvBuildInfo -- the BUILD poco (worker actions, BUILD_*) rebuilt to the exemplar surface (patterns.md § THE
//	GETTER SETUP). Styled for the JSON anatomy (json.md §2): the §9 `produces` block -- what laying the build
//	creates: the improvement/route FKs, the plot-type changes, the per-feature add/remove rows (chop hammers /
//	extra time / tech gate) and the per-terrain terraform rows -- is ONE typed section member, served whole and
//	read per natural enum. The build's own tech MEANS gate + its positive bonus prereqs are authored as
//	requires.build atoms (census: a team-scoped TECH presence per gated build, plot-scoped BONUS_ connectivity
//	atoms on the 3 geoglyph builds) and MATERIALIZED at mapFrom from the composed tree
//	(docs/architecture/patterns.md §Materialize at mapFrom -- bare member reads, never per-call tree walks). cost.gold / cost.time +
//	identity.consumesUnit are intrinsics. Builds author NO §6 modifier family and NO §8 classification block
//	(census: type/produces/cost/ui/identity/world/requires only), so no modifier surface is composed. No legacy
//	getter name survives (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)); the ObsoleteTech plane is CENSUS-DELETED (0/304 builds
//	author `obsoletedBy`, 0 techs author `obsoletes.builds`, and no load pass ever fed the member).
//
//	Live callers (consumer rewiring is stage-4): getImprovement / getRoute -> worker-AI target selection;
//	isFeatureRemove -> CvCityAI/CvWorkerAI chop logic; getTime -> build-turn estimate; getTechPrereq ->
//	CvPlot::canBuild + the bTestVisible legacy leg of CvPlayer::canBuild + UI help (the flipped unlock gate is
//	the STANDARDIZED enabler's builds domain -- CvPlayer::m_enabler.builds, a bare member read);
//	getFeatureTech/getFeatureTime/getFeatureProduction -> the per-feature chop tech-gate/time/hammers
//	(CvPlot::getBuildTime/getFeatureProduction, CvPlayer::canBuild, CvDLLWidgetData help); isDisabled/setDisabled
//	-> a RUNTIME toggle (never JSON-authored) read live by CvPlayer::canBuild, written by mod Python settings
//	scripts (Assets/Python/Afforess/ANewDawnSettings.py).
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"    // ImprovementTypes / RouteTypes / TerrainTypes / FeatureTypes / BonusTypes / NO_*
#include "Defines/CvStructs.h"  // FeatureStruct / TerrainStructs
#include <vector>

// The §9 `produces` section -- what laying the build creates. One typed unit, fully redefined each mapFrom.
struct CvBuildProduces
{
	CvBuildProduces()
	{
		clear();
	}
	void clear()
	{
		eImprovement = NO_IMPROVEMENT;
		eRoute = NO_ROUTE;
		eTerrainChange = NO_TERRAIN;
		eFeatureChange = NO_FEATURE;
		featureRows.clear();
		terraformRows.clear();
	}
	ImprovementTypes eImprovement;               // produces.improvement (FK)
	RouteTypes eRoute;                           // produces.route (FK)
	TerrainTypes eTerrainChange;                 // produces.terrainChange (terraform-to terrain; NO_TERRAIN = none)
	FeatureTypes eFeatureChange;                 // produces.featureChange (feature planted/changed-to; NO_FEATURE = none)
	std::vector<FeatureStruct> featureRows;      // produces.features[] {feature, tech?, time?, production?, remove?}
	std::vector<TerrainStructs> terraformRows;   // produces.terraform[] {terrain, tech?, time?}
};

class CvBuildInfo : public CvInfo
{
public:
	CvBuildInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvRequires* getRequires() const { return &m_requires; }
	const CvBuildProduces& getProduces() const { return m_produces; }   // §9 produces (bespoke section)

	// ======================= 2. CLASSIFICATION / MODIFIER GROUPS -- none (builds author neither) ============

	// ======================= 3. PRODUCES reads -- the FKs + the per-enum row reads ==========================
	ImprovementTypes getImprovement() const { return m_produces.eImprovement; }
	RouteTypes getRoute() const { return m_produces.eRoute; }
	TerrainTypes getTerrainChange() const { return m_produces.eTerrainChange; }
	FeatureTypes getFeatureChange() const { return m_produces.eFeatureChange; }
	// per-feature rows: remove=true is the chop (+production hammers, +time); a remove=false entry carrying
	// only a tech is the per-feature TECH GATE (e.g. "road on a swamp needs Canal Systems")
	bool isFeatureRemove(FeatureTypes eFeature) const;
	TechTypes getFeatureTech(FeatureTypes eFeature) const;
	int getFeatureTime(FeatureTypes eFeature) const;
	int getFeatureProduction(FeatureTypes eFeature) const;
	// per-terrain terraform rows (CvPlayer::canBuild + CvPlot::getBuildTime walk these)
	TechTypes getTerraformTech(TerrainTypes eTerrain) const;
	int getTerraformTime(TerrainTypes eTerrain) const;

	// ======================= 4. REQUIRES views -- materialized at mapFrom ===================================
	// The build's OWN tech MEANS gate (requires.build's one team-scoped PRESENCE clause -- curate_build.py's
	// _requires() emits exactly one {type:TECH_x, scope:"team"} entry per gated build).
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	// The positive bonus prereqs (requires.build's plot-scoped BONUS_ presence clauses -- the 3 geoglyph
	// builds; the connection-gated placement check itself also rides the cascade requires.build evaluation).
	const std::vector<BonusTypes>& getPrereqBonuses() const { return m_aePrereqBonusTypes; }

	// ======================= 5. INTRINSIC -- bare typed reads (the census set) ==============================
	int getGoldCost() const { return m_iGoldCost; }             // cost.gold
	int getTime() const { return m_iTime; }                     // cost.time
	bool isConsumesUnit() const { return m_bConsumesUnit; }     // identity.consumesUnit (the build consumes the worker)
	// RUNTIME toggle (never JSON-authored) -- Python mod settings (ANewDawnSettings.py) flip builds on/off live.
	bool isDisabled() const { return m_bDisabled; }
	void setDisabled(bool bNewValue) { m_bDisabled = bNewValue; }
	// EXE-bound surface (served by the CvBuildInfo shim leaf)
	DllExport int getEntityEvent() const { return m_iEntityEvent; }   // world.art.entityEvent (the on-map worker animation)
	DllExport int getMissionType() const { return m_iMissionType; }   // RUNTIME-assigned (setMissionType at load, CvXMLLoadUtilitySet), NOT JSON
	void setMissionType(int iNewType) { m_iMissionType = iNewType; }

protected:
	virtual CvRequires* mutRequires() { return &m_requires; }

private:
	// --- the composed section units ---
	CvRequires m_requires;
	CvBuildProduces m_produces;   // the §9 produces section (typed at mapFrom)

	// --- the materialized requires.build views ---
	TechTypes m_eTechPrereq;                        // the team-scoped TECH presence clause
	std::vector<BonusTypes> m_aePrereqBonusTypes;   // the plot-scoped BONUS_ presence clauses

	// --- the intrinsic members (materialized once at mapFrom; getters are bare reads) ---
	int m_iGoldCost;        // cost.gold
	int m_iTime;            // cost.time
	int m_iEntityEvent;     // world.art.entityEvent (ENTITY_EVENT_* id; default ENTITY_EVENT_NONE)
	int m_iMissionType;     // RUNTIME-assigned via setMissionType (default NO_MISSION), never serialized/JSON
	bool m_bConsumesUnit;   // identity.consumesUnit
	bool m_bDisabled;       // RUNTIME toggle (default false; never JSON-authored)
};

#endif // CV_JSON_BUILD_INFO_H
