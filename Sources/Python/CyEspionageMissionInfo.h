#pragma once

#ifndef CyEspionageMissionInfo_h__
#define CyEspionageMissionInfo_h__

//
//	CyEspionageMissionInfo -- the ESPIONAGEMISSION accessor, a sibling of CyWorldInfo / CyGameSpeedInfo on the
//	PER-INFO plane the Python read boundary is built from ([patterns.md] THE PYTHON READ BOUNDARY). The id is the
//	mission, passed per call.
//
//	⛔ WHY THESE ARE NAMED HERE RATHER THAN ON THE GENERIC PLANE -- and this registry is the case that PROVED the
//	rule rather than merely illustrating it. Every read below belongs to ONE registry, so a
//	`getIntrinsic("ESPIONAGEMISSION_", id, PYINT_...)` slot read leaves the call site naming a slot instead of the
//	value. That is not only opaque, it FAILS SILENTLY: a slot wired for some other prefix falls through to -1,
//	which is indistinguishable from a legitimate answer. The espionage advisor asked the generic plane for a COST
//	that was only ever wired for BUILDING_, read -1 for all 29 missions, classified none of them, and handed the
//	engine a -1 mission id -- surfacing as an ACCESS_VIOLATION inside a boost::python call, a whole screen away
//	from the read that was wrong. A named accessor cannot fail that way: an unwired read does not compile.
//
//	⚠ EACH READ IS EARNED BY A LIVE CALL SITE -- the five here are exactly what the espionage advisor's mission
//	classification loop asks for. It is not a mirror of the legacy per-field getter contract
//	([DEC-new-getter-surface]), which is a different axis: CvEspionageMissionInfo carries ~25 fields and the
//	other ~20 stay unpublished until something actually asks.
//
class CyEspionageMissionInfo
{
public:
	CyEspionageMissionInfo() {}

	// The mission's base cost. -1 means the id does not name a mission, so a caller can tell "not a mission"
	// from "a mission that happens to be free".
	int getCost(int iMission) const;
	// Passive missions are the ones bought once and read thereafter (see demographics / research / the city
	// visibility tier), as opposed to an acted mission.
	bool isPassive(int iMission) const;
	// The three passive kinds the advisor distinguishes; a passive mission matching none of them is the plain
	// city-visibility one.
	bool isInvestigateCity(int iMission) const;
	bool isSeeDemographics(int iMission) const;
	bool isSeeResearch(int iMission) const;

	static void pythonPublish();
};

#endif // CyEspionageMissionInfo_h__
