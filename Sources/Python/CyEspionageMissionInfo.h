#pragma once

#ifndef CyEspionageMissionInfo_h__
#define CyEspionageMissionInfo_h__

//
//	CyEspionageMissionInfo -- the ESPIONAGEMISSION accessor, a sibling of CyWorldInfo / CyGameSpeedInfo on the
//	PER-INFO plane the Python read boundary is built from ([patterns.md] THE PYTHON READ BOUNDARY). The id is the
//	mission, passed per call.
//
//	Why these reads are named here rather than asked of the generic slot plane: docs/architecture/patterns/
//	06-the-python-read-boundary-one.md (the espionage advisor crash). Each read is earned by a live call site, the
//	espionage advisor's mission list, never mirrored from the legacy per-field getters.
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
	///<summary>Whether the mission is aimed at one of the target's cities rather than at the target itself.</summary>
	bool isTargetsCity(int iMission) const;
	///<summary>The tech the mission needs, or -1.</summary>
	int getTechPrereq(int iMission) const;

	static void pythonPublish();
};

#endif // CyEspionageMissionInfo_h__
