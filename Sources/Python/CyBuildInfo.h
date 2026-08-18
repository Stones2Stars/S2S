#pragma once

#ifndef CyBuildInfo_h__
#define CyBuildInfo_h__

#include <boost/python/list.hpp>

//
//	CyBuildInfo -- the BUILD accessor, one of the PER-INFO accessors the Python read boundary is built from
//	([patterns.md] THE PYTHON READ BOUNDARY).
//
//	⛔ HOMING IS THE REQUIREMENT, AND "NAMED" ALONE DOES NOT SATISFY IT (owner): an endpoint belongs on the
//	accessor for the type it addresses. These live here rather than on [CyInfo], whose job is what is genuinely
//	UNIFORM across every registry -- identity text, the classification tests, the edge families, the group reads.
//
//	⚑ WHAT THESE PUBLISH IS ALREADY CARRIED. Every read is a bare fetch of the build's own authored member: the
//	cost/time it charges, what laying it PRODUCES (json par.9 `produces`), and its per-feature rows. The boundary
//	was missing a read, never the data.
//
//	⛔ THERE IS NO "WHICH UNITS CAN DO THIS" READ HERE, and that is deliberate rather than an omission. It is a
//	CROSS-LINK, so it is answered by the build's own reverse edge family (EDGEF_RELATED / EDGEB_UNITS, landed
//	once at load by the reverse pass) through the generic CyInfo::getEdgeIds -- never by this accessor sweeping
//	the unit registry per call, which is the own-data inversion docs/cascade.md §1 (reverse lookups are populated once, at load) bans.
//
class CyBuildInfo
{
public:
	CyBuildInfo() {}

	// What laying this build PRODUCES -- the `produces` FKs. -1 when it produces neither.
	int getImprovement(int iBuild) const;
	int getRoute(int iBuild) const;

	// The build's own cost. `getTime` is x100 like every amount, so a reader reduces at its point of use
	// (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)); the gold cost is a whole count and is not scaled.
	int getGoldCost(int iBuild) const;
	int getTime(int iBuild) const;

	// The build CONSUMES the worker that performs it.
	bool isConsumesUnit(int iBuild) const;

	// The team-scoped TECH this build needs, or -1.
	int getTechPrereq(int iBuild) const;

	// The build's per-FEATURE rows -- one dict per feature the build ACTS ON, each carrying
	// {"feature", "tech", "time", "production", "remove"}.
	//
	// ⚑ It is the build's OWN authored list, not a registry walk. The legacy page swept every feature id asking
	// this build four questions about each, which is the own-data inversion docs/cascade.md §1 (reverse lookups are populated once, at load) bans: a
	// build acts on a handful of features and already names them.
	// ⚠ `tech` is -1 where the row gates on nothing, and `time` is x100 like every amount.
	python::list getFeatureRows(int iBuild) const;

	static void pythonPublish();
};

#endif // CyBuildInfo_h__
