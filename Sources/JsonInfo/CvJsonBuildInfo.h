#pragma once
#ifndef CV_JSON_BUILD_INFO_H
#define CV_JSON_BUILD_INFO_H

//
//	CvJsonBuildInfo -- the JSON real poco for worker BUILD actions (BUILD_*). Live-caller surface: what the build
//	PRODUCES (improvement / route / feature-removal, json §9 `produces`) + its cost/time + whether it consumes the
//	worker. The tech prerequisite / obsolete-tech ride the CvJsonInfo base availability model (requires.build). No
//	hotkey base (no getBuildInfo(...) hotkey caller). No cascade here.
//
//	Live callers (verified 2026-07-07): getImprovement / getRoute -> worker-AI target selection; isFeatureRemove ->
//	CvCityAI/CvWorkerAI chop logic; getTime -> build-turn estimate; getType/getDescription -> UI.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // ImprovementTypes / RouteTypes / FeatureTypes / NO_*
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
	bool isFeatureRemove(int iFeature) const;               // clears feature iFeature
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
	int m_iTime;                       // cost.time
	int m_iCost;                       // cost.gold
	int m_iEntityEvent;                // world.art.entityEvent (ENTITY_EVENT_* id; default ENTITY_EVENT_NONE)
	int m_iMissionType;                // RUNTIME-assigned via setMissionType (default NO_MISSION), never serialized/JSON
	bool m_bKill;                      // identity.consumesUnit
	std::vector<FeatureTypes> m_aeFeatureRemove;   // produces.featureRemove (features this build clears)
	// ⏳ NOT yet mapped (need the curator's per-feature `produces` shape confirmed): getFeatureTime /
	//    getFeatureProduction / getFeatureTech (the chop time / hammers / prereq per removed feature).
};

#endif // CV_JSON_BUILD_INFO_H
