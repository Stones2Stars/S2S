#pragma once

#ifndef CyImprovementInfo_h__
#define CyImprovementInfo_h__

#include <boost/python/list.hpp>

//
//	CyImprovementInfo -- the IMPROVEMENT accessor, one of the PER-INFO accessors the Python read boundary is built
//	from ([patterns.md] THE PYTHON READ BOUNDARY).
//
//	⛔ HOMING IS THE REQUIREMENT, AND "NAMED" ALONE DOES NOT SATISFY IT (owner): an endpoint belongs on the
//	accessor for the type it addresses, because a flat class accumulating UNIT, BUILDING and IMPROVEMENT reads
//	side by side is the spaghetti wearing named endpoints. So these live here rather than on [CyInfo], whose job
//	is what is genuinely UNIFORM across every registry -- identity text, the classification tests, the edge
//	families, the group reads.
//
//	⛔ AND THE CALL SITE NAMES THE THING IT FETCHES. A generic slot read is decoupled from the global context and
//	STILL fails that test, because the caller names a slot rather than the value.
//
//	⚑ WHAT THESE PUBLISH IS ALREADY CARRIED. Every read below is a bare fetch of the improvement's own authored
//	member: the placement gates are what `CvPlot::canHaveImprovement` runs on and the build list is what the
//	worker AI walks, so the engine could not function without them. The boundary was missing a read, never the
//	data -- so nothing here derives, and nothing here is a legacy per-FIELD getter revived
//	(docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy) bans mirroring the old `CvXInfo` contract; a named accessor per info TYPE is a
//	different axis).
//
//	The id is the IMPROVEMENT, passed per call: an accessor holds no bound entity, so it is a plain read surface
//	rather than a handle with a lifetime.
//
class CyImprovementInfo
{
public:
	CyImprovementInfo() {}

	// The BUILDS that lay this improvement -- the reverse of each build's `produces.improvement` ([json.md] 2:
	// `produces` is an auxiliary section, not an `enables` edge).
	// ⛔ NOT an edge read: the reverse pass lands a build onto the TECHS its per-terrain/per-feature rows gate
	// on, never onto the improvement, so an EDGEB_BUILDS lookup answers empty -- which reads exactly like "no
	// build makes this". What carries it is the improvement's own member, filled by that same pass.
	python::list getBuilds(int iImprovement) const;

	// The PLACEMENT axes: what a plot must hold for this improvement to be laid on it.
	bool isValidOnBonus(int iImprovement, int iBonus) const;
	bool isValidOnTerrain(int iImprovement, int iTerrain) const;
	bool isValidOnFeature(int iImprovement, int iFeature) const;
	bool isValidOnPeak(int iImprovement) const;
	bool isValidOnHills(int iImprovement) const;

	// The RELIEF / WATER / ADJACENCY restrictions on where it may be laid, each its own named question.
	bool isWaterOnly(int iImprovement) const;
	bool isPeakOnly(int iImprovement) const;
	bool isFlatlandsOnly(int iImprovement) const;
	bool isRiverSideOnly(int iImprovement) const;
	bool isRequiresFeature(int iImprovement) const;
	bool isRequiresIrrigation(int iImprovement) const;
	bool isNoFreshWater(int iImprovement) const;

	/// <summary>Turns this improvement takes to mature into its `identity.upgradesTo` successor; 0 when it
	/// never upgrades, which is what a caller tests to decide whether an upgrade timer exists at all.</summary>
	int getUpgradeTime(int iImprovement) const;

	static void pythonPublish();
};

#endif // CyImprovementInfo_h__
